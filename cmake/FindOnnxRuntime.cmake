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

# Determine platform suffix for download
if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_ORT_PLATFORM "win-x64")
  else()
    set(_ORT_PLATFORM "win-x86")
  endif()
  set(_ORT_EXT "zip")
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
endif()

# Search user-specified dir first
if(ONNXRUNTIME_DIR)
  find_library(ONNXRUNTIME_LIB
    NAMES onnxruntime
    PATHS "${ONNXRUNTIME_DIR}/lib"
    NO_DEFAULT_PATH
  )
  find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_DIR}/include"
    NO_DEFAULT_PATH
  )
endif()

# Auto-download if not found
if(NOT ONNXRUNTIME_LIB OR NOT ONNXRUNTIME_INCLUDE_DIR)
  set(_ORT_DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/onnxruntime")
  set(_ORT_ARCHIVE "${_ORT_DOWNLOAD_DIR}/onnxruntime.${_ORT_EXT}")
  set(_ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-${_ORT_PLATFORM}-${ONNXRUNTIME_VERSION}.${_ORT_EXT}")
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

    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xf "${_ORT_ARCHIVE}"
      WORKING_DIRECTORY "${_ORT_DOWNLOAD_DIR}"
      RESULT_VARIABLE _extract_result
    )
    if(NOT _extract_result EQUAL 0)
      message(FATAL_ERROR "Failed to extract ONNX Runtime")
    endif()
  endif()

  set(ONNXRUNTIME_DIR "${_ORT_ROOT}")

  find_library(ONNXRUNTIME_LIB
    NAMES onnxruntime
    PATHS "${ONNXRUNTIME_DIR}/lib"
    NO_DEFAULT_PATH
  )
  find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_DIR}/include"
    NO_DEFAULT_PATH
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
