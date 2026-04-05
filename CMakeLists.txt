cmake_minimum_required(VERSION 3.20)
project(godot-piper-plus CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# MSVC compile options
# Use static CRT (/MT for Release, /MTd for Debug) to match godot-cpp and ExternalProject libs.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options(/FS)
  add_compile_options(/EHsc)
  add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

# External CMake args propagation for ExternalProject builds
set(EXTERNAL_CMAKE_ARGS "")
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # iOS cross-compilation: pass toolchain file to ExternalProject for proper sysroot setup.
  # If the ios.toolchain.cmake was used (CMAKE_TOOLCHAIN_FILE is set and PLATFORM is defined),
  # propagate it; otherwise fall back to CMAKE_SYSTEM_NAME=iOS with explicit sysroot.
  if(CMAKE_TOOLCHAIN_FILE AND DEFINED PLATFORM)
    set(EXTERNAL_CMAKE_ARGS
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
      -DPLATFORM=${PLATFORM}
      -DDEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}
    )
  else()
    set(EXTERNAL_CMAKE_ARGS
      -DCMAKE_SYSTEM_NAME=iOS
      -DCMAKE_OSX_ARCHITECTURES=arm64
      -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
    )
  endif()
elseif(APPLE)
  if(CMAKE_OSX_ARCHITECTURES)
    set(EXTERNAL_CMAKE_ARGS -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES})
  else()
    set(EXTERNAL_CMAKE_ARGS -DCMAKE_OSX_ARCHITECTURES=arm64)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
  # Android cross-compilation: pass NDK toolchain to ExternalProject
  set(EXTERNAL_CMAKE_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DANDROID_ABI=${ANDROID_ABI}
    -DANDROID_PLATFORM=${ANDROID_PLATFORM}
  )
else()
  set(EXTERNAL_CMAKE_ARGS "")
endif()

# ---- godot-cpp ----
# Default to editor-compatible binaries for local multi-config builds.
# CI/package builds can still override this explicitly with -DGODOTCPP_TARGET=template_release.
if(NOT DEFINED GODOTCPP_TARGET OR GODOTCPP_TARGET STREQUAL "")
  if(CMAKE_CONFIGURATION_TYPES)
    set(_PIPER_PLUS_DEFAULT_GODOTCPP_TARGET "template_debug")
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(_PIPER_PLUS_DEFAULT_GODOTCPP_TARGET "template_debug")
  else()
    set(_PIPER_PLUS_DEFAULT_GODOTCPP_TARGET "template_release")
  endif()
  set(GODOTCPP_TARGET "${_PIPER_PLUS_DEFAULT_GODOTCPP_TARGET}" CACHE STRING "" FORCE)
endif()
set_property(CACHE GODOTCPP_TARGET PROPERTY STRINGS "template_debug;template_release;editor")

# ExternalProject configuration should follow the selected Godot artifact flavor.
# Multi-config generators otherwise install Release libs by default, which breaks
# local Debug/template_debug builds on Windows.
if(CMAKE_CONFIGURATION_TYPES)
  if(GODOTCPP_TARGET STREQUAL "template_debug" OR GODOTCPP_TARGET STREQUAL "editor")
    set(EXTERNAL_BUILD_TYPE "Debug")
  else()
    set(EXTERNAL_BUILD_TYPE "Release")
  endif()
