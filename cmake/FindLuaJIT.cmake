# Locate LuaJIT without changing FindLua's LUA_* variables. The two backends
# must remain independent when USE_LUAJIT changes in an existing build tree.
# Defines LUAJIT_INCLUDE_DIR, LUAJIT_LIBRARY, LUAJIT_LIBRARIES and
# LUAJIT_VERSION_STRING (and the usual LuaJIT_FOUND/LUAJIT_FOUND result).

find_path(LUAJIT_INCLUDE_DIR luajit.h
  HINTS ENV LUA_DIR
  PATH_SUFFIXES include/luajit-2.1 include/luajit-2.0 include
  PATHS ~/Library/Frameworks /Library/Frameworks /sw /opt/local /opt/csw /opt
)

find_library(LUAJIT_LIBRARY
  NAMES luajit-5.1
  HINTS ENV LUA_DIR
  PATH_SUFFIXES lib
  PATHS ~/Library/Frameworks /Library/Frameworks /sw /opt/local /opt/csw /opt
)

set(LUAJIT_LIBRARIES "${LUAJIT_LIBRARY}")
if(LUAJIT_LIBRARY AND UNIX AND NOT APPLE)
  find_library(LUAJIT_MATH_LIBRARY m)
  if(LUAJIT_MATH_LIBRARY)
    list(APPEND LUAJIT_LIBRARIES "${LUAJIT_MATH_LIBRARY}")
  endif()
  get_filename_component(_luajit_library_ext "${LUAJIT_LIBRARY}" EXT)
  if(_luajit_library_ext STREQUAL CMAKE_STATIC_LIBRARY_SUFFIX)
    list(APPEND LUAJIT_LIBRARIES ${CMAKE_DL_LIBS})
  endif()
  unset(_luajit_library_ext)
endif()

if(LUAJIT_INCLUDE_DIR AND EXISTS "${LUAJIT_INCLUDE_DIR}/luajit.h")
  file(STRINGS "${LUAJIT_INCLUDE_DIR}/luajit.h" _luajit_version_line
    REGEX "^#define[ \t]+LUAJIT_VERSION[ \t]+\"LuaJIT .+\"")
  string(REGEX REPLACE "^#define[ \t]+LUAJIT_VERSION[ \t]+\"LuaJIT ([^\"]+)\".*"
    "\\1" LUAJIT_VERSION_STRING "${_luajit_version_line}")
  unset(_luajit_version_line)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LuaJIT
  REQUIRED_VARS LUAJIT_LIBRARY LUAJIT_INCLUDE_DIR
  VERSION_VAR LUAJIT_VERSION_STRING)
mark_as_advanced(LUAJIT_INCLUDE_DIR LUAJIT_LIBRARY LUAJIT_MATH_LIBRARY)
