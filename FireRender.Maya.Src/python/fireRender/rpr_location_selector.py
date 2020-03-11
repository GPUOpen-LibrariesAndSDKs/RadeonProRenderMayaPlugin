#
# Copyright 2020 Advanced Micro Devices, Inc
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#    http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import maya.cmds as cmds
import maya.mel as mel
import maya.OpenMayaUI as omui
import os.path
import json
import math


# Global properties.
# -----------------------------------------------------------------------------
mapWidth = 1024
mapHeight = 512


# Show the location selector window.
# -----------------------------------------------------------------------------
def show(latitude, longitude, utcOffset):
    RPRLocationSelector(latitude, longitude, utcOffset).show()


# A location selector for the sky generator. It allows
# the user to click on a position on the map, or search
# for a location by name. A plug-in command is used to
# speed up location data processing. If the user clicks
# the OK button, the data is applied to the sun/sky system.
# -----------------------------------------------------------------------------
class RPRLocationSelector:

    # Initialize.
    # -----------------------------------------------------------------------------
    def __init__(self, latitude, longitude, utcOffset) :

        self.latLong = (latitude, longitude)
        self.utcOffset = utcOffset
        self.searchDialog = RPRLocationSearchDialog(self)

    # Show the material browser.
    # -----------------------------------------------------------------------------
    def show(self):

        # Load location data.
        self.loadLocations()

        # Create the window layout.
        self.createLayout()

    # Load location data.
    # -----------------------------------------------------------------------------
    def loadLocations(self):

        # Get the path to the location data files.
        locationPath = os.path.dirname(os.path.abspath(__file__)) + "/locations/"
        locationPath = locationPath.replace("\\", "/")

        # Initialize the location command by calling it with the path.
        cmds.fireRenderLocation(p=locationPath)


    # Create the browser layout.
    # -----------------------------------------------------------------------------
    def createLayout(self):

        # Delete any existing window.
        if (cmds.window("RPRLocationSelectorWindow", exists = True)) :
            cmds.deleteUI("RPRLocationSelectorWindow")

        # Create a new window.
        self.window = cmds.window("RPRLocationSelectorWindow",
                                  resizeToFitChildren=True, sizeable=False,
                                  maximizeButton=False, minimizeButton=False,
                                  width=1044, height=566,
                                  title="Radeon ProRender Location Selector")

        # Create the form that will contain the UI elements.
        self.form = cmds.formLayout(numberOfDivisions=100, width=1044, height=566)

        # Create the map image and location marker.
        self.map = cmds.image(image="location_selector/world_map.jpg", width=1024, height=512)
        self.marker = cmds.image(image="location_selector/map_marker.png")

        # Create an invisible button to receive mouse clicks.
        clickArea = cmds.iconTextButton(style='iconOnly', command=self.selectMapLocation)

        # Create the Search, OK and Cancel buttons.
        searchButton = cmds.button(label="Search", command=self.openSearchDialog, width=100)
        okButton = cmds.button(label="OK", command=self.applyLocation, width=100)
        cancelButton = cmds.button(label="Cancel", command=self.close, width=100)

        # Create the component layout.
        cmds.formLayout(self.form, edit=True,
                        attachForm=[(self.map, 'top', 10), (self.map, 'left', 10),
                                    (clickArea, 'top', 8), (clickArea, 'left', 8),
                                    (clickArea, 'bottom', 38), (clickArea, 'right', 8),
                                    (searchButton, 'left', 10), (searchButton, 'bottom', 10),
                                    (okButton, 'bottom', 10),
                                    (cancelButton, 'right', 10), (cancelButton, 'bottom', 10)],
                        attachControl=[(okButton, 'right', 10, cancelButton)])

        cmds.setParent('..')

        # Set the initial map marker position.
        self.setMarkerToCurrentLocation()

        # Show the location selector window.
        cmds.showWindow(self.window)


    # Select the location from the map.
    # -----------------------------------------------------------------------------
    def selectMapLocation(self):
        # Get cursor position in map space.
        cursor = self.getMapCursor()
        x = cursor.x()
        y = cursor.y()

        # Set the location of the map marker.
        self.setMarkerPosition(x, y)
        self.latLong = cartesianToGeographic(x, y)

        # Update the UTC offset from the geographic coordinate.
        self.utcOffset = cmds.fireRenderLocation(latitude=self.latLong[0],
                                                 longitude=self.latLong[1])


    # Get the cursor position in map space.
    # -----------------------------------------------------------------------------
    def getMapCursor(self):

        # PySide and Shiboken APIs were updated in
        # Maya 2017, so the correct versions must be
        # imported based on the Maya API version.
        mayaVersion = cmds.about(apiVersion=True)

        if mayaVersion >= 201700:

            # Import Qt libraries.
            from PySide2 import QtGui, QtCore, QtWidgets
            from shiboken2 import wrapInstance

            # Get the map image as a Qt widget.
            ptr = omui.MQtUtil.findControl(self.map)
            map =  wrapInstance(long(ptr), QtWidgets.QWidget)

            # Transform the cursor into map widget space.
            cursor = QtGui.QCursor().pos()
            return map.mapFromGlobal(cursor)

        else:

            # Import Qt libraries.
            from PySide import QtGui, QtCore
            from shiboken import wrapInstance

            # Get the map image as a Qt widget.
            ptr = omui.MQtUtil.findControl(self.map)
            map = wrapInstance(long(ptr), QtGui.QWidget)

            # Transform the cursor into map widget space.
            cursor = QtGui.QCursor().pos()
            return map.mapFromGlobal(cursor)


    # Set the position of the map marker.
    # -----------------------------------------------------------------------------
    def setMarkerPosition(self, x, y):

        x = min(mapWidth, max(0, x))
        y = min(mapHeight, max(0, y))

        cmds.formLayout(self.form, edit=True,
                        attachForm=[(self.marker, 'left', x + 5),
                                    (self.marker, 'top', y + 5)])


    # Set the marker to the current location.
    # -----------------------------------------------------------------------------
    def setMarkerToCurrentLocation(self):
        p = geographicToCartesian(self.latLong[0], self.latLong[1])
        self.setMarkerPosition(p[0], p[1])


    # Set the current location.
    # -----------------------------------------------------------------------------
    def setCurrentLocation(self, latitude, longitude, utcOffset):

        self.latLong = (latitude, longitude)
        self.utcOffset = utcOffset

        self.setMarkerToCurrentLocation()


    # Open the location search dialog.
    # -----------------------------------------------------------------------------
    def openSearchDialog(self, *args):

        # Show the dialog.
        self.searchDialog.show()

        # Update the map marker when the dialog is closed.
        self.setMarkerToCurrentLocation()

    # Apply the current location to the sun/sky system.
    # -----------------------------------------------------------------------------
    def applyLocation(self, *args):

        command = ("AERPRSetSkyLocation(" +
                   str(self.latLong[0]) + ", " +
                   str(self.latLong[1]) + ", " +
                   str(self.utcOffset) + ");")

        mel.eval(command)

        self.close()


    # Close the window.
    # -----------------------------------------------------------------------------
    def close(self, *args):

        cmds.deleteUI("RPRLocationSelectorWindow")


