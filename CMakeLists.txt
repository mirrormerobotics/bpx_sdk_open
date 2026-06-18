cmake_minimum_required(VERSION 3.10)

project(bpx_sdk_examples LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(bpx_sdk SHARED IMPORTED)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" BPX_SDK_ARCH)
if(BPX_SDK_ARCH MATCHES "^(amd64|x64)$")
    set(BPX_SDK_ARCH x86_64)
elseif(BPX_SDK_ARCH STREQUAL "arm64")
    set(BPX_SDK_ARCH aarch64)
endif()

if(WIN32)
    set(BPX_SDK_RUNTIME_LIBRARY_CANDIDATES
        "${CMAKE_CURRENT_LIST_DIR}/bin/bpx_sdk_${BPX_SDK_ARCH}.dll"
        "${CMAKE_CURRENT_LIST_DIR}/lib/bpx_sdk_${BPX_SDK_ARCH}.dll"
    )
    set(BPX_SDK_IMPORT_LIBRARY_CANDIDATES
        "${CMAKE_CURRENT_LIST_DIR}/lib/bpx_sdk_${BPX_SDK_ARCH}.lib"
        "${CMAKE_CURRENT_LIST_DIR}/bin/bpx_sdk_${BPX_SDK_ARCH}.lib"
    )
else()
    set(BPX_SDK_LIBRARY_CANDIDATES
        "${CMAKE_CURRENT_LIST_DIR}/lib/libbpx_sdk_${BPX_SDK_ARCH}.so"
    )
endif()

foreach(BPX_SDK_RUNTIME_LIBRARY_CANDIDATE IN LISTS BPX_SDK_RUNTIME_LIBRARY_CANDIDATES)
    if(EXISTS "${BPX_SDK_RUNTIME_LIBRARY_CANDIDATE}")
        set(BPX_SDK_RUNTIME_LIBRARY_PATH "${BPX_SDK_RUNTIME_LIBRARY_CANDIDATE}")
        break()
    endif()
endforeach()

foreach(BPX_SDK_IMPORT_LIBRARY_CANDIDATE IN LISTS BPX_SDK_IMPORT_LIBRARY_CANDIDATES BPX_SDK_LIBRARY_CANDIDATES)
    if(EXISTS "${BPX_SDK_IMPORT_LIBRARY_CANDIDATE}")
        set(BPX_SDK_IMPORT_LIBRARY_PATH "${BPX_SDK_IMPORT_LIBRARY_CANDIDATE}")
        break()
    endif()
endforeach()

set_target_properties(bpx_sdk PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    INTERFACE_LINK_LIBRARIES Threads::Threads
)

if(WIN32)
    if(NOT BPX_SDK_RUNTIME_LIBRARY_PATH)
        message(FATAL_ERROR "No BPX SDK runtime DLL found for architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(NOT BPX_SDK_IMPORT_LIBRARY_PATH)
        message(FATAL_ERROR "No BPX SDK import library found for architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    set_target_properties(bpx_sdk PROPERTIES
        IMPORTED_LOCATION "${BPX_SDK_RUNTIME_LIBRARY_PATH}"
        IMPORTED_IMPLIB "${BPX_SDK_IMPORT_LIBRARY_PATH}"
    )
else()
    if(NOT BPX_SDK_IMPORT_LIBRARY_PATH)
        message(FATAL_ERROR "No BPX SDK shared library found for architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    set_target_properties(bpx_sdk PROPERTIES
        IMPORTED_LOCATION "${BPX_SDK_IMPORT_LIBRARY_PATH}"
    )
endif()

function(add_bpx_sdk_example target source)
    add_executable(${target}
        ${source}
    )
    target_link_libraries(${target}
        PRIVATE
            bpx_sdk
    )
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "${CMAKE_CURRENT_LIST_DIR}/lib"
    )
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BPX_SDK_RUNTIME_LIBRARY_PATH}"
                "$<TARGET_FILE_DIR:${target}>"
            VERBATIM
        )
    endif()
endfunction()

add_bpx_sdk_example(request_robot_state_example example/request_robot_state_example.cpp)
add_bpx_sdk_example(motion_level_control_example example/motion_level_control_example.cpp)
add_bpx_sdk_example(joint_level_control_example example/joint_level_control_example.cpp)
