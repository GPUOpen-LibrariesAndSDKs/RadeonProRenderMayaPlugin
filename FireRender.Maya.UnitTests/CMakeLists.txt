set(PROJECT_NAME RadeonProRender.UnitTests)

################################################################################
# Source groups
################################################################################
set(Header_Files
    "../FireRender.Maya.Src/FireRenderPortableUtils.h"
    "stdafx.h"
    "targetver.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "stdafx.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE fireRenderUnitTests)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
################################################################################
# Output directory
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_RELEASE     "${CMAKE_SOURCE_DIR}//bin/RadeonProRender.UnitTests/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/"
    OUTPUT_DIRECTORY_RELEASE2022 "${CMAKE_SOURCE_DIR}//bin/RadeonProRender.UnitTests/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/"
    OUTPUT_DIRECTORY_RELEASE2023 "${CMAKE_SOURCE_DIR}//bin/RadeonProRender.UnitTests/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/"
    OUTPUT_DIRECTORY_RELEASE2024 "${CMAKE_SOURCE_DIR}//bin/RadeonProRender.UnitTests/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE     "TRUE"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE2022 "TRUE"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE2023 "TRUE"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE2024 "TRUE"
)
################################################################################
# Include directories
################################################################################
target_include_directories(${PROJECT_NAME} PUBLIC
    "$ENV{VCInstallDir}UnitTest/include"
)

################################################################################
# Compile definitions
################################################################################
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:"
        "_DEBUG"
    ">"
    "$<$<CONFIG:Debug2022>:"
        "WIN32;"
        "_DEBUG"
    ">"
    "$<$<CONFIG:Debug2023>:"
        "WIN32;"
        "_DEBUG"
    ">"
    "$<$<CONFIG:Debug2024>:"
        "WIN32;"
        "_DEBUG"
    ">"
    "$<$<CONFIG:Release>:"
        "NDEBUG"
    ">"
    "$<$<CONFIG:Release2022>:"
        "NDEBUG"
    ">"
    "$<$<CONFIG:Release2023>:"
        "NDEBUG"
    ">"
    "$<$<CONFIG:Release2024>:"
        "NDEBUG"
    ">"
    "UNICODE;"
    "_UNICODE"
)

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /Od
        >
        $<$<CONFIG:Debug2022>:
            /Od
        >
        $<$<CONFIG:Debug2023>:
            /Od
        >
        $<$<CONFIG:Debug2024>:
            /Od
        >
        $<$<CONFIG:Release>:
            /O2;
            /Oi;
            /Gy
        >
        $<$<CONFIG:Release2022>:
            /O2;
            /Oi;
            /Gy
        >
        $<$<CONFIG:Release2023>:
            /O2;
            /Oi;
            /Gy
        >
        $<$<CONFIG:Release2024>:
            /O2;
            /Oi;
            /Gy
        >
        /W3;
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /Y-
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:
            /OPT:REF;
            /OPT:ICF
        >
        $<$<CONFIG:Release2022>:
            /OPT:REF;
            /OPT:ICF
        >
        $<$<CONFIG:Release2023>:
            /OPT:REF;
            /OPT:ICF
        >
        $<$<CONFIG:Release2024>:
            /OPT:REF;
            /OPT:ICF
        >
        /DEBUG:FULL;
        /SUBSYSTEM:WINDOWS;
        /INCREMENTAL
    )
endif()

target_link_directories(${PROJECT_NAME} PUBLIC
    "$ENV{VCInstallDir}UnitTest/lib"
)

