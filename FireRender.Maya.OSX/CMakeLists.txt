################################################################################
# Mac-specific stuff
################################################################################
if(APPLE)

#cmake_policy(SET CMP0142 NEW)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug2022>:
		-O0
    >
    $<$<CONFIG:Debug2023>:
		-O0
    >
    $<$<CONFIG:Debug2024>:
		-O0
    >
    $<$<CONFIG:Release2022>:
		-Os
    >
    $<$<CONFIG:Release2023>:
		-Os
    >
    $<$<CONFIG:Release2024>:
		-Os
    >
    -std=c++17;
)


target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug2022>:
        -Wl,-executable_path,/Applications/Autodesk/maya2022/Maya.app/Contents/MacOS 
        -Wl,-exported_symbol,__Z16initializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE,
        -Wl,-exported_symbol,__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE
        -Wl,-exported_symbol,_MApiVersion
    >
    $<$<CONFIG:Debug2023>:
        -Wl,-executable_path,/Applications/Autodesk/maya2023/Maya.app/Contents/MacOS 
        -Wl,-exported_symbol,__Z16initializePluginN8Autodesk4Maya16OpenMaya202300007MObjectE,
        -Wl,-exported_symbol,__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202300007MObjectE
        -Wl,-exported_symbol,_MApiVersion 
    >
    $<$<CONFIG:Debug2024>:
        -Wl,-executable_path,/Applications/Autodesk/maya2024/Maya.app/Contents/MacOS 
        -Wl,-exported_symbol,__Z16initializePluginN8Autodesk4Maya16OpenMaya202400007MObjectE,
        -Wl,-exported_symbol,__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202400007MObjectE
        -Wl,-exported_symbol,_MApiVersion
    >
    $<$<CONFIG:Release2022>:
        -Wl,-executable_path,/Applications/Autodesk/maya2022/Maya.app/Contents/MacOS 
        -Wl,-exported_symbol,__Z16initializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE,
        -Wl,-exported_symbol,__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE
        -Wl,-exported_symbol,_MApiVersion
    >
    $<$<CONFIG:Release2023>:
        -Wl,-executable_path,/Applications/Autodesk/maya2023/Maya.app/Contents/MacOS 
        -Wl,-exported_symbol,__Z16initializePluginN8Autodesk4Maya16OpenMaya202300007MObjectE,
        -Wl,-exported_symbol,__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202300007MObjectE
        -Wl,-exported_symbol,_MApiVersion
    >
    $<$<CONFIG:Release2024>:
        -Wl,-executable_path,/Applications/Autodesk/maya2024/Maya.app/Contents/MacOS 
        -Wl,-exported_symbol,__Z16initializePluginN8Autodesk4Maya16OpenMaya202400007MObjectE,
        -Wl,-exported_symbol,__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202400007MObjectE
        -Wl,-exported_symbol,_MApiVersion
    >
	-lOpenMaya -lOpenMayaAnim -lOpenMayaUI -lOpenMayaRender -lFoundation -framework OpenGL -ltbb -lglew -lOpenMayaFX -headerpad_max_install_names
)
	
	
set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE TRUE)
#set_target_properties(${PROJECT_NAME}
#	PROPERTIES BUNDLE_EXTENSION 
#)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug2022>:"
		"MAYA2022"
    ">"
    "$<$<CONFIG:Debug2023>:"
		"MAYA2023"
    ">"
    "$<$<CONFIG:Debug2024>:"
		"MAYA2024"
    ">"
    "$<$<CONFIG:Release2022>:"
        "NDEBUG"
		"MAYA2022"
    ">"
    "$<$<CONFIG:Release2023>:"
        "NDEBUG"
		"MAYA2023"
    ">"
    "$<$<CONFIG:Release2024>:"
        "NDEBUG"
		"MAYA2024"
    ">"
	"MAC_PLUGIN"
	"OSMac_" 
	"REQUIRE_IOSTREAM" 
	"MAYA_PLUGIN"
)


target_link_directories(${PROJECT_NAME} PUBLIC
	"$<$<CONFIG:Debug2022>:"
		"/Applications/Autodesk/maya2022/Maya.app/Contents/MacOS"
		"/Applications/Autodesk/maya2022/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Debug2023>:"
		"/Applications/Autodesk/maya2023/Maya.app/Contents/MacOS"
		"/Applications/Autodesk/maya2023/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Debug2024>:"
		"/Applications/Autodesk/maya2024/Maya.app/Contents/MacOS"
		"/Applications/Autodesk/maya2024/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Release2022>:"
		"/Applications/Autodesk/maya2022/Maya.app/Contents/MacOS"
		"/Applications/Autodesk/maya2022/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Release2023>:"
		"/Applications/Autodesk/maya2023/Maya.app/Contents/MacOS"
		"/Applications/Autodesk/maya2023/plug-ins/xgen/lib"
    ">"
    "$<$<CONFIG:Release2024>:"
		"/Applications/Autodesk/maya2024/Maya.app/Contents/MacOS"
		"/Applications/Autodesk/maya2024/plug-ins/xgen/lib"
    ">"
	"../RadeonProRenderSDK/RadeonProRender/binMacOS"
	"../RadeonProImageProcessingSDK/OSX/Dynamic"
	"../RadeonProRenderSharedComponents/OpenImageIO/OSX/bin"
	"../RadeonProRenderSharedComponents/OpenVDB/OSX/lib"
)


