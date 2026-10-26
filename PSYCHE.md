# ios-psyche: ios-sweet on the Xiaomi Mi 12X

This fork ports [ios-sweet](https://github.com/dipa-studio/ios-sweet) (iOS 16
userland on XNU, Redmi Note 10 Pro `sweet`, SM7150) to the **Xiaomi Mi 12X
(`psyche`, Qualcomm SM8250-AC "kona", Snapdragon 870)**.

**Status: untested on hardware.** Everything below was derived from the
psyche and kona device trees (LineageOS `android_kernel_xiaomi_sm8250`:
`kona.dtsi`, `kona-usb.dtsi`, `kona-sde.dtsi`, `psyche-sm8250.dtsi`,
`xiaomi-sm8250-common.dtsi`, `dsi-panel-l3a-mp-42-02-0a-dsc-cmd.dtsi`) and
Linux's DPU driver, not from a booted phone. The first boots are
there to confirm or correct it; follow the bring-up order at the end.

## What is the same as on sweet

Most of the hardware the boot path touches sits at the same addresses on
SM7150 and SM8250, so most of the project carries over unchanged:

| Block | sweet (SM7150) | psyche (SM8250) |
| --- | --- | --- |
| GICv3 | GICD `0x17a00000`, GICR `0x17a60000` | same |
| UFS host | `ufshc@1d84000` | same (UFS 3.1) |
| PS_HOLD | `0x0c264000` | same |
| ABL splash framebuffer | `0x9c000000`, 1080×2400 | same address and size |
| SPMI arbiter v5 | core `0xc440000`, obsrvr `0xe600000`, cnfg `0xc40a000` | same |
| Power, volume-down | PMIC PON, SID 0 `0x0810` | PM8150 PON, same registers |
| Timer | 19.2 MHz | same |
| PSCI, MPIDR | `smc`, Aff1 = core | same |
| Boot image | v2 | v2 for the non-virtual-A/B build (check, see below) |

## What differs, and what this port changes

| Area | sweet | psyche | Change |
| --- | --- | --- | --- |
| Virtual timer INTID | 19 (`<GIC_PPI 3>`) | **27** (`<GIC_PPI 11>`) | ADT `virtual-timer-intid` (`mkadt.py --device psyche`), shim `board.h` |
| IMEM restart reason | `0x146aa65c` | **`0x146bf65c`** | shim `board.h` |
| CPUs | 6× A55 + 2× A76 | **4× A55 + 3× A77 + 1× A77 prime** | ADT clock and compatible strings; same MPIDRs `0x000`–`0x700`, so the SMP code is unchanged |
| Panel | video mode, scans out by itself | **DSI command mode with DSC** (`l3a 42 02 0a`) | shim `display.c`: ping-pong autorefresh, see below |
| Volume-up | PM6150L GPIO2 (SID 4) | **PM8150 GPIO6 (SID 0)** | patch 0082 |
| Touch | Goodix GT9896, I²C, QUP SE7 `0xa84000`, L11C | **ST FTS (fts_521), SPI, QUP SE4 `0x990000`**, reset GPIO 33, IRQ GPIO 39, L10C + L7A | patch 0082 turns touch **off** on psyche; driver not written yet |
| USB | DWC3 `0xa600000`, QUSB2 PHY `0x88e2000` | DWC3 `0xa600000`, **Synopsys femto PHY `0x88e3000`** + QMP | patch 0082 turns USB **off** on psyche; PHY not ported yet |
| Battery | PM6150 fuel gauge | **TI bq27z561** (I²C) + PM8150B | patch 0082 keeps the static 100 % source on psyche |

### The command-mode panel

