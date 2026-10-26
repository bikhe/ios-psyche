#pragma once
/*
 * Per-board constants of the shim. build.sh passes -DBOARD_PSYCHE or
 * -DBOARD_SWEET (DEVICE=psyche|sweet). Everything else in the shim is the
 * same on both boards: GICv3 at 0x17a00000/0x17a60000, PS_HOLD at
 * 0x0c264000, UFS host at 0x01d84000 and the ABL splash at 0x9c000000.
 */

#if defined(BOARD_PSYCHE)
/*
 * Xiaomi Mi 12X (psyche), SM8250 "kona". Addresses from kona.dtsi and
 * psyche-sm8250.dtsi (LineageOS android_kernel_xiaomi_sm8250).
 */
#define BOARD_NAME		"psyche"
#define IMEM_RESTART_REASON	0x146bf65cUL	/* qcom,msm-imem@146bf000 restart_reason@65c */
/* kona arch_timer: <GIC_PPI 13>, <14>, <11>, <12>: the usual INTIDs */
#define VTIMER_INTID		27
#define PTIMER_INTID		30
/*
 * The panel (dsi-panel-l3a-mp-42-02-0a-dsc-cmd) is a DSI command-mode
 * panel: it only shows what the display engine sends it, and in command
 * mode the engine sends a frame only when kicked. See display.c.
 */
#define BOARD_CMD_PANEL		1
#define MDP_BASE		0x0ae01000UL	/* mdss_mdp "mdp", DPU 6.0 */

#elif defined(BOARD_SWEET)
/* Redmi Note 10 Pro (sweet), SM7150. */
#define BOARD_NAME		"sweet"
#define IMEM_RESTART_REASON	0x146aa65cUL	/* qcom,msm-imem restart_reason@65c */
#define VTIMER_INTID		19	/* DT: <GIC_PPI 3 ...>, not the usual 27 */
#define PTIMER_INTID		18	/* DT: <GIC_PPI 2 ...> */
#define BOARD_CMD_PANEL		0	/* video-mode panel, scans out on its own */

#else
#error "build with -DBOARD_PSYCHE or -DBOARD_SWEET (shim/build.sh DEVICE=...)"
#endif
