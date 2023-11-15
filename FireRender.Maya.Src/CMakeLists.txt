
################################################################################
# Source groups
################################################################################
#set(no_group_source_files
#    "frMaya.module"
#    "renderDesc/FireRenderRenderer.xml"
#)
#source_group("" FILES ${no_group_source_files})

set(AOVs
    "FireRenderAOV.cpp"
    "FireRenderAOV.h"
    "FireRenderAOVs.cpp"
    "FireRenderAOVs.h"
)
source_group("AOVs" FILES ${AOVs})

set(Commands
    "EnableSaveIntermediateCmd.cpp"
    "EnableSaveIntermediateCmd.h"
    "FireRenderCmd.cpp"
    "FireRenderCmd.h"
    "FireRenderConvertVRayCmd.cpp"
    "FireRenderConvertVRayCmd.h"
    "FireRenderExportCmd.cpp"
    "FireRenderExportCmd.h"
    "FireRenderImageComparing.cpp"
    "FireRenderImageComparing.h"
    "FireRenderImportCmd.cpp"
    "FireRenderImportCmd.h"
    "FireRenderImportExportXML.cpp"
    "FireRenderImportExportXML.h"
    "FireRenderImportXML.cpp"
    "FireRenderViewportCmd.cpp"
    "FireRenderViewportCmd.h"
    "RenderCacheWarningDialog.cpp"
    "RenderCacheWarningDialog.h"
    "RenderProgressBars.cpp"
    "RenderProgressBars.h"
    "FireRenderImportMaterialX.h"
    "FireRenderImportMaterialX.cpp"
)
source_group("Commands" FILES ${Commands})

set(Context
    "Context/ContextCreator.cpp"
    "Context/ContextCreator.h"
    "Context/FireRenderContext.cpp"
    "Context/FireRenderContext.h"
    "Context/HybridContext.cpp"
    "Context/HybridContext.h"
    "Context/HybridProContext.cpp"
    "Context/HybridProContext.h"
    "Context/TahoeContext.cpp"
    "Context/TahoeContext.h"
)
source_group("Context" FILES ${Context})

set(Environment
    "FireRenderEnvironmentLight.cpp"
    "FireRenderEnvironmentLight.h"
    "FireRenderIBL.cpp"
    "FireRenderIBL.h"
    "FireRenderLocationCmd.cpp"
    "FireRenderLocationCmd.h"
    "FireRenderSkyLocator.cpp"
    "FireRenderSkyLocator.h"
    "SkyAttributes.cpp"
    "SkyAttributes.h"
    "SkyBuilder.cpp"
    "SkyBuilder.h"
    "SkyGen.cpp"
    "SkyGen.h"
    "SkyLocatorMesh.cpp"
    "SkyLocatorMesh.h"
)
source_group("Environment" FILES ${Environment})

set(Header_Files
    "../RadeonProRenderSharedComponents/src/Alembic/AlembicWrapper.hpp"
    "../RadeonProRenderSharedComponents/src/Athena/athenaWrap.h"
    "../RadeonProRenderSharedComponents/src/IESLight/IESLightRepresentationCalc.h"
    "../RadeonProRenderSharedComponents/src/IESLight/IESprocessor.h"
    "../RadeonProRenderSharedComponents/src/ImageFilter/ImageFilter.h"
    "../RadeonProRenderSharedComponents/src/SunPosition/SPA.h"
    "../RadeonProRenderSharedComponents/src/Utils/Utils.h"
    "../RadeonProRenderSharedComponents/src/XMLMaterialExport/XMLMaterialExportCommon.h"
    "AnimationExporter.h"
    "athenaCmd.h"
    "attributeNames.h"
    "base_mesh.h"
    "common.h"
    "CompositeWrapper.h"
    "FireMaterialViewRenderer.h"
    "FireMaya.h"
    "Context/FireRenderContextIFace.h"
    "FireRenderGlobals.h"
    "FireRenderGPUCache.h"
    "FireRenderMaterialSwatchRender.h"
    "FireRenderMeshMASH.h"
    "FireRenderObjects.h"
    "FireRenderProduction.h"
    "FireRenderSwatchInstance.h"
    "frWrap.h"
    "GLTFTranslator.h"
    "InstancerMASH.h"
    "MaterialLoader.h"
    "NorthStarRenderingHelper.h"
    "OptionVarHelpers.h"
    "RenderRegion.h"
    "RenderStamp.h"
    "RenderStampUtils.h"
    "RenderViewUpdater.h"
    "resource.h"
    "RprComposite.h"
    "ShadersManager.h"
    "TileRenderer.h"
    "ViewportTexture.h"
    "VRay.h"
	"VulcanUtils.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Hosek
    "Hosek/ArHosekSkyModel.cpp"
    "Hosek/ArHosekSkyModel.h"
    "Hosek/ArHosekSkyModelData_CIEXYZ.h"
    "Hosek/ArHosekSkyModelData_RGB.h"
    "Hosek/ArHosekSkyModelData_Spectral.h"
)
source_group("Hosek" FILES ${Hosek})

