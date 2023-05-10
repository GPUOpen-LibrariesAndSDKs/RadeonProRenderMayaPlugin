#/usr/bin/python3

#
# Copyright 2020 Advanced Micro Devices, Inc
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import os
import stat
import sys
import argparse
import shutil
import subprocess
from pathlib import Path

import create_osx_build_output

sys.path.insert(1, '../FireRender.Maya.OSX/frMayaPluginMac')
import remappath

parser = argparse.ArgumentParser()
parser.add_argument('--sign', action='store_true')
parser.add_argument('--nomatlib', action='store_true')

args = parser.parse_args()

signApp = args.sign
noMatLib = args.nomatlib

installer_build_dir = Path("./.installer_build")

# Ugly but works for now
module_source_dir = Path("../FireRender.Maya.OSX/frMayaPluginMac")
shelf_source_dir = Path("../FireRender.Maya.Src/Shelfs")

if installer_build_dir.exists():
    shutil.rmtree(str(installer_build_dir))

buildInstallerApp = True

plugin_version, plugin_version_parts = create_osx_build_output.ReadAddOnVersion()
print("Version info: %s %s" % (plugin_version, plugin_version_parts))

# Need the following directories:
# dist/Maya/plugin
# dist/Maya/libs
# dist/Maya/materials
# dist/resources
# dist/scripts
# dist/pkg
# dist/bld
#
dist_dir = installer_build_dir / 'dist'
addon_files_dist_dir = dist_dir / 'Maya/plugin'
addon_files_dist_dir.mkdir(parents=True)
libs_files_dist_dir = dist_dir / 'Maya/lib'
libs_files_dist_dir.mkdir(parents=True)
material_library_dist_dir = dist_dir / 'Maya/materials'
modules_dist_dir = dist_dir / 'Maya/modules'
scripts_dist_dir = dist_dir / 'Maya/scripts'
scripts_dist_dir.mkdir(parents=True)
resource_files_dist_dir = dist_dir / 'resources'
resource_files_dist_dir.mkdir(parents=True)
pkg_files_dist_dir = dist_dir / 'pkg'
pkg_files_dist_dir.mkdir(parents=True)
bld_files_dist_dir = dist_dir / 'bld'
bld_files_dist_dir.mkdir(parents=True)
scripts_files_dist_dir = dist_dir / 'scripts'
scripts_files_dist_dir.mkdir(parents=True)

build_output_dir = Path("/Users/Shared/RadeonProRender")

if noMatLib:
	print("Not adding matlib\n");
else:
	create_osx_build_output.CreateMaterialLibrary(str(material_library_dist_dir))

support_path = Path('./darwin-support')
for name in ['welcome.html']:
    shutil.copy(str(support_path / name), str(resource_files_dist_dir))

legal_path = Path('../Legal')
for name in ['eula.txt']:
    shutil.copy(str(legal_path / name), str(resource_files_dist_dir))

if buildInstallerApp:
    for name in ['README-INSTALLERAPP.txt']:
        shutil.copy(str(support_path / name), str(bld_files_dist_dir / "README.txt"))
else:
    for name in ['README.txt']:
        shutil.copy(str(support_path / name), str(bld_files_dist_dir / "README.txt"))

for name in ['postinstall', 'uninstall']:
    shutil.copy(str(support_path / name), str(scripts_dist_dir /  name) )

# Special case - name is postinstall so that pkgbuild system knows to run it
for name in ['postinstall-checker']:
    shutil.copy(str(support_path / name), str(scripts_files_dist_dir) + str("/postinstall"))

#copy notices.txt
shutil.copy(str("./notices.txt"), str(dist_dir / "Maya/notices.txt"))

# Build the checker

lib_path_hint = ''
checker_path = support_path / 'Checker'
checker_build_path = checker_path / '.build'
checker_build_path.mkdir(exist_ok=True)
subprocess.check_call(['cmake', '-DCMAKE_LIBRARY_PATH='+ lib_path_hint +'/lib/x86_64-darwin-gnu', '..'], cwd=str(checker_build_path))
subprocess.check_call(['make'], cwd=str(checker_build_path))

