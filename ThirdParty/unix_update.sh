#!/bin/bash

ThirdPartyDir="../../RadeonProRenderThirdPartyComponents"

if [ -d "$ThirdPartyDir" ]; then
    echo Updating $ThirdPartyDir

    rm -rf 'RadeonProImageProcessing'
    rm -rf 'RadeonProRender SDK'
    rm -rf RadeonProRender-GLTF
    rm -rf glew
    rm -rf oiio-mac

    cp -r $ThirdPartyDir/RadeonProImageProcessing RadeonProImageProcessing
    cp -r "$ThirdPartyDir/RadeonProRender SDK" "RadeonProRender SDK"
    cp -r $ThirdPartyDir/RadeonProRender-GLTF RadeonProRender-GLTF
    cp -r $ThirdPartyDir/glew glew
    cp -r $ThirdPartyDir/oiio-mac oiio-mac
	
    echo ===============================================================
    pushd $ThirdPartyDir 
    git remote update
    git status -uno
    popd
	
else
    echo Cannot update as $ThirdPartyDir missing
fi
