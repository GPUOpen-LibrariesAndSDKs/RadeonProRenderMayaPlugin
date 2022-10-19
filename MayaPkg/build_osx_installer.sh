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


pushd ../FireRender.Maya.OSX/frMayaPluginMac/frMayaMac

xcodebuild -scheme "Release 2020" -project ./RadeonProRender.xcodeproj clean
xcodebuild -scheme "Release 2022" -project ./RadeonProRender.xcodeproj clean
xcodebuild -scheme "Release 2023" -project ./RadeonProRender.xcodeproj clean

xcodebuild -quiet -scheme "Release 2020" -project ./RadeonProRender.xcodeproj -UseModernBuildSystem=NO
if test $? -eq 0
then
	echo "Release 2020 built successfully"
else
	echo "Release 2020 failed to build"
	popd
	exit 1
fi

xcodebuild -quiet -scheme "Release 2022" -project ./RadeonProRender.xcodeproj -UseModernBuildSystem=NO
if test $? -eq 0
then
	echo "Release 2022 built successfully"
else
	echo "Release 2022 failed to build"
	popd
	exit 1
fi

xcodebuild -quiet -scheme "Release 2023" -project ./RadeonProRender.xcodeproj -UseModernBuildSystem=NO
if test $? -eq 0
then
	echo "Release 2023 built successfully"
else
	echo "Release 2023 failed to build"
	popd
	exit 1
fi

popd

rm -rf ./darwin-support/Checker/.build
# Parameters: --sign will prompt user for the signing id
python3 build_osx_installer.py  --nomatlib  $1

# Clean the dev modules to enable testing locally

rm -f /Users/Shared/Autodesk/modules/maya/2020/rpr.mod
rm -f /Users/Shared/Autodesk/modules/maya/2022/rpr.mod
rm -f /Users/Shared/Autodesk/modules/maya/2023/rpr.mod