for name in ['checker']:
    shutil.copy(str(support_path / "Checker/.build" / name), str(addon_files_dist_dir))

# Copy libraries

create_osx_build_output.copy_libs(libs_files_dist_dir)

# Remap library paths

opath = "/Users/Shared/RadeonProRender/lib"
npath = "/Users/Shared/RadeonProRender/Maya/lib"

#gather all libs npath
library_list = []
for name in os.listdir((str(libs_files_dist_dir))):
    if name.endswith(".dylib"):
        library_list.append(name)

remappath.remapLibraryPaths(library_list, Path(libs_files_dist_dir), ".dylib", Path(npath))

# Add the plugin files

for name in ['icons','images','plug-ins','renderDesc','scripts','shaders','shelves', 'data', 'hipbin']:
    shutil.copytree(str(build_output_dir / name), str(addon_files_dist_dir / name))

for name in ['convertAI2RPR.py', 'convertRS2RPR.py', 'convertVR2RPR.py']:
    shutil.copy(str(Path('../RadeonProRenderSharedComponents/SceneConversionScripts') / name), str(Path(addon_files_dist_dir) / 'scripts/fireRender' / name))

for dirlib in os.listdir(str(addon_files_dist_dir/"plug-ins")):
    if (dirlib != ".DS_Store"):
        remappath.remapLibraryPaths(library_list, Path(addon_files_dist_dir/"plug-ins"/dirlib), ".bundle", Path(npath))

for name in ['modules']:
    shutil.copytree(str(build_output_dir / name), str(modules_dist_dir))

# Remap the directory for the installed plugin
for name in ['rpr2020.mod','rpr2022.mod','rpr2023.mod']:
    module_file = modules_dist_dir/name;
    module_file_content = module_file.read_text()
    os.remove(str(module_file))
    updated_module_file_content = module_file_content.replace("/Users/Shared/RadeonProRender","/Users/Shared/RadeonProRender/Maya/plugin")
    module_file.write_text(updated_module_file_content)

# Create the RprMaya.plist file from the template using the version number

template_plist_file=Path("./RprMayaTemplate.plist")
maya_plist_file=Path("./RprMaya.plist")

content=template_plist_file.read_text()
updated_content=content.replace("<VERSION>",plugin_version)
maya_plist_file.write_text(updated_content)

# Build packages

buildDMG = True
debuggingOpenInstaller = False

pkg_name = '%s/RadeonProRenderPlugin-%s.pkg' % (str(pkg_files_dist_dir),plugin_version)

if not buildInstallerApp:
    cmd = ['pkgbuild',
           '--identifier', 'com.amd.rpr.inst-maya.app',
           '--root', '.installer_build/dist/Maya',
           '--install-location', '/Users/Shared/RadeonProRender/Maya',
           '--scripts', '.installer_build/dist/scripts',
           pkg_name
        ]
else:
    cmd = ['pkgbuild',
           '--identifier', 'com.amd.rpr.inst-maya.app',
           '--root', '.installer_build/dist/Maya',
           '--install-location', '/Users/Shared/RadeonProRender/Maya',
            pkg_name
           ]
subprocess.check_call(cmd)

# productbuild --synthesize --package pkg_name .installer_build/RprMaya.plist
cmd = ['productbuild',
       '--synthesize',
       '--package', pkg_name,
       '.installer_build/RprMaya.plist'
       ]
subprocess.check_call(cmd)

if not buildInstallerApp:
    inst_name = '%s/RadeonProRenderPlugin-installer-%s.pkg' % (str(bld_files_dist_dir),plugin_version)
    cmd = ['productbuild',
           '--distribution', str(maya_plist_file),
           '--resources', '.installer_build/dist/resources',
           '--package-path', '.installer_build/dist/pkg',
           inst_name
           ]
    subprocess.check_call(cmd)

