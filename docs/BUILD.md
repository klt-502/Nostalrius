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
