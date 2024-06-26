// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2019-2022, Stephan Gerhold <stephan@gerhold.net> */

#include <string.h>
#include <bits.h>
#include <debug.h>
#include <dev/fbcon.h>
#include <reg.h>
#include <arch/ops.h>

#include "cont-splash.h"
#include "mdp.h"

#if MDP4
/* Defines already in mdp4.h, only make names consistent */
#define PIPE_SRC_IMG_SIZE	PIPE_SRC_SIZE
#define PIPE_SRC_OUT_SIZE	PIPE_OUT_SIZE
#define PIPE_SRC0_ADDR		PIPE_SRCP0_ADDR
#define PIPE_SRC_YSTRIDE	PIPE_SRC_YSTRIDE1
#elif MDP5
#define PIPE_SRC_SIZE		PIPE_SSPP_SRC_SIZE
#define PIPE_SRC_IMG_SIZE	PIPE_SSPP_SRC_IMG_SIZE
#define PIPE_SRC_XY		PIPE_SSPP_SRC_XY
#define PIPE_SRC_OUT_SIZE	PIPE_SSPP_SRC_OUT_SIZE
#define PIPE_OUT_XY		PIPE_SSPP_OUT_XY
#define PIPE_SRC0_ADDR		PIPE_SSPP_SRC0_ADDR
#define PIPE_SRC_YSTRIDE	PIPE_SSPP_SRC_YSTRIDE
#define PIPE_SRC_FORMAT		PIPE_SSPP_SRC_FORMAT
#endif

struct mdp_pipe {
	const char *name;
	uint32_t base;
};

static const struct mdp_pipe mdp_pipes[] = {
#if MDP4
	{
		.name = "RGB1",
		.base = MDP_RGB1_BASE,
	},
#elif MDP5
	{
		.name = "VIG_0",
		.base = MDP_VP_0_VIG_0_BASE,
	},
	{
		.name = "RGB_0",
		.base = MDP_VP_0_RGB_0_BASE,
	},
	{
		.name = "DMA_0",
		.base = MDP_VP_0_DMA_0_BASE,
	},
#endif
};

static const struct mdp_pipe *mdp_find_pipe(struct fbcon_config *fb)
{
	const struct mdp_pipe *pipe = mdp_pipes;
	const struct mdp_pipe *pipe_end = pipe + ARRAY_SIZE(mdp_pipes);

	for (; pipe < pipe_end; pipe++) {
		fb->base = (void *)readl(pipe->base + PIPE_SRC0_ADDR);
		if (fb->base)
			return pipe;
	}
	return NULL;
}

bool mdp_read_pipe_config(struct fbcon_config *fb)
{
	uint32_t src_size, img_size, src_xy, out_size, out_xy, stride, format, bpp;
	const struct mdp_pipe *pipe;

#if MDP4
	if (readl(MDP_LAYERMIXER_IN_CFG) == 0)
		return false; /* No pipes active */
#endif

	pipe = mdp_find_pipe(fb);
	if (!pipe)
		return false;

	src_size = readl(pipe->base + PIPE_SRC_SIZE);
	img_size = readl(pipe->base + PIPE_SRC_IMG_SIZE);
	src_xy = readl(pipe->base + PIPE_SRC_XY);
	out_size = readl(pipe->base + PIPE_SRC_OUT_SIZE);
	out_xy = readl(pipe->base + PIPE_OUT_XY);
	stride = readl(pipe->base + PIPE_SRC_YSTRIDE);
	format = readl(pipe->base + PIPE_SRC_FORMAT);
	bpp = BITS_SHIFT(format, 10, 9) + 1; /* SRC_BPP */

	dprintf(INFO, "MDP continuous splash detected: pipe %s, base: %p, stride: %d, "
		"src: %dx%d (%d,%d), img: %dx%d, out: %dx%d (%d,%d), format: %#x (bpp: %d)\n",
		pipe->name, fb->base, stride,
		MDP_X(src_size), MDP_Y(src_size), MDP_X(src_xy), MDP_Y(src_xy),
		MDP_X(img_size), MDP_Y(img_size),
		MDP_X(out_size), MDP_Y(out_size), MDP_X(out_xy), MDP_Y(out_xy),
		format, bpp
	);

	fb->width = MDP_X(img_size);
	fb->height = MDP_Y(img_size);
	fb->stride = stride / bpp;
	fb->bpp = bpp * 8;

	if (bpp == 2)
		fb->format = FB_FORMAT_RGB565;
	else
		fb->format = FB_FORMAT_RGB888;

	return true;
}

