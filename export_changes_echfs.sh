#!/bin/sh

set -e

mkdir -p mnt
echfs-fuse --mbr -p0 ZenithOS.hdd mnt

rm -rf src
mkdir src
cp -rv mnt/* src/

sync

fusermount -u mnt

rm -rf mnt
