@echo off

setlocal enabledelayedexpansion

:get_plugin_version
for /f %%v in ('powershell -executionPolicy bypass -file GetVersion.ps1') do (
	set MAYA_PLUGIN_VERSION=%%v
)

:cleanup
del RadeonProRender*.msi
rmdir /S /Q  bin dist output system PluginInstaller\bin PluginInstaller\obj PluginInstaller\Modules PluginInstaller\Generated

:: parse options
if "%1"=="clean" goto :eof

if "%1"=="build_installer" goto :build_installer

:build_plugin
echo Building Radeon ProRender plug-ins for Maya

pushd ..\..\RadeonProRenderMayaPlugin

call build_windows.cmd

popd

:build_installer
echo Building Radeon ProRender for Maya installer %MAYA_PLUGIN_VERSION%

call create_module.cmd 2017
call create_module.cmd 2018
call create_module.cmd 2019

:: update ThirdParty
set ThirdPartyDir="..\ThirdParty"

xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\bin" "system\PluginInstaller\InputData\feature_Core\bin\*"
xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\data" "system\PluginInstaller\InputData\feature_Core\data\*"
xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\icons" "system\PluginInstaller\InputData\feature_Core\icons\*"
xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\images" "system\PluginInstaller\InputData\feature_Core\images\*"
xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\renderDesc" "system\PluginInstaller\InputData\feature_Core\renderDesc\*"
xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\scripts" "system\PluginInstaller\InputData\feature_Core\scripts\*"
xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\shaders" "system\PluginInstaller\InputData\feature_Core\shaders\*"

xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\plug-ins\2017" "system\PluginInstaller\InputData\feature_2017\2017\*"
xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\plug-ins\2018" "system\PluginInstaller\InputData\feature_2018\2018\*"
xcopy /S /Q "..\..\RadeonProRenderMayaPlugin\dist\plug-ins\2019" "system\PluginInstaller\InputData\feature_2019\2019\*"

:: scene convertion scripts
mkdir "system\PluginInstaller\InputData\feature_ConvScripts"
copy /Y "%ThirdPartyDir%\SceneConvertionScripts\convertAI2RPR.py" "system\PluginInstaller\InputData\feature_ConvScripts\convertAI2RPR.py"
copy /Y "%ThirdPartyDir%\SceneConvertionScripts\convertRS2RPR.py" "system\PluginInstaller\InputData\feature_ConvScripts\convertRS2RPR.py"


:: copy material library
:: EXCLUDE MATERIAL LIBRARY FROM INSTALLER
if 0 == 1 (
set MaterialLibraryXML="..\MaterialLibrary\2.0\xml_catalog_output"
set InstallerMatLibFolder="system\PluginInstaller\InputData\feature_MaterialLibrary\Xml"
xcopy /S /Q %materialLibraryXML% %InstallerMatLibFolder%\*

set MaterialLibraryMaps="..\MaterialLibrary\2.0\Maps"
set InstallerMatLibMapsFolder="system\PluginInstaller\InputData\feature_MaterialLibrary\Maps"
xcopy /S /Q %materialLibraryMaps% %InstallerMatLibMapsFolder%\*
)

:: build an installer and patch its name with a version
call build.cmd

move output\PluginInstaller\Release\en-US\RadeonProRenderSetup.msi "RadeonProRenderMaya_%MAYA_PLUGIN_VERSION%.msi"
