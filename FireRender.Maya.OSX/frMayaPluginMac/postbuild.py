import os
import sys
import shutil
import subprocess

from pathlib import Path

sys.path.insert(1, '../FireRender.Maya.OSX/frMayaPluginMac')
import remappath

# Search all libs in particular folder and fix path them. Also fixpath bundle file

path_to_search = "/Users/Shared/RadeonProRender/lib"
name_ending = ".dylib"

library_list = []

for name in os.listdir((str(path_to_search))):
    if name.endswith(name_ending):
        library_list.append(name)

print("Library List to postprocess")
print(library_list)

remappath.remapLibraryPaths(library_list, Path(path_to_search), name_ending, Path(path_to_search))

bundle_path = "/Users/Shared/RadeonProRender/plug-ins/" + os.environ['SCHEME_NAME'].split()[1];
remappath.remapLibraryPaths(library_list, Path(bundle_path), ".bundle", Path(path_to_search))


