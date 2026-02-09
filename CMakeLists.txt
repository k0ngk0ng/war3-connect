cmake_minimum_required(VERSION 3.15)
project(war3-platform LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Common compile definitions ────────────────────────────────────────
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

# ── cJSON (static library, shared by server / client) ─────────────────
add_library(cjson STATIC
    third_party/cJSON/cJSON.c
)
target_include_directories(cjson PUBLIC third_party/cJSON)
target_compile_definitions(cjson PUBLIC CJSON_HIDE_SYMBOLS)

# ── Common protocol library ───────────────────────────────────────────
add_library(common STATIC
    common/protocol.c
)
target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR})

# ══════════════════════════════════════════════════════════════════════
#  1. Server  (cross-platform: Windows + Linux + macOS)
# ══════════════════════════════════════════════════════════════════════
add_executable(war3-lobby-server
    server/main.c
    server/server.c
    server/handler.c
    server/user.c
    server/room.c
)
target_link_libraries(war3-lobby-server PRIVATE common cjson)
target_include_directories(war3-lobby-server PRIVATE ${CMAKE_SOURCE_DIR})

if(WIN32)
    target_link_libraries(war3-lobby-server PRIVATE ws2_32)
endif()

# ══════════════════════════════════════════════════════════════════════
#  2. Client GUI  (Windows only – Win32 API)
# ══════════════════════════════════════════════════════════════════════
if(WIN32)
    add_executable(war3-platform WIN32
        client/main.c
        client/gui.c
        client/gui_style.c
        client/gui_login.c
        client/gui_lobby.c
        client/gui_room.c
        client/net_client.c
        client/game_launcher.c
        client/injector.c
    )
    target_link_libraries(war3-platform PRIVATE common cjson ws2_32 comctl32)
    target_include_directories(war3-platform PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(war3-platform PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN
    )
endif()

# ══════════════════════════════════════════════════════════════════════
#  3. Hook DLL  (Windows only – 32-bit)
# ══════════════════════════════════════════════════════════════════════
if(WIN32)
    add_library(war3hook SHARED
        hook_dll/dllmain.c
        hook_dll/hook.c
        hook_dll/config.c
    )
    target_link_libraries(war3hook PRIVATE ws2_32)
    target_include_directories(war3hook PRIVATE ${CMAKE_SOURCE_DIR})
    set_target_properties(war3hook PROPERTIES PREFIX "")
endif()

# ══════════════════════════════════════════════════════════════════════
#  (Legacy) Old standalone launcher – kept for reference
# ══════════════════════════════════════════════════════════════════════
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/src/launcher/main.c")
    add_executable(war3-connect-legacy
        src/launcher/main.c
        src/launcher/injector.c
    )
    target_link_libraries(war3-connect-legacy PRIVATE ws2_32)
    set_target_properties(war3-connect-legacy PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
