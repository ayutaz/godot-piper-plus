if(NOT DEFINED OUTPUT OR OUTPUT STREQUAL "")
  message(FATAL_ERROR "CombineStaticArchives.cmake requires OUTPUT")
endif()

if(NOT DEFINED PRIMARY OR PRIMARY STREQUAL "")
  message(FATAL_ERROR "CombineStaticArchives.cmake requires PRIMARY")
endif()

if(NOT DEFINED INPUTS OR INPUTS STREQUAL "")
  message(FATAL_ERROR "CombineStaticArchives.cmake requires INPUTS")
endif()

string(REPLACE "|" ";" _combine_inputs "${INPUTS}")

set(_all_inputs "${PRIMARY}")
list(APPEND _all_inputs ${_combine_inputs})

foreach(_input IN LISTS _all_inputs)
  if(NOT EXISTS "${_input}")
    message(FATAL_ERROR "Static archive input does not exist: ${_input}")
  endif()
endforeach()

file(REMOVE "${OUTPUT}")

execute_process(
  COMMAND xcrun libtool -static -o "${OUTPUT}" ${_all_inputs}
  RESULT_VARIABLE _combine_result
  OUTPUT_VARIABLE _combine_stdout
  ERROR_VARIABLE _combine_stderr
)

if(NOT _combine_result EQUAL 0)
  message(FATAL_ERROR
    "Failed to combine static archives into ${OUTPUT}\n"
    "stdout:\n${_combine_stdout}\n"
    "stderr:\n${_combine_stderr}")
endif()
