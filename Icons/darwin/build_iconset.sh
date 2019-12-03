#!/bin/bash

make_icon_set()
{
    APP=$1
    MASTER="radeon-badge"
    TMP_ICON_DIR="/tmp/$APP.iconset"

    rm -rf "$TMP_ICON_DIR"
    mkdir "$TMP_ICON_DIR"

    sips -z 16 16     $MASTER.png --out $TMP_ICON_DIR/icon_16x16.png
    sips -z 32 32     $MASTER.png --out $TMP_ICON_DIR/icon_16x16@2x.png
    sips -z 32 32     $MASTER.png --out $TMP_ICON_DIR/icon_32x32.png
    sips -z 64 64     $MASTER.png --out $TMP_ICON_DIR/icon_32x32@2x.png
    sips -z 128 128   $MASTER.png --out $TMP_ICON_DIR/icon_128x128.png
    sips -z 256 256   $MASTER.png --out $TMP_ICON_DIR/icon_128x128@2x.png
    sips -z 256 256   $MASTER.png --out $TMP_ICON_DIR/icon_256x256.png
    sips -z 512 512   $MASTER.png --out $TMP_ICON_DIR/icon_256x256@2x.png
    sips -z 512 512   $MASTER.png --out $TMP_ICON_DIR/icon_512x512.png

    cp $MASTER.png $TMP_ICON_DIR/icon_512x512@2x.png

    iconutil -c icns -o $APP.icns "$TMP_ICON_DIR"

    rm -r "$TMP_ICON_DIR"
}

make_icon_set "RadeonProRenderBlenderInstaller"
make_icon_set "RadeonProRenderMayaInstaller"