set(IPR
    "FireRenderIpr.cpp"
    "FireRenderIpr.h"
)
source_group("IPR" FILES ${IPR})

set(Lights
    "Lights/FireRenderLightCommon.cpp"
    "Lights/FireRenderLightCommon.h"
)
source_group("Lights" FILES ${Lights})

set(Lights__IES
    "Lights/IES/FireRenderIESLight.cpp"
    "Lights/IES/FireRenderIESLight.h"
    "Lights/IES/IESLightLocatorMesh.cpp"
    "Lights/IES/IESLightLocatorMesh.h"
)
source_group("Lights\\IES" FILES ${Lights__IES})

set(Lights__PhysicalLight
    "Lights/PhysicalLight/FireRenderPhysicalLightLocator.cpp"
    "Lights/PhysicalLight/FireRenderPhysicalLightLocator.h"
    "Lights/PhysicalLight/FireRenderPhysicalOverride.cpp"
    "Lights/PhysicalLight/FireRenderPhysicalOverride.h"
    "Lights/PhysicalLight/PhysicalLightAttributes.cpp"
    "Lights/PhysicalLight/PhysicalLightAttributes.h"
    "Lights/PhysicalLight/PhysicalLightData.cpp"
    "Lights/PhysicalLight/PhysicalLightData.h"
    "Lights/PhysicalLight/PhysicalLightGeometryUtility.cpp"
    "Lights/PhysicalLight/PhysicalLightGeometryUtility.h"
)
source_group("Lights\\PhysicalLight" FILES ${Lights__PhysicalLight})

set(Materials
    "FireRenderAO.cpp"
    "FireRenderAO.h"
    "FireRenderArithmetic.cpp"
    "FireRenderArithmetic.h"
    "FireRenderBlendMaterial.cpp"
    "FireRenderBlendMaterial.h"
    "FireRenderBlendValue.cpp"
    "FireRenderBlendValue.h"
    "FireRenderBump.cpp"
    "FireRenderBump.h"
    "FireRenderChecker.cpp"
    "FireRenderChecker.h"
    "FireRenderDisplacement.cpp"
    "FireRenderDisplacement.h"
    "FireRenderDot.cpp"
    "FireRenderDot.h"
    "FireRenderFresnel.cpp"
    "FireRenderFresnel.h"
    "FireRenderFresnelSchlick.cpp"
    "FireRenderFresnelSchlick.h"
    "FireRenderGradient.cpp"
    "FireRenderGradient.h"
    "FireRenderLookup.cpp"
    "FireRenderLookup.h"
    "FireRenderMaterial.cpp"
    "FireRenderMaterial.h"
    "FireRenderNoise.cpp"
    "FireRenderNoise.h"
    "FireRenderNormal.cpp"
    "FireRenderNormal.h"
    "FireRenderPassthrough.cpp"
    "FireRenderPassthrough.h"
    "FireRenderPBRMaterial.cpp"
    "FireRenderPBRMaterial.h"
    "FireRenderShadowCatcherMaterial.cpp"
    "FireRenderShadowCatcherMaterial.h"
    "FireRenderStandardMaterial.cpp"
    "FireRenderStandardMaterial.h"
    "FireRenderTexture.cpp"
    "FireRenderTexture.h"
    "FireRenderToonMaterial.cpp"
    "FireRenderToonMaterial.h"
    "FireRenderTransparentMaterial.cpp"
    "FireRenderTransparentMaterial.h"
    "FireRenderVolumeMaterial.cpp"
    "FireRenderVolumeMaterial.h"
    "FireRenderVoronoi.cpp"
    "FireRenderVoronoi.h"
    "SubsurfaceMaterial.cpp"
    "SubsurfaceMaterial.h"
    "FireRenderMaterialXMaterial.cpp"
    "FireRenderMaterialXMaterial.h"
)
source_group("Materials" FILES ${Materials})

