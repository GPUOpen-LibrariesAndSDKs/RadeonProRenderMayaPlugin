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


import sys
import os
import subprocess
import shutil
import datetime
import errno
import re


# Read plug-in info from the common header file.
# -----------------------------------------------------------------------------
def read_plugin_info() :

	global plugin_name
	global plugin_version

	common_file_name = "../fireRender/common.h"
	common_file = open( common_file_name, "r" )
	s = common_file.read()

	val = re.match( "(?s).*#define PLUGIN_VERSION\s\"(\S+)\"", s)
	if val == None :
		raise NameError("Unable to read plug-in version")

	plugin_version = val.group(1)

	val = re.match( "(?s).*#define FIRE_RENDER_FILE_NAME\s\"(\S+)\"", s)
	if val == None :
		raise NameError("Unable to read plug-in name")

	plugin_name = val.group(1)

	#print( "Plugin Name: \"%s\"" % plugin_name)
	#print( "Plugin Version: \"%s\"" % plugin_version)

	common_file.close()


# Create a module file for the specified Maya version.
# -----------------------------------------------------------------------------
def create_module(maya_version) :

	module_directory = "dist/modules/" + maya_version + "/"
	module_file_name = module_directory + plugin_name +".module"

	#print("Generate module: " + module_file_name)

	if os.path.exists(module_directory) == False :
		os.makedirs(module_directory)	

	module_file = open(module_file_name, "w")

	install_location = "/opt/AMD/RadeonProRenderPlugins/Maya"

	module_file.write("+ MAYAVERSION:%s %s %s %s\n" % (maya_version, plugin_name, plugin_version, install_location))
	module_file.write("PATH+:=bin\n")
	module_file.write("MAYA_RENDER_DESC_PATH+:=renderDesc\n")
	module_file.write("RPR_MATERIAL_LIBRARY_PATH:=materials\n")
	module_file.write("RPR_SCRIPTS_PATH:=scripts\n")
	module_file.write("XBMLANGPATH+:=icons\n")
	module_file.write("plug-ins: plug-ins/%s/\n" % maya_version)
	
	module_file.close()


read_plugin_info()

create_module("2016")create_module("2016.5")create_module("2017")

print(plugin_version)
