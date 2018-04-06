
# copy all the resources and dependencies


cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shaders" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/icons" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/images" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/renderDesc" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/scripts" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/python/" "/Users/Shared/RadeonProRender/scripts/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/Shared/RadeonProRender/shelves/"
cp -r "$PROJECT_DIR/../../../ThirdParty/RadeonProRender SDK/Mac/lib" "/Users/Shared/RadeonProRender/"
cp    "$PROJECT_DIR/../../../ThirdParty/RadeonProImageProcessing/Mac/lib/libRadeonImageFilters64.dylib" "/Users/Shared/RadeonProRender/lib"

mkdir -p "/Users/Shared/RadeonProRender/modules"
cp "$PROJECT_DIR/../rpr2016.mod" "/Users/Shared/RadeonProRender/modules/rpr2016.mod"
cp "$PROJECT_DIR/../rpr2016.5.mod" "/Users/Shared/RadeonProRender/modules/rpr2016.5.mod"
cp "$PROJECT_DIR/../rpr2017.mod" "/Users/Shared/RadeonProRender/modules/rpr2017.mod"
cp "$PROJECT_DIR/../rpr2018.mod" "/Users/Shared/RadeonProRender/modules/rpr2018.mod"

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
if [ -d "/Users/Shared/Autodesk/modules/maya/2018/" ]; then
cp "$PROJECT_DIR/../rpr2018.mod" "/Users/Shared/Autodesk/modules/maya/2018/rpr.mod"
fi

localUsers=$( dscl . list /Users UniqueID | awk '$2 >= 501 {print $1}' | grep -v admin )
for userName in $localUsers; do
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2016/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2016/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2016.5/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2016.5/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2018/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2018/prefs/shelves/"
fi

done