set(MayaStandardNodesSupport
    "MayaStandardNodesSupport/AddDoubleLinearConverter.cpp"
    "MayaStandardNodesSupport/AddDoubleLinearConverter.h"
    "MayaStandardNodesSupport/BaseConverter.cpp"
    "MayaStandardNodesSupport/BaseConverter.h"
    "MayaStandardNodesSupport/BlendColorsConverter.cpp"
    "MayaStandardNodesSupport/BlendColorsConverter.h"
    "MayaStandardNodesSupport/BlendTwoAttrConverter.cpp"
    "MayaStandardNodesSupport/BlendTwoAttrConverter.h"
    "MayaStandardNodesSupport/Bump2dConverter.cpp"
    "MayaStandardNodesSupport/Bump2dConverter.h"
    "MayaStandardNodesSupport/ClampConverter.cpp"
    "MayaStandardNodesSupport/ClampConverter.h"
    "MayaStandardNodesSupport/ColorMathConverter.cpp"
    "MayaStandardNodesSupport/ColorMathConverter.h"
    "MayaStandardNodesSupport/ContrastConverter.cpp"
    "MayaStandardNodesSupport/ContrastConverter.h"
    "MayaStandardNodesSupport/FileNodeConverter.cpp"
    "MayaStandardNodesSupport/FileNodeConverter.h"
    "MayaStandardNodesSupport/GammaCorrectConverter.cpp"
    "MayaStandardNodesSupport/GammaCorrectConverter.h"
    "MayaStandardNodesSupport/HSVToRGBConverter.cpp"
    "MayaStandardNodesSupport/HSVToRGBConverter.h"
    "MayaStandardNodesSupport/MultDoubleLinearConverter.cpp"
    "MayaStandardNodesSupport/MultDoubleLinearConverter.h"
    "MayaStandardNodesSupport/MultiplyDivideConverter.cpp"
    "MayaStandardNodesSupport/MultiplyDivideConverter.h"
    "MayaStandardNodesSupport/NodeCheckerConverter.cpp"
    "MayaStandardNodesSupport/NodeCheckerConverter.h"
    "MayaStandardNodesSupport/NodeConverterUtil.cpp"
    "MayaStandardNodesSupport/NodeConverterUtil.h"
    "MayaStandardNodesSupport/NodeProcessingUtils.cpp"
    "MayaStandardNodesSupport/NodeProcessingUtils.h"
    "MayaStandardNodesSupport/NoiseConverter.cpp"
    "MayaStandardNodesSupport/NoiseConverter.h"
    "MayaStandardNodesSupport/Place2dTextureConverter.cpp"
    "MayaStandardNodesSupport/Place2dTextureConverter.h"
    "MayaStandardNodesSupport/PlusMinusAverageConverter.cpp"
    "MayaStandardNodesSupport/PlusMinusAverageConverter.h"
    "MayaStandardNodesSupport/ProjectionNodeConverter.cpp"
    "MayaStandardNodesSupport/ProjectionNodeConverter.h"
    "MayaStandardNodesSupport/RampNodeConverter.cpp"
    "MayaStandardNodesSupport/RampNodeConverter.h"
    "MayaStandardNodesSupport/RemapHSVConverter.cpp"
    "MayaStandardNodesSupport/RemapHSVConverter.h"
    "MayaStandardNodesSupport/RemapValueConverter.cpp"
    "MayaStandardNodesSupport/RemapValueConverter.h"
    "MayaStandardNodesSupport/ReverseMapConverter.cpp"
    "MayaStandardNodesSupport/ReverseMapConverter.h"
    "MayaStandardNodesSupport/RGBToHSVConverter.cpp"
    "MayaStandardNodesSupport/RGBToHSVConverter.h"
    "MayaStandardNodesSupport/SetRangeConverter.cpp"
    "MayaStandardNodesSupport/SetRangeConverter.h"
    "MayaStandardNodesSupport/VectorProductConverter.cpp"
    "MayaStandardNodesSupport/VectorProductConverter.h"
)
source_group("MayaStandardNodesSupport" FILES ${MayaStandardNodesSupport})

#set(Resources__Icons
#    "icons/amd.xpm"
#)
#source_group("Resources\\Icons" FILES ${Resources__Icons})

#set(Resources__Python
#    "python/__init__.py"
#    "python/fireRender/__init__.py"
#    "python/fireRender/fireRenderMenu.py"
#    "python/fireRender/fireRenderUtils.py"
#    "python/fireRender/renderViewUtils.py"
#    "python/fireRender/rpr_location_selector.py"
#    "python/fireRender/rpr_material_browser.py"
#    "python/fireRender/vrayUtils.py"
#)
#source_group("Resources\\Python" FILES ${Resources__Python})

#set(Resources__Scripts
#    "scripts/AboutAndCopyright.html"
#    "scripts/common.mel"
#    "scripts/createFireRenderCommonGlobalsTab.mel"
#    "scripts/createFireRenderGlobalsTab.mel"
#    "scripts/fireRenderBatch.mel"
#    "scripts/fireRenderEnvironment.mel"
#    "scripts/fireRenderIpr.mel"
#    "scripts/fireRenderUpdateAE.mel"
#    "scripts/physicalLightScripts.mel"
#    "scripts/registerFireRender.mel"
#    "scripts/rprUnifiedRenderGlobalsWindow201800.mel"
#    "scripts/rprUnifiedRenderGlobalsWindow201900.mel"
#    "scripts/rprUnifiedRenderGlobalsWindow202000.mel"
#    "scripts/setupFireRenderNodeClassification.mel"
#    "scripts/shelfCommands.mel"
#)
#source_group("Resources\\Scripts" FILES ${Resources__Scripts})

