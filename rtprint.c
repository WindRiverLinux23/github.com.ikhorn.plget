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

#include <stdio.h>
#include <unistd.h>
#include "plget.h"
#include "debug.h"

void *rtprint(void *arg)
{
	unsigned long cnt;

	pr_progress_bar("Measuring: ", plget->icnt, 100);

	for (;;) {
		cnt = plget->icnt;

		if (plget->inum)
			pr_progress_bar("Measuring: ", plget->icnt, plget->inum);

		fflush(stdout);

		if (cnt == plget->inum)
			break;

		sleep(1);
	}
	printf("\n");

	return 0;
}
