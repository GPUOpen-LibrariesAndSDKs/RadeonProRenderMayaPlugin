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

def tokenHasLinkFromList(token, library_list):
    for name in library_list:
        if token.find(name.encode()) != -1:
            return True

def remapLibraryPaths(library_list, dpath, strEnding, targetPath):
    print("\nRemapping paths %s\n" % str(dpath))
    # Hande ids first
    for lib in os.listdir(str(dpath)):
        if lib.endswith(strEnding):
            print("Processing lib: " + lib)
            #Fix Library Id
            print ("change Id")
            cmd=['install_name_tool','-id', str(targetPath) +str("/") + lib, str(dpath / lib)]
            #cmd=['install_name_tool','-id', str(npath +str("/") + lib), str(dpath / lib)]
            print(cmd)
            result = subprocess.check_output(cmd)

            print ("change dependencies")
            #Fix dependencies
            cmd=['otool','-L', str(dpath / lib)]
            result = subprocess.check_output(cmd)
            tokens = result.split()

            #remove first element because this is just name of the file followed with colon, e.g "libTahoe64:"
            tokens.pop(0)

            for t in tokens:
                if tokenHasLinkFromList(t, library_list):
                    print("found: %s" % t)
                    dependentlib = os.path.basename(t).decode('utf-8')
                    print("lib %s" % dependentlib)

                    newPathValue = str(targetPath) +str("/") + dependentlib
                    oldPathValue = t.decode('utf-8')

                    if oldPathValue == newPathValue:
                        #print ("old path and new are equal. Skip")
                        continue

                    cmd=['install_name_tool','-change', oldPathValue, newPathValue, str(dpath / lib)]
                    print(cmd)
                    result = subprocess.check_output(cmd)
                    print(result)
                    print("\n")

    for lib in os.listdir(str(dpath)):
        if lib.endswith(strEnding):
            cmd=['otool','-L', str(dpath / lib)]
            result = subprocess.check_output(cmd)
            tokens = result.split()
            for t in tokens:
                if t.find(b'@rpath') != -1:
                    print("WARNING: Dynamic library %s has rpath reference" % str(dpath / lib))
                    print("\t %s\n" % t)
