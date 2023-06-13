echo creating dist folders

set SolutionDir=%1
set SolutionDir=%SolutionDir%..\
echo %SolutionDir%

set ProjectDir=%SolutionDir%FireRender.Maya.Src
echo %ProjectDir%

set TargetPath=%2
echo %TargetPath%

set FR_MAYA_VERSION=%3
echo %FR_MAYA_VERSION%

md %SolutionDir%dist
md %SolutionDir%dist\bin
md %SolutionDir%dist\scripts
md %SolutionDir%dist\plug-ins
md %SolutionDir%dist\plug-ins\%FR_MAYA_VERSION%
md %SolutionDir%dist\images
md %SolutionDir%dist\icons
md %SolutionDir%dist\shaders
md %SolutionDir%dist\renderDesc
md %SolutionDir%dist\hipbin


XCOPY /E /Y %ProjectDir%\scripts\* %SolutionDir%dist\scripts\
XCOPY /E /Y %ProjectDir%\python %SolutionDir%dist\scripts\

COPY %TargetPath% %SolutionDir%dist\plug-ins\%FR_MAYA_VERSION%\* /Y

COPY %ProjectDir%\images\* %SolutionDir%dist\images\ /Y

XCOPY /E /Y %ProjectDir%\icons\* %SolutionDir%dist\icons\
COPY %ProjectDir%\shaders\* %SolutionDir%dist\shaders\ /Y

COPY %ProjectDir%\renderDesc\* %SolutionDir%dist\renderDesc\ /Y
COPY %SolutionDir%RadeonProRenderSDK\hipbin\* %SolutionDir%dist\hipbin\ /Y

COPY "%SolutionDir%RadeonProRenderSDK\RadeonProRender\binWin64\*.dll" %SolutionDir%dist\bin\ /Y
COPY "%SolutionDir%RadeonProImageProcessingSDK\Windows\Dynamic\*.dll" %SolutionDir%dist\bin\ /Y
COPY "%SolutionDir%RadeonProRenderSharedComponents\OpenVDB\Windows\bin\*.dll" %SolutionDir%dist\bin\ /Y
COPY "%SolutionDir%RadeonProRenderSharedComponents\Alembic\bin\*.dll" %SolutionDir%dist\bin\ /Y
COPY "%SolutionDir%RadeonProRenderSharedComponents\OpenImageIO\Windows\bin\*.dll" %SolutionDir%dist\bin\ /Y
XCOPY /S /Y /I "%SolutionDir%RadeonProImageProcessingSDK\models" %SolutionDir%dist\data\models

COPY %SolutionDir%RadeonProRenderSharedComponents\glew\bin\x64\*.dll %SolutionDir%dist\bin\ /Y

echo %USERPROFILE%

IF EXIST "%USERPROFILE%\Documents\maya\%FR_MAYA_VERSION%\prefs\shelves\" COPY %ProjectDir%\Shelfs\* "%USERPROFILE%\Documents\maya\%FR_MAYA_VERSION%\prefs\shelves\" /Y

IF EXIST "%USERPROFILE%\Documents\maya\%FR_MAYA_VERSION%-x64\prefs\shelves" COPY %ProjectDir%\Shelfs\* "%USERPROFILE%\Documents\maya\%FR_MAYA_VERSION%-x64\prefs\shelves\" /Y

EXIT 0