#set(Resources__Scripts__AttributeEditorTemplates
#    "scripts/AERPRAddMaterialTemplate.mel"
#    "scripts/AERPRBlendMaterialTemplate.mel"
#    "scripts/AERPRGlobalsTemplate.mel"
#    "scripts/AERPRIBLTemplate.mel"
#    "scripts/AERPRIESTemplate.mel"
#    "scripts/AERPRMaterialTemplate.mel"
#    "scripts/AERPRPBRMaterialTemplate.mel"
#    "scripts/AERPRPhysicalLightTemplate.mel"
#    "scripts/AERPRSkyTemplate.mel"
#    "scripts/AERPRSubsurfaceMaterialTemplate.mel"
#    "scripts/AERPRUberMaterialTemplate.mel"
#    "scripts/AERPRVolumeMaterialTemplate.mel"
#)
#source_group("Resources\\Scripts\\AttributeEditorTemplates" FILES ${Resources__Scripts__AttributeEditorTemplates})

#set(Resources__Shaders
##    "shaders/rprBlitColor.ogsfx"
##    "shaders/rprBlitColorGL.cgfx"
#)
#source_group("Resources\\Shaders" FILES ${Resources__Shaders})

#set(Resources__Shelfs
#    "Shelfs/shelf_Radeon_ProRender.mel"
#)
#source_group("Resources\\Shelfs" FILES ${Resources__Shelfs})

set(Source_Files
 #   "../RadeonProRenderSDK/RadeonProRender/inc/Math/half.cpp"
    "../RadeonProRenderSDK/RadeonProRender/rprTools/RPRStringIDMapper.cpp"
    "../RadeonProRenderSDK/RadeonProRender/rprTools/RprTools.cpp"
    "../RadeonProRenderSharedComponents/src/Alembic/AlembicWrapper.cpp"
    "../RadeonProRenderSharedComponents/src/Athena/athenaWrap.cpp"
    "../RadeonProRenderSharedComponents/src/IESLight/IESLightRepresentationCalc.cpp"
    "../RadeonProRenderSharedComponents/src/IESLight/IESprocessor.cpp"
    "../RadeonProRenderSharedComponents/src/ImageFilter/ImageFilter.cpp"
    "../RadeonProRenderSharedComponents/src/SunPosition/SPA.cpp"
    "../RadeonProRenderSharedComponents/src/Utils/Utils.cpp"
    "../RadeonProRenderSharedComponents/src/XMLMaterialExport/XMLMaterialExportCommon.cpp"
    "AnimationExporter.cpp"
    "athenaCmd.cpp"
    "CompositeWrapper.cpp"
    "FireMaterialViewRenderer.cpp"
    "FireMaya.cpp"
    "FireRenderBevel.cpp"
    "FireRenderBevel.h"
#    "FireRenderBlendOverride.cpp"
#    "FireRenderBlendOverride.h"
    "FireRenderDoublesided.cpp"
    "FireRenderDoublesided.h"
    "FireRenderGlobals.cpp"
    "FireRenderGPUCache.cpp"
    "FireRenderHairs.cpp"
    "FireRenderMaterialSwatchRender.cpp"
    "FireRenderMeshMASH.cpp"
    "FireRenderObjects.cpp"
    "FireRenderProduction.cpp"
    "FireRenderRamp.cpp"
    "FireRenderRamp.h"
    "FireRenderSurfaceOverride.cpp"
    "FireRenderSurfaceOverride.h"
    "FireRenderSwatchInstance.cpp"
    "FireRenderThread.cpp"
    "FireRenderThread.h"
    "FireRenderVolume.cpp"
    "frWrap.cpp"
    "GLTFTranslator.cpp"
    "InstancerMASH.cpp"
    "MaterialLoader.cpp"
    "NorthStarRenderingHelper.cpp"
    "OptionVarHelpers.cpp"
    "pluginMain.cpp"
    "RenderStamp.cpp"
    "RenderStampUtils.cpp"
    "RenderViewUpdater.cpp"
    "RprComposite.cpp"
    "ShadersManager.cpp"
    "TileRenderer.cpp"
    "ViewportTexture.cpp"
    "VRay.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(Source_Win
    "RadeonProRender.cpp"
)
source_group("Source Win" FILES ${Source_Win})

set(Translators
    "Translators/MeshTranslator.cpp"
    "Translators/MeshTranslator.h"
    "Translators/MultipleShaderMeshTranslator.cpp"
    "Translators/MultipleShaderMeshTranslator.h"
    "Translators/SingleShaderMeshTranslator.cpp"
    "Translators/SingleShaderMeshTranslator.h"
    "Translators/Translators.cpp"
    "Translators/Translators.h"
)
source_group("Translators" FILES ${Translators})

set(Utils
    "athenaSystemInfo_Win.cpp"
    "athenaSystemInfo_Win.h"
    "AutoLock.h"
    "DependencyNode.cpp"
    "DependencyNode.h"
    "FastNoise.cpp"
    "FastNoise.h"
    "FileSystemUtils.cpp"
    "FileSystemUtils.h"
    "FireRenderError.cpp"
    "FireRenderError.h"
    "FireRenderImageUtil.cpp"
    "FireRenderImageUtil.h"
    "FireRenderLayeredTextureUtils.cpp"
    "FireRenderLayeredTextureUtils.h"
    "FireRenderMath.h"
    "FireRenderUtils.cpp"
    "FireRenderUtils.h"
    "GlobalRenderUtilsDataHolder.cpp"
    "GlobalRenderUtilsDataHolder.h"
    "Logger.h"
    "StartupContextChecker.cpp"
    "StartupContextChecker.h"
)
source_group("Utils" FILES ${Utils})

