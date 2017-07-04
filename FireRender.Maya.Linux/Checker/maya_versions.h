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
