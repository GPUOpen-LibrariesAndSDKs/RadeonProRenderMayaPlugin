pushd ../FireRender.Maya.OSX/frMayaPluginMac/frMayaMac

xcodebuild -scheme "Release 2018" -project ./RadeonProRender.xcodeproj clean
xcodebuild -scheme "Release 2019" -project ./RadeonProRender.xcodeproj clean
xcodebuild -scheme "Release 2020" -project ./RadeonProRender.xcodeproj clean

xcodebuild -quiet -scheme "Release 2018" -project ./RadeonProRender.xcodeproj -UseModernBuildSystem=NO
if test $? -eq 0
then
	echo "Release 2019 built successfully"
else
	echo "Release 2019 failed to build"
	popd
	exit 1	
fi

xcodebuild -quiet -scheme "Release 2019" -project ./RadeonProRender.xcodeproj -UseModernBuildSystem=NO
if test $? -eq 0
then
	echo "Release 2019 built successfully"
else
	echo "Release 2019 failed to build"
	popd
	exit 1
fi

xcodebuild -quiet -scheme "Release 2020" -project ./RadeonProRender.xcodeproj -UseModernBuildSystem=NO
if test $? -eq 0
then
	echo "Release 2020 built successfully"
else
	echo "Release 2020 failed to build"
	popd
	exit 1
fi

popd

rm -rf ./darwin-support/Checker/.build
# Parameters: --sign will prompt user for the signing id
python3 build_osx_installer.py  --nomatlib  $1

# Clean the dev modules to enable testing locally

rm -f /Users/Shared/Autodesk/modules/maya/2018/rpr.mod
rm -f /Users/Shared/Autodesk/modules/maya/2019/rpr.mod
rm -f /Users/Shared/Autodesk/modules/maya/2020/rpr.mod
