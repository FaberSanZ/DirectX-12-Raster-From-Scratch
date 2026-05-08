add_library(Common STATIC)

target_sources(Common PRIVATE
    Desktop/Window.cpp
    Desktop/Window.h
    Graphics/DescriptorHeap.cpp
    Graphics/DescriptorHeap.h
    Graphics/GUI.cpp
    Graphics/GUI.h
    ShaderCompiler.h
)

target_include_directories(Common
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(Common
    PUBLIC
        dx12_sdk
        dxc_sdk
        ImGui
)

dx12_set_target_layout(Common "")
