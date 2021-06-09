#
# Copyright 2020 Advanced Micro Devices, Inc
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#    http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import glob
import maya.cmds
import sys

def setShaderCachePathEnvironment(version):
    projectPath = maya.cmds.workspace(q = True, rd = True) + "cache/"
    frCacheVersionPath = "fr_render/" + version + "/"
    frcachepath = projectPath + frCacheVersionPath
    if not os.path.exists(frcachepath):
        os.makedirs(frcachepath)

    if sys.version_info[0] < 3:
        os.environ["FR_SHADER_CACHE_PATH"] = frcachepath.encode("utf8")
    else:
        os.environ["FR_SHADER_CACHE_PATH"] = frcachepath
