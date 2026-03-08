cmake_minimum_required(VERSION 3.20)
project(godot-piper-plus CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
cmake_policy(SET CMP0135 NEW)

# MSVC runtime library settings
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
  cmake_policy(SET CMP0091 NEW)
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options(/EHsc)
  add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

# External CMake args propagation for ExternalProject builds
if(APPLE)
  if(CMAKE_OSX_ARCHITECTURES)
    set(EXTERNAL_CMAKE_ARGS -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES})
  else()
    set(EXTERNAL_CMAKE_ARGS -DCMAKE_OSX_ARCHITECTURES=arm64)
  endif()
elseif(MSVC)
  set(EXTERNAL_CMAKE_ARGS
    -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
  )
else()
  set(EXTERNAL_CMAKE_ARGS "")
endif()

# ---- godot-cpp ----
# Map CMAKE_BUILD_TYPE to godot-cpp target
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(GODOTCPP_TARGET "template_debug" CACHE STRING "" FORCE)
else()
  set(GODOTCPP_TARGET "template_release" CACHE STRING "" FORCE)
endif()
add_subdirectory(thirdparty/godot-cpp)

# ---- External dependencies ----
# Japanese G2P: OpenJTalk (Modified BSD) + HTSEngine (Modified BSD)
# English G2P: Flite (BSD-like) - TODO M2
# Inference: ONNX Runtime (MIT, dynamic link)
# NOTE: espeak-ng/piper-phonemize removed due to GPL-3.0 license contamination
include(ExternalProject)
include(cmake/HTSEngine.cmake)
include(cmake/OpenJTalk.cmake)
include(cmake/FindOnnxRuntime.cmake)

# ---- GDExtension library ----
set(PIPER_PLUS_SOURCES
  src/register_types.cpp
  src/piper_tts.cpp
  src/piper_core/piper.cpp
  src/piper_core/openjtalk_phonemize.cpp
  src/piper_core/custom_dictionary.cpp
  src/piper_core/phoneme_parser.cpp
)

set(PIPER_PLUS_C_SOURCES
  src/piper_core/openjtalk_wrapper.c
  src/piper_core/openjtalk_error.c
  src/piper_core/openjtalk_dictionary_manager.c
  src/piper_core/openjtalk_security.c
)

add_library(piper_plus SHARED ${PIPER_PLUS_SOURCES} ${PIPER_PLUS_C_SOURCES})

target_include_directories(piper_plus PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/piper_core
  ${OPENJTALK_DIR}/include
  ${HTS_ENGINE_DIR}/include
  ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(piper_plus PRIVATE godot-cpp)

# Static libraries: OpenJTalk, HTSEngine
# Dynamic libraries: ONNX Runtime
if(WIN32)
  target_link_libraries(piper_plus PRIVATE
    ${OPENJTALK_DIR}/lib/openjtalk.lib
    ${OPENJTALK_DIR}/lib/mecab.lib
    ${HTS_ENGINE_DIR}/lib/HTSEngine.lib
    ${ONNXRUNTIME_LIB}
  )
else()
  target_link_libraries(piper_plus PRIVATE
    ${OPENJTALK_DIR}/lib/libopenjtalk.a
    ${OPENJTALK_DIR}/lib/libmecab.a
    ${HTS_ENGINE_DIR}/lib/libHTSEngine.a
    ${ONNXRUNTIME_LIB}
  )
endif()

# Ensure external deps are built before our library
add_dependencies(piper_plus
  hts_engine_external
  openjtalk_external
)

# Platform-specific link flags
if(APPLE)
  set_target_properties(piper_plus PROPERTIES
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
elseif(UNIX AND NOT APPLE)
  set_target_properties(piper_plus PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
  target_link_libraries(piper_plus PRIVATE pthread)
endif()

# Output naming: use godot-cpp's suffix for consistent naming
# Produces: libpiper_plus.<platform>.<target>.<arch>.<ext>
get_target_property(_GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX_GENEX)

set_target_properties(piper_plus PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "piper_plus.${_GODOTCPP_SUFFIX}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/addons/piper_plus/bin"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/addons/piper_plus/bin"
)
