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

import maya.cmds
import maya.mel
import os
from . import common
import maya.mel as mel

def exportFireRenderScene(data):
    mel.eval('source shelfCommands.mel; exportSceneRPR();')

def exportFireRenderSelection(data):
    basicFilter = "*.rpr;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0)
    if filePath:
        maya.cmds.fireRenderExport(selection=1,file=filePath[0])

def exportFireRenderMaterial(data):
    selection = maya.cmds.ls(sl=1) or []
    if len(selection) == 0:
        maya.cmds.confirmDialog( title='Error', message="Please select a shader", icon='critical' , button=["close"])
        return
    basicFilter = "*.frMat;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0)
    if filePath:
        maya.cmds.fireRenderExport(material=selection[0],file=filePath[0])

def importFireRenderMaterial(data):
    print ("import material")
    basicFilter = "*.frMat;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0, fileMode=1)
    if filePath:
        shader = maya.cmds.fireRenderImport(material=os.path.basename(filePath[0]).split('.')[0], file=filePath[0])
        if shader:
            maya.cmds.select(shader, r=1)

def exportRPRSelectedMaterialsXml(data):
    filePath = maya.cmds.fileDialog2(dialogStyle=0, fileMode=2)
    if filePath:
        maya.cmds.RPRXMLExport(s=1,file=filePath[0])

def exportRPRMaterialsXml(data):
    filePath = maya.cmds.fileDialog2(dialogStyle=0, fileMode=2)
    if filePath:
        maya.cmds.RPRXMLExport(file=filePath[0])

def importRPRMaterialsXml(data):
    basicFilter = "*.xml;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0, fileMode=1)
    if (filePath is not None) :
        for fileN in filePath :
            maya.cmds.RPRXMLImport(file=fileN)

def showRPRMaterialLibrary(value):
    mel.eval('source shelfCommands.mel; openMaterialLibraryRPR();')

def showRPRMaterialXLibrary(value):
    mel.eval('source shelfCommands.mel; openMaterialLibrary2RPR();')

def enableDebugTracing(value):
    maya.cmds.fireRender(d=value)

def enableAthena(value):
    maya.cmds.athenaEnable(ae=value)

def openTraceFolder(data):
    maya.cmds.fireRender(of="RPR_MAYA_TRACE_PATH")

def showRPRSettings(data):
    mel.eval('source shelfCommands.mel; openSettingsTabRPR();')

def startProductionRender(data):
    mel.eval('source shelfCommands.mel; startProductionRenderRPR();')

def convertVRayToRPR(data):
    from . import convertVR2RPR as vrayconvert
    vrayconvert.manual_launch()

def convertArnoldToRPR(data):
    from . import convertAI2RPR as aiconvert
    aiconvert.manual_launch()

def convertRedshiftToRPR(data):
    from . import convertRS2RPR as rsconvert
    rsconvert.manual_launch()

def openAbout(data):
    mel.eval('source shelfCommands.mel; openPluginInfoRPR();')

def openDocumentation(data):
    mel.eval('launch -web \"https://radeon-pro.github.io/RadeonProRenderDocs/en/index.html\";')

def createFireRenderMenu():
    gMainWindow = "MayaWindow";
    if not maya.cmds.menu("showFireRenderMenuCtrl", exists=1):
        name = common.pluginName
        showFireRenderMenuCtrl = maya.cmds.menu("showFireRenderMenuCtrl", p=gMainWindow, to=True, l=name)

        frRenderMenu = maya.cmds.menuItem("FrRenderMenu", subMenu=True, label="Render", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrStartRendering", label="Start a Production Render", p=frRenderMenu, c=startProductionRender)

        maya.cmds.menuItem( divider=True, p=frRenderMenu )

        maya.cmds.menuItem( divider=True, p=showFireRenderMenuCtrl )

        frExportMenu = maya.cmds.menuItem("FrExportMenu",subMenu=True, label="Export", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrExportScene", label="Export Scene", p=frExportMenu, c=exportFireRenderScene)

        maya.cmds.menuItem("FrExportSelectedMaterialsXML", label="Export Selected Materials to XML", p=frExportMenu, c=exportRPRSelectedMaterialsXml)
        maya.cmds.menuItem("FrExportAllMaterialsXML", label="Export All Materials to XML", p=frExportMenu, c=exportRPRMaterialsXml)
        frImportMenu = maya.cmds.menuItem("FrImportMenu",subMenu=True, label="Import", p=showFireRenderMenuCtrl)

        maya.cmds.menuItem("FrImportMaterialXML", label="Import XML Material", p=frImportMenu, c=importRPRMaterialsXml)

        maya.cmds.menuItem("FrRPRMaterialLibrary", label="Radeon ProRender Material Library", p=showFireRenderMenuCtrl, c=showRPRMaterialLibrary)
        maya.cmds.menuItem("FrMaterialXMaterialLibrary", label="AMD MaterialX Library", p=showFireRenderMenuCtrl, c=showRPRMaterialXLibrary)

        maya.cmds.menuItem( divider=True, p=showFireRenderMenuCtrl )

        frRPRLights = maya.cmds.menuItem("frRPRLights", subMenu=True, label="Lights", p=showFireRenderMenuCtrl)
        mel.eval('source shelfCommands.mel; createLightsSubmenuItems("frRPRLights");')

        frRPRVolumes = maya.cmds.menuItem("frRPRVolumes", subMenu=True, label="Volumes", p=showFireRenderMenuCtrl)
        mel.eval('source shelfCommands.mel; createVolumesSubmenuItems("frRPRVolumes");')

        frRPRConvert = maya.cmds.menuItem("frRPRConvert", subMenu=True, label="Convert", p=showFireRenderMenuCtrl)
        
        maya.cmds.menuItem("FrConvertArnold", label="Convert Arnold Scene to RPR", p=frRPRConvert, c=convertArnoldToRPR)
        maya.cmds.menuItem("FrConvertRedshift", label="Convert Redshift Scene to RPR", p=frRPRConvert, c=convertRedshiftToRPR)
        maya.cmds.menuItem("FrConvertVRay", label="Convert VRay Scene to RPR", p=frRPRConvert, c=convertVRayToRPR)

        frRPRSettings = maya.cmds.menuItem("frRPRSettings", label="Settings", p=showFireRenderMenuCtrl, c=showRPRSettings)

        maya.cmds.menuItem( divider=True, p=showFireRenderMenuCtrl )

        frAthenaMenu = maya.cmds.menuItem("FrAthenaMenu", subMenu=True, label="Athena", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrAthenaEnable", label="Enable Athena", p=frAthenaMenu, cb=(True), c=enableAthena)

        frDebugMenu = maya.cmds.menuItem("FrDebugMenu", subMenu=True, label="Debug", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrDebugTrace", label="Enable Tracing", p=frDebugMenu, cb=("RPR_MAYA_TRACE_PATH" in os.environ), c=enableDebugTracing)
        maya.cmds.menuItem("FrOpenTraceFolder", label="Open Trace Folder", p=frDebugMenu, c=openTraceFolder)

        frRPRHelp = maya.cmds.menuItem("frRPRHelp", subMenu=True, label="Help", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrHelpAbout", label="Radeon ProRender Information", p=frRPRHelp, c=openAbout)
        maya.cmds.menuItem("FrHelpDocumentation", label="On-line documentation", p=frRPRHelp, c=openDocumentation)

def removeFireRenderMenu():
    if maya.cmds.menu("showFireRenderMenuCtrl", exists=1):
        maya.cmds.deleteUI("showFireRenderMenuCtrl")
