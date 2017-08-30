#!/bin/bash

# The install location for the plug-in.
installDirectory="/opt/AMD/RadeonProRenderPlugins/Maya"

# Required packages.
packageEmbree='embree-lib-2.12.0'
packageOpenImageIO='OpenImageIO-1.2.3'
packageOpenColorIO='OpenColorIO'
packageLibgomp='libgomp'
packageGlibc='glibc'

# Auto load plug-in string.
autoLoadPlugin='evalDeferred("autoLoadPlugin(\\"\\", \\"RadeonProRender\\", \\"RadeonProRender\\")");'

# Get the current user.
currentUser=`who am i | awk '{ print $1 }'`

# Installer text colours.
red='\033[1;31m'
blue='\033[0;34m'
default='\033[0m'

# Check that the Centos major version is correct.
centosVersion="$(rpm -q --queryformat '%{VERSION}' centos-release)"
rhelVersion="$(cat /etc/redhat-release | cut -d ' ' -f 7 | cut -d '.' -f 1)"
if [[ "$centosVersion" != "6" ]] && [[ "$rhelVersion" != "6" ]]; then
    while true; do
        read -e -p "This product is built for CentOS 6 and Red Hat Enterprise Linux 6 - you may encounter errors. Do you wish to continue? (y or n)" yn
        case $yn in
	    [Yy]* ) break;;
	    [Nn]* ) exit;;
	    * ) echo "Please answer yes or no.";;
        esac
    done 
fi

# Check that a compatible version of Maya is installed.
if [ -d /usr/autodesk/maya2016 ]; then
    maya2016Found=true
fi

if [ -d /usr/autodesk/maya2016.5 ]; then
    maya20165Found=true
fi

if [ -d /usr/autodesk/maya2017 ]; then
    maya2017Found=true
fi

if [ "$maya2016Found" != true ] &&
   [ "$maya20165Found" != true ] &&
   [ "$maya2017Found" != true ]; then
    echo -e "${red}Unable to find a compatible version of Maya on this system.${default}" 
    exit 1
fi

# Check that the user has super user privilege.
if [[ $EUID -ne 0 ]]; then
    echo -e "${red}Please install with super user privilege.${default}" 
    exit 1
fi

# Display the EULA and require confirmation after scrolling to the end.
echo -e "${blue}Installing AMD Radeon ProRender for Maya${default}"
echo -e "AMD Radeon ProRender for Maya EULA"
less -e eula.txt
while true; do
    read -p "Do you accept the agreement? (y or n) " yn
    case $yn in
	[Yy]* ) break;;
	[Nn]* ) exit;;
	* ) echo "Please answer yes or no.";;
    esac
done

# Check system capabilities.
./bin/checker

# Install packages required by the plug-in.
echo -e "${blue}Installing Embree${default}"
yum -y install packages/embree-lib-2.12.0-1.x86_64.rpm

echo -e "${blue}Installing OpenImageIO${default}"
yum -y install epel-release
yum -y install $packageOpenImageIO

echo -e "${blue}Installing libgomp${default}"
yum -y install $packageLibgomp

echo -e "${blue}Installing glibc${default}"
yum -y install $packageGlibc

# Check that the required packages were installed.
if ! rpm -q $packageEmbree 2>&1 > /dev/null; then
    echo -e "${red}Unable to install required package: $packageEmbree.${default}" 
    exit 1
fi

if ! rpm -q $packageOpenImageIO 2>&1 > /dev/null; then
    echo -e "${red}Unable to install required package: $packageOpenImageIO.${default}" 
    exit 1
fi

if ! rpm -q $packageOpenColorIO 2>&1 > /dev/null; then
    echo -e "${red}Unable to install required package: $packageOpenColorIO.${default}"
    exit 1
fi

if ! rpm -q $packageLibgomp 2>&1 > /dev/null; then
    echo -e "${red}Unable to install required package: $packageLibgomp.${default}" 
    exit 1
fi

if ! rpm -q $packageGlibc 2>&1 > /dev/null; then
    echo -e "${red}Unable to install required package: $packageGlibc.${default}" 
    exit 1
fi

# Create the install location directory structure.
rm -rf $installDirectory
mkdir -pm755 "/opt/AMD"
mkdir -pm755 "/opt/AMD/RadeonProRenderPlugins"
mkdir -pm755 "/opt/AMD/RadeonProRenderPlugins/Maya"

