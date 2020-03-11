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
import maya.OpenMaya as om
import common

converters = {}

def convertFileNode(node):
    return node,"outColor"

def convertAnimCurveTU(node):
    return node,"output"

def translatedFloatAttribute(vrayNode, vrayAttr, frNode, frAttribute, noValue=False):
    diffuseConnection = maya.cmds.listConnections("%s.%s" % (vrayNode, vrayAttr), p=1) or []
    if len(diffuseConnection) > 0:
        nodeName = diffuseConnection[0].split('.')[0]
        typeName = maya.cmds.objectType(nodeName)
        if typeName in converters:
            newNode, outAttribute = converters[typeName](nodeName)
            maya.cmds.connectAttr("%s.%s" % (newNode, outAttribute), "%s.%s" % (frNode, frAttribute))
    elif not noValue:
        value = maya.cmds.getAttr("%s.%s" % (vrayNode, vrayAttr))
        maya.cmds.setAttr("%s.%s" % (frNode,frAttribute), value)

def translatedColorAttribute(vrayNode, vrayAttr, frNode, frAttribute, noValue=False):
    diffuseConnection = maya.cmds.listConnections("%s.%s" % (vrayNode, vrayAttr), p=1) or []
    vrChildren = maya.cmds.attributeQuery(vrayAttr, listChildren=1,node=vrayNode)
    frChildren = maya.cmds.attributeQuery(frAttribute, listChildren=1,node=frNode)
    redConnection = maya.cmds.listConnections("%s.%s.%s" % (vrayNode, vrayAttr, vrChildren[0]), p=1) or []
    greenConnection = maya.cmds.listConnections("%s.%s.%s" % (vrayNode, vrayAttr, vrChildren[1]), p=1) or []
    blueConnection = maya.cmds.listConnections("%s.%s.%s" % (vrayNode, vrayAttr, vrChildren[2]), p=1) or []
    if len(diffuseConnection) > 0:
        nodeName = diffuseConnection[0].split('.')[0]
        typeName = maya.cmds.objectType(nodeName)
        if typeName in converters:
            newNode, outAttribute = converters[typeName](nodeName)
            maya.cmds.connectAttr("%s.%s" % (newNode, outAttribute), "%s.%s" % (frNode, frAttribute))

    if len(redConnection) > 0:
        nodeName = redConnection[0].split('.')[0]
        typeName = maya.cmds.objectType(nodeName)
        if typeName in converters:
            newNode, outAttribute = converters[typeName](nodeName)
            maya.cmds.connectAttr("%s.%s" % (newNode, outAttribute), "%s.%s.%s" % (frNode, frAttribute, frChildren[0]))

    if len(greenConnection) > 0:
        nodeName = greenConnection[0].split('.')[0]
        typeName = maya.cmds.objectType(nodeName)
        if typeName in converters:
            newNode, outAttribute = converters[typeName](nodeName)
            maya.cmds.connectAttr("%s.%s" % (newNode, outAttribute), "%s.%s.%s" % (frNode, frAttribute, frChildren[1]))

    if len(blueConnection) > 0:
        nodeName = blueConnection[0].split('.')[0]
        typeName = maya.cmds.objectType(nodeName)
        if typeName in converters:
            newNode, outAttribute = converters[typeName](nodeName)
            maya.cmds.connectAttr("%s.%s" % (newNode, outAttribute), "%s.%s.%s" % (frNode, frAttribute, frChildren[2])) 

    if (not noValue) and (len(diffuseConnection) == 0) and (len(redConnection) == 0) and (len(greenConnection) == 0) and (len(blueConnection) == 0):
        value = maya.cmds.getAttr("%s.%s" % (vrayNode, vrayAttr))[0]
        maya.cmds.setAttr("%s.%s" % (frNode,frAttribute), value[0], value[1], value[2], type="double3")

