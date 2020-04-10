#!python3

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


import sys
import os
import platform
import subprocess
import datetime
import shutil
import errno
import re
import tempfile
import zipfile
import io

from pathlib import Path

materialLibraryXML = "../MaterialLibrary/2.0/xml_catalog_output"
materialLibraryMaps = "../MaterialLibrary/2.0/Maps"

repo_root = Path('../../RadeonProRenderMayaPlugin')

def repo_root_pushd():
    os.chdir(str(repo_root))
	
def repo_root_popd():
    os.chdir("../MayaPkg")

def Copy(src, dest):
    try:
        shutil.copytree(src, dest)
    except OSError as e:
        # If the error was caused because the source wasn't a directory
        if e.errno == errno.ENOTDIR:
            shutil.copy(src, dest)
        else:
            raise NameError('Directory not copied. Error: %s' % e)


if platform.system() in ("Linux","Darwin"):
    import pwd

    def get_user_name():
        return pwd.getpwuid(os.getuid())[0]
else:
    def get_user_name():
        return os.getlogin()


def enumerate_addon_data(lib_path,version, target):
    pyrpr_path = repo_root / 'src/bindings/pyrpr'

    repo_root_pushd()
	
    git_commit = subprocess.check_output('git rev-parse HEAD'.split())
    git_tag = subprocess.check_output('git describe --tags --match builds/*'.split())
		
    repo_root_popd()

    version_text = """version=%r
git_commit=%s
git_tag=%s
user=%s
timestamp=%s
target=%s
    """%(
        version,
        repr(git_commit.strip().decode('utf-8')),
        repr(git_tag.strip().decode('utf-8')),
        repr(get_user_name()),
        tuple(datetime.datetime.now().timetuple()),
        repr(target)
        )
    yield version_text.encode('utf-8'), 'version.py'

    #test version a bit
    version_dict = {}
    exec(version_text, {}, version_dict)
    assert version == version_dict['version']
    if lib_path:
        for name in os.listdir(lib_path):
            yield Path(lib_path)/name, name
    else:
        rprsdk_bin = repo_root / 'RadeonProRenderSDK/RadeonProRender/binMacOS'
        for name in os.listdir((str(rprsdk_bin))):
            yield Path(rprsdk_bin)/name, name

        name_ending = ""
        rpipsdk_bin = repo_root / 'RadeonProImageProcessingSDK/OSX'
        name_ending = ".dylib"
        for name in os.listdir((str(rpipsdk_bin))):
            if name.endswith(name_ending):
                yield Path(rpipsdk_bin)/name, name

    # copy pyrpr files
    pyrpr_folders = [pyrpr_path/'.build', pyrpr_path/'src']
    for pyrpr_folder in pyrpr_folders:
        for root, dirs, names in os.walk(str(pyrpr_folder)):
            for name in names:
                if any(ext in Path(name).suffixes for ext in ['.py', '.pyd', '.json']):
                    yield Path(root)/name, Path(root).relative_to(pyrpr_folder)/name

    # copy RPRBlenderHelper files
    rprblenderhelper_folder = repo_root / 'RPRBlenderHelper/.build'
    rprblenderhelper_files = ['libRPRBlenderHelper.dylib']

    for name in rprblenderhelper_files:
        root = rprblenderhelper_folder
        yield Path(root)/name, name

    # copy bindings files
    rprbindings_folder = repo_root / 'src/bindings/pyrpr/.build'
    rprbindings_files = ['_cffi_backend.cpython-35m-darwin.so']
    
    for name in rprbindings_files:
        root = rprbindings_folder
        yield Path(root)/name, name
    
    # copy addon python code
    rprblender_root = str(repo_root / 'src/rprblender')
    for root, dirs, names in os.walk(rprblender_root):
        for name in names:
            if name.endswith('.py') and name != 'configdev.py':
                yield Path(root)/name, Path(root).relative_to(rprblender_root)/name

    # copy img folder
    img_root = str(repo_root / 'src/rprblender/img' )
    for root, dirs, names in os.walk(img_root):
        for name in names:
            if Path(name).suffix in ['.hdr', '.jpg', '.png', '.bmp', '.tga']:
                yield Path(root)/name, Path(root).relative_to(img_root)/'img'/name

    # copy other files
    rprblender_root = str(repo_root / 'src/rprblender')
    for root, dirs, names in os.walk(rprblender_root):
        for name in names:
            if name == 'EULA.html':
                yield Path(root)/name, Path(root).relative_to(rprblender_root)/name
            elif name in ['RadeonProRender_ci.dat', 'RadeonProRender_co.dat', 'RadeonProRender_cs.dat']:
                yield Path(root)/name, Path(root).relative_to(rprblender_root)/name


