
# copy all the resources and dependencies

cp -r "$PROJECT_DIR/../../../FireRender.Maya.Plugin/shaders" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Plugin/icons" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Plugin/images" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Plugin/renderDesc" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Plugin/scripts" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Plugin/python/" "/Users/Shared/RadeonProRender/scripts/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Plugin/shelfs/" "/Users/Shared/RadeonProRender/shelves/"
cp -r "$PROJECT_DIR/../../../ThirdParty/RadeonProRender SDK/Mac/lib" "/Users/Shared/RadeonProRender/"

# copy module files

if [ -d "/Users/Shared/Autodesk/modules/maya/2016/" ]; then
cp "$PROJECT_DIR/../rpr2016.mod" "/Users/Shared/Autodesk/modules/maya/2016/rpr.mod"
fi
if [ -d "/Users/Shared/Autodesk/modules/maya/2016.5/" ]; then
cp "$PROJECT_DIR/../rpr2016.5.mod" "/Users/Shared/Autodesk/modules/maya/2016.5/rpr.mod"
fi
if [ -d "/Users/Shared/Autodesk/modules/maya/2017/" ]; then
cp "$PROJECT_DIR/../rpr2017.mod" "/Users/Shared/Autodesk/modules/maya/2017/rpr.mod"
fi

localUsers=$( dscl . list /Users UniqueID | awk '$2 >= 501 {print $1}' | grep -v admin )
for userName in $localUsers; do
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2016/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../fireRender/Shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2016/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2016.5/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../fireRender/Shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2016.5/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../fireRender/Shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/"
fi

done
