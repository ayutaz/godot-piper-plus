# FindOnnxRuntime.cmake - Find pre-built ONNX Runtime
#
# Usage: set ONNXRUNTIME_DIR to the pre-built package root before calling.
# If not set, attempts to auto-download v1.24.0.
#
# Sets:
#   ONNXRUNTIME_FOUND
#   ONNXRUNTIME_INCLUDE_DIR
#   ONNXRUNTIME_LIB
#   ONNXRUNTIME_DLL (Windows only)

set(ONNXRUNTIME_VERSION "1.24.3" CACHE STRING "ONNX Runtime version")
set(ONNXRUNTIME_WEB_STATIC_LIB "" CACHE FILEPATH "Path to libonnxruntime_webassembly.a for Web builds")

# Determine platform suffix for download
if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_ORT_PLATFORM "win-x64")
  else()
    set(_ORT_PLATFORM "win-x86")
  endif()
  set(_ORT_EXT "zip")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # iOS builds require a pre-built static ONNX Runtime; no auto-download available.
  # Set ONNXRUNTIME_DIR to the pre-built package root (with lib/ and include/).
  message(STATUS "iOS detected: ONNX Runtime auto-download is not available. Set ONNXRUNTIME_DIR manually.")
elseif(APPLE)
  if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(_ORT_PLATFORM "osx-arm64")
  else()
    set(_ORT_PLATFORM "osx-x86_64")
  endif()
  set(_ORT_EXT "tgz")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(_ORT_PLATFORM "linux-aarch64")
  else()
    set(_ORT_PLATFORM "linux-x64")
  endif()
  set(_ORT_EXT "tgz")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(_ORT_PLATFORM "android")
  set(_ORT_EXT "aar")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  # Web builds require a pre-built static ONNX Runtime; no auto-download available.
  # Set ONNXRUNTIME_DIR to the package root or ONNXRUNTIME_WEB_STATIC_LIB directly.
  message(STATUS "Web detected: ONNX Runtime auto-download is not available. Set ONNXRUNTIME_DIR or ONNXRUNTIME_WEB_STATIC_LIB manually.")
endif()

# Search user-specified dir first
if(ONNXRUNTIME_DIR)
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(_ORT_WEB_STATIC_LIB_CANDIDATE "${ONNXRUNTIME_DIR}/lib/libonnxruntime_webassembly.a")
    if(EXISTS "${_ORT_WEB_STATIC_LIB_CANDIDATE}")
      set(ONNXRUNTIME_LIB "${_ORT_WEB_STATIC_LIB_CANDIDATE}")
    endif()

    find_library(ONNXRUNTIME_LIB
      NAMES onnxruntime_webassembly onnxruntime
      PATHS "${ONNXRUNTIME_DIR}/lib"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )
  else()
    find_library(ONNXRUNTIME_LIB
      NAMES onnxruntime
      PATHS "${ONNXRUNTIME_DIR}/lib"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )
  endif()
  find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_DIR}/include"
    PATH_SUFFIXES "" "onnxruntime/core/session"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
  )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND ONNXRUNTIME_WEB_STATIC_LIB AND NOT ONNXRUNTIME_LIB)
  set(ONNXRUNTIME_LIB "${ONNXRUNTIME_WEB_STATIC_LIB}")
endif()

