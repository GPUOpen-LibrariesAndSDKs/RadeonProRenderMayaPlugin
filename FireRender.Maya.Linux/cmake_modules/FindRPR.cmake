set(RPR_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/RadeonProRender SDK/Linux-CentOS/")
set(RPR_COMPILE_DEFINITIONS "REQURE_IOSTREAM; _BOOL; LINUX; MAYA2016; MAYA_PLUGIN")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

find_path(RPR_LIBRARY_DIR libRadeonProRender64.so
    PATHS
        ${RPR_LOCATION}
    PATH_SUFFIXES
        "lib/"
    DOC "RPR library path"
)
find_path(RPR_INCLUDE_DIR RadeonProRender.h
    PATHS
        ${RPR_LOCATION}
    PATH_SUFFIXES
        "inc/"
    DOC "RPR include path"
)

set(_RPR_LIBRARIES RadeonProRender64 RprLoadStore64 RprSupport64 Tahoe64)
foreach(RPR_LIB ${_RPR_LIBRARIES})
    find_library(RPR_${RPR_LIB}_LIBRARY NAMES ${RPR_LIB} PATHS ${RPR_LIBRARY_DIR} NO_DEFAULT_PATH)
    set(RPR_LIBRARIES ${RPR_LIBRARIES} ${RPR_${RPR_LIB}_LIBRARY})
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RPR DEFAULT_MSG RPR_INCLUDE_DIR RPR_LIBRARY_DIR)

function(RPR_PLUGIN _target)
    set_target_properties(${_target} PROPERTIES
        COMPILE_DEFINITIONS "${RPR_COMPILE_DEFINITIONS}"
        PREFIX ""
        SUFFIX ".so"
    )
endfunction()

