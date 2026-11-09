# Keep backend discovery separate. In particular, an old FindLuaJIT cache must
# never supply a library to FindLua when the user switches USE_LUAJIT off.
option(USE_LUAJIT "Use LuaJIT instead of standard Lua 5.2" ON)

if(USE_LUAJIT)
  find_package(LuaJIT REQUIRED)
  set(ZEDAS_LUA_BACKEND "LuaJIT")
  set(ZEDAS_LUA_INCLUDE_DIR "${LUAJIT_INCLUDE_DIR}")
  set(ZEDAS_LUA_LIBRARIES "${LUAJIT_LIBRARIES}")
  set(_zedas_lua_backend_headers "#include <luajit.h>\n#if LUA_VERSION_NUM != 501\n#error LuaJIT requires its Lua 5.1 headers\n#endif")
  set(_zedas_lua_backend_call "luaJIT_setmode(state, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_ON);")
else()
  # Older project versions stored LuaJIT paths in all of these generic cache
  # entries. Rediscover standard Lua rather than trusting those stale values.
  # Use CMAKE_PREFIX_PATH for non-system installations, for either backend.
  foreach(_zedas_lua_variable LUA_INCLUDE_DIR LUA_LIBRARY LUA_LIBRARIES
      LUA_MATH_LIBRARY LUA_VERSION_STRING LUA_VERSION_MAJOR LUA_VERSION_MINOR
      LUA_VERSION_PATCH)
    unset(${_zedas_lua_variable} CACHE)
    unset(${_zedas_lua_variable})
  endforeach()
  # The repository's historical FindLua.cmake does not parse modern Lua 5.2
  # version macros reliably. Use CMake's maintained module for standard Lua.
  set(_zedas_saved_module_path "${CMAKE_MODULE_PATH}")
  set(CMAKE_MODULE_PATH "")
  find_package(Lua 5.2 EXACT REQUIRED MODULE)
  set(CMAKE_MODULE_PATH "${_zedas_saved_module_path}")
  set(ZEDAS_LUA_BACKEND "Lua 5.2")
  set(ZEDAS_LUA_INCLUDE_DIR "${LUA_INCLUDE_DIR}")
  set(ZEDAS_LUA_LIBRARIES "${LUA_LIBRARIES}")
  set(_zedas_lua_backend_headers "#if LUA_VERSION_NUM != 502\n#error Standard Lua 5.2 headers are required\n#endif")
  set(_zedas_lua_backend_call "(void)lua_rawlen(state, -1);")
endif()

# Compile and link against the selected headers and libraries on every
# configure. Native builds also execute a tiny Lua-only version check, catching
# a library with overlapping symbols but a different ABI/version. No server,
# database, runtime configuration or game data is involved.
include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES "${ZEDAS_LUA_INCLUDE_DIR}")
set(CMAKE_REQUIRED_LIBRARIES "${ZEDAS_LUA_LIBRARIES}")
set(_zedas_lua_probe "
#include <cstring>
extern \"C\" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
${_zedas_lua_backend_headers}
}
int main() {
  lua_State* state = luaL_newstate();
  if (!state) return 1;
  luaL_openlibs(state);
  lua_getglobal(state, \"_VERSION\");
  const char* version = lua_tostring(state, -1);
  const bool matches = version && std::strcmp(version, LUA_VERSION) == 0;
  ${_zedas_lua_backend_call}
  lua_close(state);
  return matches ? 0 : 2;
}")
unset(ZEDAS_LUA_ABI_LINKS CACHE)
unset(ZEDAS_LUA_ABI_LINKS)
check_cxx_source_compiles("${_zedas_lua_probe}" ZEDAS_LUA_ABI_LINKS)
if(NOT ZEDAS_LUA_ABI_LINKS)
  message(FATAL_ERROR "${ZEDAS_LUA_BACKEND} headers/libraries failed the Lua ABI link check. Check CMAKE_PREFIX_PATH and backend-specific cache paths.")
endif()
if(NOT CMAKE_CROSSCOMPILING)
  unset(ZEDAS_LUA_ABI_RUNS CACHE)
  unset(ZEDAS_LUA_ABI_RUNS)
  check_cxx_source_runs("${_zedas_lua_probe}" ZEDAS_LUA_ABI_RUNS)
  if(NOT ZEDAS_LUA_ABI_RUNS)
    message(FATAL_ERROR "${ZEDAS_LUA_BACKEND} headers/libraries failed the Lua runtime version check. Check library paths and the runtime loader.")
  endif()
endif()
cmake_pop_check_state()
message(STATUS "Zedas Lua backend: ${ZEDAS_LUA_BACKEND}; headers: ${ZEDAS_LUA_INCLUDE_DIR}; libraries: ${ZEDAS_LUA_LIBRARIES}")