# Copy plug-in files.
echo -e "${blue}Installing plug-in files...${default}"
cp -ra ./core/* $installDirectory

# Ensure module directories exist.
mkdir -pm755 "/usr/autodesk/modules"
mkdir -pm755 "/usr/autodesk/modules/maya"

if [ ! -d /usr/autodesk/modules/maya/2016 ]; then
    mkdir -pm755 "/usr/autodesk/modules/maya/2016"
    chown $currentUser:$currentUser "/usr/autodesk/modules/maya/2016"
fi

if [ ! -d /usr/autodesk/modules/maya/2016.5 ]; then
    mkdir -pm755 "/usr/autodesk/modules/maya/2016.5"
    chown $currentUser:$currentUser "/usr/autodesk/modules/maya/2016.5"
fi

if [ ! -d /usr/autodesk/modules/maya/2017 ]; then
    mkdir -pm755 "/usr/autodesk/modules/maya/2017"
    chown $currentUser:$currentUser "/usr/autodesk/modules/maya/2017"
fi

# Copy plug-in modules.
cp -a ./modules/2016/RadeonProRender.module /usr/autodesk/modules/maya/2016
chown $currentUser:$currentUser "/usr/autodesk/modules/maya/2016/RadeonProRender.module"

cp -a ./modules/2016.5/RadeonProRender.module /usr/autodesk/modules/maya/2016.5
chown $currentUser:$currentUser "/usr/autodesk/modules/maya/2016.5/RadeonProRender.module"

cp -a ./modules/2017/RadeonProRender.module /usr/autodesk/modules/maya/2017
chown $currentUser:$currentUser "/usr/autodesk/modules/maya/2017/RadeonProRender.module"

# Copy the shelves.
if [ -d /home/$currentUser/maya/2016/prefs/shelves ]; then
    cp -a ./shelves/shelf_Radeon_ProRender.mel /home/$currentUser/maya/2016/prefs/shelves
    chown $currentUser:$currentUser "/home/$currentUser/maya/2016/prefs/shelves/shelf_Radeon_ProRender.mel"
fi

if [ -d /home/$currentUser/maya/2016.5/prefs/shelves ]; then
    cp -a ./shelves/shelf_Radeon_ProRender.mel /home/$currentUser/maya/2016.5/prefs/shelves
    chown $currentUser:$currentUser "/home/$currentUser/maya/2016.5/prefs/shelves/shelf_Radeon_ProRender.mel"
fi

if [ -d /home/$currentUser/maya/2017/prefs/shelves ]; then
    cp -a ./shelves/shelf_Radeon_ProRender.mel /home/$currentUser/maya/2017/prefs/shelves
    chown $currentUser:$currentUser "/home/$currentUser/maya/2017/prefs/shelves/shelf_Radeon_ProRender.mel"
fi

# Update plug-in preferences to auto load the plug-in.
if [ -f /home/$currentUser/maya/2016/prefs/pluginPrefs.mel ]; then
    sed -i '/'"$autoLoadPlugin"'/d' /home/$currentUser/maya/2016/prefs/pluginPrefs.mel
    sed -i '$ a '"$autoLoadPlugin" /home/$currentUser/maya/2016/prefs/pluginPrefs.mel
fi

if [ -f /home/$currentUser/maya/2016.5/prefs/pluginPrefs.mel ]; then
    sed -i '/'"$autoLoadPlugin"'/d' /home/$currentUser/maya/2016.5/prefs/pluginPrefs.mel
    sed -i '$ a '"$autoLoadPlugin" /home/$currentUser/maya/2016.5/prefs/pluginPrefs.mel
fi

if [ -f /home/$currentUser/maya/2017/prefs/pluginPrefs.mel ]; then
    sed -i '/'"$autoLoadPlugin"'/d' /home/$currentUser/maya/2017/prefs/pluginPrefs.mel
    sed -i '$ a '"$autoLoadPlugin" /home/$currentUser/maya/2017/prefs/pluginPrefs.mel
fi

# Maya 2016 bundles its own copy of libOpenColorIO which conflicts
# with the version installed as a dependency of OpenImageIO. Rename
# the Maya version so it will use the installed version instead.
if [ -f /usr/autodesk/maya2016/lib/libOpenColorIO.so.1 ]; then
    mv /usr/autodesk/maya2016/lib/libOpenColorIO.so.1 /usr/autodesk/maya2016/lib/libOpenColorIO.so.1.bak
fi

# Install complete.
echo -e "${blue}Installation complete.${default}"

