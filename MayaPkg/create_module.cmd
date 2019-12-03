@echo off

if "%1"=="" exit /B 1

set MayaVer=%1
set TargetFolder=PluginInstaller\Modules\%MayaVer%

mkdir %TargetFolder%

(
	echo + MAYAVERSION:%MayaVer% RadeonProRender %MAYA_PLUGIN_VERSION% %%ProgramFiles%%/AMD/RadeonProRenderPlugins/Maya
    echo PATH+:=bin
    echo MAYA_RENDER_DESC_PATH+:=renderDesc
	echo XBMLANGPATH+:=icons
    echo RPR_SCRIPTS_PATH:=scripts
    echo plug-ins: plug-ins/%MayaVer%/
) > %TargetFolder%\RadeonProRender.module
