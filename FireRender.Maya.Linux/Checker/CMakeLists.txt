# We require 2.8
cmake_minimum_required(VERSION 2.8)

set(SOURCE_FILES
  checker.cpp
  formSelect.cpp
  maya_versions.h
  formSelect.h)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ../../ThirdParty/OpenCL/include "../../ThirdParty/RadeonProRender SDK/Linux-CentOS/inc")

add_executable(checker  ${SOURCE_FILES})
set_target_properties(checker PROPERTIES COMPILE_FLAGS "-std=c++14")
target_link_libraries(checker dl pci)
