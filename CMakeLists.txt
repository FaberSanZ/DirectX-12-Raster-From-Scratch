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
else()
    set(DX12_PLATFORM_DIR "Win32")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(DX12_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(DX12_SRC_DIR "${DX12_ROOT_DIR}/Src")
set(DX12_PACKAGES_DIR "${DX12_SRC_DIR}/packages")

set(DXC_PACKAGE_DIR "${DX12_PACKAGES_DIR}/Microsoft.Direct3D.DXC.1.8.2505.32/build/native")
set(DXC_INCLUDE_DIR "${DXC_PACKAGE_DIR}/include")
set(DXC_LIB_DIR "${DXC_PACKAGE_DIR}/lib/${DX12_PLATFORM_DIR}")
set(DXC_BIN_DIR "${DXC_PACKAGE_DIR}/bin/${DX12_PLATFORM_DIR}")

add_library(dx12_sdk INTERFACE)
target_compile_definitions(dx12_sdk INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(dx12_sdk INTERFACE d3d12 dxgi d3dcompiler)

add_library(dxc_sdk INTERFACE)
target_include_directories(dxc_sdk INTERFACE "${DXC_INCLUDE_DIR}")
target_link_directories(dxc_sdk INTERFACE "${DXC_LIB_DIR}")
target_link_libraries(dxc_sdk INTERFACE dxcompiler)

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
    if(EXISTS "${DXC_BIN_DIR}/dxcompiler.dll")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DXC_BIN_DIR}/dxcompiler.dll"
                "$<TARGET_FILE_DIR:${target}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DXC_BIN_DIR}/dxil.dll"
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