def convertVrayMtl(vrayNode):
    frStandard = maya.cmds.shadingNode(common.nodeName + "StandardMaterial", asShader=1)
    attributeMap = {}
    attributeMap["diffuseColor"] = "diffuseColor"
    attributeMap["illumColor"] = "emissiveColor"
    attributeMap["reflectionColor"] = "reflectColor"
    attributeMap["refractionColor"] = "refractColor"

    for key in attributeMap:
        translatedColorAttribute(vrayNode, key, frStandard, attributeMap[key])

    attributeMap = {}
    attributeMap["roughnessAmount"] = "diffuseRoughness"
    attributeMap["reflectionColorAmount"] = "reflectWeight"
    attributeMap["reflectionGlossiness"] = "reflectRoughness"
    attributeMap["refractionIOR"] = "refractIor"
    attributeMap["refractionColorAmount"] = "refractWeight"

    for key in attributeMap:
        value = maya.cmds.getAttr("%s.%s" % (vrayNode, key))
        diffuseConnection = maya.cmds.listConnections("%s.%s" % (vrayNode, key), p=1) or []
        if len(diffuseConnection) > 0:
            nodeName, outAttribute = diffuseConnection[0].split('.')[0]
            typeName = maya.cmds.objectType(nodeName)
            if typeName in converters:
                newNode = converters[typeName](nodeName)
                maya.cmds.connectAttr("%s.%s" % (newNode, outAttribute), "%s.%s" % (frStandard, attributeMap[key]))
        else:
            if key == "reflectionGlossiness":
                value = 1.0 - value
            if key == "reflectionColorAmount":
                value = value * 0.5
            maya.cmds.setAttr("%s.%s" % (frStandard, attributeMap[key]), value)


    emissiveColor = maya.cmds.getAttr("%s.emissiveColor" % frStandard)[0]
    emissiveWeight = 0.2126 * emissiveColor[0] + 0.7152 * emissiveColor[1] + 0.0722 * emissiveColor[2]
    maya.cmds.setAttr("%s.emissiveWeight" % frStandard, emissiveWeight)

    attributeMap = {}
    if not maya.cmds.getAttr("%s.lockFresnelIORToRefractionIOR" % vrayNode):
        attributeMap["fresnelIOR"] = "reflectIor"
    else:
        attributeMap["refractionIOR"] = "reflectIor"
    for key in attributeMap:
        value = maya.cmds.getAttr("%s.%s" % (vrayNode, key))
        diffuseConnection = maya.cmds.listConnections("%s.%s" % (vrayNode, key), p=1) or []
        if len(diffuseConnection) > 0:
            nodeName = diffuseConnection[0].split('.')[0]
            typeName = maya.cmds.objectType(nodeName)
            if typeName in converters:
                newNode, outAttribute = converters[typeName](nodeName)
                maya.cmds.connectAttr("%s.%s" % (newNode, outAttribute), "%s.%s" % (frStandard, attributeMap[key]))
        else:
            if key == "reflectionGlossiness":
                value = 1.0 - value
            maya.cmds.setAttr("%s.%s" % (frStandard, attributeMap[key]), value)
    return frStandard, "outColor"

def convertVrayBlendMtl(vrayNode):
    additive = maya.cmds.getAttr("%s.additive_mode" % vrayNode)
    frShaderName = common.nodeName +"BlendMaterial"
    if additive:
        frShaderName = common.nodeName + "AddMaterial"
    frBlend = maya.cmds.shadingNode(frShaderName, asShader=1)
    translatedColorAttribute(vrayNode, "base_material", frBlend, "color0", noValue=True)
    currentBlend = frBlend
    for i in range(9):
        coatShader = maya.cmds.listConnections("%s.%s_%d" % (vrayNode, "coat_material", i), p=1) or []
        if len(coatShader) == 0:
            continue
        if not additive:
            translatedFloatAttribute(vrayNode, "%s_%d.%s_%dR" % ("blend_amount", i, "blend_amount", i), currentBlend, "weight", noValue=False)
        newBlend = maya.cmds.shadingNode(frShaderName, asShader=1)
        nodeName = coatShader[0].split('.')[0]
        typeName = maya.cmds.objectType(nodeName)
        if typeName in converters:
            newNode, outAttribute = converters[typeName](nodeName)
            maya.cmds.connectAttr("%s.%s" % (newNode, outAttribute), "%s.%s" % (newBlend, "color0"))
            maya.cmds.connectAttr("%s.outColor" % newBlend, "%s.color1" % currentBlend)
            currentBlend = newBlend
    return frBlend


