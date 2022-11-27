:: Copyright 2020 Advanced Micro Devices, Inc
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::     http://www.apache.org/licenses/LICENSE-2.0
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.

@echo off

set mayaVersions=2020 2022 2023

setlocal enabledelayedexpansion

:vs_setup

call vs_path.bat

:: VS not detected
if %vs_ver%=="" goto :vs_error

:: check VS version
set vs_major=%vs_ver:~0,2%


if %vs_major%==16 (
	echo Visual Studio 2019 is installed.
	echo "%vs_dir%"

	echo Trying to setup toolset 142 of Visual Studio 2019.

	set vs19="%vs_dir%\VC\Auxiliary\Build\vcvarsall.bat"

	pushd .	
	call !vs19! amd64 -vcvars_ver=14.2
	popd

	goto :build_plugin
)

:vs_error
	echo Visual Studio 2019 with platform toolset 142 has to be installed.
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
