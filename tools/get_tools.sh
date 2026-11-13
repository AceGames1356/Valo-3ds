#!/bin/sh
# Installs makerom + bannertool (needed only for .3ds / .cia packaging) on Linux x86_64.
# Used by the GitHub Actions "package" job; you can run it locally too (needs curl, unzip,
# git, cmake, a C/C++ compiler, libssl-dev, zlib1g-dev, libpng-dev). PREFIX defaults to /usr/local/bin.
set -e
BIN=${PREFIX:-/usr/local/bin}
WORK=$(mktemp -d); cd "$WORK"
have() { command -v "$1" >/dev/null 2>&1; }

if ! have makerom; then
  echo "== makerom: trying a prebuilt release =="
  URL=$(curl -fsSL https://api.github.com/repos/3DSGuy/Project_CTR/releases 2>/dev/null \
        | grep -o '"browser_download_url": *"[^"]*"' | grep -i 'makerom' | grep -i 'ubuntu' | grep -i 'x86_64' \
        | head -n 1 | sed 's/.*"\(http[^"]*\)"/\1/' || true)
  if [ -n "$URL" ]; then
    echo "downloading $URL"
    if curl -fsSL "$URL" -o makerom.zip && unzip -o -q makerom.zip; then
      F=$(find . -name makerom -type f | head -n 1)
      [ -n "$F" ] && cp "$F" "$BIN/makerom" && chmod +x "$BIN/makerom"
    fi
  fi
fi
if ! have makerom; then
  echo "== makerom: building from source =="
  git clone --depth 1 https://github.com/3DSGuy/Project_CTR.git
  ( cd Project_CTR/makerom && (make deps || true) && make -j2 )
  F=$(find Project_CTR -name makerom -type f -perm -u+x | head -n 1)
  cp "$F" "$BIN/makerom"; chmod +x "$BIN/makerom"
fi

if ! have bannertool; then
  echo "== bannertool: building from source =="
  git clone --depth 1 --recursive https://github.com/carstene1ns/3ds-bannertool.git
  cmake -S 3ds-bannertool -B bt -DCMAKE_BUILD_TYPE=Release
  cmake --build bt -j2
  F=$(find bt -name bannertool -type f | head -n 1)
  cp "$F" "$BIN/bannertool"; chmod +x "$BIN/bannertool"
fi
echo "makerom:    $(command -v makerom)"
echo "bannertool: $(command -v bannertool)"
