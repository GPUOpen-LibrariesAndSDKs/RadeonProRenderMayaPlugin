INSTALLER
=========
You will need to download and install WIX Toolset v2: http://wixtoolset.org/
You can also download a Visual Studio Extension.

To build installer you will need all of the Maya SDKs and Maya versions installed (2015, 2016, 2016 ext2/2016_5, 2017).

You will need all of the environment variables setup as described above (locations may vary).
You will need to install Python 2.7: https://www.python.org/downloads/ to c:\Python27 (or change in build_windows_installer.cmd):

In order to build, projects PluginInstaller and PluginInstallerDll need to be loaded to the solution.

Inspect paths in build_windows_installer.cmd (for python) and builder.py (for Visual Studio and relative path positions).

Build by starting build_windows_installer.cmd from the command line.

DEPENDENCIES
============
The installer build setup requires that the RadeonProRenderMayaPlugin repo is cloned in the same directory as RadeonProRenderPkgPlugin.

