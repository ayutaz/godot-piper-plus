set(_hts_engine_source_dir "${HTS_ENGINE_SOURCE_DIR}")
set(_piper_plus_source_dir "${PIPER_PLUS_SOURCE_DIR}")

if(NOT EXISTS "${_hts_engine_source_dir}/lib/HTS_misc.c")
  message(FATAL_ERROR "HTSEngine source file not found: ${_hts_engine_source_dir}/lib/HTS_misc.c")
endif()

file(COPY_FILE
  "${_piper_plus_source_dir}/cmake/HTSEngine_CMakeLists.txt"
  "${_hts_engine_source_dir}/CMakeLists.txt"
  ONLY_IF_DIFFERENT
)

set(_misc_file "${_hts_engine_source_dir}/lib/HTS_misc.c")
file(READ "${_misc_file}" _misc_source)

set(_needle [=[      fpos_t pos;
      fgetpos((FILE *) fp->pointer, &pos);
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__ANDROID__)
      return (size_t) pos;
#else
      return (size_t) pos.__pos;
#endif                          /* _WIN32 || __CYGWIN__ || __APPLE__ || __ANDROID__ */]=])

set(_replacement [=[#if defined(__EMSCRIPTEN__)
      long position = ftell((FILE *) fp->pointer);
      return position < 0 ? (size_t) 0 : (size_t) position;
#else
      fpos_t pos;
      fgetpos((FILE *) fp->pointer, &pos);
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__ANDROID__)
      return (size_t) pos;
#else
      return (size_t) pos.__pos;
#endif                          /* _WIN32 || __CYGWIN__ || __APPLE__ || __ANDROID__ */
#endif                          /* __EMSCRIPTEN__ */]=])

string(REPLACE "${_needle}" "${_replacement}" _patched_misc_source "${_misc_source}")

if(_patched_misc_source STREQUAL _misc_source)
  message(FATAL_ERROR "Failed to patch HTS_misc.c for Emscripten portability")
endif()

file(WRITE "${_misc_file}" "${_patched_misc_source}")
