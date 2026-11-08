cmake_minimum_required(VERSION 3.21)
project(findmouse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 32-bit builds are not supported: the plugin is developed, shipped and tested
# as x64, and a Wow64 process would be an untested configuration for a global
# hook. Refuse rather than build something nobody verified.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ra9fael.findmouse must be built as x64 (pointer size 8)")
endif()

# The generated C++ template builds the exe straight into the plugin folder,
# because plugin.json names it as runtime.program. Keep that convention.
add_executable(findmouse main.cpp mouse_hook.cpp ripple_overlay.cpp findmouse.rc)
# GetTickCount64, per-monitor DPI and the X-button messages want a modern SDK
# baseline; the host itself is Win10+ anyway.
target_compile_definitions(findmouse PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_link_libraries(findmouse PRIVATE user32 gdi32)

if(MSVC)
    target_compile_options(findmouse PRIVATE /utf-8 /W4 /permissive-)
    # Static CRT: the exe needs no runtime DLL, which the strict sandbox (low
    # integrity) would not load either; normal mode just starts faster.
    set_target_properties(findmouse PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(findmouse PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall -Wextra -O2)
    target_link_options(findmouse PRIVATE -static -static-libgcc -static-libstdc++)
endif()

set_target_properties(findmouse PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(findmouse PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_SOURCE_DIR}")
endforeach()

# Pure-logic tests: shake_logic.h and the helpers in settings.h. shake_logic.h
# has no OS dependency; settings.h pulls winshun.h, which is header-only, so
# the test binary still needs no link libraries beyond the C runtime.
add_executable(findmouse_tests tests/test_shake_logic.cpp)
target_compile_definitions(findmouse_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    WINVER=0x0A00 _WIN32_WINNT=0x0A00)
if(MSVC)
    target_compile_options(findmouse_tests PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(findmouse_tests PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall -Wextra -O2)
endif()
set_target_properties(findmouse_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(findmouse_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/tests")
endforeach()

enable_testing()
add_test(NAME findmouse_logic COMMAND findmouse_tests "${CMAKE_SOURCE_DIR}/plugin.json")
