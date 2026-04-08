# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\DrawTools_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\DrawTools_autogen.dir\\ParseCache.txt"
  "DrawTools_autogen"
  )
endif()
