# Build verification

`.github/workflows/build.yml` compiles `zedas-easy-mode` on Ubuntu 24.04 on
pushes and pull requests targeting that branch. It also supports manual runs.
Its matrix builds and tests both `USE_LUAJIT=ON` and `USE_LUAJIT=OFF`, with
both development packages installed in each job to expose mixed discovery.
The workflow has read-only repository permissions. It requires no secrets,
database service, runtime configuration, game map, or Tibia client.

To reproduce on Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake ninja-build libboost-system-dev libboost-filesystem-dev libpugixml-dev libluajit-5.1-dev luajit liblua5.2-dev lua5.2 default-libmysqlclient-dev libgmp-dev python3
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_LUAJIT=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

`USE_LUAJIT` defaults to `ON` and requires LuaJIT. `OFF` requires standard
Lua **5.2**; it does not silently fall back to LuaJIT or another Lua version.
For standard Lua, configure another build directory with `-DUSE_LUAJIT=OFF`.
No `LUA_LIBRARY`, `LUA_LIBRARIES` or include-directory override is needed.
Use `CMAKE_PREFIX_PATH` for dependencies installed outside system prefixes.

LuaJIT discovery uses separate `LUAJIT_INCLUDE_DIR` / `LUAJIT_LIBRARY` cache
entries. Standard Lua discovery discards the old generic `LUA_*` discovery
cache before searching, so switching the option in an existing build directory
also works after builds from the old mixed-cache implementation. Generic
`LUA_*` overrides are intentionally not preserved; use `CMAKE_PREFIX_PATH`.
CMake prints the chosen headers and libraries, compiles/links an ABI probe and,
on native builds, executes a Lua-only runtime-version check. Cross builds run
the compile/link check without executing target binaries.

The `lua_backend_selection` CTest needs both development packages. It checks
fresh configurations, the default, ON → OFF → ON in one build directory,
legacy mixed cache entries, and rejection of mismatched LuaJIT headers/libraries.
The probe never starts the server or opens a database connection.

Compilation links the database client library but does not connect to a database
or launch the server. Compilation and automated tests do not replace testing
target indicators, movement, item updates, or command bindings in Tibia 7.72.

Keep the Classic checkout/database and tag `v0.5-endless-consumables` unchanged.

The server and native tests share `tfs_engine`. Their entry-point translation
units construct the main Lua environment before `Game` (which owns script
interfaces), and destroy it afterward. Main-state initialization happens after
the environment's derived members are constructed. This avoids dependence on
static-library link order; sanitizer checks cover this startup path.

For additional native memory/undefined-behavior checks, configure another build
with `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"`
and `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"`, compile it, then run
CTest with `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`.
Leak detection is excluded because fixtures retain process-lifetime engine
objects; address, reference-lifetime and undefined-behavior checks remain active.
Record normal and sanitizer results with the feature commits being validated.
