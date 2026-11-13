#!/bin/sh
# Packages the ELF built by `make` into a .3ds (CCI) or .cia using makerom + bannertool.
# usage: tools/package.sh 3ds|cia [target-name]
# NOTE: the .3ds produced by makerom is not a signed retail image. It runs on emulators
# (Citra / Azahar) and on custom-firmware consoles (e.g. via flashcart or FBI-style loaders).
set -e
MODE=$1; TARGET=${2:-valorant3ds}
cd "$(dirname "$0")/.."
need() { command -v "$1" >/dev/null 2>&1 || { echo "error: '$1' not found in PATH. See README, section 'Packaging .3ds / .cia'."; exit 1; }; }
case "$MODE" in 3ds|cia) ;; *) echo "usage: $0 3ds|cia [target]"; exit 2;; esac
need makerom; need bannertool
[ -f "$TARGET.elf" ] || { echo "error: $TARGET.elf missing - run 'make' first"; exit 1; }
[ -f "$TARGET.smdh" ] || { echo "error: $TARGET.smdh missing - run 'make' first"; exit 1; }
mkdir -p build
bannertool makebanner -i assets/banner.png -a assets/banner.wav -o build/banner.bnr
COMMON="-rsf assets/app.rsf -target t -exefslogo -elf $TARGET.elf -icon $TARGET.smdh -banner build/banner.bnr -DAPP_TITLE=Valo3DS -DAPP_PRODUCT_CODE=V3DS -DAPP_UNIQUE_ID=0xF7A31"
if [ "$MODE" = "3ds" ]; then makerom -f cci -o "$TARGET.3ds" $COMMON; echo "built $TARGET.3ds"
else makerom -f cia -o "$TARGET.cia" $COMMON; echo "built $TARGET.cia"; fi
