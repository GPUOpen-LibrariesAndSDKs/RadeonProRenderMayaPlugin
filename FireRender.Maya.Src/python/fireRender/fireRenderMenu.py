import maya.cmds
import maya.mel
import os
import common
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
    print "import material"
    basicFilter = "*.frMat;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0, fileMode=1)
    if filePath:
        shader = maya.cmds.fireRenderImport(material=os.path.basename(filePath[0]).split('.')[0], file=filePath[0])
        if shader:
            maya.cmds.select(shader, r=1)

def exportRPRSelectedMaterialsXml(data):
    basicFilter = "*.xml;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0)
    if filePath:
        maya.cmds.RPRXMLExport(s=1,file=filePath[0])

def exportRPRMaterialsXml(data):
    basicFilter = "*.xml;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0)
    if filePath:
        maya.cmds.RPRXMLExport(file=filePath[0])

def importRPRMaterialsXml(data):
    basicFilter = "*.xml;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0, fileMode=1)
    for fileN in filePath :
        maya.cmds.RPRXMLImport(file=fileN)

def showRPRMaterialLibrary(value):
    mel.eval('source shelfCommands.mel; openMaterialLibraryRPR();')

def enableDebugTracing(value):
    maya.cmds.fireRender(d=value)

def openTraceFolder(data):
    maya.cmds.fireRender(of="RPR_MAYA_TRACE_PATH")

def showRPRSettings(data):
    mel.eval('source shelfCommands.mel; openSettingsTabRPR();')

def startProductionRender(data):
    mel.eval('source shelfCommands.mel; startProductionRenderRPR();')

def convertVRayToRPR(data):
    mel.eval('source shelfCommands.mel; convertVRayObjects();')

def openAbout(data):
    mel.eval('source shelfCommands.mel; openPluginInfoRPR();')

def openDocumentation(data):
    mel.eval('launch -web \"https://radeon-prorender.github.io/maya/\";')

def setRenderProductionQualityHigh(data):
    mel.eval('source \"presets.mel\";\n\nSetProdQualityPreset(2);')

def setRenderProductionQualityMedium(data):
    mel.eval('source \"presets.mel\";\n\nSetProdQualityPreset(1);')

def setRenderProductionQualityLow(data):
    mel.eval('source \"presets.mel\";\n\nSetProdQualityPreset(0);')

def setRenderViewportQualityHigh(data):
    mel.eval('source \"presets.mel\";\n\nSetViewportQualityPreset(2);')

def setRenderViewportQualityMedium(data):
    mel.eval('source \"presets.mel\";\n\nSetViewportQualityPreset(1);')

def setRenderViewportQualityLow(data):
    mel.eval('source \"presets.mel\";\n\nSetViewportQualityPreset(0);')

