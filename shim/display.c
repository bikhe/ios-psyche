/*
 * Keep a command-mode panel showing the splash framebuffer.
 *
 * sweet's video-mode panel is scanned out continuously, so writing the
 * framebuffer at 0x9c000000 is enough. psyche's panel is a DSI command-mode
 * panel: the display engine (DPU) sends a frame only when software writes
 * CTL_START, and ABL kicks it only when ABL redraws. Instead of teaching
 * the shim and xnu to kick every frame, turn on the ping-pong block's
 * autorefresh: the hardware then refetches the framebuffer and sends a new
 * frame on every tear-check (TE) pulse, as a video-mode panel would.
 *
 * Nothing is reconfigured: only ping-pong blocks that ABL already runs in
 * command mode (tear check enabled) get autorefresh, and only control paths
 * ABL already set up (CTL_TOP != 0) get one CTL_START to begin.
 *
 * Register layout: Linux drivers/gpu/drm/msm/disp/dpu1/dpu_hw_pingpong.c,
 * dpu_hw_ctl.c and catalog/dpu_6_0_sm8250.h (offsets from MDP_BASE).
 */
#include <stdint.h>
#include "board.h"
#include "console.h"
#include "display.h"

#if BOARD_CMD_PANEL

#define NR_CTL			6
#define CTL_OFF(i)		(0x1000UL + 0x200UL * (i))
#define CTL_TOP			0x014
#define CTL_START		0x01c

#define NR_PP			6
#define PP_OFF(i)		(0x70000UL + 0x800UL * (i))
#define PP_TEAR_CHECK_EN	0x000
#define PP_AUTOREFRESH_CONFIG	0x030
#define PP_AUTOREFRESH_EN	(1U << 31)
#define PP_AUTOREFRESH_FRAMES	1	/* new frame on every TE pulse */

/* Source pipes: VIG0-3, DMA0-3 */
static const uint32_t sspp_off[] = {
	0x4000, 0x6000, 0x8000, 0xa000, 0x24000, 0x26000, 0x28000, 0x2a000,
};
#define SSPP_SRC0_ADDR		0x14
#define SSPP_SRC_YSTRIDE0	0x24

static inline uint32_t rd32(uintptr_t a) { return *(volatile uint32_t *)a; }
static inline void wr32(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }

static uint32_t pp_mask, ctl_mask, sspp_addr, sspp_stride;

void display_start(void)
{
	pp_mask = ctl_mask = 0;
	for (unsigned i = 0; i < NR_PP; i++) {
		uintptr_t pp = MDP_BASE + PP_OFF(i);
		if (!(rd32(pp + PP_TEAR_CHECK_EN) & 1))
			continue;
		wr32(pp + PP_AUTOREFRESH_CONFIG, PP_AUTOREFRESH_EN | PP_AUTOREFRESH_FRAMES);
		pp_mask |= 1U << i;
	}
	__asm__ volatile("dsb sy" ::: "memory");
	for (unsigned i = 0; i < NR_CTL; i++) {
		uintptr_t ctl = MDP_BASE + CTL_OFF(i);
		if (rd32(ctl + CTL_TOP) == 0)
			continue;
		wr32(ctl + CTL_START, 1);
		ctl_mask |= 1U << i;
	}
	/* Which pipe scans out the splash, and with what stride (expect FB_STRIDE). */
	for (unsigned i = 0; i < sizeof(sspp_off) / sizeof(sspp_off[0]); i++) {
		uint32_t a = rd32(MDP_BASE + sspp_off[i] + SSPP_SRC0_ADDR);
		if (a >= FB_BASE && a < FB_BASE + 0x2300000UL) {
			sspp_addr = a;
			sspp_stride = rd32(MDP_BASE + sspp_off[i] + SSPP_SRC_YSTRIDE0) & 0xffff;
			break;
		}
	}
}

void display_report(void)
{
	con_hex2("PANEL", "pp=", pp_mask, "ctl=", ctl_mask);
	con_hex2("SCANOUT", "addr=", sspp_addr, "stride=", sspp_stride);
}

#else

void display_start(void) {}
void display_report(void) {}

#endif
