#
# Copyright 2020 Advanced Micro Devices, Inc
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


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
cp "$PROJECT_DIR/../rpr2018.mod" "/Users/Shared/RadeonProRender/modules/rpr2018.mod"
cp "$PROJECT_DIR/../rpr2019.mod" "/Users/Shared/RadeonProRender/modules/rpr2019.mod"
cp "$PROJECT_DIR/../rpr2020.mod" "/Users/Shared/RadeonProRender/modules/rpr2020.mod"

# copy module files

if [ -d "/Users/Shared/Autodesk/modules/maya/2018/" ]; then
cp "$PROJECT_DIR/../rpr2018.mod" "/Users/Shared/Autodesk/modules/maya/2018/rpr.mod"
fi
if [ -d "/Users/Shared/Autodesk/modules/maya/2019/" ]; then
cp "$PROJECT_DIR/../rpr2019.mod" "/Users/Shared/Autodesk/modules/maya/2019/rpr.mod"
fi
if [ -d "/Users/Shared/Autodesk/modules/maya/2020/" ]; then
cp "$PROJECT_DIR/../rpr2020.mod" "/Users/Shared/Autodesk/modules/maya/2020/rpr.mod"
fi

localUsers=$( dscl . list /Users UniqueID | awk '$2 >= 501 {print $1}' | grep -v admin )
for userName in $localUsers; do

if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2018/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2018/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2019/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2019/prefs/shelves/"
fi
if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2020/prefs/shelves/" ]; then
cp -r "$PROJECT_DIR/../../../FireRender.Maya.Src/shelfs/" "/Users/$userName/Library/Preferences/Autodesk/maya/2020/prefs/shelves/"
fi


done
