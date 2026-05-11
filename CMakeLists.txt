cmake_minimum_required(VERSION 3.24)

project(DX12RasterFromScratch LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DX12_PLATFORM_DIR "x64")
    set(DX12_SDK_ARCH_DIR "x64")
else()
    set(DX12_PLATFORM_DIR "Win32")
    set(DX12_SDK_ARCH_DIR "x86")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(DX12_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(DX12_SRC_DIR "${DX12_ROOT_DIR}/Src")

set(WINDOWS_KITS_10_DIR "$ENV{ProgramFiles\(x86\)}/Windows Kits/10")

if(NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    file(GLOB DX12_WINDOWS_SDK_INCLUDE_DIRS LIST_DIRECTORIES true "${WINDOWS_KITS_10_DIR}/Include/10.*")
    list(SORT DX12_WINDOWS_SDK_INCLUDE_DIRS)
    list(GET DX12_WINDOWS_SDK_INCLUDE_DIRS -1 DX12_LATEST_WINDOWS_SDK_INCLUDE_DIR)
    get_filename_component(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "${DX12_LATEST_WINDOWS_SDK_INCLUDE_DIR}" NAME)
endif()

set(DX12_WINDOWS_SDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
set(DX12_WINDOWS_SDK_INCLUDE_UM "${WINDOWS_KITS_10_DIR}/Include/${DX12_WINDOWS_SDK_VERSION}/um")
set(DX12_WINDOWS_SDK_LIB_UM "${WINDOWS_KITS_10_DIR}/Lib/${DX12_WINDOWS_SDK_VERSION}/um/${DX12_SDK_ARCH_DIR}")
set(DX12_WINDOWS_SDK_BIN "${WINDOWS_KITS_10_DIR}/bin/${DX12_WINDOWS_SDK_VERSION}/${DX12_SDK_ARCH_DIR}")

find_path(DXC_INCLUDE_DIR dxcapi.h PATHS "${DX12_WINDOWS_SDK_INCLUDE_UM}" NO_DEFAULT_PATH)
find_library(DXC_LIB dxcompiler PATHS "${DX12_WINDOWS_SDK_LIB_UM}" NO_DEFAULT_PATH)
find_file(DXC_COMPILER_DLL dxcompiler.dll PATHS "${DX12_WINDOWS_SDK_BIN}" NO_DEFAULT_PATH)
find_file(DXC_DXIL_DLL dxil.dll PATHS "${DX12_WINDOWS_SDK_BIN}" NO_DEFAULT_PATH)

if(NOT DXC_INCLUDE_DIR OR NOT DXC_LIB)
    message(FATAL_ERROR "DXC was not found in the Windows SDK. Install a Windows 10/11 SDK that includes dxcapi.h and dxcompiler.lib.")
endif()

add_library(dx12_sdk INTERFACE)
target_compile_definitions(dx12_sdk INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(dx12_sdk INTERFACE d3d12 dxgi d3dcompiler)

add_library(dxc_sdk INTERFACE)
target_include_directories(dxc_sdk INTERFACE "${DXC_INCLUDE_DIR}")
target_link_libraries(dxc_sdk INTERFACE "${DXC_LIB}")

function(dx12_set_target_layout target folder_name)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${DX12_PLATFORM_DIR}/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${DX12_PLATFORM_DIR}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${DX12_PLATFORM_DIR}/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${DX12_PLATFORM_DIR}/$<CONFIG>"
    )
    if(NOT folder_name STREQUAL "")
        set_target_properties(${target} PROPERTIES FOLDER "${folder_name}")
    endif()
endfunction()

function(dx12_link_dxc_runtime target)
    if(DXC_COMPILER_DLL AND DXC_DXIL_DLL)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DXC_COMPILER_DLL}"
                "$<TARGET_FILE_DIR:${target}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DXC_DXIL_DLL}"
                "$<TARGET_FILE_DIR:${target}>"
            VERBATIM
        )
    endif()
endfunction()

set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(External/imgui)
add_subdirectory(External/fastgltf EXCLUDE_FROM_ALL)
add_subdirectory(Src/Common)
add_subdirectory(Src)

if(TARGET ImGui)
    set_target_properties(ImGui PROPERTIES FOLDER "External\\ImGui")
endif()
if(TARGET fastgltf)
    set_target_properties(fastgltf PROPERTIES FOLDER "External\\gltf")
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ImGuiExample)
