# Build verification

`.github/workflows/build.yml` compiles `zedas-easy-mode` on Ubuntu 24.04 on
pushes and pull requests targeting that branch. It also supports manual runs.
The workflow has read-only repository permissions. It requires no secrets,
database service, runtime configuration, game map, or Tibia client.

To reproduce on Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential cmake ninja-build libboost-system-dev libboost-filesystem-dev libpugixml-dev libluajit-5.1-dev luajit default-libmysqlclient-dev libgmp-dev python3
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_LUAJIT=ON -DLUA_INCLUDE_DIR=/usr/include/luajit-2.1
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

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
All ten current tests passed in both the normal and sanitizer builds locally.
