Prerequisites:
- - - - - - - -
You will need to install Homebrew, OpenImageIO, GLEW (2.0):
For these, run these 3 commands 1 by 1:
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
/usr/local/bin/brew install homebrew/science/openimageio
/usr/local/bin/brew install glew


Build-ing:
- - - - - -
xCode Project in theory is prepared fully.
-It will compile the plugin into rprMayaPluginMac.bundle
-Will clear any previous builds/files from your Mac to be sure all files are up to date after each build
-Will Create and Copy resource folder structure into dist folder under:
“fireRenderMac/frMayaPluginMac/dist/“
(this is mostly needed only for Installer for easier access)
-Will copy the full dist folder onto final location:
“/Users/Shared/RadeonProRender/“
-Will copy the necessary rpr.mod files under Library/Preferences… for all users on your machine. (so that Maya can find the plugin and load it)

Debugging:
- - - - - -

In debugging options for xcode you can set it to wait for an app (browse to Maya.app)

optional env variables for debugging

set trace output on
    export FR_TRACE_OUTPUT=/Users/$userName/dev/AMD/frMaya/Trace/Local

override mat library path
    export RPR_MATERIAL_LIBRARY_PATH=/Users/$userName/Dev/AMD/ProRenderMaxMaterialLibrary/exported

Known Issues:
- - - - - - -
!!!!-Libraries provided by AMD have to be modified!!!!
Read: rpr-lib-FIXES.txt for more.

-If a crash would happen for some reason either reinstall the plugin or if you are developing, build it again (it will do a clear and reinstall of the plugin).
I’ve experienced that Mac/Maya caches the bad state where it crashed and then on forward trying to use the plugin again can cause crashes from then on forward otherwise.

-Currently RPR Core doesn’t support NVIDIA cards on Mac.
	(We are currently not checking for it (though could be relative easily done in “checkGpuCompatibleWithFR()”))
-CPU Rendering:
	- Can’t seem to handle complex scenes random crashes happening at frContextRender and frContextCreateMesh
	- If you have RPR viewport active creating IBL will result in instant crash.
	- The very first frame rendered seems to have memory junk in it.

