/*
 * Copyright (C) 2017
 * Authors:	Ivan Khoronzhuk <ivan.khoronzhuk@linaro.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "plget_args.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>

#define MEASUREMENTS_NUM		5
#define NSEC_PER_USEC			1000ULL

static void res_print_clock_info(int clock, char *clock_name)
{
	struct timespec res[MEASUREMENTS_NUM];
	struct timespec ts1, ts2;
	int res_is_avarage;
	double aver_res;
	int i, j, k;
	int ret;

	ret = clock_getres(clock, &ts1);
	if (ret < 0)
		perror("clock_getres");

	printf("-----------------------------------------\n");
	printf("%s info:\n", clock_name);
	printf("Declared resolution: %ldns\n", ts1.tv_nsec);

	for (j =0; j < MEASUREMENTS_NUM; j++) {
		for (i = 0, k = 0; k < 8;) {
			clock_gettime(clock, &ts1);
			clock_gettime(clock, &ts2);

			if (ts1.tv_nsec != ts2.tv_nsec) {
				if (i > 1)
					break;
				else {
					k++;
					continue;
				}
			}
			i++;
		}

		ts_sub(&ts2, &ts1, &res[j]);
		if (res[j].tv_sec)
			plget_fail("too long time diff");
	}

	/* check if res is real or average */
	aver_res = res[0].tv_nsec;
	for (i = 1; i < MEASUREMENTS_NUM; i++) {
		if (res[i].tv_nsec != res[i-1].tv_nsec)
			res_is_avarage = 1;
		aver_res += res[i].tv_nsec;
	}

	if (res_is_avarage) {
		aver_res /= MEASUREMENTS_NUM;
		printf("Average \"userspace\" access resolution: %gns\n", aver_res);
	} else {
		printf("Real resolution: %ldns\n", res->tv_nsec);
	}

	/* Count in 1sec*/
	clock_gettime(clock, &ts1);
	printf("%s count:", clock_name);
	for (i = 0; i < 3;) {
		clock_gettime(clock, &ts2);
		ts_sub(&ts2, &ts1, res);
		if (res->tv_sec >= 1) {
			ts1 = ts2;
			printf(" %d", ++i);
			fflush(stdout);
		}
	}
	printf("\n");
}

static int res_tx_lat_print(void)
{
	struct timespec *rtime;
	int print_flags;
	struct stats *v;
	int n = 0;

	print_flags = plget->flags & PLF_PLAIN_FORMAT ? STATS_PLAIN_OUTPUT : 0;

	if (plget->flags & PLF_LATENCY_STAT) {
		stats_diff(&tx_hw_v, &tx_sw_v, &temp);
		n |= stats_print("\ndma + NIC tx latency, us (not complete "
				 "driver latency, driver s/w ts -> wire)" ,
				 &temp, print_flags, NULL);

		stats_diff(&tx_sw_v, &tx_app_v, &temp);
		n |= stats_print("\nstack + packet scheduler + part of "
				 "driver tx latency, us (app -> some place in the "
				 "NIC driver, app -> driver s/w ts)",
				  &temp, print_flags, NULL);

		stats_diff(&tx_hw_v, &tx_app_v, &temp);
		n |= stats_print("\ncomplete tx latency, us (driver latency + "
				 "stack latency, app -> wire)",
				  &temp, print_flags, NULL);
	}

	if (plget->flags & PLF_SCHED_STAT) {
		struct stats *v;
		int i;

		v = tx_sch_v;
		stats_diff(v, &tx_app_v, &temp);
		n |= stats_print("\nstack tx latency, us (based on s/w "
				 "timestamps, app -> packet scheduler)",
				  &temp, print_flags, NULL);

		for (i = 1; i < plget->dev_deep; i++) {
			printf("psched%d -> psched%d\n", i, i + 1);
			stats_diff(&tx_sch_v[i], v, &temp);
			n |= stats_print("\nbetween device (sched) tx latency, "
					 "us (based on s/w timestamps, psched "
					 "-> psched)",
					 &temp, print_flags, NULL);
			v = &tx_sch_v[i];
		}

		stats_diff(&tx_sw_v, v, &temp);
		n |= stats_print("\npacket scheduler + part of driver tx "
				 "latency, us (packet scheduler -> driver s/w "
				 "ts)", &temp, print_flags, NULL);

		stats_diff(&tx_hw_v, v, &temp);
		n |= stats_print("\ndriver + packet scheduler tx latency, "
				 "us (packet scheduler entrance -> wire)",
				  &temp, print_flags, NULL);
	}

	if (plget->flags & PLF_HW_STAT) {
		if (plget->flags & PLF_RTIME) {
			rtime = &plget->rtime;
		} else {
			rtime = plget->mod == ECHO_LAT ? rx_hw_v.start_ts :
							 tx_hw_v.start_ts;
		}

		n |= stats_print("\nhw tx time, us", &tx_hw_v, print_flags,
				 rtime);
	}

	if (plget->flags & PLF_IPGAP_STAT) {
		v = (plget->flags & PLF_DIS_HW_TS) ? &rx_sw_v : &rx_hw_v;
		n |= stats_print("\ngap of hw tx time, us", v,
				  print_flags | STATS_GAP_DATA, NULL);
	}

	return n;
}

