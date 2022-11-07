# Radeon ProRender Maya Plugin

Development requires Maya 2020 or later.

Dependencies
============

External dependencies must be included in the repository's ThirdParty directory, with the exception of the Maya SDK, see next section.
Please check the README in the ThirdParty directory to see  how to acquire the required libraries.

There is shared components repository included to the project as a submodule. Please check Submodules section.

Please contact a Radeon ProRender AMD developer to find out how to get access to this repository.

Environment Variables
=====================

The following environment variables are assumed defined, you will probably need to add these (Maya defines none).
If you are not building the installer you do not need all versions.

```
	MAYA_SDK_2020
	MAYA_x64_2020
	MAYA_SDK_2022
	MAYA_x64_2022
	MAYA_SDK_2023
	MAYA_x64_2023

eg. (assume Maya 2020 SDK is installed to C:\Program Files\Autodesk\Maya2020\devkitBase folder and so on):
	set MAYA_SDK_2020=C:\Program Files\Autodesk\Maya2020
	set MAYA_x64_2020=C:\Program Files\Autodesk\Maya2020
	set MAYA_SDK_2022=C:\Program Files\Autodesk\Maya2022
	set MAYA_x64_2022=C:\Program Files\Autodesk\Maya2022
	set MAYA_SDK_2023=C:\Program Files\Autodesk\Maya2023
	set MAYA_x64_2023=C:\Program Files\Autodesk\Maya2023
```

You need to restart Developer Studio to .

Submodules
=====================
Plugin includes 3 submodules:
RadeonProRender SDK:
git@github.com:Radeon-Pro/RadeonProRenderSDK.git

Image Processing Library:
git@github.com:Radeon-Pro/RadeonProImageProcessingSDK.git

Shared Components:
git@github.com:Radeon-Pro/RadeonProRenderSharedComponents.git

All of them are included via SSH protocol. You will need to create and install SSH keys
to access submodules. OpenSSH can be installed along with Git.

At first you need to generate SSH key pair:

```
ssh-keygen -t rsa

```
It will ask for location and pass phrase. Accept the default location (usually C:\Documents and Settings\username\.ssh\ or
C:\Users\username\.ssh) by pressing ENTER. After that, make sure to set a strong pass phrase for the key.

Then open the file id_rsa.pub (a public SSH key) and copy its content. You need to add this key to your github account.
Go to Github accout settings, SSH and GPG keys, click New GPG key. In the "Key" field, paste your public SSH key and
click Add GPG key.



Once you've got an access and setup ssh please update submodules from the parent folder of the project:

```
$ git submodule init
$ git submodule update

```

or 

`git submodule update --init -f --recursive`


Running/Debugging the Build on Windows
======================================

In order to run/debug with RPR Renderer Maya needs to find the plugin build products and other associated files.

Set a system environment variable showing location of RprMaya repository folder

```
	SET FR_MAYA_PLUGIN_DEV_PATH={RprMaya Root Folder}
eg:
	set FR_MAYA_PLUGIN_DEV_PATH=E:\Dev\AMD\RprMaya
```

Copy the frMaya.module file to:

```
	%COMMONPROGRAMFILES%\Autodesk Shared\Modules\maya\2020
	%COMMONPROGRAMFILES%\Autodesk Shared\Modules\maya\2022
	%COMMONPROGRAMFILES%\Autodesk Shared\Modules\maya\2023
```
	etc

Note that it is named intentionally named differently to installer RadeonProRender.module files to avoid conflicts.

Useful Info
===========

Most Mel commands for Maya, can be found here:

```
	C:\Program Files\Autodesk\Maya2016\scripts\startup\defaultRuntimeCommands.mel
```

Shelfs can be easily created from the Shelf editor within Maya application.

In order to setup a UI with attribute (for example into render settings):
-create attribute in Mel script with correct hierarchy setup (for Example check out: createFireREnderGlobalsTab.mel file)
-declare attribute in RadeonProRenderGlobals
-write initialization code
-write attribute data in FireRender.xml (not sure if this is actually needed)
-Read attribute's value in FireRenderUtils->readFromCurrentScene(bool)

Versioning
==========

The version number of the plugin should be increased when releasing a new version of the plugin. This is done by
changing the version.h file in the top level directory. A build script will update the build number every time
a commit is done to the master branch so it is only necessary to update the major or minor versions as required
on new releases of the plugin.
