#!/bin/bash 

echo 'Installing Radeon ProRender Maya plugin…'

echo "\tCleaning \"/Users/Shared/RadeonProRender/\""

if [ -d "/Users/Shared/RadeonProRender/" ]; then
num=`uuidgen`
tmpDirname="/Users/Shared/RadeonProRender"-$num
mv "/Users/Shared/RadeonProRender/" $tmpDirname
fi

# Copy the main directory for the plugin

echo "\tCopying plugin bundle to \"/Users/Shared/RadeonProRender/\""

cp -r RadeonProRender "/Users/Shared/RadeonProRender/"

# Copy the material library

cp -r exported "/Users/Shared/RadeonProRender/materials"

# Copy the module files

echo "\tInstalling module files into \"/Users/Shared/Autodesk/modules/maya\""

if [ -d "/Users/Shared/Autodesk/modules/maya/2016/" ]; then
cp "./modules/rpr2016.mod" "/Users/Shared/Autodesk/modules/maya/2016/RadeonProRender.mod"
fi
if [ -d "/Users/Shared/Autodesk/modules/maya/2016.5/" ]; then
cp "./modules/rpr2016.5.mod" "/Users/Shared/Autodesk/modules/maya/2016.5/RadeonProRender.mod"
fi
if [ -d "/Users/Shared/Autodesk/modules/maya/2017/" ]; then
cp "./modules/rpr2017.mod" "/Users/Shared/Autodesk/modules/maya/2017/RadeonProRender.mod"
fi

localUsers=$( dscl . list /Users UniqueID | awk '$2 >= 501 {print $1}' | grep -v admin )
for userName in $localUsers; do
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2016/prefs/shelves/" ]; then
cp -r "./RadeonProRender/shelves/" "/Users/$userName/Library/Preferences/Autodesk/maya/2016/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2016.5/prefs/shelves/" ]; then
cp -r "./RadeonProRender/shelves/" "/Users/$userName/Library/Preferences/Autodesk/maya/2016.5/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/" ]; then
cp -r "./RadeonProRender/shelves/" "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/"
fi
done

if [ -d "$tmpDirname" ]; then
echo "Please manually remove:" 
echo $tmpDirname
fi

echo "Done."
