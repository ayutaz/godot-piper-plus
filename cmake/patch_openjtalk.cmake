# Patch script for OpenJTalk ExternalProject
# Usage: cmake -DSOURCE_DIR=<src> -DCMAKE_SOURCE=<cmakelists> -P patch_openjtalk.cmake

# 1. Copy our custom CMakeLists.txt
file(COPY "${CMAKE_SOURCE}" DESTINATION "${SOURCE_DIR}")
get_filename_component(_name "${CMAKE_SOURCE}" NAME)
if(NOT _name STREQUAL "CMakeLists.txt")
  file(RENAME "${SOURCE_DIR}/${_name}" "${SOURCE_DIR}/CMakeLists.txt")
endif()

# 2. Patch mecab/src/dictionary.cpp: remove std::binary_function (removed in C++17)
set(_dict_file "${SOURCE_DIR}/mecab/src/dictionary.cpp")
if(EXISTS "${_dict_file}")
  file(READ "${_dict_file}" _content)
  string(REPLACE
    "struct pair_1st_cmp: public std::binary_function<bool, T1, T2> {"
    "struct pair_1st_cmp {"
    _content "${_content}")
  file(WRITE "${_dict_file}" "${_content}")
endif()
