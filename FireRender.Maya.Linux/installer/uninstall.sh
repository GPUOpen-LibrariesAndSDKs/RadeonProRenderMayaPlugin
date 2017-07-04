# The install location for the plug-in.
installDirectory="/opt/AMD/RadeonProRenderPlugins/Maya"

# Auto load plug-in string.
autoLoadPlugin='evalDeferred("autoLoadPlugin(\\"\\", \\"RadeonProRender\\", \\"RadeonProRender\\")");'

# Get the current user.
currentUser=`who am i | awk '{ print $1 }'`

# Installer text colours.
red='\033[1;31m'
blue='\033[0;34m'
default='\033[0m'

# Check that the user has super user privilege.
if [[ $EUID -ne 0 ]]; then
    echo -e "${red}Please uninstall with super user privilege.${default}" 
    exit 1
fi

# Maya 2016 bundles its own copy of libOpenColorIO which conflicts
# with the version installed as a dependency of OpenImageIO. The Maya 
# version was renamed to avoid conflict, so restore it here.
if [ -f /usr/autodesk/maya2016/lib/libOpenColorIO.so.1.bak ]; then
    mv /usr/autodesk/maya2016/lib/libOpenColorIO.so.1.bak /usr/autodesk/maya2016/lib/libOpenColorIO.so.1
fi

# Remove plug-in modules.
if [ -f /usr/autodesk/modules/maya/2016/RadeonProRender.module ]; then
    rm /usr/autodesk/modules/maya/2016/RadeonProRender.module
fi

if [ -f /usr/autodesk/modules/maya/2016.5/RadeonProRender.module ]; then
    rm /usr/autodesk/modules/maya/2016.5/RadeonProRender.module
fi

if [ -f /usr/autodesk/modules/maya/2017/RadeonProRender.module ]; then
    rm /usr/autodesk/modules/maya/2017/RadeonProRender.module
fi

# Remove shelves.
if [ -f /home/$currentUser/maya/2016/prefs/shelves/shelf_Radeon_ProRender.mel ]; then
    rm /home/$currentUser/maya/2016/prefs/shelves/shelf_Radeon_ProRender.mel
fi

if [ -f /home/$currentUser/maya/2016.5/prefs/shelves/shelf_Radeon_ProRender.mel ]; then
    rm /home/$currentUser/maya/2016.5/prefs/shelves/shelf_Radeon_ProRender.mel
fi

if [ -f /home/$currentUser/maya/2017/prefs/shelves/shelf_Radeon_ProRender.mel ]; then
    rm /home/$currentUser/maya/2017/prefs/shelves/shelf_Radeon_ProRender.mel
fi

# Remove auto load from plug-in preferences.
if [ -f /home/$currentUser/maya/2016/prefs/pluginPrefs.mel ]; then
    sed -i '/'"$autoLoadPlugin"'/d' /home/$currentUser/maya/2016/prefs/pluginPrefs.mel
fi

if [ -f /home/$currentUser/maya/2016.5/prefs/pluginPrefs.mel ]; then
    sed -i '/'"$autoLoadPlugin"'/d' /home/$currentUser/maya/2016.5/prefs/pluginPrefs.mel
fi

if [ -f /home/$currentUser/maya/2017/prefs/pluginPrefs.mel ]; then
    sed -i '/'"$autoLoadPlugin"'/d' /home/$currentUser/maya/2017/prefs/pluginPrefs.mel
fi

# Remove the plug-in files.
rm -rf $installDirectory

# Uninstall complete.
echo -e "${blue}Uninstall complete.${default}"