# Make postinstall app

def make_app(dirpath,scriptpath):
    sname = os.path.basename(scriptpath)
    sdir = os.path.dirname(scriptpath)
    app_path=os.path.join(dirpath,sname)
    app_script_dir=os.path.join(app_path+str(".app"),"Contents/MacOS")
    if not os.path.exists(app_script_dir):
        os.makedirs(app_script_dir)
    app_script_name = os.path.join(app_script_dir,str(sname))
    shutil.copy(scriptpath,app_script_name)

    state = os.stat(app_script_name)
    os.chmod(app_script_name,state.st_mode|stat.S_IEXEC)

info_plist_str = """<?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
        <key>CFBundleDevelopmentRegion</key>
        <string>English</string>
        <key>CFBundleExecutable</key>
        <string><APPNAME></string>
        <key>CFBundleGetInfoString</key>
        <string>0.1.0, Copyright 2018 AMD</string>
        <key>CFBundleIconFile</key>
        <string><APPNAME>.icns</string>
        <key>CFBundleIdentifier</key>
        <string>com.amd.<APPNAME></string>
        <key>CFBundleDocumentTypes</key>
        <array>
        </array>
        <key>CFBundleInfoDictionaryVersion</key>
        <string>6.0</string>
        <key>CFBundlePackageType</key>
        <string>APPL</string>
        <key>CFBundleShortVersionString</key>
        <string>0.1.0</string>
        <key>CFBundleSignature</key>
        <string><APPSIGNATURE></string>
        <key>CFBundleVersion</key>
        <string>0.1.0</string>
        <key>NSHumanReadableCopyright</key>
        <string>Copyright 2018 AMD.</string>
        <key>LSMinimumSystemVersion</key>
        <string>10.13.3</string>
    </dict>
    </plist>
"""

trampoline_str = """#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
int main(int argc, const char *argv[])
{
    if (!argv[0]) return -1;
    char* argv0 = strdup(argv[0]);
    if (!argv0) return -1;
    char *dir_str = dirname(argv0);
    char sysbuf[1024];
    sprintf(sysbuf,"%s/../Resources/%s %s",dir_str,"<APPNAME>",dir_str);
    printf("config %s %s %s\\n",argv0,dir_str,sysbuf);
    system(sysbuf);
    free(argv0);
    return 0;
}
"""

exe_str = """#!/bin/bash
dir="$1"
checker="$dir/../MacOS/checker"
postinstall="/Users/Shared/RadeonProRender/Maya/scripts/postinstall"
uninstall="/Users/Shared/RadeonProRender/Maya/scripts/uninstall"
pkg="$dir/../Packages/<PKGNAME>"

operation() {
osascript <<'END'
tell me to activate
set question to display dialog "A Radeon ProRender Maya install package has been found. Select from the following options:" buttons {"Install", "Uninstall"} default button 2
set answer to button returned of question
if answer is equal to "Install" then
return 0
end if
error number -1
END
}

moveinstalltotrash() {
osascript <<'END'
tell application "Finder"
set sourceFolder to POSIX file "/Users/Shared/RadeonProRender/Maya/"
delete sourceFolder  # move to trash
end tell
END
}

dist="/Users/Shared/RadeonProRender/Maya/"
if [ -d "$dist" ]
then
    operation
fi

if [ $? -eq 0 ]
then
    /System/Library/CoreServices/Installer.app/Contents/MacOS/Installer "$pkg"
    if [ -d "$dist" ]
    then
        "$postinstall"
    fi
else
    "$uninstall"
    moveinstalltotrash
fi

"""

