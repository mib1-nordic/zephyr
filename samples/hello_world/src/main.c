/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <stdio.h>

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
#if 1
	while (1) {
		k_sleep(K_USEC(100));
		k_busy_wait(100);
	}
#endif
	return 0;
}
