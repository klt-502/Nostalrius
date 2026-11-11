# Build verification

`.github/workflows/build.yml` compiles `zedas-easy-mode` on Ubuntu 24.04 on
pushes and pull requests targeting that branch. It also supports manual runs.
Its matrix builds and tests both `USE_LUAJIT=ON` and `USE_LUAJIT=OFF`, with
both development packages installed in each job to expose mixed discovery.
Both jobs compile serially with `USE_COTIRE=OFF` and check the actual compiler
commands for cotire, forced prefix headers and PCH flags.
The workflow has read-only repository permissions. It requires no secrets,
live database credentials, runtime configuration, game map, or Tibia client.
Broker SQL tests use an isolated MariaDB 10.11 CI service initialized from the
repository schema and additive migration, with public fixture-only credentials.
Local builds need no database; `broker_store` skips unless its explicit fixture
port is provided. See `BROKER_PERSISTENCE.md`.

To reproduce on Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake ninja-build libboost-system-dev libboost-filesystem-dev libpugixml-dev libluajit-5.1-dev luajit liblua5.2-dev lua5.2 default-libmysqlclient-dev libgmp-dev python3
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_LUAJIT=ON -DUSE_COTIRE=OFF
cmake --build build --parallel 1
ctest --test-dir build --output-on-failure
```

`USE_COTIRE` defaults to **OFF**, the supported low-memory path. This bypasses
the cotire module entirely: no generated prefix-header analysis, precompiled
header or unity target is created. Normal `otpch.h` includes remain ordinary
includes. The gameplay code and Lua choice are unchanged. Use serial compilation
on memory-constrained machines; normal parallel builds are also supported.

At `f83a1fd`, Ubuntu 24.04/GCC 13 was reported to exhaust roughly 13 GB in the
legacy cotire/PCH path even with one compiler process. Serial compilation alone
does not fix that path. This option avoids it; it does not claim to repair the
underlying legacy cotire/compiler incompatibility. `-DUSE_COTIRE=ON` remains an
explicit legacy opt-in for environments where it works.

To recover an existing build, rerun its configure command with
`-DUSE_COTIRE=OFF`, then build. A fresh directory such as `build-low-memory`
also avoids stale compiler artifacts. To inspect a reused build explicitly,
configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and run:

```sh
python3 tests/cmake/check_no_cotire.py build/compile_commands.json
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

The native `engine_defaults` regression also loads a sparse configuration.
Configuration getters must consume the Lua value they push on every path,
including fallback values for missing or wrongly typed settings. Leaving those
values on the stack exhausted standard Lua 5.2's reserved stack slots and caused
a crash; both backend jobs now exercise the corrected, balanced getters.

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
