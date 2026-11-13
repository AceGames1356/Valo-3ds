#!/bin/sh
# Convenience wrapper around the Makefile.
#   ./build.sh            build valorant3ds.3dsx
#   ./build.sh 3ds        also build valorant3ds.3ds (needs makerom + bannertool)
#   ./build.sh cia        also build valorant3ds.cia (needs makerom + bannertool)
#   ./build.sh test       run the host-side test suite (gcc only)
#   ./build.sh clean
set -e
cd "$(dirname "$0")"
cmd=${1:-all}
if [ "$cmd" = "test" ]; then exec sh tools/run_tests.sh; fi
if [ -z "$DEVKITPRO" ] || [ -z "$DEVKITARM" ]; then
  if [ -d /opt/devkitpro ]; then
    export DEVKITPRO=/opt/devkitpro
    export DEVKITARM=/opt/devkitpro/devkitARM
  else
    echo "error: devkitPro not found. Install devkitARM + libctru (https://devkitpro.org/wiki/Getting_Started)"
    echo "       then: export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM"
    exit 1
  fi
fi
case "$cmd" in
  all|3dsx) make -j"$(nproc 2>/dev/null || echo 2)" ;;
  3ds|cia)  make -j"$(nproc 2>/dev/null || echo 2)" && make "$cmd" ;;
  clean)    make clean ;;
  *) echo "unknown target '$cmd'"; exit 2 ;;
esac
