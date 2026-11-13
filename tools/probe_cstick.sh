#!/bin/sh
# Picks the C-Stick API that exists in the installed libctru and prints a -D flag.
# (libctru versions differ: hidCstickRead vs. the older ir:rst irrst* functions.)
DKP=${DEVKITPRO:-/opt/devkitpro}
ARM=${DEVKITARM:-$DKP/devkitARM}
CC="$ARM/bin/arm-none-eabi-gcc"
INC="$DKP/libctru/include"
try() { printf '%s\n' "$1" | "$CC" -x c -fsyntax-only -Werror=implicit-function-declaration -I"$INC" - >/dev/null 2>&1; }
if try '#include <3ds.h>
void f(circlePosition *p){ hidCstickRead(p); }'; then echo "-DV3DS_CSTICK_HID"
elif try '#include <3ds.h>
void f(circlePosition *p){ irrstInit(); irrstScanInput(); irrstCstickRead(p); irrstExit(); }'; then echo "-DV3DS_CSTICK_IRRST"
else echo "-DV3DS_CSTICK_NONE"; fi
