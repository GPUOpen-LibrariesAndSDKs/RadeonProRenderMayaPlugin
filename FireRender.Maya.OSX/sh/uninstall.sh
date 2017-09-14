#!/bin/bash

echo "Uninstalling RadeonProRender..."

if [ -d "/Users/Shared/RadeonProRender" ]; then
    echo "Removing: /Users/Shared/RadeonProRender"
    rm -rf "/Users/Shared/RadeonProRender"
fi

rm -f /Users/Shared/Autodesk/modules/maya/2016/rpr.mod
rm -f /Users/Shared/Autodesk/modules/maya/2017/rpr.mod

localUsers=$( dscl . list /Users UniqueID | awk '$2 >= 501 {print $1}' | grep -v admin )

for userName in $localUsers; do
    echo "Removing shelves for $userName..."
    if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2016/prefs/shelves/" ]; then
        rm -f "/Users/$userName/Library/Preferences/Autodesk/maya/2016/prefs/shelves/shelf_Radeon_ProRender.mel"
    fi
    if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/" ]; then
        rm -f "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/shelf_Radeon_ProRender.mel"
    fi
done

