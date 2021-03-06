/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019-2021, The Linux Foundation. All rights reserved. */
/* Copyright (C) 2021 XiaoMi, Inc. */

#ifndef __VIRTIO_CLK_COMMON__
#define __VIRTIO_CLK_COMMON__

#include <linux/types.h>

/*
 * struct clk_virtio_desc - virtio clock descriptor
 * clk_names: the pointer of clock name pointer
 * num_clks: number of clocks
 * reset_names: the pointer of reset name pointer
 * num_resets: number of resets
 */
struct clk_virtio_desc {
	const char * const *clk_names;
	size_t num_clks;
	const char * const *reset_names;
	size_t num_resets;
};

extern const struct clk_virtio_desc clk_virtio_sm8150_gcc;
extern const struct clk_virtio_desc clk_virtio_sm8150_scc;
extern const struct clk_virtio_desc clk_virtio_sm6150_gcc;
extern const struct clk_virtio_desc clk_virtio_sm6150_scc;
extern const struct clk_virtio_desc clk_virtio_sa8195p_gcc;
extern const struct clk_virtio_desc clk_virtio_direwolf_gcc;

#endif