set(Viewport
    "FireRenderOverride.cpp"
    "FireRenderOverride.h"
    "FireRenderTextureCache.cpp"
    "FireRenderTextureCache.h"
    "FireRenderViewport.cpp"
    "FireRenderViewport.h"
    "FireRenderViewportBlit.cpp"
    "FireRenderViewportBlit.h"
    "FireRenderViewportManager.cpp"
    "FireRenderViewportManager.h"
    "FireRenderViewportOperation.cpp"
    "FireRenderViewportOperation.h"
    "FireRenderViewportUI.cpp"
    "FireRenderViewportUI.h"
)
source_group("Viewport" FILES ${Viewport})

set(Volumes
    "Volumes/FireRenderVolumeLocator.cpp"
    "Volumes/FireRenderVolumeLocator.h"
    "Volumes/FireRenderVolumeOverride.cpp"
    "Volumes/FireRenderVolumeOverride.h"
    "Volumes/VolumeAttributes.cpp"
    "Volumes/VolumeAttributes.h"
)
source_group("Volumes" FILES ${Volumes})

#set(docs
#    "docs/Context Parameters.txt"
#    "docs/Material Node Parameters.txt"
#)
#source_group("docs" FILES ${docs})

if(WIN32)
set(ALL_FILES
#    ${no_group_source_files}
    ${AOVs}
    ${Commands}
    ${Context}
    ${Environment}
    ${Header_Files}
    ${Hosek}
    ${IPR}
    ${Lights}
    ${Lights__IES}
    ${Lights__PhysicalLight}
    ${Materials}
    ${MayaStandardNodesSupport}
    #${Resources__Icons}
    #${Resources__Python}
    #${Resources__Scripts}
    #${Resources__Scripts__AttributeEditorTemplates}
    #${Resources__Shaders}
    #${Resources__Shelfs}
    ${Source_Files}
    ${Translators}
    ${Utils}
    ${Viewport}
    ${Volumes}
    ${Source_Win}
    #${docs}
)
else()
set(ALL_FILES
    ${AOVs}
    ${Commands}
    ${Context}
    ${Environment}
    ${Header_Files}
    ${Hosek}
    ${IPR}
    ${Lights}
    ${Lights__IES}
    ${Lights__PhysicalLight}
    ${Materials}
    ${MayaStandardNodesSupport}
    ${Source_Files}
    ${Translators}
    ${Utils}
    ${Viewport}
    ${Volumes}
)
endif()

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
################################################################################
# Includes for CMake from *.props
################################################################################
##use_props(${PROJECT_NAME} Debug       "FireRenderMaya.cmake")
##use_props(${PROJECT_NAME} Debug       "frMaya2022.cmake")
#use_props(${PROJECT_NAME} Debug2022   "FireRenderMaya.cmake")
#use_props(${PROJECT_NAME} Debug2022   "frMaya2022.cmake")
#use_props(${PROJECT_NAME} Debug2023   "FireRenderMaya.cmake")
#use_props(${PROJECT_NAME} Debug2023   "FrMaya2023.cmake")
#use_props(${PROJECT_NAME} Debug2024   "FireRenderMaya.cmake")
#use_props(${PROJECT_NAME} Debug2024   "FrMaya2024.cmake")
##use_props(${PROJECT_NAME} Release     "FireRenderMaya.cmake")
##use_props(${PROJECT_NAME} Release     "frMaya2022.cmake")
#use_props(${PROJECT_NAME} Release2022 "FireRenderMaya.cmake")
#use_props(${PROJECT_NAME} Release2022 "frMaya2022.cmake")
#use_props(${PROJECT_NAME} Release2023 "FireRenderMaya.cmake")
#use_props(${PROJECT_NAME} Release2023 "FrMaya2023.cmake")
#use_props(${PROJECT_NAME} Release2024 "FireRenderMaya.cmake")
#use_props(${PROJECT_NAME} Release2024 "FrMaya2024.cmake")

if(WIN32)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		VS_GLOBAL_KEYWORD "Win32Proj"
	)

	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/DELAYLOAD:RadeonProRender64.dll")

	set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".mll")
endif()

################################################################################
# Target name
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
#    TARGET_NAME_DEBUG       "${PROJECT_NAME}$ENV{TargetExt}"
    TARGET_NAME_DEBUG2022   "${PROJECT_NAME}$ENV{TargetExt}"
    TARGET_NAME_DEBUG2023   "${PROJECT_NAME}$ENV{TargetExt}"
    TARGET_NAME_DEBUG2024   "${PROJECT_NAME}$ENV{TargetExt}"
