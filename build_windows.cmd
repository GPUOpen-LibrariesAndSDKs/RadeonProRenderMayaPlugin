@echo off

set mayaVersions=2017 2018 2019

setlocal enabledelayedexpansion

:update_3dparty
pushd ThirdParty
call win_update.cmd
popd

:vs_setup

call vs_path.bat

:: VS not detected
if %vs_ver%=="" goto :vs_error

:: check VS version
set vs_major=%vs_ver:~0,2%

if %vs_major%==14 (
	echo Visual Studio 2015 is installed.

	pushd "%VS140COMNTOOLS%..\..\VC"
	call vcvarsall.bat amd64
	popd
	
	goto :build_plugin
)

set vs17=""

if %vs_major%==15 (
	echo Visual Studio 2017 is installed.
	echo "%vs_dir%"

	echo Trying to setup toolset 14 [Visual Studio 2015] of Visual Studio 2017.

	set vs17="%vs_dir%\VC\Auxiliary\Build\vcvarsall.bat"

	pushd .	
	call !vs17! amd64 -vcvars_ver=14.0
	popd

	goto :build_plugin
)

:vs_error
	echo Visual Studio 2015 or newer has to be installed.
	echo Newer version of Visual Studio will be used if it's present (v140 toolset has to be installed).
	goto :eof

:build_plugin

:cleanup
rmdir /S /Q bin Debug dist x64

for %%a in (%mayaVersions%) do (
	set mayaSDK=MAYA_SDK_%%a

	if not defined !mayaSDK! (
		echo !mayaSDK! is not set. Skip.
	) else (
		echo !mayaSDK! is set. Build.
		msbuild RadeonProRenderMayaPlugin.sln /property:Configuration=Release%%a /property:Platform=x64
	)
)
