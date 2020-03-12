/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/

#pragma once

//I'll do my best to develop a program that manages Maya versions from year 'MIN_MAYA_VERSION' to year 'MIN_MAYA_VERSION + NB_MAYA_VERSION'
//so if a new Maya version is released, we don't have too many modifications to implement in this installer.
#define NB_MAYA_VERSION 1000
#define MIN_MAYA_VERSION 2000


enum INSTALL_TYPE
{
        INSTALL_TYPE_UNDEF = 0,
        INSTALL_TYPE_MAYAPLUGIN,
        INSTALL_TYPE_MATERIALLIB,
        INSTALL_TYPE_MAYA_PLUGIN_AND_MATLIB,

};
