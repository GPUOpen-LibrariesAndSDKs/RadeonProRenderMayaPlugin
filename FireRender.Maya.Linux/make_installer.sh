#!/bin/bash

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


# Make the distribution package.
sh ./make_dist.sh

# Generate plug-in module files and get the current plug-in version.
version=$(python generate_modules.py)

# Clean the installer build directory.
rm -rf installer/build
mkdir installer/build

# Copy installer files to the distribution.
cp -ar installer/eula.txt dist
cp -ar installer/install.sh dist
cp -ar installer/uninstall.sh dist/core

# Copy required packages to distribution.
mkdir dist/packages
cp -ar packages/* dist/packages

# Copy materials to the distribution.
mkdir dist/core/materials
cp -ar ../../ProRenderMaxMaterialLibrary/exported/* dist/core/materials
cp -ar ../../ProRenderMaxMaterialLibrary/matlib/RadeonProRMaps dist/core/materials

# Make the self extracting package. It will run install.sh upon extraction.
makeself ./dist ./installer/build/RadeonProRenderForMaya.$version.run "Radeon ProRender for Maya" ./install.sh

# Archive the installer to preserve the execute permission on the run file.
echo "Creating archive..."
cd installer/build/
tar -czvf RadeonProRenderForMaya.Linux.$version.tar.gz RadeonProRenderForMaya.$version.run

# Installer creation complete.
echo "Installer creation complete."
