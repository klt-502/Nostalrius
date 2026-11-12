cmake_minimum_required(VERSION 2.8)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project(tfs)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(USE_COTIRE "Enable legacy cotire/PCH acceleration (can require substantial memory)" OFF)
if(USE_COTIRE)
    include(cotire)
endif()

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

if(USE_COTIRE)
    set_target_properties(tfs_engine PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/otpch.h")
    set_target_properties(tfs_engine PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
    cotire(tfs_engine)
else()
    message(STATUS "Zedas low-memory build: cotire, generated prefix headers and PCH disabled")
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(PythonInterp 3 REQUIRED)
    add_executable(easymode_engine_test tests/easymode_engine_test.cpp tests/easyspawn_engine_test.cpp)
    target_include_directories(easymode_engine_test PRIVATE src)
    target_link_libraries(easymode_engine_test tfs_engine)
    foreach(scenario enabled disabled feature_disabled invalid_config defaults commands_disabled target_disabled gold_disabled
            bag_enabled bag_disabled spill_enabled spill_hunting spill_only spill_invalid_policy spill_missing_policy wave2_disabled respawn_enabled broker broker_catalog broker_restart loot_classes)
        add_test(NAME engine_${scenario} COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/run_engine_test.py
            $<TARGET_FILE:easymode_engine_test> ${CMAKE_SOURCE_DIR} ${scenario})
    endforeach()
    set_tests_properties(engine_broker engine_broker_restart PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(easymode_constants_test tests/easymode_constants_test.cpp)
    target_include_directories(easymode_constants_test PRIVATE src)
    add_test(NAME easymode_constants COMMAND easymode_constants_test)
    add_executable(easyspawn_test tests/easyspawn_test.cpp src/easyspawn.cpp)
    target_include_directories(easyspawn_test PRIVATE src)
    target_link_libraries(easyspawn_test ${PUGIXML_LIBRARIES})
    add_test(NAME easyspawn_policy_and_deadline COMMAND easyspawn_test
        ${CMAKE_SOURCE_DIR}/tests/fixtures/easyspawns.xml)
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

if(BUILD_TESTING)
    add_executable(brokerstore_test tests/brokerstore_test.cpp src/brokerstore.cpp)
    target_include_directories(brokerstore_test PRIVATE src)
    target_compile_definitions(brokerstore_test PRIVATE ZEDAS_BROKER_FAULT_TEST)
    # Assertions are acceptance checks, also in Release CI.
    target_compile_options(brokerstore_test PRIVATE -UNDEBUG)
    target_link_libraries(brokerstore_test ${MYSQL_CLIENT_LIBS})
    add_test(NAME broker_store COMMAND brokerstore_test)
    set_tests_properties(broker_store PROPERTIES SKIP_RETURN_CODE 77)
endif()
