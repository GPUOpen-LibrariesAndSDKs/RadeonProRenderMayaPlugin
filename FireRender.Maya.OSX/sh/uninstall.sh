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


echo "Uninstalling RadeonProRender..."

if [ -d "/Users/Shared/RadeonProRender" ]; then
    echo "Removing: /Users/Shared/RadeonProRender"
    rm -rf "/Users/Shared/RadeonProRender"
fi

rm -f /Users/Shared/Autodesk/modules/maya/2016/rpr.mod
rm -f /Users/Shared/Autodesk/modules/maya/2017/rpr.mod
rm -f /Users/Shared/Autodesk/modules/maya/2018/rpr.mod
rm -f /Users/Shared/Autodesk/modules/maya/2019/rpr.mod

localUsers=$( dscl . list /Users UniqueID | awk '$2 >= 501 {print $1}' | grep -v admin )

for userName in $localUsers; do
    echo "Removing shelves for $userName..."
    if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/" ]; then
        rm -f "/Users/$userName/Library/Preferences/Autodesk/maya/2017/prefs/shelves/shelf_Radeon_ProRender.mel"
    fi
    if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2018/prefs/shelves/" ]; then
        rm -f "/Users/$userName/Library/Preferences/Autodesk/maya/2018/prefs/shelves/shelf_Radeon_ProRender.mel"
    fi
    if [ -d "/Users/$userName/Library/Preferences/Autodesk/maya/2019/prefs/shelves/" ]; then
        rm -f "/Users/$userName/Library/Preferences/Autodesk/maya/2019/prefs/shelves/shelf_Radeon_ProRender.mel"
    fi
done