static int res_rx_lat_print(void)
{
	struct timespec *rtime;
	int print_flags;
	struct stats *v;
	int n = 0;

	print_flags = plget->flags & PLF_PLAIN_FORMAT ? STATS_PLAIN_OUTPUT : 0;

	if (plget->flags & PLF_HW_STAT) {
		if (plget->flags & PLF_RTIME)
			rtime = &plget->rtime;
		else
			rtime = plget->mod == RTT_MOD ? tx_hw_v.start_ts :
							rx_hw_v.start_ts;

		n |= stats_print("\nhw rx time, us", &rx_hw_v, print_flags,
				 rtime);
	}

	if (plget->flags & PLF_IPGAP_STAT) {
		v = (plget->flags & PLF_DIS_HW_TS) ? &rx_sw_v : &rx_hw_v;
		n |= stats_print("\ngap of sw rx time, us", v,
				 print_flags | STATS_GAP_DATA, NULL);
	}

	if (plget->flags & PLF_LATENCY_STAT) {
		stats_diff(&rx_sw_v, &rx_hw_v, &temp);
		n |= stats_print("\ndriver rx latency, us (no stack latency, "
				 "wire -> net subsystem)",
				 &temp, print_flags, NULL);
		stats_diff(&rx_app_v, &rx_sw_v, &temp);
		n |= stats_print("\nstack rx latency, us (no driver latency,  "
				 "net subsystem -> app)",
				 &temp, print_flags, NULL);
		stats_diff(&rx_app_v, &rx_hw_v, &temp);
		n |= stats_print("\ncomplete rx latency, us (driver latency + "
				 "stack latency, wire -> app)",
				 &temp, print_flags, NULL);
	}

	return n;
}

static void res_rtt_print(void)
{
	struct stats *a_stat, *b_stat;
	char *a_ts_base, *b_ts_base;
	int print_flags;

	print_flags = plget->flags & PLF_PLAIN_FORMAT ? STATS_PLAIN_OUTPUT : 0;

	if (ts_correct(tx_hw_v.start_ts)) {
		a_stat = &tx_hw_v;
		a_ts_base = "hw";
	} else if (ts_correct(tx_sw_v.start_ts)) {
		a_stat = &tx_sw_v;
		a_ts_base = "sw";
	} else {
		a_stat = &tx_app_v;
		a_ts_base = "app";
	}

	if (ts_correct(rx_hw_v.start_ts)) {
		b_stat = &rx_hw_v;
		b_ts_base = "hw";
	} else if (ts_correct(rx_sw_v.start_ts)) {
		b_stat = &rx_sw_v;
		b_ts_base = "sw";
	} else {
		b_stat = &rx_app_v;
		b_ts_base = "app";
	}

	printf("RTT (round trip time) for this HOST based on "
		"tx %s and rx %s timestamps\n", a_ts_base, b_ts_base);

	stats_diff(b_stat, a_stat, &temp);
	stats_print("\nRTT (no rx/tx latencies of this HOST, us",
		    &temp, print_flags, NULL);
}

static struct stats *res_best_rx_vect(void)
{
	if (ts_correct(rx_hw_v.start_ts))
		return &rx_hw_v;
	else if (ts_correct(rx_sw_v.start_ts))
		return &rx_sw_v;
	else
		return &rx_app_v;
}

