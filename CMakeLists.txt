cmake_minimum_required(VERSION 3.20)
project(madmax_coop C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    src
    src/vendor/minhook/include
    src/vendor/imgui
    src/vendor/imgui/backends
    src/vendor/enet/include
    src/vendor/lua
)

# MinHook
add_subdirectory(src/vendor/minhook)

# ENet
file(GLOB ENET_SOURCES "src/vendor/enet/*.c")
add_library(enet STATIC ${ENET_SOURCES})
target_compile_definitions(enet PRIVATE WIN32 _WINSOCK_DEPRECATED_NO_WARNINGS)
target_link_libraries(enet winmm ws2_32)

# Lua (core only, no onelua.c or tests)
file(GLOB LUA_SOURCES "src/vendor/lua/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "(lua\\.c|luac\\.c|onelua\\.c|ltests\\.c)$")
add_library(lua STATIC ${LUA_SOURCES})

# ImGui
set(IMGUI_SOURCES
    src/vendor/imgui/imgui.cpp
    src/vendor/imgui/imgui_draw.cpp
    src/vendor/imgui/imgui_tables.cpp
    src/vendor/imgui/imgui_widgets.cpp
    src/vendor/imgui/backends/imgui_impl_win32.cpp
    src/vendor/imgui/backends/imgui_impl_dx11.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_link_libraries(imgui d3d11 d3dcompiler)

# Client Mod Injected DLL (named madmax_client, output file dxgi.dll)
file(GLOB_RECURSE CLIENT_SOURCES "src/client/*.cpp" "src/client/*.h")
add_library(madmax_client SHARED ${CLIENT_SOURCES} src/client/dxgi_proxy.def)
target_compile_definitions(madmax_client PRIVATE WIN32 _WINSOCK_DEPRECATED_NO_WARNINGS)
target_link_libraries(madmax_client PRIVATE minhook enet lua imgui d3d11 dxgi.lib d3dcompiler ws2_32 winmm)
set_target_properties(madmax_client PROPERTIES PREFIX "" OUTPUT_NAME "dxgi")

# Standalone Dedicated Relay Server
add_executable(madmax_server server/main.cpp)
target_compile_definitions(madmax_server PRIVATE WIN32 _WINSOCK_DEPRECATED_NO_WARNINGS)
target_link_libraries(madmax_server PRIVATE enet lua ws2_32 winmm)

# Diagnostics & Verification Executables
add_executable(test_lua server/test_lua.cpp)
target_link_libraries(test_lua PRIVATE lua)

add_executable(test_client server/test_client.cpp)
target_compile_definitions(test_client PRIVATE WIN32 _WINSOCK_DEPRECATED_NO_WARNINGS)
target_link_libraries(test_client PRIVATE enet ws2_32 winmm)
