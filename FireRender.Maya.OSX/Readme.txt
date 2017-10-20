Prerequisites:
--------------
- Install Homebrew, OpenImageIO, GLEW (2.0) by executing the following:
	- ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
	- /usr/local/bin/brew install homebrew/science/openimageio
	- /usr/local/bin/brew install glew

Building:
---------

- The xcode project is located in: FireRender.Maya.OSX/frMayaPluginMac/frMayaMac/RadeonProRender.xcodeproj
- This project has been updated to build the Maya 2017 version of the plugin
- Install the prerequisites above along with the Maya 2017 SDK before building
- The name of the plugin binary that is created is RadeonProRender.bundle
- It is automatically installed into the /Users/Shared/RadeonProRender module location
- NOTE: unfortunately the code has many warnings on the OSX platform

Running:
--------

- Running the plugin is as simple as executing Run in Xcode
- Maya is opened and the plugin module path will be setup to find the plugin
	- The module path is located at: /Users/Shared/Autodesk/modules/maya
- Open the Maya plugin manager to load the RadeonProRender.bundle plugin
	- Windows -> Settings/Preferences -> Plugin Manager

Debugging:
----------

In debugging options for xcode you can set it to wait for an app (browse to Maya.app)

Optional env variables for debugging

Set trace output on
    export FR_TRACE_OUTPUT=/Users/$userName/dev/AMD/frMaya/Trace/Local

Override mat library path
    export RPR_MATERIAL_LIBRARY_PATH=/Users/$userName/Dev/AMD/ProRenderMaxMaterialLibrary/exported

Known Issues:
- - - - - - -

-Currently RPR Core doesn’t support NVIDIA cards on Mac.
	(We are currently not checking for it (though could be relative easily done in “checkGpuCompatibleWithFR()”))
-CPU Rendering:
	- Can’t seem to handle complex scenes random crashes happening at frContextRender and frContextCreateMesh
	- If you have RPR viewport active creating IBL will result in instant crash.
	- The very first frame rendered seems to have memory junk in it.

Technical Notes:
----------------

- Resetting the Maya license information can be done by remove the following file:
	- /Library/Application Support/Autodesk/CLM/LGS


