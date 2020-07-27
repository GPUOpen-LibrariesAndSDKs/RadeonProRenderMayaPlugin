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
import os.path
import json
import math
from functools import partial
from sys import platform


# Show the material browser window.
# -----------------------------------------------------------------------------
def show() :

    print("ML Log: try show RPRMaterialBrowser")
    RPRMaterialBrowser().show()

# Get Library Path.
# -----------------------------------------------------------------------------
def getLibPath() :

	return RPRMaterialBrowser().libraryPath

# A material browser window for Radeon Pro Render materials.
# -----------------------------------------------------------------------------
class RPRMaterialBrowser(object) :

    # Constructor.
    # -----------------------------------------------------------------------------
    def __init__(self, *args) :

        # Character lengths used to calculate label widths.
        self.charLengths = [3, 3, 4, 7, 6, 9, 9, 3, 3, 3, 5, 8, 3, 4, 3, 4, 6, 6, 6, 6, 6, 6, 6,
                            6, 6, 6, 3, 3, 8, 8, 8, 5, 11, 7, 7, 7, 8, 6, 6, 8, 8, 3, 4, 6, 5, 10,
                            8, 9, 6, 9, 7, 6, 5, 8, 7, 11, 6, 5, 6, 3, 4, 3, 8, 5, 3, 6, 7, 5, 7,
                            6, 4, 7, 7, 3, 3, 6, 3, 9, 7, 7, 7, 7, 4, 5, 4, 7, 5, 9, 5, 5, 5, 3,
                            3, 3, 8, 6, 6, 0, 3, 6, 4, 9, 4, 4, 4, 13, 6, 3, 10, 0, 6, 0, 0, 3, 3,
                            4, 4, 4, 6, 11, 4, 9, 5, 3, 10, 0, 5, 5, 3, 3, 6, 6, 7, 6, 3, 5, 5,
                            10, 4, 6, 8, 0, 10, 5, 4, 8, 4, 4, 3, 7, 5, 3, 2, 4, 5, 6, 10, 10, 10,
                            5, 7, 7, 7, 7, 7, 7, 9, 7, 6, 6, 6, 6, 3, 3, 3, 3, 8, 8, 9, 9, 9, 9,
                            9, 8, 9, 8, 8, 8, 8, 5, 6, 6, 6, 6, 6, 6, 6, 6, 9, 5, 6, 6, 6, 6, 3,
                            3, 3, 3, 7, 7, 7, 7, 7, 7, 7, 8, 7, 7, 7, 7, 7, 5, 7, 5]

        # Panel background color.
        self.backgroundColor = [0.16862745098039217, 0.16862745098039217, 0.16862745098039217]

        # Set the default material icon size.
        self.setMaterialIconSize(self.getDefaultMaterialIconSize())

        # Get the material library path and
        # ensure that it's formatted correctly.5
        libraryPath = self.getLibraryPath()
        print("ML Log: library path = " + libraryPath)		
		
        self.libraryPath = libraryPath.replace("\\", "/").rstrip("/")


    # Get the library path from the registry or an environment variable.
    # -----------------------------------------------------------------------------
    def getLibraryPath(self) :

        # Read the path from the registry if running in Windows.
        if platform == "win32":

            import _winreg

            # Open the key.
            try:
                key = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, "SOFTWARE\\AMD\\RadeonProRender\\MaterialLibrary\\Maya")

                # Read the value.
                result = _winreg.QueryValueEx(key, "MaterialLibraryPath")

                # Close the key.
                _winreg.CloseKey(key)

                # Return value from the resulting tuple.
                return result[0]

            except Exception:
                try:
                    key = _winreg.OpenKey(_winreg.HKEY_CURRENT_USER, "SOFTWARE\\AMD\\RadeonProRender\\Maya")

                    # Read the value.
                    result = _winreg.QueryValueEx(key, "MaterialLibraryPath")
                    # Close the key.
                    _winreg.CloseKey(key)
                    # Return value from the resulting tuple.		
                    return result[0]

                except Exception:
                    pass

        # Otherwise, use an environment variable.
        return mel.eval("getenv(\"RPR_MATERIAL_LIBRARY_PATH\")")


    # Show the material browser.
    # -----------------------------------------------------------------------------
    def show(self) :

        # Check that the library can be found.
        if not self.checkLibrary():
            print("ML Log: ERROR: library path was not found")	
            return

        # Load material manifest and create the browser layout.
        print("ML Log: Begin loading library")
        self.loadManifest()
		
        print("ML Log: Begin creating layout")
        self.createLayout()


    # Check that the library has been installed and can be found.
    # -----------------------------------------------------------------------------
    def checkLibrary(self):

        # Check that the material library is installed.
        if len(self.libraryPath) <= 0:
            cmds.confirmDialog(title="Material Library Not Installed",
                               message="Please install the Radeon ProRender Material Library to use this feature.",
                               button ="OK")
            return False

        # Check that the material library can be found.
        if not os.path.exists(self.libraryPath):
            cmds.confirmDialog(title="Material Library Not Found",
                               message="Radeon ProRender Material Library not found at:\n\"" + self.libraryPath + "\"",
                               button ="OK")
            return False

        # Success.
        return True


    # Load the material manifest from a Json file.
    # -----------------------------------------------------------------------------
    def loadManifest(self) :

        # Read the manifest.
        manifestFile = self.libraryPath + "/manifest.json"

        with open(manifestFile) as dataFile :
            self.manifest = json.load(dataFile)

        # Map categories to materials.
        self.categoryMap = {}

        for category in self.manifest["categories"] :
            for material in category["materials"] :
                material["category"] = category


    # Create the browser layout.
    # -----------------------------------------------------------------------------
    def createLayout(self) :

        print("ML Log: createLayout")
        # Delete any existing window.
        if (cmds.window("RPRMaterialBrowserWindow", exists = True)) :
            cmds.deleteUI("RPRMaterialBrowserWindow")

        # Create a new window.
        self.window = cmds.window("RPRMaterialBrowserWindow",
                                  widthHeight=(1200, 700),
                                  title="Radeon ProRender Material Browser")

        # Place UI sections in a horizontal 3 pane layout.
        paneLayout = cmds.paneLayout(configuration='vertical3', staticWidthPane=3,
                                     separatorMovedCommand=self.updateMaterialsLayout)

        cmds.paneLayout(paneLayout, edit=True, paneSize=(1, 20, 100))
        cmds.paneLayout(paneLayout, edit=True, paneSize=(2, 52, 100))
        cmds.paneLayout(paneLayout, edit=True, paneSize=(3, 28, 100))

        # Create UI sections.
        self.createCategoriesLayout()
        self.createMaterialsLayout()
        self.createSelectedLayout()

        cmds.setParent('..')

        # Select the first material of the first category.
        self.selectMaterial(self.manifest["categories"][0]["materials"][0])
        self.selectCategory(0)

        # Show the material browser window.
        cmds.showWindow(self.window)

        # Initialize the layout.
        self.initializeLayout();


    # Create the material categories layout.
    # -----------------------------------------------------------------------------
    def createCategoriesLayout(self) :

        print("ML Log: createCategoriesLayout")
        # Create tab, form and scroll layouts.
        tabLayout = cmds.tabLayout(innerMarginWidth=5, innerMarginHeight=15, borderStyle="full")
        formLayout = cmds.formLayout(numberOfDivisions=100)
        scrollLayout = cmds.scrollLayout(backgroundColor=self.backgroundColor,
                                         horizontalScrollBarThickness=0,
                                         verticalScrollBarThickness=16,
                                         childResizable=True)

        # Lay out categories vertically.
        catergories = cmds.columnLayout()

        # Add layouts with index based names for easy lookup.
        index = 0

        for category in self.manifest["categories"] :
            cmds.iconTextButton("RPRCategory" + str(index), style='iconAndTextHorizontal',
                                image='material_browser/folder_closed.png',
                                label=category["name"], height=20,
                                command=partial(self.selectCategory, index))
            index += 1

        # Assign the form to the tab.
        cmds.tabLayout(tabLayout, edit=True, tabLabel=((formLayout, 'Categories')))

        # Lay out components within the form.
        cmds.formLayout(formLayout, edit=True,
                        attachForm=[(scrollLayout, 'top', 5), (scrollLayout, 'left', 5),
                                    (scrollLayout, 'bottom', 5), (scrollLayout, 'right', 5)])

        cmds.setParent('..')
        cmds.setParent('..')
        cmds.setParent('..')
        cmds.setParent('..')


    # Create the materials layout.
    # -----------------------------------------------------------------------------
    def createMaterialsLayout(self) :
	
        print("ML Log: createMaterialsLayout")
        # Create the tab and form layouts.
        self.materialsTab = cmds.tabLayout(borderStyle="full")
        self.materialsForm = cmds.formLayout(numberOfDivisions=100)

        # Add the icon size slider.
        iconSizeRow = cmds.rowLayout("RPRIconSize", numberOfColumns=2, columnWidth=(1, 22))
        cmds.image(image='material_browser/thumbnails.png')
        self.iconSizeSlider = cmds.intSlider(width=120, step=1, minValue=1, maxValue=4,
                                             value=self.getDefaultMaterialIconSize(),
                                             dragCommand=self.updateMaterialIconSize)
        cmds.setParent('..')

        # Add the search field.
        searchRow = cmds.rowLayout(numberOfColumns=2)
        cmds.image(image='material_browser/search.png')
        self.searchField = cmds.textField(placeholderText="Search...", width=250, height=22,
                                          textChangedCommand=self.searchMaterials)
        cmds.setParent('..')

        # Add help text.
        helpText = cmds.text(label="To import a material, double click it, or select it and click the Import button.")

        # Add the background canvas.
        bg = cmds.canvas(rgbValue=self.backgroundColor)

        # Add the scroll layout that will contain the material icons.
        self.materialsContainer = cmds.scrollLayout(backgroundColor=self.backgroundColor, childResizable=True,
                                                    resizeCommand=self.updateMaterialsLayout)
        cmds.setParent('..')

        # Assign the form to the tab.
        cmds.tabLayout(self.materialsTab, edit=True, tabLabel=((self.materialsForm, 'Materials')))

        # Lay out components within the form.
        cmds.formLayout(self.materialsForm, edit=True,
                        attachForm=[(searchRow, 'top', 5), (searchRow, 'right', 5),
                                    (iconSizeRow, 'top', 10), (iconSizeRow, 'left', 6),
                                    (self.materialsContainer, 'left', 10), (self.materialsContainer, 'bottom', 10),
                                    (self.materialsContainer, 'right', 5),
                                    (bg, 'left', 5), (bg, 'bottom', 5), (bg, 'right', 5),
                                    (helpText, 'left', 10), (helpText, 'bottom', 8)],
                        attachNone=[(searchRow, 'bottom'), (searchRow, 'left'),
                                    (iconSizeRow, 'bottom'), (iconSizeRow, 'right'),
                                    (helpText, 'right'), (helpText, 'top')],
                        attachControl=[(self.materialsContainer, 'top', 10, searchRow),
                                       (self.materialsContainer, 'bottom', 10, helpText),
                                       (bg, 'top', 5, searchRow),
                                       (bg, 'bottom', 5, helpText)])

        cmds.setParent('..')
        cmds.setParent('..')

        # Initialize the currently selected category index.
        self.selectedCategoryIndex = 0


    # Create the selected material layout.
    # -----------------------------------------------------------------------------
    def createSelectedLayout(self) :

        print("ML Log: createSelectedLayout")
        # Create a pane layout to contain material info and preview.
        paneLayout = cmds.paneLayout("RPRSelectedPane", configuration='horizontal2', staticHeightPane=2,
                                     separatorMovedCommand=self.updatePreviewLayout)

        self.createInfoLayout()
        self.createPreviewLayout()


    # Perform initial layout tasks.
    # -----------------------------------------------------------------------------
    def initializeLayout(self) :

        # Configure the selected pane.
        cmds.paneLayout("RPRSelectedPane", e=True, paneSize=(1, 100, 50))
        cmds.paneLayout("RPRSelectedPane", e=True, paneSize=(2, 100, 50))

        # Perform an initial preview layout update.
        self.updatePreviewLayout()


    # Create the info layout.
    # -----------------------------------------------------------------------------
    def createInfoLayout(self) :

        print("ML Log: createInfoLayout")
        # Create tab and form layouts.
        tabLayout = cmds.tabLayout(innerMarginWidth=8, innerMarginHeight=8, borderStyle="full")
        formLayout = cmds.formLayout(numberOfDivisions=100)
        importButton = cmds.button(label="Import", command=self.importSelectedMaterial)
        importImagesCheck = cmds.checkBox("RPRImportImagesCheck",
                                          label="Import images to project",
                                          value=self.importImagesEnabled(),
                                          changeCommand=self.importImagesChanged);

        # Add the RPR logo.
        logoBackground = cmds.canvas("RPRLogoBackground", rgbValue=[0, 0, 0])
        logo = cmds.iconTextStaticLabel("RPRLogo", style='iconOnly',
                                        image="RadeonProRenderLogo.png", width=1, height=1)

        # Add material info text.
        columnLayout = cmds.columnLayout(rowSpacing=5)

        cmds.text(label="Category:", font="boldLabelFont")
        cmds.text("RPRCategoryText", recomputeSize=False)
        cmds.canvas(height=10)
        cmds.text(label="Name:", font="boldLabelFont")
        cmds.text("RPRNameText", recomputeSize=False)
        cmds.canvas(height=10)
        cmds.text(label="File name:", font="boldLabelFont")
        cmds.text("RPRFileNameText", recomputeSize=False)
        cmds.canvas(height=10)

        cmds.setParent('..')
        cmds.setParent('..')
        cmds.setParent('..')

        # Assign the form to the tab.
        cmds.tabLayout(tabLayout, edit=True, tabLabel=((formLayout, 'Info')))

        # Lay out components within the form.
        cmds.formLayout(formLayout, edit=True,
                        attachControl=[(columnLayout, 'top', 10, logo),
                                       (importImagesCheck, 'bottom', 10, importButton)],
                        attachForm=[(logo, 'left', 8), (logo, 'right', 8), (logo, 'top', 8),
                                    (logoBackground, 'left', 8), (logoBackground, 'right', 8),
                                    (logoBackground, 'top', 8), (importButton, 'left', 10),
                                    (importButton, 'bottom', 10), (importButton, 'right', 10),
                                    (columnLayout, 'left', 10), (columnLayout, 'right', 10),
                                    (importImagesCheck, 'left', 10)])


    # Create the preview layout.
    # -----------------------------------------------------------------------------
    def createPreviewLayout(self) :

        print("ML Log: createPreviewLayout")
        # Create tab layout.
        tabLayout = cmds.tabLayout(borderStyle="full")

        # Add horizontal and vertical flow layouts and
        # spacers to center the image in the preview area.
        flowLayout = cmds.flowLayout("RPRPreviewArea")
        cmds.canvas("RPRHSpacer", width=1, height=1)

        cmds.flowLayout("RPRPreviewFlow", vertical=True)
        cmds.canvas("RPRVSpacer", width=1, height=1)

        cmds.iconTextStaticLabel("RPRPreviewImage", style='iconOnly', width=1, height=1)
        cmds.setParent('..')

        cmds.setParent('..')
        cmds.setParent('..')
        cmds.setParent('..')

        # Assign the top flow layout to the tab.
        cmds.tabLayout(tabLayout, edit=True, tabLabel=((flowLayout, 'Preview')))


    # Called when the import images check box changes.
    # -----------------------------------------------------------------------------
    def importImagesChanged(self, *args) :

        enabled = cmds.checkBox("RPRImportImagesCheck", query=True, value=True);
        cmds.optionVar(intValue=("RPR_ImportMaterialImages", int(enabled)));
        print("Import changed - " + str(enabled));


    # Return true if image importing is enabled.
    # -----------------------------------------------------------------------------
    def importImagesEnabled(self) :

        # Read the stored setting if it exists.
        if cmds.optionVar(exists="RPR_ImportMaterialImages"):
            return cmds.optionVar(query="RPR_ImportMaterialImages") != 0;

        # Default to true.
        return True;


    # Select a material category by index.
    # -----------------------------------------------------------------------------
    def selectCategory(self, index) :
	
        print("ML Log: selectCategory")
        # Populate the materials view from the selected category.
        self.materials = self.manifest["categories"][index]["materials"]
        self.populateMaterials()

        # Update the folder open / closed state on the category list.
        cmds.iconTextButton("RPRCategory" + str(self.selectedCategoryIndex),
                            edit=True, image='material_browser/folder_closed.png')

        cmds.iconTextButton("RPRCategory" + str(index),
                            edit=True, image='material_browser/folder_open.png')

        self.selectedCategoryIndex = index

        # Clear the search field.
        cmds.textField(self.searchField, edit=True, text="")


    # Select a material and update the info an preview panels.
    # -----------------------------------------------------------------------------
    def selectMaterial(self, material) :

        print("ML Log: selectMaterial")
	
        fileName = material["fileName"]
        imageFileName = self.libraryPath + "/" + fileName + "/" + fileName + ".jpg"
		
        print("ML Log: fileName = " + fileName)
        print("ML Log: imageFileName = " + imageFileName)		

        cmds.iconTextStaticLabel("RPRPreviewImage", edit=True, image=imageFileName)
        cmds.text("RPRCategoryText", edit=True, label=material["category"]["name"])
        cmds.text("RPRNameText", edit=True, label=material["name"])
        cmds.text("RPRFileNameText", edit=True, label=material["fileName"] + ".xml")

        self.selectedMaterial = material
        self.updatePreviewLayout()


    # Update the height of the materials flow layout
    # based on the width of its container and the
    # number of children. This is required so
    # a scrollable flow layout works properly.
    # -----------------------------------------------------------------------------
    def updateMaterialsLayout(self) :

        # Determine the width of the material view
        # and the total number of materials to display.
        width = cmds.scrollLayout(self.materialsContainer, query=True, width=True)
        count = cmds.flowLayout("RPRMaterialsFlow", query=True, numberOfChildren=True)

        if (count <= 0) :
            return

        # Calculate the number of materials that can fit on
        # a row and the total required height of the container.
        perRow = max(1, math.floor((width - 18) / self.cellWidth))
        height = math.ceil(count / perRow) * self.cellHeight

        cmds.flowLayout("RPRMaterialsFlow", edit=True, height=height)

        # Adjust the form to be narrower than the tab that
        # contains it. This is required so the form doesn't
        # prevent the tab layout shrinking.
        materialsWidth = cmds.tabLayout(self.materialsTab, query=True, width=True)
        newMaterialsWidth = max(1, materialsWidth - 10)
        cmds.formLayout(self.materialsForm, edit=True, width=newMaterialsWidth)

        # Hide the icon size slider if there isn't enough room for it.
        cmds.rowLayout("RPRIconSize", edit=True, visible=(materialsWidth > 440))

        # Update the preview layout.
        self.updatePreviewLayout()


    # Update the size and position of the preview image.
    # This is required because the script-able Maya UI
    # doesn't provide enough control over layout.
    # -----------------------------------------------------------------------------
    def updatePreviewLayout(self) :

        print("ML Log: updatePreviewLayout")
        # Determine the size of the preview area.
        width = cmds.flowLayout("RPRPreviewArea", query=True, width=True)
        height = cmds.flowLayout("RPRPreviewArea", query=True, height=True)

        # Choose the smallest dimension and shrink
        # slightly so the enclosing layouts can shrink.
        size = min(width, height) - 10

        # Calculate the horizontal and vertical
        # offsets required to center the preview image.
        hOffset = max(0, (width - size) / 2.0)
        vOffset = max(0, (height - size) / 2.0)

        # Clamp the size to a minimum of 64.
        newWidth = max(64, width - 10)
        newHeight = max(64, height - 10)
        size = max(64, size)

        # Update the layout and image sizes.
        cmds.flowLayout("RPRPreviewFlow", edit=True, width=newWidth, height=newHeight)
        cmds.iconTextStaticLabel("RPRPreviewImage", edit=True, width=size, height=size)
        cmds.canvas("RPRHSpacer", edit=True, width=hOffset, height=1)
        cmds.canvas("RPRVSpacer", edit=True, width=1, height=vOffset)

        # Update the RPR logo size.
        logoWidth = cmds.iconTextStaticLabel("RPRLogo", query=True, width=True)
        logoHeight = max(min(logoWidth * 0.16, 60), 1)
        cmds.iconTextStaticLabel("RPRLogo", edit=True, height=logoHeight)
        cmds.canvas("RPRLogoBackground", edit=True, height=logoHeight)


    # Update the material icon size from the slider.
    # -----------------------------------------------------------------------------
    def updateMaterialIconSize(self, *args) :

        # Calculate the new icon size and repopulate the view.
        value = cmds.intSlider(self.iconSizeSlider, query=True, value=True)
        self.setMaterialIconSize(value)
        self.populateMaterials()

        # Save the size setting to user preferences.
        cmds.optionVar(intValue=['RPRIconSize', value])


    # Set the size of the material icons.
    # -----------------------------------------------------------------------------
    def setMaterialIconSize(self, value) :

        # Set the size to a power of two.
        size = pow(2, value + 4)
        self.iconSize = size

        # Use horizontal cells for small icons
        # and vertical cells for large icons.
        if (size < 64) :
            self.cellWidth = size + 200
            self.cellHeight = size + 10
        else :
            self.cellWidth = size + 10
            self.cellHeight = size + 30


    # Get the default icon size value.
    # -----------------------------------------------------------------------------
    def getDefaultMaterialIconSize(self) :

        # Query user preferences.
        if (cmds.optionVar(exists="RPRIconSize")) :
            return cmds.optionVar(query="RPRIconSize")

        # Use a default value if no preference found.
        return 3


    # Search materials for the specified string.
    # -----------------------------------------------------------------------------
    def searchMaterials(self, *args) :

        print("ML Log: searchMaterials")
        # Convert the search string to lower
        # case so the search is not case sensitive.
        searchString = cmds.textField(self.searchField, query=True, text=True).lower()
        print("ML Log: searchString = " + searchString)
		
        # Check that the string is long enough
        # to search and not whitespace.
        if (len(searchString) < 2 or searchString.isspace()) :
            return

        # Set current materials to the search result.
        self.materials = []

        for category in self.manifest["categories"] :
            for material in category["materials"] :
                if (searchString in material["name"].lower()) :
                    self.materials.append(material)

        # Repopulate the material view.
        self.populateMaterials()


    # Populate the materials view with a list of materials.
    # -----------------------------------------------------------------------------
    def populateMaterials(self) :

        print("ML Log: populateMaterials")	
        # Remove any existing materials.
        if (cmds.layout("RPRMaterialsFlow", exists=True)) :
            cmds.deleteUI("RPRMaterialsFlow", layout=True)

        # Ensure that the material container is the current parent.
        cmds.setParent(self.materialsContainer)

        # Create the new flow layout.
        cmds.flowLayout("RPRMaterialsFlow", columnSpacing=0, wrap=True)

        # Add materials for the selected category.
        for material in self.materials :
            fileName = material["fileName"]
            imageFileName = self.libraryPath + "/" + fileName + "/" + fileName + ".jpg"

            materialName = material["name"]

            # Horizontal layout for small icons.
            if (self.iconSize < 64) :
                iconWidth = self.iconSize + 5
                cmds.rowLayout(width=self.cellWidth, height=self.cellHeight, numberOfColumns=2,
                               columnWidth2=(self.iconSize, self.cellWidth - iconWidth - 5))

                cmds.iconTextButton(style='iconOnly', image=imageFileName, width=self.iconSize,
                                    height=self.iconSize, command=partial(self.selectMaterial, material),
                                    doubleClickCommand=partial(self.importMaterial, material))

                cmds.iconTextButton(style='textOnly', height=self.iconSize,
                                    label=self.getTruncatedText(materialName, self.cellWidth - iconWidth - 5),
                                    align="left", command=partial(self.selectMaterial, material),
                                    doubleClickCommand=partial(self.importMaterial, material))

            # Vertical layout for large icons.
            else :
                cmds.columnLayout(width=self.cellWidth, height=self.cellHeight)
                cmds.iconTextButton(style='iconOnly', image=imageFileName, width=self.iconSize,
                                    height=self.iconSize, command=partial(self.selectMaterial, material),
                                    doubleClickCommand=partial(self.importMaterial, material))

                cmds.text(label=self.getTruncatedText(materialName, self.iconSize),
                          align="center", width=self.iconSize)

            cmds.setParent('..')

        # Perform an initial layout update.
        self.updateMaterialsLayout()


    # Import the currently selected material into Maya.
    # -----------------------------------------------------------------------------
    def importSelectedMaterial(self, *args) :

        self.importMaterial(self.selectedMaterial)


    # Import a material into Maya.
    # -----------------------------------------------------------------------------
    def importMaterial(self, material) :
	
        print("ML Log: importMaterial")
        fileName = material["fileName"]
        filePath = self.libraryPath + "/" + fileName + "/" + fileName + ".xml"
        print("ML Log: fileName" + fileName)
        print("ML Log: filePath" + filePath)
        cmds.RPRXMLImport(file=filePath, importImages=self.importImagesEnabled())


    # Return the width of a text UI element given it's label string.
    # -----------------------------------------------------------------------------
    def getTextWidth(self, text) :

        # Iterate over the string characters adding character widths.
        width = 0
        charCount = len(self.charLengths)

        for c in text :
            i = ord(c) - 32

            if (i < 0 or i >= charCount) :
                continue

            width += self.charLengths[i]

        return width


    # Return a string truncated to fit in the required width if necessary.
    # -----------------------------------------------------------------------------
    def getTruncatedText(self, text, requiredWidth) :

        # Return the string immediately if it fits within the required width.
        if (self.getTextWidth(text) < requiredWidth) :
            return text

        # Reserve enough space for the ellipsis (...).
        requiredWidth -= (self.getTextWidth(".") * 3)

        # Iterate over the string characters.
        width = 0
        charCount = len(self.charLengths)
        truncated = ""

        for c in text :

            # Get the index of the character in the char lengths array.
            i = ord(c) - 32
            if (i < 0 or i >= charCount) :
                continue

            width += self.charLengths[i]

            # Add characters until the required width is reached.
            if (width < requiredWidth) :
                truncated += c
            else :
                break

        # Add the ellipsis.
        if (len(truncated) < len(text)) :
            truncated += "..."

        return truncated