converters["VRayMtl"] = convertVrayMtl
converters["file"] = convertFileNode
converters["VRayBlendMtl"] = convertVrayBlendMtl
converters["animCurveTU"] = convertAnimCurveTU

def get_shading_groups(shape_node):
    sg_nodes = maya.cmds.listConnections(shape_node, type='shadingEngine') or []
    return sg_nodes

def isVisible(node):
    sList = om.MSelectionList()
    sList.add(node)
    nodePath = om.MDagPath()
    sList.getDagPath(0,nodePath)
    return nodePath.isVisible()

def convertVrayScene():
    if not maya.cmds.pluginInfo("vrayformaya",q=1,loaded=1):
        return
    convertShaders = False
    meshes = maya.cmds.ls(type="mesh")
    for mesh in meshes:
        if not isVisible(mesh):
            continue
        sgs = list(set(get_shading_groups(mesh)))
        for sg in sgs:
            surfaceShader = maya.cmds.listConnections("%s.surfaceShader" % sg, d=0, s=1) or []
            shaderType = maya.cmds.objectType(surfaceShader[0])
            if shaderType in converters:
                if not convertShaders:
                    result =  maya.cmds.confirmDialog( title='Vray shaders/light found', message='Do you want convert vray shaders?', button=['Yes','No'], defaultButton='Yes', cancelButton='No', dismissString='No' )
                    if  result == "No":
                        return
                    convertShaders = True
                newShader = converters[shaderType](surfaceShader[0])
                maya.cmds.connectAttr("%s.outColor" % newShader[0], "%s.surfaceShader" % sg, f=True)

    #search for dome lights
    domes = maya.cmds.ls(type="VRayLightDomeShape") or []
    for dome in domes:
        if maya.cmds.objExists("RPRIBLShape"):
            iblNode = "RPRIBLShape"
        else:
            iblNode = maya.cmds.createNode(common.nodeName + "IBL", n="RPRIBLShape")
            parentNone = maya.cmds.listRelatives(iblNode, p=True)
            maya.cmds.setAttr(parentNone[0] + ".scaleX", 1000)
            maya.cmds.setAttr(parentNone[0] + ".scaleY", 1000)
            maya.cmds.setAttr(parentNone[0] + ".scaleZ", 1000)
            maya.cmds.rename(parentNone[0], common.nodeName + "IBL")
            print parentNone
            if len(parentNone) > 0:
                oldNode = maya.cmds.listConnections("RadeonProRenderGlobals.imageBasedLighting") or []
                if len(oldNode) > 0:
                    maya.cmds.disconnectAttr(oldNode[0] + ".message", "RadeonProRenderGlobals.imageBasedLighting")
                    maya.cmds.delete(oldNode[0])
                maya.cmds.connectAttr(parentNone[0] + ".message", "RadeonProRenderGlobals.imageBasedLighting")

        if maya.cmds.objExists(iblNode):
            enabled = maya.cmds.getAttr("%s.enabled" % dome)
            if enabled and isVisible(dome):
                if not convertShaders:
                    result =  maya.cmds.confirmDialog( title='Vray shaders/light found', message='Do you want convert vray shaders?', button=['Yes','No'], defaultButton='Yes', cancelButton='No', dismissString='No' )
                    if  result == "No":
                        return
                    convertShaders = True
                intensity = maya.cmds.getAttr("%s.intensityMult" % dome)
                maya.cmds.setAttr("%s.intensity" % iblNode, intensity)
                useDomeTex = maya.cmds.getAttr("%s.useDomeTex" % dome)
                if useDomeTex:
                    domeTexConnections = maya.cmds.listConnections("%s.domeTex" % dome) or []
                    for connection in domeTexConnections:
                        if maya.cmds.objectType(connection) == "file":
                            filePath = maya.cmds.getAttr("%s.computedFileTextureNamePattern" % connection)
                            maya.cmds.setAttr("%s.filePath" % iblNode, filePath, type="string")
                        if maya.cmds.objectType(connection) == "file":
                            filePath = maya.cmds.getAttr("%s.computedFileTextureNamePattern" % connection)
                            maya.cmds.setAttr("%s.filePath" % iblNode, filePath, type="string")
                    # disable dome
                    maya.cmds.setAttr("%s.enabled" % dome, 0)
        break
