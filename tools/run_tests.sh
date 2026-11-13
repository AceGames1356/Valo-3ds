#!/bin/sh
# Host-side verification of everything except the libctru platform layer.
# Needs only gcc + libm. Runs with AddressSanitizer + UBSan.
set -e
cd "$(dirname "$0")/.."
CC=${CC:-gcc}
CF="-std=c99 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined"
CORE="source/game.c source/ai.c source/map.c source/data.c source/settings.c source/gfx.c source/render3d.c source/hud.c source/app.c"
HOST="tests/host/platform_host.c"
OUT=${TMPDIR:-/tmp}/v3ds_tests
mkdir -p "$OUT"

echo "== map layout + connectivity =="
$CC $CF tests/map_test.c source/map.c source/data.c -lm -o "$OUT/map_test"
"$OUT/map_test" | grep -E "site|open cells|TEST"

echo "== headless bot-vs-bot simulation (24 matches, all maps x difficulties) =="
$CC $CF tests/sim_test.c source/game.c source/ai.c source/map.c source/data.c source/settings.c $HOST -lm -o "$OUT/sim_test"
"$OUT/sim_test" | tail -3

echo "== deterministic rules: weapons, armor, spike, economy, abilities, movement =="
$CC $CF tests/rules_test.c source/game.c source/ai.c source/map.c source/data.c source/settings.c $HOST -lm -o "$OUT/rules_test"
"$OUT/rules_test" | tail -2

echo "== end-to-end touch UI + controls =="
$CC $CF tests/ui_test.c $CORE $HOST -lm -o "$OUT/ui_test"
"$OUT/ui_test"

echo "== main loop smoke test (3000 polls, real main.c) =="
$CC $CF source/main.c $CORE $HOST -lm -o "$OUT/main_smoke"
V3DS_HOST_POLLS=3000 "$OUT/main_smoke" && echo "main loop OK"

echo "== platform_3ds.c type-check against mock libctru header =="
for d in V3DS_CSTICK_HID V3DS_CSTICK_IRRST V3DS_CSTICK_NONE; do
  $CC -std=gnu99 -Wall -Wextra -fsyntax-only -D$d -Itests/mock3ds -Isource source/platform_3ds.c
done
echo "ALL HOST TESTS PASSED"
