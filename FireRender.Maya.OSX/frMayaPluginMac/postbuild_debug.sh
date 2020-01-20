echo "post build debug started"
if [ "$ACTION" = "clean" ]; then
	exit
fi

# copy all the resources and dependencies

cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shaders" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/icons" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/images" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/renderDesc" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/scripts" "/Users/Shared/RadeonProRender/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/python/" "/Users/Shared/RadeonProRender/scripts/"
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/Shared/RadeonProRender/shelves/"

cp -r "$PROJECT_DIR/../../../RadeonProRenderSDK/RadeonProRender/binMacOS/" "/Users/Shared/RadeonProRender/lib/"
cp    "$PROJECT_DIR/../../../RadeonProImageProcessingSDK/radeonimagefilters-1.4.3-2839b0-OSX-rel/bin/libRadeonImageFilters64.1.4.3.dylib" "/Users/Shared/RadeonProRender/lib/libRadeonImageFilters64.1.4.3.dylib"
cp -r  "$PROJECT_DIR/../../../ThirdParty/oiio-mac/bin/" "/Users/Shared/RadeonProRender/lib"

# Make fix pathes in libraries and bundles
python3 $PROJECT_DIR/../postbuild.py


mkdir -p "/Users/Shared/RadeonProRender/modules"
cp "$PROJECT_DIR/../rpr2016.mod" "/Users/Shared/RadeonProRender/modules/rpr2016.mod"
cp "$PROJECT_DIR/../rpr2016.5.mod" "/Users/Shared/RadeonProRender/modules/rpr2016.5.mod"
cp "$PROJECT_DIR/../rpr2017.mod" "/Users/Shared/RadeonProRender/modules/rpr2017.mod"
cp "$PROJECT_DIR/../rpr2018.mod" "/Users/Shared/RadeonProRender/modules/rpr2018.mod"
cp "$PROJECT_DIR/../rpr2019.mod" "/Users/Shared/RadeonProRender/modules/rpr2019.mod"

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
if [ -d "/Users/Shared/Autodesk/modules/maya/2019/" ]; then
cp "$PROJECT_DIR/../rpr2019.mod" "/Users/Shared/Autodesk/modules/maya/2019/rpr.mod"
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
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2019/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2019/prefs/shelves/"
fi

done