target_include_directories(${PROJECT_NAME} PUBLIC
    "$<$<CONFIG:Debug2022>:"
        "/Applications/Autodesk/maya2022/include"
        "/Applications/Autodesk/maya2022/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Debug2023>:"
        "/Applications/Autodesk/maya2023/include"
        "/Applications/Autodesk/maya2023/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Debug2024>:"
        "/Applications/Autodesk/maya2024/include"
        "/Applications/Autodesk/maya2024/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Release2022>:"
        "/Applications/Autodesk/maya2022/include"
        "/Applications/Autodesk/maya2022/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Release2023>:"
        "/Applications/Autodesk/maya2023/include"
        "/Applications/Autodesk/maya2023/plug-ins/xgen/include"
    ">"
    "$<$<CONFIG:Release2024>:"
        "/Applications/Autodesk/maya2024/include"
        "/Applications/Autodesk/maya2024/plug-ins/xgen/include"
    ">"
    "../FireRender.Maya.Src"
    "../FireRender.Maya.Src/Context"
    "../RadeonProRenderSharedComponents/OpenImageIO/OSX/include"
)

target_sources(${PROJECT_NAME} PUBLIC
    "frMayaPluginMac/athenaSystemInfo_Mac.h"
)

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)

#set(LIB_RIF_PATH ../RadeonProImageProcessingSDK/OSX/Dynamic)
#find_library(LIB_RADEON_IMAGE_FILTERS NAMES libRadeonImageFilters.1.7.1.dylib PATHS LIB_RIF_PATH)
#if (NOT LIB_RADEON_IMAGE_FILTERS)
#    message(FATAL_ERROR "libRadeonImageFilters not found")
#endif()
#message(${LIB_RADEON_IMAGE_FILTERS})
#add_library(librif SHARED IMPORTED)
#set_target_properties(librif PROPERTIES IMPORTED_LOCATION ${LIB_RADEON_IMAGE_FILTERS})
#target_link_libraries(${PROJECT_NAME} librif)

#set(LIB_OIIO_PATH ../RadeonProRenderSharedComponents/OpenImageIO/OSX/bin)
#find_library(LIB_OPEN_IMAGE NAMES libOpenImageIO.1.6.dylib PATHS LIB_OIIO_PATH)
#if (NOT LIB_OPEN_IMAGE)
#    message(FATAL_ERROR "libOpenImageIO not found")
#endif()
#message(${LIB_OPEN_IMAGE})
#add_library(liboiio SHARED IMPORTED)
#set_target_properties(liboiio PROPERTIES IMPORTED_LOCATION ${LIB_OPEN_IMAGE})
#target_link_libraries(${PROJECT_NAME} liboiio)

#set(LIB_OIIO_PATH ../RadeonProRenderSharedComponents/OpenImageIO/OSX/bin)
#message(${LIB_OIIO_PATH})
#find_library(LIB_OIIO NAMES libOpenImageIO.1.6.dylib PATHS LIB_OIIO_PATH)
#if (NOT LIB_OIIO)
#    message(FATAL_ERROR "libOpenImageIO not found")
#endif()
#message(${LIB_OIIO})
#add_library(liboiio SHARED IMPORTED)
#set_target_properties(librif PROPERTIES IMPORTED_LOCATION ${LIB_OIIO})
#target_link_libraries(${PROJECT_NAME} liboiio)

#find_library(LIB_OPEN_IMAGE NAMES libOpenImageIO.1.6.dylib PATHS "../RadeonProRenderSharedComponents/OpenImageIO/OSX/bin")
#if (NOT LIB_RADEON_IMAGE_FILTERS)
#    message(FATAL_ERROR "libOpenImageIO not found")
#endif()

#target_link_libraries(${PROJECT_NAME} -framework ${LIB_OPEN_IMAGE})

#find_library(LIB_ADSK_XGEN NAMES libAdskXGen.dylib PATHS "../RadeonProRenderSharedComponents/OpenImageIO/OSX/bin")
#if (NOT LIB_ADSK_XGEN)
#    message(FATAL_ERROR "libAdskXGen not found")
#endif()

#target_link_libraries(${PROJECT_NAME} ${LIB_ADSK_XGEN})

#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libRadeonImageFilters.1.7.1.dylib")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libOpenImageIO.1.6.dylib")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libblosc.a")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libboost_iostreams.a")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libopenvdb.a")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libsnappy.a")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libz.a")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libProRenderGLTF.dylib")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libAdskXGen.dylib")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libRprLoadStore64.dylib")
#target_link_libraries(${PROJECT_NAME} PRIVATE "-framework libRadeonProRender64.dylib")

endif()
