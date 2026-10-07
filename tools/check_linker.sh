#!/bin/sh
# Verify that the application cannot reach the config page.
#
# WHY THIS EXISTS. The last 2 KB page of flash (APP_CFG_FLASH_ADDR in
# Core/Inc/app/app_config.h) holds the module config: node_id, magic, CRC. An
# application image that grows into that page is erased-and-programmed over it
# every time it is flashed - and the module comes up unconfigured, silent on
# CAN1, with nothing in the build to say why.
#
# The linker script is what stops the image from growing there: its FLASH
# region was cut to 50K by hand in phase 4. But STM32F303K8TX_FLASH.ld is a
# CubeMX-generated file, and a Generate can put it back to 64K without a word.
# This check turns that silence into a stopped build, the way
# check_contracts.sh does for the contract files.
#
# Usage: tools/check_linker.sh   (exit 1 when the FLASH region reaches the page)

set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
LD="$ROOT/STM32F303K8TX_FLASH.ld"
CFG_H="$ROOT/Core/Inc/app/app_config.h"

fail() {
  echo "check_linker: FAILED - $1" >&2
  exit 1
}

[ -f "$LD" ] || fail "no linker script at $LD"
[ -f "$CFG_H" ] || fail "no $CFG_H"

# FLASH    (rx)    : ORIGIN = 0x8000000,   LENGTH = 50K
line=$(grep -E '^[[:space:]]*FLASH[[:space:]]*\(rx\)' "$LD" || true)
[ -n "$line" ] || fail "no 'FLASH (rx)' region in $(basename "$LD")"

origin=$(echo "$line" | sed -n 's/.*ORIGIN[[:space:]]*=[[:space:]]*\(0x[0-9A-Fa-f]*\).*/\1/p')
length_k=$(echo "$line" | sed -n 's/.*LENGTH[[:space:]]*=[[:space:]]*\([0-9][0-9]*\)K.*/\1/p')
[ -n "$origin" ] || fail "cannot read ORIGIN of FLASH in $(basename "$LD")"
[ -n "$length_k" ] || fail "cannot read LENGTH of FLASH (expected NNK) in $(basename "$LD")"

# #define APP_CFG_FLASH_ADDR        0x0800F800u
cfg=$(sed -n 's/^#define[[:space:]][[:space:]]*APP_CFG_FLASH_ADDR[[:space:]][[:space:]]*\(0x[0-9A-Fa-f]*\)[uU]\{0,1\}.*/\1/p' "$CFG_H")
[ -n "$cfg" ] || fail "cannot read APP_CFG_FLASH_ADDR from $(basename "$CFG_H")"

end=$(( origin + length_k * 1024 ))
cfg_dec=$(( cfg ))

if [ "$end" -gt "$cfg_dec" ]; then
  echo "check_linker: FAILED - the application may reach the config page." >&2
  echo "" >&2
  printf '  %s: FLASH ORIGIN %s LENGTH %sK ends at 0x%08X\n' "$(basename "$LD")" "$origin" "$length_k" "$end" >&2
  printf '  config page (APP_CFG_FLASH_ADDR) starts at   0x%08X\n' "$cfg_dec" >&2
  echo "" >&2
  echo "  A CubeMX Generate may have rewritten the linker script. Set the FLASH" >&2
  echo "  region back to LENGTH = 50K (CLAUDE.md 4 / app_config.h flash map)." >&2
  exit 1
fi

printf 'check_linker: ok, FLASH ends at 0x%08X, config page at 0x%08X.\n' "$end" "$cfg_dec"
exit 0
