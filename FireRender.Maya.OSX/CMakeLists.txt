################################################################################
# Mac-specific stuff
################################################################################
if(APPLE)
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug2022>:
		/-O0
    >
    $<$<CONFIG:Debug2023>:
		/-O0
    >
    $<$<CONFIG:Debug2024>:
		/-O0
    >
    $<$<CONFIG:Release2022>:
		/-Os
    >
    $<$<CONFIG:Release2023>:
		/-Os
    >
    $<$<CONFIG:Release2024>:
		/-Os
    >
    /-std=c++17;
)


target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug2022>:
        -Wl,
		-executable_path,
		/Applications/Autodesk/maya2022/Maya.app/Contents/MacOS -Wl,
		-exported_symbol,
		__Z16initializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
		__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
    >
    $<$<CONFIG:Debug2023>:
        -Wl,
		-executable_path,
		/Applications/Autodesk/maya2023/Maya.app/Contents/MacOS -Wl,
		-exported_symbol,
		__Z16initializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
		__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
    >
    $<$<CONFIG:Debug2024>:
        -Wl,
		-executable_path,
		/Applications/Autodesk/maya2024/Maya.app/Contents/MacOS -Wl,
		-exported_symbol,
		__Z16initializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
		__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
    >
    $<$<CONFIG:Release2022>:
        -Wl,
		-executable_path,
		/Applications/Autodesk/maya2022/Maya.app/Contents/MacOS -Wl,
		-exported_symbol,
		__Z16initializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
		__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
    >
    $<$<CONFIG:Release2023>:
        -Wl,
		-executable_path,
		/Applications/Autodesk/maya2023/Maya.app/Contents/MacOS -Wl,
		-exported_symbol,
		__Z16initializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
		__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
    >
    $<$<CONFIG:Release2024>:
        -Wl,
		-executable_path,
		/Applications/Autodesk/maya2024/Maya.app/Contents/MacOS -Wl,
		-exported_symbol,
		__Z16initializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
		__Z18uninitializePluginN8Autodesk4Maya16OpenMaya202200007MObjectE -Wl,
		-exported_symbol,
    >
	_MApiVersion -lOpenMaya -lOpenMayaAnim -lOpenMayaUI -lOpenMayaRender -lFoundation -framework OpenGL -ltbb -lglew -lOpenMayaFX -headerpad_max_install_names
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
)

endif()