def make_installer_app(appName,appExe,dirpath,signing_str):
    icon_file = Path("../Icons/darwin//RadeonProRenderMayaInstaller.icns")
    bundle_path=os.path.join(dirpath,appName+".app")
    contents_path=os.path.join(bundle_path,"Contents")
    for name in ['MacOS', 'Contents', 'Resources', 'Packages']:
        os.makedirs(os.path.join(contents_path,name))
    # Build and place the package into the app
    inst_name = '%s/RadeonProRenderPlugin-installer-%s.pkg' % (os.path.join(contents_path,'Packages'),plugin_version)
    cmd = ['productbuild',
           '--distribution', 'RprMaya.plist',
           '--resources', '.installer_build/dist/resources',
           '--package-path', '.installer_build/dist/pkg',
           inst_name
           ]
    subprocess.check_call(cmd)
    # Copy files
    shutil.copy(str(icon_file),os.path.join(contents_path,'Resources'))
    shutil.copy(str(appExe),os.path.join(contents_path,'MacOS'))
    # Make the trampoline
    trampoline_cpp = os.path.join(str(installer_build_dir),"main.cpp")
    with open(trampoline_cpp,"w") as f:
        final_trampoline_str = trampoline_str.replace("<APPNAME>",appName)
        f.write(final_trampoline_str)
    cmd = [ 'clang', trampoline_cpp, '-o',  os.path.join(os.path.join(contents_path,'MacOS'),appName) ]
    subprocess.check_call(cmd)
    # Make the exe
    app_exe = os.path.join(os.path.join(contents_path,'Resources'),appName)
    with open(app_exe, "w") as f:
        final_exe_str = exe_str.replace("<PKGNAME>",os.path.basename(inst_name))
        f.write(final_exe_str)
    # Make exe executable
    state = os.stat(app_exe)
    os.chmod(app_exe,state.st_mode|stat.S_IEXEC)
    # Make the PkgInfo
    app_signature = "APPLRpMy"
    pkg_info = os.path.join(contents_path,'PkgInfo')
    with open(pkg_info, "w") as f:
        f.write(app_signature)
        f.write("\n")
    # Make the Info.plist
    info_plist = os.path.join(contents_path,'Info.plist')
    with open(info_plist, "w") as f:
        info_plist_str1 = info_plist_str.replace("<APPNAME>",appName)
        info_plist_str2 = info_plist_str1.replace("<APPSIGNATURE>",app_signature)
        f.write(info_plist_str2)
    # Sign if required
    # Check Gatekeeper conformance with: spctl -a -t exec -vv .installer_build/dist/bld/RadeonProRenderMayaInstaller.app/
    if signing_str:
        # Sign the checker
        cmd = ['codesign', '--sign', signing_str, os.path.join(os.path.join(contents_path,'MacOS'),"checker") ]
        subprocess.check_call(cmd)
        # Sign the trampoline
        cmd = ['codesign', '--sign', signing_str, os.path.join(os.path.join(contents_path,'MacOS'),appName) ]
        subprocess.check_call(cmd)
        # Sign the app bundle
        cmd = ['codesign', '--sign', signing_str, bundle_path ]
        try:
            subprocess.check_call(cmd)
        except:
            print("Bundle may already be signed\n")

if not buildInstallerApp:
    make_app(str(bld_files_dist_dir),str(support_path / "postinstall"))
    make_app(str(bld_files_dist_dir),str(support_path / "uninstall"))
else:
    app_exe = "./darwin-support/Checker/.build/checker"
    signing_str = ""
    if signApp:
        signing_str = input("##### Enter the Developer ID:")
    make_installer_app("RadeonProRenderMayaInstaller",app_exe,str(bld_files_dist_dir),signing_str)

# Create DMG

if buildDMG:
    dmg_name = '.installer_build/RadeonProRenderMaya_%s.dmg' % plugin_version
    cmd = ['hdiutil',
           'create',
           '-volname', 'RadeonProRenderMaya-%s' % plugin_version,
           '-srcfolder', str(bld_files_dist_dir),
           '-ov',
           dmg_name]
    subprocess.check_call(cmd)
    if debuggingOpenInstaller:
        os.system("open %s" % dmg_name)
else:
    if debuggingOpenInstaller:
        os.system("open %s" % inst_name)




