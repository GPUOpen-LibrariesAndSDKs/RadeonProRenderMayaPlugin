This project builds an MSI file which can be used to install FireRender for Maya

Visual Studio requirements: WiX toolset, could be downloaded here
http://wixtoolset.org/releases/

Note: if you get the following error
	error MSB4036: The "WixAssignCulture" task was not found.

it may be because of an annoying NET 3.5 dependency in parts of WiX, try closing VS and follow instructions at this link

http://stackoverflow.com/questions/35132076/cannot-build-wix-project-on-windows-10


----------------------------------------------------------------------------------
builder.py - script for full assemble installer
build_installer.cmd - command file for execute builder.py with python check

Requirements:
- Maya 2016 & 2017
- Maya dev-kits 2016 & 2017
- Python 2.x and above


For assemble build:

1. First of all we update repository frMaya & FireRenderMaxMaterialLibrary, also check paths, please.
2. Increment version if it need (from frMaya\fireRender\common.h)
3. We should execute "frMaya\PluginInstaller\build_installer.cmd"
4. After that we have output file "frMaya/output/_ProductionBuild/RadeonProRenderForMaya.<version>.msi"
5. In the end we shouldn't forget commit files (common.h, Modules, Variables.wxi)


we can build installer in MSVC after build by script, but it use for debug and test only (not for publish)


Known troubles:
I have had a trouble with my MSVC, sometimes I couldn't load projects.
Remove folder c:\Users\<user_name>\AppData\Local\Microsoft\VisualStudio\14.0\ComponentModelCache helps for me.
