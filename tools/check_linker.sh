#!/bin/sh
# Verify that the linker scripts keep each image inside its part of flash.
#
# WHY THIS EXISTS. Flash holds three things (Core/Inc/app/app_config.h):
#
#   APP_BOOT_BASE       0x08000000  bootloader   12 KB
#   APP_IMAGE_BASE      0x08003000  application  50 KB
#   APP_CFG_FLASH_ADDR  0x0800F800  config        2 KB  (node_id, magic, CRC)
#
# An image that grows over its neighbour is erased-and-programmed over it every
# time it is flashed: over the config, the module comes up unconfigured and
# silent on CAN1; over the bootloader, it cannot be configured at all - and
# nothing in the build says why. The linker scripts are what stop that, but
# STM32F303K8TX_FLASH.ld is CubeMX-generated and a Generate can rewrite it
# without a word. This check turns that silence into a stopped build, the way
# check_contracts.sh does for the contract files.
#
# Usage:
#   tools/check_linker.sh          the application (repo root linker script)
#   tools/check_linker.sh boot     the bootloader (VF_TempHumi_Bootloader/)
# Exit 1 when the FLASH region leaves its part of the map.

set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CFG_H="$ROOT/Core/Inc/app/app_config.h"
MODE=${1:-app}

fail() {
  echo "check_linker: FAILED - $1" >&2
  exit 1
}

case "$MODE" in
  app)  LD="$ROOT/STM32F303K8TX_FLASH.ld" ;;
  boot) LD="$ROOT/VF_TempHumi_Bootloader/STM32F303K8TX_FLASH.ld" ;;
  *)    fail "unknown mode '$MODE' (app or boot)" ;;
esac

[ -f "$LD" ] || fail "no linker script at $LD"
[ -f "$CFG_H" ] || fail "no $CFG_H"

# #define APP_CFG_FLASH_ADDR        0x0800F800u
hex_define() {
  sed -n "s/^#define[[:space:]][[:space:]]*$1[[:space:]][[:space:]]*\(0x[0-9A-Fa-f]*\)[uU]\{0,1\}.*/\1/p" "$CFG_H"
}
boot_base=$(hex_define APP_BOOT_BASE)
image_base=$(hex_define APP_IMAGE_BASE)
cfg=$(hex_define APP_CFG_FLASH_ADDR)
[ -n "$boot_base" ] || fail "cannot read APP_BOOT_BASE from $(basename "$CFG_H")"
[ -n "$image_base" ] || fail "cannot read APP_IMAGE_BASE from $(basename "$CFG_H")"
[ -n "$cfg" ] || fail "cannot read APP_CFG_FLASH_ADDR from $(basename "$CFG_H")"

# FLASH    (rx)    : ORIGIN = 0x8000000,   LENGTH = 50K
line=$(grep -E '^[[:space:]]*FLASH[[:space:]]*\(rx\)' "$LD" || true)
[ -n "$line" ] || fail "no 'FLASH (rx)' region in $LD"
origin=$(echo "$line" | sed -n 's/.*ORIGIN[[:space:]]*=[[:space:]]*\(0x[0-9A-Fa-f]*\).*/\1/p')
length_k=$(echo "$line" | sed -n 's/.*LENGTH[[:space:]]*=[[:space:]]*\([0-9][0-9]*\)K.*/\1/p')
[ -n "$origin" ] || fail "cannot read ORIGIN of FLASH in $LD"
[ -n "$length_k" ] || fail "cannot read LENGTH of FLASH (expected NNK) in $LD"

start=$(( origin ))
end=$(( origin + length_k * 1024 ))

report() {
  echo "check_linker: FAILED - $1" >&2
  echo "" >&2
  printf '  %s: FLASH ORIGIN %s LENGTH %sK = 0x%08X .. 0x%08X\n' "$LD" "$origin" "$length_k" "$start" "$end" >&2
  printf '  allowed: %s\n' "$2" >&2
  echo "" >&2
  echo "  A CubeMX Generate may have rewritten the linker script - see" >&2
  echo "  docs/VF_TH_IOC_Guide.md A12 (application) and B4 (bootloader)." >&2
  exit 1
}

if [ "$MODE" = "boot" ]; then
  allowed=$(printf 'ORIGIN 0x%08X, end <= 0x%08X (12K)' "$(( boot_base ))" "$(( image_base ))")
  [ "$start" -eq "$(( boot_base ))" ] || report "the bootloader does not start at APP_BOOT_BASE." "$allowed"
  [ "$end" -le "$(( image_base ))" ] || report "the bootloader may reach the application." "$allowed"
  printf 'check_linker: ok (bootloader), FLASH 0x%08X .. 0x%08X, application at 0x%08X.\n' "$start" "$end" "$(( image_base ))"
  exit 0
fi

# Application. Two layouts are legal while phase 5 moves the image:
#   ORIGIN = APP_BOOT_BASE   one image, no bootloader on the chip yet;
#   ORIGIN = APP_IMAGE_BASE  two images, behind the bootloader.
# Either way it must end before the config page.
allowed=$(printf 'ORIGIN 0x%08X (one image) or 0x%08X (behind the bootloader), end <= 0x%08X' \
          "$(( boot_base ))" "$(( image_base ))" "$(( cfg ))")
if [ "$start" -eq "$(( image_base ))" ]; then
  layout="behind the bootloader"
elif [ "$start" -eq "$(( boot_base ))" ]; then
  layout="ONE IMAGE - no bootloader yet"
else
  report "the application starts neither at APP_BOOT_BASE nor at APP_IMAGE_BASE." "$allowed"
fi
[ "$end" -le "$(( cfg ))" ] || report "the application may reach the config page." "$allowed"

printf 'check_linker: ok (application, %s), FLASH 0x%08X .. 0x%08X, config page at 0x%08X.\n' \
       "$layout" "$start" "$end" "$(( cfg ))"
exit 0
