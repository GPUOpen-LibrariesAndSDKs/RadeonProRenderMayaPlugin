set(MAYA_LOCATION "/usr/autodesk/maya${MAYA_VERSION}/")
set(MAYA_COMPILE_DEFINITIONS "REQURE_IOSTREAM; _BOOL; LINUX; MAYA2016; MAYA_PLUGIN")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

find_path(MAYA_LIBRARY_DIR libOpenMaya.so
    PATHS
        ${MAYA_LOCATION}
    PATH_SUFFIXES
        "lib/"
    DOC "Maya library path"
)
find_path(MAYA_INCLUDE_DIR maya/MFn.h
    PATHS
        ${MAYA_LOCATION}
    PATH_SUFFIXES
        "include/"
    DOC "Maya include path"
)

set(_MAYA_LIBRARIES OpenMaya OpenMayaAnim OpenMayaFX OpenMayaRender OpenMayaUI Foundation)
foreach(MAYA_LIB ${_MAYA_LIBRARIES})
    find_library(MAYA_${MAYA_LIB}_LIBRARY NAMES ${MAYA_LIB} PATHS ${MAYA_LIBRARY_DIR} NO_DEFAULT_PATH)
    set(MAYA_LIBRARIES ${MAYA_LIBRARIES} ${MAYA_${MAYA_LIB}_LIBRARY})
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Maya DEFAULT_MSG MAYA_INCLUDE_DIR MAYA_LIBRARY_DIR)

function(MAYA_PLUGIN _target)
    set_target_properties(${_target} PROPERTIES
        COMPILE_DEFINITIONS "${MAYA_COMPILE_DEFINITIONS}"
        PREFIX ""
        SUFFIX ".so"
    )
endfunction()

