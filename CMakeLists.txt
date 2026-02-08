cmake_minimum_required(VERSION 3.15)
project(war3-connect LANGUAGES C)

set(CMAKE_C_STANDARD 11)

add_library(war3hook SHARED
    src/hook_dll/dllmain.c
    src/hook_dll/hook.c
    src/hook_dll/config.c
)
target_link_libraries(war3hook PRIVATE ws2_32)
set_target_properties(war3hook PROPERTIES PREFIX "")

add_executable(war3-connect
    src/launcher/main.c
    src/launcher/injector.c
)
target_link_libraries(war3-connect PRIVATE ws2_32)
