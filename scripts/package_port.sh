#!/bin/sh
# Packages a console port as an SD card layout zip: scripts/package_port.sh switch|wiiu|3ds
# (run `make -C ports/<port>` first). Output: LittleAlchemy2-<port>.zip in the repository root.
set -e
cd "$(dirname "$0")/.."
case "$1" in
	switch) dir=switch/LittleAlchemy2; files=ports/switch/LittleAlchemy2.nro ;;
	wiiu)   dir=wiiu/apps/LittleAlchemy2; files="ports/wiiu/LittleAlchemy2.wuhb ports/wiiu/LittleAlchemy2.rpx" ;;
	3ds)    dir=3ds/LittleAlchemy2; files=ports/3ds/LittleAlchemy2.3dsx ;;
	*) echo "usage: $0 switch|wiiu|3ds" >&2; exit 1 ;;
esac
out=$(mktemp -d)
mkdir -p "$out/$dir"
cp $files "$out/$dir/"
cp apps/LittleAlchemy2/icons.bin "$out/$dir/"
cp -R apps/LittleAlchemy2/audio "$out/$dir/"
rm -f "LittleAlchemy2-$1.zip"
(cd "$out" && zip -qr "$OLDPWD/LittleAlchemy2-$1.zip" . -x '*.DS_Store')
rm -rf "$out"
echo "LittleAlchemy2-$1.zip"