static struct stats *res_best_tx_vect(void)
{
	if (ts_correct(tx_hw_v.start_ts))
		return &tx_hw_v;
	else if (ts_correct(tx_sw_v.start_ts))
		return &tx_sw_v;
	else
		return &tx_app_v;
}

void res_title_print(void)
{
	struct timespec ts1, ts2, res;
	int ptp_fd;

	if (!(plget->flags & PLF_TITLE))
		return;

	res_print_clock_info(CLOCK_REALTIME, "CLOCK_REALTIME");

	if (access("/dev/ptp0", 0)) {
		perror("PHC is not registered");
		goto ptp_err;
	}

	ptp_fd = open("/dev/ptp0", O_RDWR);
	if (ptp_fd == -1) {
		perror("open PHC");
		goto ptp_err;
	}

	res_print_clock_info(ptp_fd, "PHC");
	printf("-----------------------------------------\n");

	/* Check roughly if timeline is same for Sys and PHC */
	clock_gettime(ptp_fd, &ts1);
	clock_gettime(CLOCK_MONOTONIC, &ts2);
	ts_sub(&ts2, &ts1, &res);
	int usec_diff = res.tv_nsec / NSEC_PER_USEC;
	if (res.tv_sec || usec_diff > 5000)
		printf("You need to run phc2sys in order to align timeline\n");

	printf("PHC vs CLOCK MONOTONIC = %lus %luns\n", res.tv_sec, res.tv_nsec);

	close(ptp_fd);
ptp_err:
	printf("-----------------------------------------\n");
}

void res_print_time(void)
{
	struct timespec time;
	__u64 val;

	clock_gettime(CLOCK_REALTIME, &time);

	val = NSEC_PER_SEC * time.tv_sec + time.tv_nsec;
	printf("%llu\n", val);
}

int res_get_intf_speed(void)
{
	struct ethtool_cmd edata;
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(ifr));
	memset(&edata, 0, sizeof(edata));

	strncpy(ifr.ifr_name, plget->if_name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (char *)&edata;

	edata.cmd = ETHTOOL_GSET;

	ret = ioctl(plget->sfd, SIOCETHTOOL, &ifr);
	if (ret < 0) {
		printf("SIOCETHTOOL failed, supposing 100Mbs: %s\n", strerror(errno));
		return SPEED_100;
	}

	return ethtool_cmd_speed(&edata);
}

void res_stats_print(void)
{
	unsigned long long int ftt;
	int mod = plget->mod;
	int rx_tx_lat = mod == ECHO_LAT || mod == RTT_MOD;
	int print_rx_lat = mod == RX_LAT || rx_tx_lat;
	int print_tx_lat = mod == TX_LAT || rx_tx_lat;
	int n = 0, n2 = 0;
	int header_size;
	int pnum, speed;

	printf("\n");
	if (print_tx_lat)
		n2 = res_tx_lat_print();

	if (print_rx_lat)
		n = res_rx_lat_print();

	if (mod == ECHO_LAT || mod == RTT_MOD) {
		if (n != n2)
			printf("rx ts num != tx ts num: %d != %d\n", n, n2);

		pnum = n > n2 ? n2 : n;

		if (mod == RTT_MOD)
			res_rtt_print();
	} else if (mod == PKT_GEN) {
		pnum = plget->pkt_num;
	} else {
		pnum = n | n2;
	}

	if (mod == RX_LAT || mod == ECHO_LAT) {
		header_size = (plget->pkt_type == PKT_RAW ||
			       plget->pkt_type == PKT_XDP) ? 0 : ETH_HLEN;

		if (plget->pkt_type == PKT_UDP)
			header_size += 28;

		plget->frame_size = header_size + plget->sk_payload_size;
	}

	if (plget->frame_size) {
		speed = res_get_intf_speed();
		printf("Interface speed returned: %dMbps\n", speed);
		printf("Frame size: %d\n", plget->frame_size);
		if (speed > 0) {
			ftt = (double)(plget->frame_size * 8 * 1000) / speed;
			printf("Frame transmission time: %lluns (%gus)\n", ftt,
				ftt / 1000.0);
		}
	}

	printf("number of packets: %d\n", pnum);

	if (mod == TX_LAT || mod == RTT_MOD)
		stats_vrate_print(res_best_tx_vect(), plget->frame_size);

	if (mod == RX_LAT || mod == ECHO_LAT)
		stats_vrate_print(res_best_rx_vect(), plget->frame_size);

	printf("\n");
}
