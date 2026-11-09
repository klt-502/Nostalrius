cmake_minimum_required(VERSION 2.8)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project(tfs)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cotire)

add_compile_options(-Wall  -pipe -fvisibility=hidden)

if (CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-fno-strict-aliasing)
endif()

include(FindCXX11)

# Find packages.
find_package(GMP REQUIRED)
find_package(PugiXML REQUIRED)
find_package(MySQL)
find_package(Threads)

include(SelectLua)
if(USE_LUAJIT AND APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "-pagezero_size 10000 -image_base 100000000")
endif()

find_package(Boost 1.53.0 COMPONENTS system filesystem REQUIRED)

include(src/CMakeLists.txt)
list(REMOVE_ITEM tfs_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/otserv.cpp)
add_library(tfs_engine STATIC ${tfs_SRC})
add_executable(tfs src/otserv.cpp)

include_directories(${MYSQL_INCLUDE_DIR} ${ZEDAS_LUA_INCLUDE_DIR} ${Boost_INCLUDE_DIRS} ${PUGIXML_INCLUDE_DIR} ${GMP_INCLUDE_DIR})
target_link_libraries(tfs_engine PUBLIC ${MYSQL_CLIENT_LIBS} ${ZEDAS_LUA_LIBRARIES} ${Boost_LIBRARIES} ${Boost_FILESYSTEM_LIBRARIES} ${PUGIXML_LIBRARIES} ${GMP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(tfs tfs_engine)

set_target_properties(tfs_engine PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/otpch.h")
set_target_properties(tfs_engine PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
cotire(tfs_engine)

include(CTest)
if(BUILD_TESTING)
    find_package(PythonInterp 3 REQUIRED)
    add_executable(easymode_engine_test tests/easymode_engine_test.cpp)
    target_include_directories(easymode_engine_test PRIVATE src)
    target_link_libraries(easymode_engine_test tfs_engine)
    foreach(scenario enabled disabled feature_disabled invalid_config defaults commands_disabled target_disabled gold_disabled bag_enabled)
        add_test(NAME engine_${scenario} COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/run_engine_test.py
            $<TARGET_FILE:easymode_engine_test> ${CMAKE_SOURCE_DIR} ${scenario})
    endforeach()
    add_executable(easymode_constants_test tests/easymode_constants_test.cpp)
    target_include_directories(easymode_constants_test PRIVATE src)
    add_test(NAME easymode_constants COMMAND easymode_constants_test)
    if(NOT CMAKE_CROSSCOMPILING)
        add_test(NAME lua_backend_selection COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/cmake/test_lua_selection.py
            --cmake=${CMAKE_COMMAND} --source=${CMAKE_SOURCE_DIR}
            "--prefix-path=${CMAKE_PREFIX_PATH}" --compiler=${CMAKE_CXX_COMPILER})
    endif()
    find_program(LUAJIT_EXECUTABLE NAMES luajit)
    if(LUAJIT_EXECUTABLE)
        add_test(NAME create_item_validation COMMAND ${LUAJIT_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/create_item_test.lua
            ${CMAKE_SOURCE_DIR}/data/talkactions/scripts/create_item.lua)
    endif()
endif()
