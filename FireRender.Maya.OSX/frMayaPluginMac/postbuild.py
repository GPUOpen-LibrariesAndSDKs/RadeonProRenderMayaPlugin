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