elseif(CMAKE_BUILD_TYPE)
  set(EXTERNAL_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
  set(EXTERNAL_BUILD_TYPE "Release")
endif()

if(MSVC)
  if(EXTERNAL_BUILD_TYPE STREQUAL "Debug")
    set(_PIPER_PLUS_EXTERNAL_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
  else()
    set(_PIPER_PLUS_EXTERNAL_MSVC_RUNTIME_LIBRARY "MultiThreaded")
  endif()

  list(APPEND EXTERNAL_CMAKE_ARGS
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=${_PIPER_PLUS_EXTERNAL_MSVC_RUNTIME_LIBRARY}"
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
  )
endif()

if(CMAKE_MAKE_PROGRAM)
  list(APPEND EXTERNAL_CMAKE_ARGS
    "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
  )
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
  src/piper_core/piper_test_utils.cpp
  src/piper_core/english_phonemize.cpp
  src/piper_core/openjtalk_phonemize.cpp
  src/piper_core/custom_dictionary.cpp
  src/piper_core/language_detector.cpp
  src/piper_core/multilingual_phonemize.cpp
  src/piper_core/phoneme_parser.cpp
)

set(PIPER_PLUS_C_SOURCES
  src/piper_core/openjtalk_wrapper.c
  src/piper_core/openjtalk_api.c
  src/piper_core/openjtalk_error.c
  src/piper_core/openjtalk_dictionary_manager.c
  src/piper_core/openjtalk_security.c
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  add_library(piper_plus STATIC ${PIPER_PLUS_SOURCES} ${PIPER_PLUS_C_SOURCES})
else()
  add_library(piper_plus SHARED ${PIPER_PLUS_SOURCES} ${PIPER_PLUS_C_SOURCES})
endif()

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
elseif(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
  set_target_properties(piper_plus PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
  target_link_libraries(piper_plus PRIVATE pthread dl)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
  # Android: RPATH not applicable, pthread linked automatically by NDK
endif()

# ---- Execution Provider Detection ----
# CoreML: auto-detected on Apple platforms (standard ORT builds include CoreML EP)
# NNAPI: auto-detected on Android (standard ORT AAR includes NNAPI EP)
# DirectML: opt-in via PIPER_PLUS_WITH_DIRECTML (requires DirectML-enabled ONNX Runtime)
option(PIPER_PLUS_WITH_DIRECTML "Enable DirectML execution provider (Windows only, requires DirectML-enabled ONNX Runtime)" OFF)

if(APPLE)
  target_compile_definitions(piper_plus PRIVATE PIPER_PLUS_HAS_COREML)
  message(STATUS "Execution Provider: CoreML enabled (Apple platform)")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_compile_definitions(piper_plus PRIVATE PIPER_PLUS_HAS_NNAPI)
  message(STATUS "Execution Provider: NNAPI enabled (Android platform)")
endif()

if(WIN32 AND PIPER_PLUS_WITH_DIRECTML)
  target_compile_definitions(piper_plus PRIVATE PIPER_PLUS_HAS_DIRECTML)
  message(STATUS "Execution Provider: DirectML enabled")
endif()

# Output naming: use godot-cpp's suffix for consistent naming
# Produces: libpiper_plus.<platform>.<target>.<arch>.<ext>
get_target_property(_GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX_GENEX)

set_target_properties(piper_plus PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "piper_plus.${_GODOTCPP_SUFFIX}"
  ARCHIVE_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_SOURCE_DIR}/addons/piper_plus/bin>"
  LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_SOURCE_DIR}/addons/piper_plus/bin>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_SOURCE_DIR}/addons/piper_plus/bin>"
)

set(_PIPER_PLUS_RUNTIME_DEPENDENCY_FILES "")
set(_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_SRC "")
set(_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_DST "")
if(WIN32 AND ONNXRUNTIME_DLL)
  get_filename_component(_PIPER_PLUS_ORT_LIB_DIR "${ONNXRUNTIME_DLL}" DIRECTORY)
  file(GLOB _PIPER_PLUS_RUNTIME_DEPENDENCY_FILES
    "${_PIPER_PLUS_ORT_LIB_DIR}/onnxruntime*.dll"
  )
  set(_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_SRC "onnxruntime.dll")
  set(_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_DST "onnxruntime.windows.x86_64.dll")
elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  get_filename_component(_PIPER_PLUS_ORT_LIB_DIR "${ONNXRUNTIME_LIB}" DIRECTORY)
  file(GLOB _PIPER_PLUS_RUNTIME_DEPENDENCY_FILES
    "${_PIPER_PLUS_ORT_LIB_DIR}/libonnxruntime*.dylib"
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android" AND ONNXRUNTIME_DIR)
  file(GLOB _PIPER_PLUS_RUNTIME_DEPENDENCY_FILES
    "${ONNXRUNTIME_DIR}/lib/libonnxruntime*.so"
  )
  set(_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_SRC "libonnxruntime.so")
  set(_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_DST "libonnxruntime.android.arm64.so")
elseif(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
  get_filename_component(_PIPER_PLUS_ORT_LIB_DIR "${ONNXRUNTIME_LIB}" DIRECTORY)
  file(GLOB _PIPER_PLUS_RUNTIME_DEPENDENCY_FILES
    "${_PIPER_PLUS_ORT_LIB_DIR}/libonnxruntime*.so*"
  )
endif()

set(_PIPER_PLUS_TEST_PROJECT_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/project/addons/piper_plus/bin")
set(_PIPER_PLUS_TEST_PROJECT_ADDON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/project/addons/piper_plus")

add_custom_command(TARGET piper_plus POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_PIPER_PLUS_TEST_PROJECT_ADDON_DIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_PIPER_PLUS_TEST_PROJECT_BIN_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/addons/piper_plus/piper_plus.gdextension"
    "${_PIPER_PLUS_TEST_PROJECT_ADDON_DIR}/piper_plus.gdextension"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:piper_plus>"
    "${_PIPER_PLUS_TEST_PROJECT_BIN_DIR}/$<TARGET_FILE_NAME:piper_plus>"
  VERBATIM
)

if(MSVC)
  add_custom_command(TARGET piper_plus POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      "-DSRC=$<TARGET_PDB_FILE:piper_plus>"
      "-DDST=${_PIPER_PLUS_TEST_PROJECT_BIN_DIR}/$<TARGET_PDB_FILE_NAME:piper_plus>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyIfExists.cmake"
    VERBATIM
  )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  add_custom_command(TARGET piper_plus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:godot-cpp>"
      "$<TARGET_FILE_DIR:piper_plus>/$<TARGET_FILE_NAME:godot-cpp>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:godot-cpp>"
      "${_PIPER_PLUS_TEST_PROJECT_BIN_DIR}/$<TARGET_FILE_NAME:godot-cpp>"
    VERBATIM
  )

  set(_PIPER_PLUS_IOS_DEPENDENCY_FILES
    "${ONNXRUNTIME_LIB}"
    "${OPENJTALK_DIR}/lib/libopenjtalk.a"
    "${OPENJTALK_DIR}/lib/libmecab.a"
    "${HTS_ENGINE_DIR}/lib/libHTSEngine.a"
  )

  foreach(_PIPER_PLUS_IOS_DEPENDENCY_FILE IN LISTS _PIPER_PLUS_IOS_DEPENDENCY_FILES)
    if(EXISTS "${_PIPER_PLUS_IOS_DEPENDENCY_FILE}")
      get_filename_component(_PIPER_PLUS_IOS_DEPENDENCY_NAME
        "${_PIPER_PLUS_IOS_DEPENDENCY_FILE}" NAME)
      add_custom_command(TARGET piper_plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${_PIPER_PLUS_IOS_DEPENDENCY_FILE}"
          "$<TARGET_FILE_DIR:piper_plus>/${_PIPER_PLUS_IOS_DEPENDENCY_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${_PIPER_PLUS_IOS_DEPENDENCY_FILE}"
          "${_PIPER_PLUS_TEST_PROJECT_BIN_DIR}/${_PIPER_PLUS_IOS_DEPENDENCY_NAME}"
        VERBATIM
      )
    endif()
  endforeach()
endif()

foreach(_PIPER_PLUS_RUNTIME_DEPENDENCY_FILE IN LISTS _PIPER_PLUS_RUNTIME_DEPENDENCY_FILES)
  if(EXISTS "${_PIPER_PLUS_RUNTIME_DEPENDENCY_FILE}")
    get_filename_component(_PIPER_PLUS_RUNTIME_DEPENDENCY_NAME
      "${_PIPER_PLUS_RUNTIME_DEPENDENCY_FILE}" NAME)
    add_custom_command(TARGET piper_plus POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_PIPER_PLUS_RUNTIME_DEPENDENCY_FILE}"
        "$<TARGET_FILE_DIR:piper_plus>/${_PIPER_PLUS_RUNTIME_DEPENDENCY_NAME}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_PIPER_PLUS_RUNTIME_DEPENDENCY_FILE}"
        "${_PIPER_PLUS_TEST_PROJECT_BIN_DIR}/${_PIPER_PLUS_RUNTIME_DEPENDENCY_NAME}"
      VERBATIM
    )

    if(_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_DST AND
       _PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_SRC AND
       _PIPER_PLUS_RUNTIME_DEPENDENCY_NAME STREQUAL _PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_SRC)
      add_custom_command(TARGET piper_plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${_PIPER_PLUS_RUNTIME_DEPENDENCY_FILE}"
          "$<TARGET_FILE_DIR:piper_plus>/${_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_DST}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${_PIPER_PLUS_RUNTIME_DEPENDENCY_FILE}"
          "${_PIPER_PLUS_TEST_PROJECT_BIN_DIR}/${_PIPER_PLUS_RUNTIME_DEPENDENCY_ALIAS_DST}"
        VERBATIM
      )
    endif()
  endif()
endforeach()

# ---- Documentation ----
# Python3 is required by godot-cpp's target_doc_sources() to generate doc_source.cpp.
# godot-cpp calls find_package(Python3) in its subdirectory scope, but the result
# is not visible here. We must find Python3 in this scope so that the
# add_custom_command inside generate_doc_source() receives a valid Python3_EXECUTABLE.
find_package(Python3 3.4 REQUIRED)
file(GLOB DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc_classes/*.xml")
if(DOC_FILES)
  target_doc_sources(piper_plus "${DOC_FILES}")
endif()

# ---- Tests ----
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  # Windows: prevent dynamic CRT conflicts
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(tests)
endif()
