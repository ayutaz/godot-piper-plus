include(ExternalProject)

set(PIPER_PHONEMIZE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pi")

ExternalProject_Add(
  piper_phonemize_external
  PREFIX "${CMAKE_CURRENT_BINARY_DIR}/p"
  URL "https://github.com/rhasspy/piper-phonemize/archive/refs/heads/master.zip"
  DOWNLOAD_NO_PROGRESS TRUE
  TIMEOUT 600
  TLS_VERIFY ON
  DOWNLOAD_TRIES 3
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=${PIPER_PHONEMIZE_DIR}
    -DBUILD_SHARED_LIBS:BOOL=OFF
    -DESPEAK_NG_BUILD_SHARED:BOOL=OFF
    ${EXTERNAL_CMAKE_ARGS}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG>
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target install --config $<CONFIG>
)
