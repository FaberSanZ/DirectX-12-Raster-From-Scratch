function(add_dx12_example target_name)
    set(options)
    set(oneValueArgs SOURCE_DIR)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(EXAMPLE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${target_name} ${EXAMPLE_SOURCES})
    if(EXAMPLE_SOURCE_DIR)
        target_include_directories(${target_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_SOURCE_DIR}")
    else()
        target_include_directories(${target_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${target_name}")
    endif()
    target_link_libraries(${target_name} PRIVATE dx12_sdk dxc_sdk)
    dx12_set_target_layout(${target_name} "")
    dx12_link_dxc_runtime(${target_name})
endfunction()

set(DX12_EXAMPLES_WITH_COMMON
    VertexBuffer
    IndexBuffer
    DepthTests
    BasicSynchronizing
    Blending
    ConstantBuffer
    BasicTexture
    RootConstants
    ImGuiExample
)

add_dx12_example(ClearScreen
    SOURCES
        ClearScreen/ClearScreen.cpp
)

add_dx12_example(Pipeline
    SOURCES
        Pipeline/Pipeline.cpp
)

add_dx12_example(BasicSynchronizing
    SOURCES
        BasicSynchronizing/BasicSynchronizing.cpp
        BasicSynchronizing/WindowApp.cpp
)

add_dx12_example(Blending
    SOURCES
        Blending/Blending.cpp
        Blending/WindowApp.cpp
)

add_dx12_example(VertexBuffer
    SOURCES
        VertexBuffer/VertexBuffer.cpp
)

add_dx12_example(IndexBuffer
    SOURCES
        IndexBuffer/IndexBuffer.cpp
)

add_dx12_example(DepthTests
    SOURCES
        DepthTests/DepthTests.cpp
)

add_dx12_example(ConstantBuffer
    SOURCES
        ConstantBuffer/ConstantBuffer.cpp
)

add_dx12_example(BasicTexture
    SOURCES
        BasicTexture/main.cpp
)

add_dx12_example(RootConstants
    SOURCES
        RootConstants/RootConstants.cpp
)

add_dx12_example(ImGuiExample
    SOURCE_DIR "ImGui"
    SOURCES
        ImGui/main.cpp
)

foreach(example_target IN LISTS DX12_EXAMPLES_WITH_COMMON)
    target_link_libraries(${example_target} PRIVATE Common)
endforeach()
