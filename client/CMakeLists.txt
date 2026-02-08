cmake_minimum_required(VERSION 3.15)

# ── War3 Platform GUI Client ────────────────────────────────────────
# Windows-only Win32 application.  Build with:
#   cmake -B build -G "Visual Studio 17 2022" -A Win32
#   cmake --build build --config Release --target war3-platform

add_executable(war3-platform WIN32
    main.c
    gui.c
    gui_login.c
    gui_lobby.c
    gui_room.c
    net_client.c
    game_launcher.c
    injector.c
    ../common/protocol.c
    ../third_party/cJSON/cJSON.c
)

target_include_directories(war3-platform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(war3-platform PRIVATE
    CJSON_HIDE_SYMBOLS
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(war3-platform PRIVATE
    ws2_32
    comctl32
)

# Ensure C11 standard.
set_target_properties(war3-platform PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