enum {
  SSPP_SRC_FORMAT_SRC_RGB565    = 0x1 << 9 | 0x1 << 4 | 0x1 << 2 | 0x2 << 0,
  SSPP_SRC_FORMAT_SRC_RGB888    = 0x2 << 9 | 0x3 << 4 | 0x3 << 2 | 0x3 << 0,
  SSPP_SRC_FORMAT_SRC_XRGB8888  = 0x3 << 9 | 0x3 << 6 | 0x3 << 4 | 0x3 << 2 | 0x3 << 0,

  SSPP_SRC_FORMAT_UNPACK_RGB    = 0x1 << 17 | 0x2 << 12,
  SSPP_SRC_FORMAT_UNPACK_XRGB   = 0x1 << 17 | 0x3 << 12,

  SSPP_SRC_UNPACK_PATTERN_RGB   = 0x0 << 24 | 0x2 << 16 | 0x0 << 8 | 0x1 << 0,
  SSPP_SRC_UNPACK_PATTERN_XRGB  = 0x3 << 24 | 0x2 << 16 | 0x0 << 8 | 0x1 << 0,
};

void mdp_set_rgb565(struct fbcon_config *fb)
{
	const struct mdp_pipe *pipe;

	if (!fb)
		return;

	pipe = mdp_find_pipe(fb);
	if (!pipe)
		return;

#if MDP4
	dprintf(INFO, "%s: Not implemented for MDP4.\n", __func__);
	return;
#elif MDP5
	writel(SSPP_SRC_FORMAT_SRC_RGB565 | SSPP_SRC_FORMAT_UNPACK_RGB, pipe->base + PIPE_SRC_FORMAT);
	writel(SSPP_SRC_UNPACK_PATTERN_RGB, pipe->base + PIPE_SSPP_SRC_UNPACK_PATTERN);
	writel(fb->width * 2, pipe->base + PIPE_SRC_YSTRIDE);

	writel(1 << 3, MDP_CTL_0_BASE + CTL_FLUSH);
#endif

	fb->bpp = 2 * 8;
	fb->format = FB_FORMAT_RGB565;
}

void mdp_set_xrgb8888(struct fbcon_config *fb)
{
	const struct mdp_pipe *pipe;

	if (!fb)
		return;

	pipe = mdp_find_pipe(fb);
	if (!pipe)
		return;

#if MDP4
	dprintf(INFO, "%s: Not implemented for MDP4.\n", __func__);
	return;
#elif MDP5
	writel(SSPP_SRC_FORMAT_SRC_XRGB8888 | SSPP_SRC_FORMAT_UNPACK_XRGB, pipe->base + PIPE_SRC_FORMAT);
	writel(SSPP_SRC_UNPACK_PATTERN_XRGB, pipe->base + PIPE_SSPP_SRC_UNPACK_PATTERN);
	writel(fb->width * 4, pipe->base + PIPE_SRC_YSTRIDE);

	writel(1 << 3, MDP_CTL_0_BASE + CTL_FLUSH);
#endif

	fb_convert_to_xrgb8888(fb);

	fb->bpp = 4 * 8;
	fb->format = FB_FORMAT_RGB888;
}

void mdp_relocate(struct fbcon_config *fb, void *target)
{
	const struct mdp_pipe *pipe;
	size_t fb_size = 0;

	if (!fb)
		return;

	pipe = mdp_find_pipe(fb);
	if (!pipe)
		return;

	fb_size = fb->stride * (fb->bpp / 8) * fb->height;

	memcpy(target, fb->base, fb_size);
	arch_clean_cache_range((addr_t)target, fb_size);

#if MDP4
	dprintf(INFO, "%s: Not implemented for MDP4.\n", __func__);
	return;
#elif MDP5
	writel((uint32_t)target, pipe->base + PIPE_SRC0_ADDR);

	writel(1 << 3, MDP_CTL_0_BASE + CTL_FLUSH);
#endif

	fb->base = target;
}