def CreateAddOnModule(addOnVersion, build_output_folder):
    sys.dont_write_bytecode = True

    print('Creating build_output for AddOn version: %s' % addOnVersion)
    
    commit = subprocess.check_output(['git', 'describe', '--always'])

    build_output_rpblender = os.path.join(build_output_folder, 'rprblender')
    if os.access(build_output_rpblender, os.F_OK):
        shutil.rmtree(build_output_rpblender)

    os.makedirs(build_output_rpblender, 0x777)

    dst_fpath = build_output_rpblender + '/addon.zip'
    print('dst_fpath: ', dst_fpath)
    shutil.copy2('./addon.zip', dst_fpath)


def CreateMaterialLibrary(build_output_folder):
    sys.dont_write_bytecode = True

    print('Creating Material Library for AddOn')
    
    # build_output_matlib = os.path.join(build_output_folder, 'feature_MaterialLibrary')
    build_output_matlib = build_output_folder
    if os.access(build_output_matlib, os.F_OK):
        shutil.rmtree(build_output_matlib)

    #os.makedirs(build_output_matlib, 0x777)

    # build_output_matlibmaps = os.path.join(build_output_folder, 'feature_MaterialLibrary/RadeonProRMaps')
    build_output_matlibmaps = build_output_folder + "/RadeonProRMaps"
    #if os.access(build_output_matlibmaps, os.F_OK):
    #    shutil.rmtree(build_output_matlibmaps)

    #os.makedirs(build_output_matlibmaps, 0x777)    

    Copy( materialLibraryXML, build_output_matlib )
    Copy( materialLibraryMaps, build_output_matlibmaps )


def ReadAddOnVersion() :
    print("ReadAddOnVersion...")

    versionFileName = repo_root / 'version.h'
    result = None
    with open( str(versionFileName), "r" ) as versionFile:
        s = versionFile.read()
    
        val = s.split()
        if val == None :
            raise NameError("Can't get plugin version")
    
        version = val[2].replace("\"","")
        versionParts = version.split(".")
        if(len(versionParts) != 3):
            raise NameError("Invalid version string version.h")
    
        sPluginVersionMajor = versionParts[0].rstrip().lstrip()
        sPluginVersionMinor = versionParts[1].rstrip().lstrip()
        sPluginVersionBuild = versionParts[2].rstrip().lstrip()
    
        sPluginVersion = sPluginVersionMajor + "." + sPluginVersionMinor + "." + sPluginVersionBuild

        result = sPluginVersion, (sPluginVersionMajor, sPluginVersionMinor, sPluginVersionBuild)  
    
        print( "  pluginVersion: \"%s\"" % sPluginVersion)
    
        print("ReadAddOnVersion ok.")
    return result            


def install_tool_name(src,myzip,package_path,opath,npath):
    with tempfile.TemporaryDirectory() as d:
        shutil.copy(str(src), d)
        src_patched = Path(d)/src.name
        cmd = ['install_name_tool', '-change', opath, npath, str(src_patched)]
        print(' '.join(cmd))
        subprocess.check_call(cmd)
        myzip.write(str(src_patched),
                    arcname=str(Path('rprblender') / package_path))


def create_zip_addon(lib_path,package_name, version, target='windows'):

    with zipfile.ZipFile(package_name, 'w') as myzip:
        for src, package_path in enumerate_addon_data(lib_path,version, target=target):
            if isinstance(src, bytes):
                myzip.writestr(str(Path('rprblender') / package_path), src)
            else:
                # print("*** %s" % src.name)
                if '_cffi_backend.cpython-35m-darwin.so' == src.name:
                    install_tool_name(src,myzip,package_path,"/usr/lib/libffi.dylib","/Users/Shared/RadeonProRender/Blender/lib/libffi.dylib")
                elif 'libRPRBlenderHelper.dylib' == src.name:
                    install_tool_name(src,myzip,package_path,"/Users/Shared/RadeonProRender/lib/libRadeonProRender64.dylib","/Users/Shared/RadeonProRender/Blender/lib/libRadeonProRender64.dylib")
                elif 'libRadeonProRender64.dylib' == src.name:
                    install_tool_name(src,myzip,package_path,"/Users/Shared/RadeonProRender/lib/libOpenImageIO.1.7.dylib","/Users/Shared/RadeonProRender/Blender/lib/libOpenImageIO.1.7.dylib")
                else:
                    print('  file', src)
                    myzip.write(str(src),
                                arcname=str(Path('rprblender') / package_path))

def enumerate_lib_data():
    name_ending = ".dylib"
    paths_to_copy_to_maya_lib_folder = ['RadeonProRenderSDK/RadeonProRender/binMacOS', 
                    'RadeonProImageProcessingSDK/OSX', 
                    'ThirdParty/oiio-mac/bin']

    for dirToTakeFrom in paths_to_copy_to_maya_lib_folder:
        path = repo_root / dirToTakeFrom
        for name in os.listdir((str(path))):
            if name.endswith(name_ending):
                yield Path(path)/name, name

def copy_libs(libpath):
    for src, package_path in enumerate_lib_data():
        lpath = libpath / package_path
        print("copy libs %s %s" % (src, lpath))
        shutil.copy(str(src),str(lpath))


