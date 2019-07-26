#!/bin/bash

ThirdPartyDir="../../RadeonProRenderThirdPartyComponents"

if [ -d "$ThirdPartyDir" ]; then
    echo Updating $ThirdPartyDir

    rm -rf 'RadeonProImageProcessing'
    rm -rf 'RadeonProRender SDK'
    rm -rf RadeonProRender-GLTF
    rm -rf glew
    rm -rf oiio-mac
    rm -rf aws
	rm -rf json

    cp -r $ThirdPartyDir/RadeonProImageProcessing RadeonProImageProcessing
    cp -r "$ThirdPartyDir/RadeonProRender SDK" "RadeonProRender SDK"
    cp -r $ThirdPartyDir/RadeonProRender-GLTF RadeonProRender-GLTF
    cp -r $ThirdPartyDir/glew glew
    cp -r $ThirdPartyDir/oiio-mac oiio-mac
    cp -r $ThirdPartyDir/aws aws
	cp -r $ThirdPartyDir/json json
	
	cp $ThirdPartyDir/SceneConvertionScripts/convertAI2RPR.py ../FireRender.Maya.Src/python/fireRender/convertAI2RPR.py
	cp $ThirdPartyDir/SceneConvertionScripts/convertRS2RPR.py ../FireRender.Maya.Src/python/fireRender/convertRS2RPR.py
	
    echo ===============================================================
    pushd $ThirdPartyDir 
    git remote update
    git status -uno
    popd
	
else
    echo Cannot update as $ThirdPartyDir missing
fi
