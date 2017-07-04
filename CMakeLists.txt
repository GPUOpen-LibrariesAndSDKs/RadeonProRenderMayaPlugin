project(RadeonProRender)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/fireRenderLinux/cmake_modules)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath,'$ORIGIN/../../bin'" )

include_directories("/usr/include/OpenImageIO")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/RadeonProRender SDK/Linux-CentOS/inc/Math")

add_subdirectory(fireRender)

if(UNIX)
    add_subdirectory(fireRenderLinux)
endif(UNIX)
