@echo off

set ThirdPartyDir="..\..\RadeonProRenderThirdPartyComponents"

if exist %ThirdPartyDir% ( 
	if "%1"=="update_thirdparty" (
		echo Updating %ThirdPartyDir% 
	    echo ===============================================================
	    pushd %ThirdPartyDir% 
    	git remote update
	    git status -uno
		popd
	)
	
    rd /S /Q AxfPackage
    rd /S /Q RadeonProImageProcessing
    rd /S /Q "RadeonProRender SDK"
    rd /S /Q RadeonProRender-GLTF
    rd /S /Q oiio
    rd /S /Q glew
	rd /S /Q aws
	rd /S /Q json

    xcopy /S /Y /I %ThirdPartyDir%\AxfPackage\ReleaseDll\* AxfPackage\ReleaseDll
    xcopy /S /Y /I %ThirdPartyDir%\AxfPackage\AxfInclude\* AxfPackage\AxfInclude

    xcopy /S /Y /I %ThirdPartyDir%\RadeonProImageProcessing\* RadeonProImageProcessing
    xcopy /S /Y /I "%ThirdPartyDir%\RadeonProRender SDK\*" "RadeonProRender SDK"
    xcopy /S /Y /I %ThirdPartyDir%\RadeonProRender-GLTF\* RadeonProRender-GLTF
    xcopy /S /Y /I %ThirdPartyDir%\oiio\* oiio
    xcopy /S /Y /I %ThirdPartyDir%\glew\* glew
	xcopy /S /Y /I %ThirdPartyDir%\aws\* aws
	xcopy /S /Y /I %ThirdPartyDir%\json\* json
	
	xcopy /S /Y /I %ThirdPartyDir%\SceneConvertionScripts\convertAI2RPR.py ..\FireRender.Maya.Src\python\fireRender\convertAI2RPR.py*
	xcopy /S /Y /I %ThirdPartyDir%\SceneConvertionScripts\convertRS2RPR.py ..\FireRender.Maya.Src\python\fireRender\convertRS2RPR.py*

) else ( 
    echo Cannot update as %ThirdPartyDir% missing
)
