Prerequisites:
--------------
- Install Homebrew, OpenImageIO, GLEW (2.0) by executing the following:
	- ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
	- /usr/local/bin/brew install homebrew/science/openimageio
        - this step may not be needed anymore
	- /usr/local/bin/brew install glew


Submodules:
---------
Plugin includes several shared source files from Shared Components repository as a submodule. You may need to get an
access to the https://github.com/Radeon-Pro/RadeonProRenderSharedComponents.

Shared components submodule is included to the project via SSH protocol. You will need to create and install SSH keys
to access submodules.

At first you need to generate SSH key pair:
1. Open a terminal
2. Go to your home directory by typing cd ~/
3. Type the following command
    - ssh-keygen -t rsa
4. When you press enter, two files will be created
    ~/.ssh/id_rsa
    ~/.ssh/id_rsa.pub
5. Your public key is stored in the file ending with .pub, i.e. ~/.ssh/id_rsa.pub

In order to authenticate yourself and your device with GitHub, you need to upload your public SSH key generated above
to your GitHub account.

6. Copy public SSH key
Open a terminal and type
    $ pbcopy < ~/.ssh/id_rsa.pub

7. You need to add this key to your github account. Go to Github accout settings, SSH and GPG keys, click New GPG key.
In the "Key" field, paste your public SSH key and click Add GPG key.


Once you've got an access and setup ssh please update submodules from the parent folder of the project:

```
$ git submodule init
$ git submodule update

```

or 

`git submodule update --init -f --recursive`


Building:
---------

- The xcode project is located in: FireRender.Maya.OSX/frMayaPluginMac/frMayaMac/RadeonProRender.xcodeproj
- This project has been updated to build the Maya 2017 + 2-18 versions of the plugin
- Install the prerequisites above along with the Maya 2017 SDK before building
- For Maya 2018, the include and libraries are provided in the Maya application install
- The name of the plugin binary that is created is RadeonProRender.bundle
- Select the type of build 2017/2018 debug/release from the Product -> Schemes menu
- It is automatically installed into the /Users/Shared/RadeonProRender module location
- NOTE: unfortunately the code has many warnings on the OSX platform

Running:
--------

- Running the plugin is as simple as executing Run in Xcode
- Maya is opened and the plugin module path will be setup to find the plugin
	- The module path is located at: /Users/Shared/Autodesk/modules/maya
- Open the Maya plugin manager to load the RadeonProRender.bundle plugin
	- Windows -> Settings/Preferences -> Plugin Manager

Alternately, you can run Maya from a terminal using:
    /Applications/Autodesk/maya2020/Maya.app/Contents/MacOS/maya

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

- Currently RPR Core doesn’t support NVIDIA cards on Mac.
	(We are currently not checking for it (though could be relative easily done in “checkGpuCompatibleWithFR()”))
- Multiple rebuilding cache warnings seem to come up


Technical Notes:
----------------

You may at some point want to change your Maya license server, you can do this by resetting the licesne information.
- Resetting the Maya license information can be done by remove the following file:
	- /Library/Application Support/Autodesk/CLM/LGS

