# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\MacDockShell_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\MacDockShell_autogen.dir\\ParseCache.txt"
  "MacDockShell_autogen"
  )
endif()
