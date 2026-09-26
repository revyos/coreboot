/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2024 Zhihe Computing Technology (Shenzhen) Co., Ltd. */
#include "board.h"

/* CXDB5CBAM packages on the Melon Pi core module. */
const struct a210_dram_config melonpi_dram = {
	.profile = A210_LPDDR4X_4266_1R,
	.dq_map = {
		{ 0, 1, 4, 5, 7, 6, 2, 3 },
		{ 2, 7, 4, 5, 0, 1, 3, 6 },
		{ 3, 2, 1, 0, 4, 5, 6, 7 },
		{ 4, 5, 7, 6, 3, 2, 0, 1 },
	},
};