#    TARGET_NAME_RELEASE     "${TARGET_NAME}$ENV{TargetExt}"
    TARGET_NAME_RELEASE2022 "${TARGET_NAME}$ENV{TargetExt}"
    TARGET_NAME_RELEASE2023 "${TARGET_NAME}$ENV{TargetExt}"
    TARGET_NAME_RELEASE2024 "${TARGET_NAME}$ENV{TargetExt}"
)
################################################################################
# Output directory
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
#    OUTPUT_DIRECTORY_DEBUG       "${CMAKE_SOURCE_DIR}/bin/output/${PROJECT_NAME}/$<CONFIG>"
    OUTPUT_DIRECTORY_DEBUG2022   "${CMAKE_SOURCE_DIR}/bin/output/${PROJECT_NAME}/$<CONFIG>"
    OUTPUT_DIRECTORY_DEBUG2023   "${CMAKE_SOURCE_DIR}/bin/output/${PROJECT_NAME}/$<CONFIG>"
    OUTPUT_DIRECTORY_DEBUG2024   "${CMAKE_SOURCE_DIR}/bin/output/${PROJECT_NAME}/$<CONFIG>"
#    OUTPUT_DIRECTORY_RELEASE     "${CMAKE_SOURCE_DIR}/bin/output/${PROJECT_NAME}/$<CONFIG>"
    OUTPUT_DIRECTORY_RELEASE2022 "${CMAKE_SOURCE_DIR}/bin/output/${PROJECT_NAME}/$<CONFIG>"
    OUTPUT_DIRECTORY_RELEASE2023 "${CMAKE_SOURCE_DIR}/bin/output/${PROJECT_NAME}/$<CONFIG>"
    OUTPUT_DIRECTORY_RELEASE2024 "${CMAKE_SOURCE_DIR}/bin/output/${PROJECT_NAME}/$<CONFIG>"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
#    ARCHIVE_OUTPUT_DIRECTORY_DEBUG       "${CMAKE_CURRENT_SOURCE_DIR}/"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG2022   "${CMAKE_CURRENT_SOURCE_DIR}/"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG2023   "${CMAKE_CURRENT_SOURCE_DIR}/"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG2024   "${CMAKE_CURRENT_SOURCE_DIR}/"
#   ARCHIVE_OUTPUT_DIRECTORY_RELEASE     "${CMAKE_CURRENT_SOURCE_DIR}/"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE2022 "${CMAKE_CURRENT_SOURCE_DIR}/"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE2023 "${CMAKE_CURRENT_SOURCE_DIR}/"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE2024 "${CMAKE_CURRENT_SOURCE_DIR}/"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
#    ARCHIVE_OUTPUT_NAME_DEBUG       "$ENV{IntermediateOutputPath}${PROJECT_NAME}"
    ARCHIVE_OUTPUT_NAME_DEBUG2022   "$ENV{IntermediateOutputPath}${PROJECT_NAME}"
    ARCHIVE_OUTPUT_NAME_DEBUG2023   "$ENV{IntermediateOutputPath}${PROJECT_NAME}"
    ARCHIVE_OUTPUT_NAME_DEBUG2024   "$ENV{IntermediateOutputPath}${PROJECT_NAME}"
#    ARCHIVE_OUTPUT_NAME_RELEASE     "$ENV{IntermediateOutputPath}${PROJECT_NAME}"
    ARCHIVE_OUTPUT_NAME_RELEASE2022 "$ENV{IntermediateOutputPath}${PROJECT_NAME}"
    ARCHIVE_OUTPUT_NAME_RELEASE2023 "$ENV{IntermediateOutputPath}${PROJECT_NAME}"
    ARCHIVE_OUTPUT_NAME_RELEASE2024 "$ENV{IntermediateOutputPath}${PROJECT_NAME}"
)
################################################################################
# MSVC runtime library
################################################################################
get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
#    $<$<CONFIG:Release>:
#        MultiThreaded
#    >
    $<$<CONFIG:Release2022>:
        MultiThreaded
    >
    $<$<CONFIG:Release2023>:
        MultiThreaded
    >
    $<$<CONFIG:Release2024>:
        MultiThreaded
    >
    #$<$<NOT:$<OR:$<CONFIG:Release>,$<CONFIG:Release2022>,$<CONFIG:Release2023>,$<CONFIG:Release2024>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
    $<$<NOT:$<OR:$<CONFIG:Release2022>,$<CONFIG:Release2023>,$<CONFIG:Release2024>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
)
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

