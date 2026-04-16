# Test source files (C++)
set(TEST_SOURCES
  test_phoneme_mapping.cpp
  test_custom_dictionary.cpp
  test_english_phonemize.cpp
  test_language_detector.cpp
  test_phoneme_parser.cpp
  test_openjtalk_security.cpp
  test_openjtalk_error.cpp
  test_audio_queue.cpp
  test_piper_core.cpp
  test_streaming.cpp
)

# Source files under test (C++) - compiled directly since they're normally in a shared lib
set(SUT_CXX_SOURCES
  ${CMAKE_SOURCE_DIR}/src/piper_core/chinese_phonemize.cpp
  ${CMAKE_SOURCE_DIR}/src/piper_core/custom_dictionary.cpp
  ${CMAKE_SOURCE_DIR}/src/piper_core/english_phonemize.cpp
  ${CMAKE_SOURCE_DIR}/src/piper_core/language_detector.cpp
  ${CMAKE_SOURCE_DIR}/src/piper_core/multilingual_phonemize.cpp
  ${CMAKE_SOURCE_DIR}/src/piper_core/piper_test_utils.cpp
  ${CMAKE_SOURCE_DIR}/src/piper_core/phoneme_parser.cpp
)

# Source files under test (C)
set(SUT_C_SOURCES
  ${CMAKE_SOURCE_DIR}/src/piper_core/openjtalk_error.c
  ${CMAKE_SOURCE_DIR}/src/piper_core/openjtalk_security.c
)

add_executable(piper_plus_tests
  ${TEST_SOURCES}
  ${SUT_CXX_SOURCES}
  ${SUT_C_SOURCES}
)

target_include_directories(piper_plus_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/piper_core
  ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(piper_plus_tests PRIVATE
  GTest::gtest_main
)

# Platform-specific threading
if(UNIX AND NOT APPLE)
  target_link_libraries(piper_plus_tests PRIVATE pthread)
endif()

# CTest registration
include(GoogleTest)
gtest_discover_tests(piper_plus_tests)

if(Python3_EXECUTABLE)
  add_test(
    NAME multilingual_capability_matrix_codegen_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/sync_multilingual_capability_matrix.py
      --check
  )
endif()
