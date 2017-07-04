import maya.cmds
import maya.mel
import os
import common

def exportFireRenderScene(data):
    basicFilter = "*.frScene;;All Files (*.*)"
    filePath = maya.cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=0)
    if filePath:
        maya.cmds.fireRenderExport(sc=1,file=filePath[0])

def exportFireRenderSelection(data):
    basicFilter = "*.frScene;;All Files (*.*)"
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

def enableDebugTracing(value):
    maya.cmds.fireRender(d=value)

def openTraceFolder(data):
    maya.cmds.fireRender(of="RPR_MAYA_TRACE_PATH")

def createFireRenderMenu():
    gMainWindow = "MayaWindow";
    if not maya.cmds.menu("showFireRenderMenuCtrl", exists=1):
        name = common.pluginName
        showFireRenderMenuCtrl = maya.cmds.menu("showFireRenderMenuCtrl", p=gMainWindow, to=True, l=name)
        frExportMenu = maya.cmds.menuItem("FrExportMenu",subMenu=True, label="Export", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrExportScene", label="Export Scene", p=frExportMenu, c=exportFireRenderScene)

        maya.cmds.menuItem("FrExportSelectedMaterialsXML", label="Export Selected Materials to XML", p=frExportMenu, c=exportRPRSelectedMaterialsXml)
        maya.cmds.menuItem("FrExportAllMaterialsXML", label="Export All Materials to XML", p=frExportMenu, c=exportRPRMaterialsXml)
        frImportMenu = maya.cmds.menuItem("FrImportMenu",subMenu=True, label="Import", p=showFireRenderMenuCtrl)

        maya.cmds.menuItem("FrImportMaterialXML", label="Import XML Material", p=frImportMenu, c=importRPRMaterialsXml)
        frDebugMenu = maya.cmds.menuItem("FrDebugMenu",subMenu=True, label="Debug", p=showFireRenderMenuCtrl)
        maya.cmds.menuItem("FrDebugTrace", label="Enable Tracing", p=frDebugMenu, cb=("RPR_MAYA_TRACE_PATH" in os.environ), c=enableDebugTracing)
        maya.cmds.menuItem("FrOpenTraceFolder", label="Open Trace Folder", p=frDebugMenu, c=openTraceFolder)

def removeFireRenderMenu():
    if maya.cmds.menu("showFireRenderMenuCtrl", exists=1):
        maya.cmds.deleteUI("showFireRenderMenuCtrl")