sweet's panel scans the framebuffer continuously, so the whole project
(shim console, xnu's early console, the `AppleCLCD` framebuffer service)
just writes pixels to `0x9c000000`. psyche's panel only receives a frame when
the display engine is kicked (`CTL_START`). Instead of kicking from every
writer, the shim turns on **autorefresh** in each ping-pong block that ABL
left in command mode (tear check enabled): the DPU then refetches the
framebuffer and sends it on every TE pulse, which behaves like video mode.
It also starts each control path ABL configured once. Nothing else is
reprogrammed. The shim prints what it found on its first screen:

```
PANEL    pp=00000001 ctl=00000001     <- ping-pong / CTL masks it touched
SCANOUT  addr=9c000000 stride=00001140 <- the pipe showing the splash
```

If the screen stays on the ABL image after `fastboot boot`, autorefresh did
not take (or ABL left the display off): that is the first thing to debug.
`stride` should be `0x1140` (1080 × 4); anything else means the splash
format differs and `FB_STRIDE` (shim `console.h`, `mkadt.py`) must follow it.

## Building for psyche

The tools default to psyche; `DEVICE=sweet` / `--device sweet` still build
the original board.

```sh
python3 tools/mkadt.py shim/out/psyche.adt --device psyche --highmem
python3 tools/mkpayload.py xnu/out/kernel.development.sweet shim/out/psyche.adt shim/out/payload.bin \
    --cmdline 'debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress rd=ufsroot0 unentitled_ios_sim_launch=1 ignition_level=1 wdt_test=no_user_checkin ufsrw=1 rootrw=1 launchd_unsecure_cache=1 wdt=-1 sweetfb_hz=30'
DEVICE=psyche shim/build.sh ../shim/out/payload.bin      # -> shim/out/shim-boot.img
```

The kernel is the same SWEET kernel with patch 0082; it tells the boards
apart by `xiaomi,psyche` in the ADT root compatible. Build it with the
`xnu` workflow as in the README (Actions → xnu → Run workflow on this
branch).

Start with `cpus=1`, then `cpus=4` (the four A55s), then `cpus=8`.

### Boot image header

`shim/build.sh` makes a v2 boot image with the stock DTB, like sweet. Check
the stock psyche `boot.img` first (`unpack_bootimg --boot_img boot.img`):

- **header version 2**: unpack it with `tools/unpack_bootv2.py` as in the
  README and build normally.
- **header version 3**: the DTB lives in `vendor_boot`; build with
  `HEADER_VERSION=3 DEVICE=psyche shim/build.sh ...` (no `dtb/` needed).

### RAM

`mkadt.py --highmem` gives xnu `0x100000000`–`0x1fcc00000` (≈3.95 GiB), as on
sweet. psyche has 8 or 12 GiB of LPDDR5 starting at `0x80000000`, so this
range should be plain DDR, and kona's fixed carve-outs are all below 4 GiB.
Confirm against the live tree before the first xnu boot (from recovery):

```sh
adb shell cat /sys/firmware/fdt > dtb/psyche-live.dtb
dtc -I dtb -O dts dtb/psyche-live.dtb | grep -B2 -A2 'device_type = "memory"'
```

## Bring-up order

1. **Back up** as in the README (`persist`, `modemst1/2`, `fsg`, `fsc`,
   `boot`, `dtbo`, `vbmeta`, plus `vendor_boot` if it exists).
2. **Shim alone** (`DEVICE=psyche shim/build.sh` with no payload): the
   screen should turn red and show `SHIM psyche milestone 4`, the `PANEL` and
   `SCANOUT` lines, then the read-only UFS probe. This proves boot image,
   display and UFS. It resets to fastboot after about 3 minutes.
3. **xnu with `cpus=1`** and a root in `userdata` as in the README. The
   log ring at the end of `userdata` works the same
   (`tools/sweet-log-pull.sh` from a psyche OrangeFox build).
4. Keys: the log should show `SWEET KEYS: ... gpio-ppid=0xc5 ...`. If it
   says the volume-up GPIO is not confirmed as input, ABL did not configure
   PM8150 GPIO6; power and volume-down still work.
5. More cores (`cpus=4`, `cpus=8`).

## Not ported yet

- **Touch**: an ST FTS (fts_521, Xiaomi firmware `st_fts_l3a.ftb`) on GENI SPI
  QUP SE4. Needs a GENI SPI transfer path (sweet only has I²C), RPMh votes
  for `ldoc10` (3.0 V) and `ldoa7` (1.8 V), reset on TLMM GPIO 33 and the FTS
  event protocol. Without it the phone is usable only with the buttons.
- **USB** device mode and the live log over USB Ethernet: femto HS PHY.
- **Battery** readout: bq27z561 over I²C.
- Everything that does not work on sweet either (README, "What does not work").