# Auto-download if not found
if(NOT ONNXRUNTIME_LIB OR NOT ONNXRUNTIME_INCLUDE_DIR)
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(ONNXRUNTIME_FOUND FALSE)
    message(FATAL_ERROR "ONNX Runtime Web static library not found. Set ONNXRUNTIME_DIR or ONNXRUNTIME_WEB_STATIC_LIB for Emscripten builds.")
  endif()

  set(_ORT_DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/onnxruntime")
  set(_ORT_ARCHIVE "${_ORT_DOWNLOAD_DIR}/onnxruntime.${_ORT_EXT}")

  # Android AAR is hosted on Maven Central, not GitHub Releases
  if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(_ORT_URL "https://repo1.maven.org/maven2/com/microsoft/onnxruntime/onnxruntime-android/${ONNXRUNTIME_VERSION}/onnxruntime-android-${ONNXRUNTIME_VERSION}.aar")
  else()
    set(_ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-${_ORT_PLATFORM}-${ONNXRUNTIME_VERSION}.${_ORT_EXT}")
  endif()
  set(_ORT_ROOT "${_ORT_DOWNLOAD_DIR}/onnxruntime-${_ORT_PLATFORM}-${ONNXRUNTIME_VERSION}")

  if(NOT EXISTS "${_ORT_ROOT}")
    message(STATUS "Downloading ONNX Runtime v${ONNXRUNTIME_VERSION} for ${_ORT_PLATFORM}...")
    file(MAKE_DIRECTORY "${_ORT_DOWNLOAD_DIR}")

    if(NOT EXISTS "${_ORT_ARCHIVE}")
      file(DOWNLOAD
        "${_ORT_URL}"
        "${_ORT_ARCHIVE}"
        SHOW_PROGRESS
        STATUS _download_status
        TIMEOUT 300
      )
      list(GET _download_status 0 _status_code)
      if(NOT _status_code EQUAL 0)
        message(FATAL_ERROR "Failed to download ONNX Runtime from ${_ORT_URL}")
      endif()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Android")
      # AAR is a zip; extract native .so and headers, then create standard layout
      file(MAKE_DIRECTORY "${_ORT_ROOT}/lib" "${_ORT_ROOT}/include")
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf "${_ORT_ARCHIVE}"
        WORKING_DIRECTORY "${_ORT_DOWNLOAD_DIR}"
        RESULT_VARIABLE _extract_result
      )
      if(NOT _extract_result EQUAL 0)
        message(FATAL_ERROR "Failed to extract ONNX Runtime AAR")
      endif()
      file(GLOB _AAR_HEADERS "${_ORT_DOWNLOAD_DIR}/headers/*.h")
      file(COPY ${_AAR_HEADERS} DESTINATION "${_ORT_ROOT}/include")
      file(COPY "${_ORT_DOWNLOAD_DIR}/jni/arm64-v8a/libonnxruntime.so" DESTINATION "${_ORT_ROOT}/lib")
    else()
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf "${_ORT_ARCHIVE}"
        WORKING_DIRECTORY "${_ORT_DOWNLOAD_DIR}"
        RESULT_VARIABLE _extract_result
      )
      if(NOT _extract_result EQUAL 0)
        message(FATAL_ERROR "Failed to extract ONNX Runtime")
      endif()
    endif()
  endif()

  set(ONNXRUNTIME_DIR "${_ORT_ROOT}")

  find_library(ONNXRUNTIME_LIB
    NAMES onnxruntime
    PATHS "${ONNXRUNTIME_DIR}/lib"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
  )
  find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_DIR}/include"
    PATH_SUFFIXES "" "onnxruntime/core/session"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
  )
endif()

# Windows DLL
if(WIN32 AND ONNXRUNTIME_LIB)
  get_filename_component(_ORT_LIB_DIR "${ONNXRUNTIME_LIB}" DIRECTORY)
  find_file(ONNXRUNTIME_DLL
    NAMES onnxruntime.dll
    PATHS "${_ORT_LIB_DIR}" "${ONNXRUNTIME_DIR}/lib"
    NO_DEFAULT_PATH
  )
endif()

if(ONNXRUNTIME_LIB AND ONNXRUNTIME_INCLUDE_DIR)
  set(ONNXRUNTIME_FOUND TRUE)
  message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIB}")
  message(STATUS "ONNX Runtime include: ${ONNXRUNTIME_INCLUDE_DIR}")
else()
  set(ONNXRUNTIME_FOUND FALSE)
  message(FATAL_ERROR "ONNX Runtime not found. Set ONNXRUNTIME_DIR or ensure internet access for auto-download.")
endif()