# A dialog for searching for a location by name. When 3 or
# more letters are entered into the search field, the plug-in
# location command is called with the search string and a list
# of location strings is returned. Selecting a location updates
# the position of the map marker. Cancelling the search returns
# the map marker to its original location. Clicking OK updates
# the current location with the selected location.
# -----------------------------------------------------------------------------
class RPRLocationSearchDialog:

    # Initialize.
    # -----------------------------------------------------------------------------
    def __init__(self, selector) :
        self.selector = selector


    # Show the location search dialog.
    # -----------------------------------------------------------------------------
    def show(self):

        # Create the form that will contain the UI elements.
        self.searchDialog = cmds.layoutDialog(title="Location Search",
                                              parent=self.selector.window,
                                              uiScript=self.createSearchLayout)


    # Create the dialog UI layout.
    # -----------------------------------------------------------------------------
    def createSearchLayout(self):

        # Set the dialog form as the current parent.
        form = cmds.setParent(query=True)

        # Create the search field and result list.
        self.searchField = cmds.textField(placeholderText="Search...", height=26, width=280,
                                          textChangedCommand=self.searchLocations)

        self.searchResultList = cmds.textScrollList(width=280,
                                                    selectCommand=self.selectLocation,
                                                    doubleClickCommand=self.setLocation)

        # Create the OK and cancel buttons.
        self.okButtonSearch = cmds.button(label="OK", command=self.setLocation, enable=False, width=100)
        cancelButton = cmds.button(label="Cancel", command=self.cancel, width=100)

        # Create the component layout.
        cmds.formLayout(form, edit=True, width=300, height=300,
                        attachForm=[(self.searchField, 'top', 10), (self.searchField, 'left', 10),
                                    (self.searchField, 'right', 10), (self.searchResultList, 'left', 10),
                                    (self.searchResultList, 'right', 10), (self.okButtonSearch, 'bottom', 10),
                                    (cancelButton, 'right', 10), (cancelButton, 'bottom', 10)],
                        attachControl=[(self.searchResultList, 'top', 8, self.searchField),
                                       (self.searchResultList, 'bottom', 8, cancelButton),
                                       (self.okButtonSearch, 'right', 10, cancelButton)])


    # Cancel the location search.
    # -----------------------------------------------------------------------------
    def cancel(self, *args):

        # Close the search dialog.
        cmds.layoutDialog(dismiss=True)


    # Set the selected location as current.
    # -----------------------------------------------------------------------------
    def setLocation(self, *args):

        # Get data for the selected location.
        data = self.getSelectedLocationData()

        # Update current location data.
        self.selector.setCurrentLocation(data[0], data[1], data[2])

        # Close the search dialog.
        cmds.layoutDialog(dismiss=True)

    # Search for locations.
    # -----------------------------------------------------------------------------
    def searchLocations(self, *args):

        # Convert the search string to lower
        # case so the search is not case sensitive.
        searchString = cmds.textField(self.searchField, query=True, text=True).lower()

        # Check that the string is long enough
        # to search and not whitespace.
        if (len(searchString) < 3 or searchString.isspace()) :
            return

        # Call the location command with the search string.
        results = cmds.fireRenderLocation(search=searchString)

        # Clear any existing search results.
        cmds.textScrollList(self.searchResultList, edit=True, removeAll=True)

        # Add new results.
        for result in results :
            cmds.textScrollList(self.searchResultList, edit=True, append=result)

        # Disable the OK button.
        cmds.button(self.okButtonSearch, edit=True, enable=False)

    # Select a location from search results.
    # -----------------------------------------------------------------------------
    def selectLocation(self, *args):

        # Get data for the selected location.
        data = self.getSelectedLocationData()

        # Convert latitude / longitude to map coordinates.
        p = geographicToCartesian(data[0], data[1])

        # Update the map marker position.
        self.selector.setMarkerPosition(p[0], p[1])

        # Enable the OK button.
        cmds.button(self.okButtonSearch, edit=True, enable=True)


    # Get data for the selected location.
    # -----------------------------------------------------------------------------
    def getSelectedLocationData(self):

        # Get the selected index from the result list.
        index = cmds.textScrollList(self.searchResultList,
                                    query=True, selectIndexedItem=True)[0]

        # Perform the location command with the
        # index parameter to get location data.
        # Subtract 1 as list indices are 1 based.
        return cmds.fireRenderLocation(index=index - 1)


# Convert from geographic to Cartesian coordinates.
# -----------------------------------------------------------------------------
def geographicToCartesian(latitude, longitude):

    x = ((longitude + 180.0) / 360.0) * mapWidth
    y = ((90.0 - latitude) / 180.0) * mapHeight

    return (x, y)


# Convert from Cartesian to geographic coordinates.
# -----------------------------------------------------------------------------
def cartesianToGeographic(x, y):

    latitude = 90.0 - (y * 180.0 / mapHeight)
    longitude = (x * 360.0 / mapWidth) - 180.0

    return (latitude, longitude)