def createFireRenderMenu():
    gMainWindow = "MayaWindow";
    if not maya.cmds.menu("showFireRenderMenuCtrl", exists=1):
        name = common.pluginName
        showFireRenderMenuCtrl = maya.cmds.menu("showFireRenderMenuCtrl", p=gMainWindow, to=True, l=name)

        frRenderMenu = maya.cmds.menuItem("FrRenderMenu", subMenu=True, label="Render", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrStartRendering", label="Start a Production Render", p=frRenderMenu, c=startProductionRender)

        maya.cmds.menuItem( divider=True, p=frRenderMenu )

        frRenderProductionMenu = maya.cmds.menuItem("frRenderProductionMenu", subMenu=True, label="Production Quality", p=frRenderMenu)
        maya.cmds.radioMenuItemCollection(p=frRenderProductionMenu)
        maya.cmds.menuItem("frRenderProductionMenuLow", label="Low", p=frRenderProductionMenu, rb=(maya.cmds.getAttr('RadeonProRenderGlobals.qualityPresets')==0), c=setRenderProductionQualityLow)
        maya.cmds.menuItem("frRenderProductionMenuMedium", label="Medium", p=frRenderProductionMenu, rb=(maya.cmds.getAttr('RadeonProRenderGlobals.qualityPresets')==1), c=setRenderProductionQualityMedium)
        maya.cmds.menuItem("frRenderProductionMenuHigh", label="High", p=frRenderProductionMenu, rb=(maya.cmds.getAttr('RadeonProRenderGlobals.qualityPresets')==2), c=setRenderProductionQualityHigh)

        frRenderViewportMenu = maya.cmds.menuItem("frRenderViewportMenu", subMenu=True, label="Viewport Quality", p=frRenderMenu)
        maya.cmds.radioMenuItemCollection(p=frRenderViewportMenu)
        maya.cmds.menuItem("frRenderViewportMenuLow", label="Low", p=frRenderViewportMenu, rb=(maya.cmds.getAttr('RadeonProRenderGlobals.qualityPresetsViewport')==0), c=setRenderViewportQualityLow)
        maya.cmds.menuItem("frRenderViewportMenuMedium", label="Medium", p=frRenderViewportMenu, rb=(maya.cmds.getAttr('RadeonProRenderGlobals.qualityPresetsViewport')==1), c=setRenderViewportQualityMedium)
        maya.cmds.menuItem("frRenderViewportMenuHigh", label="High", p=frRenderViewportMenu, rb=(maya.cmds.getAttr('RadeonProRenderGlobals.qualityPresetsViewport')==2), c=setRenderViewportQualityHigh)

        maya.cmds.menuItem( divider=True, p=showFireRenderMenuCtrl )

        frExportMenu = maya.cmds.menuItem("FrExportMenu",subMenu=True, label="Export", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrExportScene", label="Export Scene", p=frExportMenu, c=exportFireRenderScene)

        maya.cmds.menuItem("FrExportSelectedMaterialsXML", label="Export Selected Materials to XML", p=frExportMenu, c=exportRPRSelectedMaterialsXml)
        maya.cmds.menuItem("FrExportAllMaterialsXML", label="Export All Materials to XML", p=frExportMenu, c=exportRPRMaterialsXml)
        frImportMenu = maya.cmds.menuItem("FrImportMenu",subMenu=True, label="Import", p=showFireRenderMenuCtrl)

        maya.cmds.menuItem("FrImportMaterialXML", label="Import XML Material", p=frImportMenu, c=importRPRMaterialsXml)

        maya.cmds.menuItem("FrRPRMaterialLibrary", label="Radeon ProRender Material Library", p=showFireRenderMenuCtrl, c=showRPRMaterialLibrary)

        maya.cmds.menuItem( divider=True, p=showFireRenderMenuCtrl )

        frRPRLights = maya.cmds.menuItem("frRPRLights", subMenu=True, label="Lights", p=showFireRenderMenuCtrl)
        mel.eval('source shelfCommands.mel; createLightsSubmenuItems("frRPRLights");')

        frRPRConvert = maya.cmds.menuItem("frRPRConvert", subMenu=True, label="Convert", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrConvertVRay", label="Convert VRay Scene to RPR", p=frRPRConvert, c=convertVRayToRPR)

        frRPRSettings = maya.cmds.menuItem("frRPRSettings", label="Settings", p=showFireRenderMenuCtrl, c=showRPRSettings)

        maya.cmds.menuItem( divider=True, p=showFireRenderMenuCtrl )

        frDebugMenu = maya.cmds.menuItem("FrDebugMenu", subMenu=True, label="Debug", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrDebugTrace", label="Enable Tracing", p=frDebugMenu, cb=("RPR_MAYA_TRACE_PATH" in os.environ), c=enableDebugTracing)
        maya.cmds.menuItem("FrOpenTraceFolder", label="Open Trace Folder", p=frDebugMenu, c=openTraceFolder)

        frRPRHelp = maya.cmds.menuItem("frRPRHelp", subMenu=True, label="Help", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrHelpAbout", label="Radeon ProRender Information", p=frRPRHelp, c=openAbout)
        maya.cmds.menuItem("FrHelpDocumentation", label="On-line documentation", p=frRPRHelp, c=openDocumentation)

def removeFireRenderMenu():
    if maya.cmds.menu("showFireRenderMenuCtrl", exists=1):
        maya.cmds.deleteUI("showFireRenderMenuCtrl")
