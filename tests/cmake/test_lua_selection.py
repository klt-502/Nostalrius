#!/usr/bin/env python3
"""Exercise backend discovery with both development packages installed.

These small, isolated CMake projects do not configure or run the game server.
They test a fresh selection, a reused cache and old mixed generic Lua entries.
"""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--prefix-path", default="")
    parser.add_argument("--compiler", default="")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="zedas-lua-selection-") as temp:
        root = Path(temp)
        source = root / "source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.5)\n"
            "project(lua_backend_probe CXX)\n"
            "list(INSERT CMAKE_MODULE_PATH 0 \"${ZEDAS_MODULE_DIR}\")\n"
            "include(SelectLua)\n"
            "file(WRITE \"${CMAKE_BINARY_DIR}/selection.txt\"\n"
            "  \"${ZEDAS_LUA_BACKEND}\\n${ZEDAS_LUA_INCLUDE_DIR}\\n"
            "${ZEDAS_LUA_LIBRARIES}\\n\")\n",
            encoding="utf-8",
        )

        def configure(name, enabled=None, cache=None, expect_success=True):
            build = root / name
            command = [args.cmake, "-S", str(source), "-B", str(build),
                       "-DZEDAS_MODULE_DIR=" + str(args.source / "cmake"),
                       "-DCMAKE_PREFIX_PATH=" + args.prefix_path]
            if args.compiler:
                command.append("-DCMAKE_CXX_COMPILER=" + args.compiler)
            if enabled is not None:
                command.append("-DUSE_LUAJIT=" + ("ON" if enabled else "OFF"))
            for key, value in (cache or {}).items():
                command.append("-D" + key + "=" + str(value))
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            if (result.returncode == 0) != expect_success:
                raise AssertionError("Unexpected configure result for " + name
                                     + "\n" + result.stdout)
            if not expect_success:
                if "Lua ABI link check" not in result.stdout:
                    raise AssertionError("Expected ABI rejection for " + name
                                         + "\n" + result.stdout)
                return None
            backend, include, libraries = (build / "selection.txt").read_text(
                encoding="utf-8").splitlines()
            expected = "LuaJIT" if enabled is not False else "Lua 5.2"
            assert backend == expected, (backend, expected)
            library_paths = libraries.split(";")
            lua_library = next(path for path in library_paths
                               if "lua" in Path(path).name.lower())
            if expected == "LuaJIT":
                assert (Path(include) / "luajit.h").is_file(), include
                assert "luajit" in Path(lua_library).name.lower(), lua_library
            else:
                assert "luajit" not in Path(lua_library).name.lower(), lua_library
                header = (Path(include) / "lua.h").read_text(encoding="utf-8")
                assert re.search(r"^#define\s+LUA_VERSION_NUM\s+502\s*$",
                                 header, re.MULTILINE), include
            return {"include": include, "library": lua_library}

        # Defaults are deterministic even when both packages are discoverable.
        jit = configure("default")
        standard = configure("fresh-standard", False)
        configure("switch", True)
        configure("switch", False)
        configure("switch", True)
        # Simulate the old FindLuaJIT pollution, including the reported mixed
        # standard-header/JIT-library case. OFF must rediscover standard Lua.
        configure("legacy-cache", False, {
            "LUA_INCLUDE_DIR": standard["include"],
            "LUA_LIBRARY": jit["library"],
            "LUA_LIBRARIES": jit["library"],
        })
        # Generic FindLua paths must have no effect on the namespaced JIT path.
        configure("generic-cache-jit", True, {
            "LUA_INCLUDE_DIR": standard["include"],
            "LUA_LIBRARY": standard["library"],
            "LUA_LIBRARIES": standard["library"],
        })
        # Explicitly poisoned backend-specific paths are caught at configure,
        # rather than producing a server binary with a mismatched Lua ABI.
        configure("wrong-jit-library", True, {
            "LUAJIT_INCLUDE_DIR": jit["include"],
            "LUAJIT_LIBRARY": standard["library"],
        }, expect_success=False)
        configure("wrong-jit-headers", True, {
            "LUAJIT_INCLUDE_DIR": standard["include"],
            "LUAJIT_LIBRARY": jit["library"],
        }, expect_success=False)
    print("Lua backend selection, cache switches and mixed-ABI rejection passed")


if __name__ == "__main__":
    main()
