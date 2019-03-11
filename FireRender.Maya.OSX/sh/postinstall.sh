#!/bin/bash

echo "Installing RPR Shelf for Users: "
localUsers=$( dscl . list /Users UniqueID | awk '$2 >= 501 {print $1}' | grep -v admin )
for userName in $localUsers; do
    echo "- $userName"
    if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/" ]; then
        cp -r "/Users/Shared/RadeonProRender/Shelves/" "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/"
    fi
    if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2018/prefs/shelves/" ]; then
        cp -r "/Users/Shared/RadeonProRender/Shelves/" "/Users/$userName/Library/Preferences/Autodesk/maya/2018/prefs/shelves/"
    fi
    if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2019/prefs/shelves/" ]; then
        cp -r "/Users/Shared/RadeonProRender/Shelves/" "/Users/$userName/Library/Preferences/Autodesk/maya/2019/prefs/shelves/"
    fi
done
