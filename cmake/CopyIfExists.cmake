if(NOT DEFINED SRC OR NOT DEFINED DST)
  message(FATAL_ERROR "CopyIfExists.cmake requires SRC and DST.")
endif()

if(EXISTS "${SRC}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC}" "${DST}"
    RESULT_VARIABLE COPY_IF_EXISTS_RESULT
  )

  if(NOT COPY_IF_EXISTS_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to copy '${SRC}' to '${DST}'.")
  endif()
endif()