################################################################################
# Compile definitions
################################################################################
if(WIN32)
target_compile_definitions(${PROJECT_NAME} PRIVATE
#    "$<$<CONFIG:Debug>:"
#        "_DEBUG"
#    ">"
    "$<$<CONFIG:Debug2022>:"
        "_DEBUG"
		"MAYA2022"
    ">"
    "$<$<CONFIG:Debug2023>:"
        "_DEBUG"
		"MAYA2023"
    ">"
    "$<$<CONFIG:Debug2024>:"
        "_DEBUG"
		"MAYA2024"
    ">"
#    "$<$<CONFIG:Release>:"
#        "NDEBUG;"
#        "TBB_USE_DEBUG=0"
#    ">"
    "$<$<CONFIG:Release2022>:"
        "NDEBUG;"
        "TBB_USE_DEBUG=0"
		"MAYA2022"
    ">"
    "$<$<CONFIG:Release2023>:"
        "NDEBUG;"
        "TBB_USE_DEBUG=0"
		"MAYA2023"
    ">"
    "$<$<CONFIG:Release2024>:"
        "NDEBUG;"
        "TBB_USE_DEBUG=0"
		"MAYA2024"
    ">"
	
	"MAYA_PLUGIN"
	"GL_INTEROP"
	"WIN32"
	"_WINDOWS"
	"_USRDLL"
	"NT_PLUGIN"
	"_HAS_ITERATOR_DEBUGGING=0"
	"_SECURE_SCL=0"
	"_SECURE_SCL_THROWS=0"
	"_SECURE_SCL_DEPRECATE=0"
	"_CRT_SECURE_NO_DEPRECATE"
	"__TBB_LIB_NAME=tbb.lib"
	"REQUIRE_IOSTREAM"
	"AW_NEW_IOSTREAMS"
	"Bits64_"
)
endif()

if(WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE
		"_WINDLL"
	)
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
#        $<$<CONFIG:Debug>:
#            /Gm;
#            /RTC1;
#            /ZI
#        >
        $<$<CONFIG:Debug2022>:
			/JMC
			/Zc:wchar_t 
			/ZI
			/Gm
			/Od
			/Zc:inline
			/fp:precise
			/errorReport:prompt 
			/WX- 
			/Zc:forScope
			/RTC1 
			/Gd 
			/MDd 
			/FC
        >
        $<$<CONFIG:Debug2023>:
			/JMC
			/Zc:wchar_t 
			/ZI
			/Gm
			/Od
			/Zc:inline
			/fp:precise
			/errorReport:prompt 
			/WX- 
			/Zc:forScope
			/RTC1 
			/Gd 
			/MDd 
			/FC
        >
        $<$<CONFIG:Debug2024>:
			/JMC
			/Zc:wchar_t 
			/ZI
			/Gm
			/Od
			/Zc:inline
			/fp:precise
			/errorReport:prompt 
			/WX- 
			/Zc:forScope
			/RTC1 
			/Gd 
			/MDd 
			/FC
        >
#        $<$<CONFIG:Release>:
#            /GF;
#            /Gy;
#            /Zi
#        >
        $<$<CONFIG:Release2022>:
			/Gy 
			/Zc:wchar_t 
			/Zi 
			/Gm- 
			/O2
			/Zc:inline 
			/fp:precise
			/errorReport:prompt 
			/GF 
			/WX- 
			/Zc:forScope 
			/Gd 
			/MT 
			/FC 			
			/EHsc 
			/nologo
        >
        $<$<CONFIG:Release2023>:
			/Gy 
			/Zc:wchar_t 
			/Zi 
			/Gm- 
			/O2
			/Zc:inline 
			/fp:precise
			/errorReport:prompt 
			/GF 
			/WX- 
			/Zc:forScope 
			/Gd 
			/MT 
			/FC 			
			/EHsc 
			/nologo
        >
        $<$<CONFIG:Release2024>:
			/Gy 
			/Zc:wchar_t 
			/Zi 
			/Gm- 
			/O2
			/Zc:inline 
			/fp:precise
			/errorReport:prompt 
			/GF 
			/WX- 
			/Zc:forScope 
			/Gd 
			/MT 
			/FC 			
			/EHsc 
			/nologo
        >
        /std:c++17;
        /W3;
        /GR;
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /GS;
        /openmp
    )
    target_link_options(${PROJECT_NAME} PRIVATE
#        $<$<CONFIG:Debug>:
#            /INCREMENTAL
#        >
        $<$<CONFIG:Debug2022>:
            /INCREMENTAL
        >
        $<$<CONFIG:Debug2023>:
            /INCREMENTAL
        >
        $<$<CONFIG:Debug2024>:
            /INCREMENTAL
        >
#        $<$<CONFIG:Release>:
#            /OPT:REF;
#            /OPT:ICF;
#            /INCREMENTAL:NO
#        >
        $<$<CONFIG:Release2022>:
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO
        >
        $<$<CONFIG:Release2023>:
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO
        >
        $<$<CONFIG:Release2024>:
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO
        >
        /DEBUG:FULL;
        /DYNAMICBASE:NO;
        /export:initializePlugin;
        /export:uninitializePlugin
    )
endif()

