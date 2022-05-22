@echo off

set vs_ver=""
set vs_dir=""

set VsWhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist %VsWhere% goto :eof

set vs_ver=""

%VsWhere% -version [16.0,17.0) -latest >temp_maya_inst_studios

for /f "usebackq tokens=1* delims=: " %%i in (temp_maya_inst_studios) do (
	if /i "%%i"=="installationVersion" set vs_ver=%%j

	if /i "%%i"=="installationPath" set vs_dir=%%j
)

del temp_maya_inst_studios
