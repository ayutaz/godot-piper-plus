include(ExternalProject)

set(HTS_ENGINE_VERSION "1.10")
set(HTS_ENGINE_DIR "${CMAKE_CURRENT_BINARY_DIR}/he")

if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Android" OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR
   (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64" AND CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  # CMake build (Windows, Android, iOS, Linux ARM64)
  ExternalProject_Add(
    hts_engine_external
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/h"
    URL "https://downloads.sourceforge.net/project/hts-engine/hts_engine%20API/hts_engine_API-${HTS_ENGINE_VERSION}/hts_engine_API-${HTS_ENGINE_VERSION}.tar.gz"
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${HTS_ENGINE_DIR}
      -DCMAKE_BUILD_TYPE:STRING=${EXTERNAL_BUILD_TYPE}
      -DBUILD_SHARED_LIBS:BOOL=OFF
      -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
      -DHTS_EMBEDDED:BOOL=ON
      -DBUILD_HTS_ENGINE_EXECUTABLE:BOOL=OFF
      ${EXTERNAL_CMAKE_ARGS}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config ${EXTERNAL_BUILD_TYPE}
    INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target install --config ${EXTERNAL_BUILD_TYPE}
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/HTSEngine_CMakeLists.txt
      <SOURCE_DIR>/CMakeLists.txt
    BUILD_BYPRODUCTS
      ${HTS_ENGINE_DIR}/lib/HTSEngine.lib
      ${HTS_ENGINE_DIR}/lib/libHTSEngine.a
  )
else()
  # Autotools build (macOS, Linux x86_64)
  ExternalProject_Add(
    hts_engine_external
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/h"
    URL "https://downloads.sourceforge.net/project/hts-engine/hts_engine%20API/hts_engine_API-${HTS_ENGINE_VERSION}/hts_engine_API-${HTS_ENGINE_VERSION}.tar.gz"
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${HTS_ENGINE_DIR} "CFLAGS=-O2 -fPIC"
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    BUILD_IN_SOURCE 1
    BUILD_BYPRODUCTS ${HTS_ENGINE_DIR}/lib/libHTSEngine.a
  )
endif()