################################################################################
# Dependencies
################################################################################
if(WIN32)
set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "OpenMayaFX"
	"AdskXGen.lib"
	"RadeonProRender64"
	"RprLoadStore64"
	"ProRenderGLTF"
	"RadeonImageFilters"
	"OpenImageIO_RPR"
	"Foundation"
	"OpenMaya"
	"OpenMayaUI"
	"OpenMayaAnim"
	"OpenMayaRender"
	"Image"
	"glew"
	"glu32"
	"OpenGL32"
	"openvdb"
	"Alembic"
	"Half-2_3"
	"hdf5"
	"Iex-2_3"
	"Imath-2_3"
)
target_link_libraries(${PROJECT_NAME} PUBLIC "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

target_link_directories(${PROJECT_NAME} PUBLIC
#    "$<$<CONFIG:Debug>:"
#        "$ENV{MAYA_X64_2022}/lib"
#    ">"
    "$<$<CONFIG:Debug2022>:"
        "$ENV{MAYA_X64_2022}/lib"
        "$ENV{MAYA_X64_2022}/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Debug2023>:"
        "$ENV{MAYA_X64_2023}/lib"
        "$ENV{MAYA_X64_2023}/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Debug2024>:"
        "$ENV{MAYA_X64_2024}/lib"
        "$ENV{MAYA_X64_2024}/plug-ins/xgen/lib"
    ">"
#    "$<$<CONFIG:Release>:"
#        "$ENV{MAYA_X64_2022}/lib"
#    ">"
    "$<$<CONFIG:Release2022>:"
        "$ENV{MAYA_X64_2022}/lib"
        "$ENV{MAYA_X64_2022}/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Release2023>:"
        "$ENV{MAYA_X64_2023}/lib"
        "$ENV{MAYA_X64_2023}/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Release2024>:"
        "$ENV{MAYA_X64_2024}/lib"
        "$ENV{MAYA_X64_2024}/plug-ins/xgen/lib"
    ">"
)
endif()

if(WIN32)
target_include_directories(${PROJECT_NAME} PUBLIC
#    "$<$<CONFIG:Debug>:"
#        "$ENV{MAYA_X64_2022}/lib"
#    ">"
    "$<$<CONFIG:Debug2022>:"
        "$ENV{MAYA_SDK_2022}/include"
        "$ENV{MAYA_X64_2022}/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Debug2023>:"
        "$ENV{MAYA_SDK_2023}/include"
        "$ENV{MAYA_X64_2023}/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Debug2024>:"
        "$ENV{MAYA_SDK_2024}/include"
        "$ENV{MAYA_X64_2024}/plug-ins/xgen/include"
    ">"
#    "$<$<CONFIG:Release>:"
#        "$ENV{MAYA_X64_2022}/lib"
#    ">"
    "$<$<CONFIG:Release2022>:"
        "$ENV{MAYA_SDK_2022}/include"
        "$ENV{MAYA_X64_2022}/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Release2023>:"
        "$ENV{MAYA_SDK_2023}/include"
        "$ENV{MAYA_X64_2023}/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Release2024>:"
        "$ENV{MAYA_SDK_2024}/include"
        "$ENV{MAYA_X64_2024}/plug-ins/xgen/include"
    ">"
)
endif()

if(WIN32)
target_link_directories(${PROJECT_NAME} PUBLIC
	"../RadeonProRenderSharedComponents/glew/lib/x64"
	"../RadeonProRenderSharedComponents/OpenImageIO/Windows/lib"
	"../RadeonProRenderSDK/RadeonProRender/libWin64"
	"../RadeonProImageProcessingSDK/Windows/Dynamic"
	"../RadeonProRenderSharedComponents/OpenVDB/Windows/lib"
	"../RadeonProRenderSharedComponents/Alembic/lib"
)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
	"../Vulkan-Headers/include"
	"../RadeonProRenderSDK/RadeonProRender/inc"
	"../RadeonProRenderSDK/RadeonProRender/rprTools"
	"../RadeonProRenderSharedComponents/glew/include"
	"../RadeonProRenderSharedComponents/OpenImageIO/Windows/include"
	"../RadeonProRenderSharedComponents/json/include"
	"../RadeonProRenderSharedComponents/src"
	"Lights/"
	"Lights/PhysicalLight"
	"../RadeonProRenderSharedComponents/src/ImageFilter"
	"../RadeonProImageProcessingSDK/include"
	"../RadeonProRenderSharedComponents/Ornatrix"
	"../RadeonProRenderSharedComponents/Ornatrix/Ephere"
	"../RadeonProRenderSharedComponents/OpenVDB/include"
	"../RadeonProRenderSharedComponents/Alembic/include"
	"../RadeonProRenderSharedComponents/Alembic/includeOpenEXR"
	"../RadeonProRenderSharedComponents"
)


target_compile_definitions(${PROJECT_NAME} PUBLIC "-D_USE_MATH_DEFINES") # needed for M_PI

if(WIN32)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
	COMMAND call ${CMAKE_SOURCE_DIR}/copy_bild.cmd "$(SolutionDir)" "$(TargetPath)" 
		$<$<CONFIG:Debug2022>:"2022">
        $<$<CONFIG:Release2022>:"2022">
        $<$<CONFIG:Debug2023>:"2023">
        $<$<CONFIG:Release2023>:"2023">
        $<$<CONFIG:Debug2024>:"2024">
        $<$<CONFIG:Release2024>:"2024">
    COMMENT "This command will copy files to dist" 
	)
endif()
