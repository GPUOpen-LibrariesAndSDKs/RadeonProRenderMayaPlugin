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

# Create build directories.
rm -rf build

mkdir build
mkdir build/2016
mkdir build/2016.5
mkdir build/2017

mkdir build/2016/debug
mkdir build/2016/release
mkdir build/2016.5/debug
mkdir build/2016.5/release
mkdir build/2017/debug
mkdir build/2017/release

 export CMAKE_CXX_COMPILER=/opt/rh/devtoolset-6/root/usr/bin/c++

# Generate Maya 2016 make files.
echo "Maya 2016 Debug"
cd build/2016/debug
cmake -DMAYA_VERSION=2016 -DCMAKE_BUILD_TYPE=Debug "../../../../" -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER
cd ../../..

echo "Maya 2016 Release"
cd build/2016/release
cmake -DMAYA_VERSION=2016 -DCMAKE_BUILD_TYPE=Release "../../../../" -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER
cd ../../..

# Generate Maya 2016.5 make files.
echo "Maya 2016.5 Debug"
cd build/2016.5/debug
cmake -DMAYA_VERSION=2016.5 -DCMAKE_BUILD_TYPE=Debug "../../../../" -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER
cd ../../..

echo "Maya 2016.5 Release"
cd build/2016.5/release
cmake -DMAYA_VERSION=2016.5 -DCMAKE_BUILD_TYPE=Release "../../../../" -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER
cd ../../..

# Generate Maya 2017 make files.
echo "Maya 2017 Debug"
cd build/2017/debug
cmake -DMAYA_VERSION=2017 -DCMAKE_BUILD_TYPE=Debug "../../../../" -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER
cd ../../..

echo "Maya 2017 Release"
cd build/2017/release
cmake -DMAYA_VERSION=2017 -DCMAKE_BUILD_TYPE=Release "../../../../" -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER
cd ../../..
