#!/usr/bin/env bash

set -e

VERSION=`bin/buildinfo.py long`
SHORT_VERSION=`bin/buildinfo.py short`

OUTDIR=release/

rm -f $OUTDIR/firmware*

mkdir -p $OUTDIR/
rm -r $OUTDIR/* || true

# Important to pull latest version of libs into all device flavors, otherwise some devices might be stale
platformio pkg update 

cp bin/device-install.* $OUTDIR
cp bin/device-update.* $OUTDIR

