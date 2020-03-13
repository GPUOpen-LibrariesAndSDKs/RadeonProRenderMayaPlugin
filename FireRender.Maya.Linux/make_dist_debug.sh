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


# Perform debug builds.
cd build/2016/debug
make
cd ../../../

cd build/2016.5/debug
make
cd ../../../

cd build/2017/debug
make
cd ../../../

# Create new dist directory.
rm -rf dist
mkdir -p dist/core
mkdir -p dist/core/plug-ins/2016
mkdir -p dist/core/plug-ins/2016.5
mkdir -p dist/core/plug-ins/2017
mkdir -p dist/shelves
mkdir -p dist/bin

# Copy RPR libraries to bin.
mkdir dist/core/bin
cp -a "../ThirdParty/RadeonProRender SDK/Linux/lib/*.so*" dist/core/bin

# Copy debug libraries.
cp -a build/2016/debug/fireRender/RadeonProRender.so dist/core/plug-ins/2016
cp -a build/2016.5/debug/fireRender/RadeonProRender.so dist/core/plug-ins/2016.5
cp -a build/2017/debug/fireRender/RadeonProRender.so dist/core/plug-ins/2017

# Copy environment check executable.
cp -a build/2017/debug/fireRenderLinux/Checker/checker dist/bin/checker

# Copy other plug-in files.
cp -ar ../fireRender/shaders dist/core
cp -ar ../fireRender/icons dist/core
cp -ar ../fireRender/images dist/core
cp -ar ../fireRender/renderDesc dist/core
cp -ar ../fireRender/scripts dist/core
cp -ar ../fireRender/python/* dist/core/scripts
cp -ar ../fireRender/Shelfs/* dist/shelves

# Show a message for debug build creation.
echo -e "\033[1;31mDebug build complete.\033[0m"

