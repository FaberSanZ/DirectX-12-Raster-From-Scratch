// ImGui.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <d3dcompiler.h>
#include "ShaderCompiler.h"
#include "Desktop/Window.h"
#include "Graphics/DescriptorHeap.h"
#include <DirectXMath.h>
#include <Graphics/GUI.h>



#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Graphics;
using namespace Desktop;
using namespace DirectX;

class Render
{
public:

    struct CameraBuffer
    {
        XMMATRIX word;
        XMMATRIX view;
        XMMATRIX projection;
    } cameraData;


public:
    Render() = default;





    bool Initialize(HWND hwnd, uint32_t width, uint32_t Heigh)
    {
        m_width = width;
        m_height = Heigh;


        m_viewport = { 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
        m_scissorRect = { 0, 0, (long)m_width, (long)m_height };


        IDXGIFactory4* factory = nullptr;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));

        D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));



        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));


        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = m_frameCount;
        swapChainDesc.Width = m_width;
        swapChainDesc.Height = m_height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        IDXGISwapChain1* tempSwapChain = nullptr;
        factory->CreateSwapChainForHwnd(m_commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, &tempSwapChain);

        //
        tempSwapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain));
        factory->Release();

        // Create command allocator and command list
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAlloc));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAlloc, nullptr, IID_PPV_ARGS(&m_commandList));

        m_commandList->Close();


        CreateSynchronizationObjects();
        CreateRenderTargetViews();
        CreateDepthBuffer();
        CreatePipeline();
        CreateMesh();
        CreateConstantBuffer();




        // Create ImGui context


		m_gui.Initialize(m_device, m_commandQueue, m_frameCount, hwnd);

        return true;
    }

    void CreateSynchronizationObjects()
    {
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        m_fenceValue = 0;
        // Create a fence for synchronization
        m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        // Create an event handle to use for frame synchronization
        m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);

        if (m_fenceEvent == nullptr)
        {
            throw std::runtime_error("Failed to create fence event.");
        }

        WaitForPreviousFrame();
    }

    void WaitForPreviousFrame()
    {
        // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
        // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
        // sample illustrates how to use fences for efficient resource usage and to
        // maximize GPU utilization.

        // Signal and increment the fence value.
        const UINT64 fence = m_fenceValue;
        m_commandQueue->Signal(m_fence, fence);
        m_fenceValue++;

        // Wait until the previous frame is finished.
        if (m_fence->GetCompletedValue() < fence)
        {
            m_fence->SetEventOnCompletion(fence, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    }


    void CreateRenderTargetViews()
    {
        // Create RTV descriptor heap
        m_rtvDescriptorHeap.Initialize(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_frameCount, false);

        for (uint32_t i = 0; i < m_frameCount; ++i)
        {
            ID3D12Resource* backBuffer = nullptr;
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHeap.GetCPUHandle(i);
            m_device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);
            m_renderTargets[i] = backBuffer;
        }

    }


    void CreateDepthBuffer()
    {
        // Create depth stencil buffer
        D3D12_RESOURCE_DESC depthStencilDesc = {};
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Width = m_width;
        depthStencilDesc.Height = m_height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthStencilDesc.SampleDesc.Count = 1;
        depthStencilDesc.SampleDesc.Quality = 0;
        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        // Clear value for the depth stencil buffer
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;


        // Create heap properties for the depth stencil buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;


        // Create the depth stencil buffer resource
        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&m_depthStencilBuffer));

        // Create descriptor heap for depth stencil view (DSV)
        m_dpvDescriptorHeap.Initialize(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);


        // Create the depth stencil view (DSV)
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.Texture2D.MipSlice = 0;

        m_device->CreateDepthStencilView(m_depthStencilBuffer, &dsvDesc, m_dpvDescriptorHeap.GetCPUHandle(0));
    }

    void CreatePipeline()
    {
        auto vertexShaderBlob = m_shaderCompiler.Compile(L"../../../../Assets/Shaders/ConstantBuffers/VertexShader.hlsl", L"VS", L"vs_6_0");
        auto pixelShaderBlob = m_shaderCompiler.Compile(L"../../../../Assets/Shaders/ConstantBuffers/PixelShader.hlsl", L"PS", L"ps_6_0");



        D3D12_DESCRIPTOR_RANGE cbvRange = {};
        cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;  // Constant Buffer View
        cbvRange.NumDescriptors = 1;
        cbvRange.BaseShaderRegister = 0;  // b0
        cbvRange.RegisterSpace = 0;
        cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


        D3D12_ROOT_PARAMETER cbvRootParam = {};
        cbvRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        cbvRootParam.DescriptorTable.NumDescriptorRanges = 1;
        cbvRootParam.DescriptorTable.pDescriptorRanges = &cbvRange;
        cbvRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


                D3D12_ROOT_PARAMETER vertexBufferParam = {};
        vertexBufferParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        vertexBufferParam.Descriptor.ShaderRegister = 0;
        vertexBufferParam.Descriptor.RegisterSpace = 0;
        vertexBufferParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_PARAMETER rootParams[] = { vertexBufferParam, cbvRootParam };

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = _countof(rootParams);
        rootSigDesc.pParameters = rootParams;
        rootSigDesc.NumStaticSamplers = 0;
        rootSigDesc.pStaticSamplers = nullptr;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;




        ID3DBlob* sigBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;

        D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlob);
        m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        // Rasterizer state
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        rasterizerDesc.FrontCounterClockwise = false;
        rasterizerDesc.DepthClipEnable = true;


        // Blend state
        // This is the blend state for the pipeline. It defines how blending is done.
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = false;
        blendDesc.IndependentBlendEnable = false;
        blendDesc.RenderTarget[0].BlendEnable = true;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


        // Depth stencil
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable = true;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable = false;



        // --- PIPELINE STATE [PSO]---
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_rootSignature;
        psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
        psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;  // Set the render target format
        psoDesc.SampleDesc.Count = 1;

        m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    }
    ID3D12Resource* CreateBuffer(const void* data, uint32_t size)
    {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = size;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ID3D12Resource* buffer = nullptr;
        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));

        if (data)
        {
            void* mappedData = nullptr;
            buffer->Map(0, nullptr, &mappedData);
            memcpy(mappedData, data, size);
            buffer->Unmap(0, nullptr);
        }

        return buffer;
    }
    void CreateMesh()
    {
        // Define vertices for a triangle
        struct Vertex
        {
            float position[4];
            float color[4];
        };

        Vertex vertices[] =
        {
            // Front face
            {{-0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},

            // Right side face
            {{ 0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},

            // Left side face
            {{-0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},

            // Back face
            {{ 0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},

            // Top face
            {{-0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
            {{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},

            // Bottom face
            {{ 0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
            {{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
            {{-0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        };
        m_vertexBuffer = CreateBuffer(vertices, sizeof(vertices));
        // Define indices for a triangle
        uint32_t indices[] =
        {
            // front face
            0, 1, 2, // first triangle
            0, 3, 1, // second triangle

            // left face
            4, 5, 6, // first triangle
            4, 7, 5, // second triangle

            // right face
            8, 9, 10, // first triangle
            8, 11, 9, // second triangle

            // back face
            12, 13, 14, // first triangle
            12, 15, 13, // second triangle

            // top face
            16, 17, 18, // first triangle
            16, 19, 17, // second triangle

            // bottom face
            20, 21, 22, // first triangle
            20, 23, 21, // second triangle
        };
        m_indexCount = _countof(indices);
        m_indexBuffer = CreateBuffer(indices, sizeof(indices));


        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_indexBufferView.SizeInBytes = sizeof(indices);
    }


    void CreateConstantBuffer()
    {
        m_cbvDescriptorHeap.Initialize(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, true);

        m_constantBufferSize = (sizeof(CameraBuffer) + 255) & ~255;
        m_constantBuffer = CreateBuffer(nullptr, m_constantBufferSize);
        m_constantBuffer2 = CreateBuffer(nullptr, m_constantBufferSize);

        m_constantBufferDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        m_constantBufferDesc.SizeInBytes = m_constantBufferSize;
        m_device->CreateConstantBufferView(&m_constantBufferDesc, m_cbvDescriptorHeap.GetCPUHandle(0));

        m_constantBuffer2Desc.BufferLocation = m_constantBuffer2->GetGPUVirtualAddress();
        m_constantBuffer2Desc.SizeInBytes = m_constantBufferSize;
        m_device->CreateConstantBufferView(&m_constantBuffer2Desc, m_cbvDescriptorHeap.GetCPUHandle(1));

        CreateCamera();
    }

    void CreateCamera()
    {
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH({ 0, 0, -3 }, { 0, 0, 0 }, { 0, 1, 0 });

        // Set up projection matrix (perspective)
        float fov = 45.0f * (3.14f / 180.0f);
        float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
        float nearZ = 0.1f;
        float farZ = 1000.0f;
        DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);

        // Transpose matrices for HLSL (row-major in C++, column-major in HLSL)
        cameraData.word = DirectX::XMMatrixIdentity();
        cameraData.view = DirectX::XMMatrixTranspose(view);
        cameraData.projection = DirectX::XMMatrixTranspose(projection);
    }


    // Cubo 1
    float cube1RotationSpeed[3] = { 0.0f, 0.0f, 0.0f }; // pitch, yaw, roll
    float cube1Position[3] = { 0.2f, 0.0f, 0.0f };

    // Cubo 2
    float cube2RotationSpeed[3] = { 0.0f, 0.0f, 0.0f };
    float cube2Position[3] = { -0.2f, 0.0f, 0.0f };

    // Ángulos acumulados
    float cube1Angles[3] = { 0.0f, 0.0f, 0.0f };
    float cube2Angles[3] = { 0.0f, 0.0f, 0.0f };



    void OnUpdate()
    {
        for (int i = 0; i < 3; ++i)
        {
            cube1Angles[i] += cube1RotationSpeed[i];
            cube2Angles[i] += cube2RotationSpeed[i];
        }


        // Cube 1
        XMMATRIX rot1 = XMMatrixRotationRollPitchYaw(cube1Angles[0], cube1Angles[1], cube1Angles[2]);
        XMMATRIX trans1 = XMMatrixTranslation(cube1Position[0], cube1Position[1], cube1Position[2]);
        cameraData.word = XMMatrixTranspose(rot1 * trans1);

        void* mappedData = nullptr;
        m_constantBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, &cameraData, sizeof(CameraBuffer));
        m_constantBuffer->Unmap(0, nullptr);




        // Cube 2
        XMMATRIX rot2 = XMMatrixRotationRollPitchYaw(cube2Angles[0], cube2Angles[1], cube2Angles[2]);
        XMMATRIX trans2 = XMMatrixTranslation(cube2Position[0], cube2Position[1], cube2Position[2]);
        cameraData.word = XMMatrixTranspose(rot2 * trans2);

        void* mappedData2 = nullptr;
        m_constantBuffer2->Map(0, nullptr, &mappedData2);
        memcpy(mappedData2, &cameraData, sizeof(CameraBuffer));
        m_constantBuffer2->Unmap(0, nullptr);
    }

    void OnRenderGui()
    {
        ImGui::Begin("Cubes Transform Control");

        if (ImGui::CollapsingHeader("Cube 1"))
        {
            ImGui::SliderFloat3("Pos 1", cube1Position, -5.0f, 5.0f);
            ImGui::SliderFloat3("Rot Speed 1", cube1RotationSpeed, -0.1f, 0.1f);
        }

        if (ImGui::CollapsingHeader("Cube 2"))
        {
            ImGui::SliderFloat3("Pos 2", cube2Position, -5.0f, 5.0f);
            ImGui::SliderFloat3("Rot Speed 2", cube2RotationSpeed, -0.1f, 0.1f);
        }

        ImGui::End();
    }





    void OnRender()
    {
        // get the current back buffer index
        uint32_t backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

        // Reset the command allocator and command list for the current frame
        m_commandAlloc->Reset();
        m_commandList->Reset(m_commandAlloc, nullptr);


		//std::cout << "Rendering frame: " << backBufferIndex << std::endl;

        // get a handle to the depth/stencil buffer
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dpvDescriptorHeap.GetCPUHandle(0);

        // get a handle to the render target view (RTV) for the current back buffer
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHeap.GetCPUHandle(backBufferIndex);

        // set the render target for the output merger stage (the output of the pipeline)
        // Set the render target view (RTV) for the current back buffer
        // Set the depth/stencil view (DSV) for the current frame
        m_commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);


        // Clear the render target
        float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);


        // Clear the depth/stencil buffer
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);


        ID3D12DescriptorHeap* heaps[] = { m_cbvDescriptorHeap.heap };

        m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        m_commandList->SetGraphicsRootSignature(m_rootSignature);
        m_commandList->SetGraphicsRootShaderResourceView(0, m_vertexBuffer->GetGPUVirtualAddress());
        m_commandList->SetGraphicsRootDescriptorTable(1, m_cbvDescriptorHeap.GetGPUHandle(0));

        m_commandList->SetPipelineState(m_pipelineState);
        m_commandList->IASetIndexBuffer(&m_indexBufferView);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);




        //  second cube
        m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        m_commandList->SetGraphicsRootSignature(m_rootSignature);
        m_commandList->SetGraphicsRootShaderResourceView(0, m_vertexBuffer->GetGPUVirtualAddress());
        m_commandList->SetGraphicsRootDescriptorTable(1, m_cbvDescriptorHeap.GetGPUHandle(1));

        m_commandList->SetPipelineState(m_pipelineState);
        m_commandList->IASetIndexBuffer(&m_indexBufferView);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);



        m_gui.NewFrame();

		OnRenderGui();

        m_gui.Render(m_commandList);



        m_commandList->Close();

        // Execute the command list
        ID3D12CommandList* ppCommandLists[] = { m_commandList };
        m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

        // Present the frame
        m_swapChain->Present(1, 0);

        // Signal and increment the fence value.
        // This will be used to synchronize the GPU and CPU.
        WaitForPreviousFrame();
    }





    void OnResize(uint32_t newWidth, uint32_t newHeight)
    {
        m_width = newWidth;
        m_height = newHeight;

        m_commandQueue->Signal(m_fence, ++m_fenceValue);
        WaitForPreviousFrame();

        for (int i = 0; i < m_frameCount; ++i)
        {
            if (m_renderTargets[i]) m_renderTargets[i]->Release();
        }
        if (m_depthStencilBuffer) m_depthStencilBuffer->Release();

        // Resize swap chain
        m_swapChain->ResizeBuffers(m_frameCount, m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

        CreateRenderTargetViews();

        CreateDepthBuffer();

        // m_viewport & Scissor Rect
        m_viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
        m_scissorRect = { 0, 0, static_cast<long>(m_width), static_cast<long>(m_height) };

        CreateCamera();
    }


    void Cleanup()
    {

        if (m_indexBuffer)
            m_indexBuffer->Release();

        if (m_vertexBuffer)
            m_vertexBuffer->Release();

        if (m_depthStencilBuffer)
            m_depthStencilBuffer->Release();


        if (m_pipelineState)
            m_pipelineState->Release();

        for (uint32_t i = 0; i < 2; ++i)
            if (m_renderTargets[i])
                m_renderTargets[i]->Release();

        m_rtvDescriptorHeap.Destroy();
        m_dpvDescriptorHeap.Destroy();

        if (m_swapChain)
            m_swapChain->Release();

        if (m_commandQueue)
            m_commandQueue->Release();

        if (m_device)
            m_device->Release();

        if (m_commandAlloc)
            m_commandAlloc->Release();

        if (m_commandList)
            m_commandList->Release();
    }
private:
    ID3D12Resource* m_vertexBuffer = nullptr;
        ID3D12Resource* m_indexBuffer = nullptr;
        D3D12_INDEX_BUFFER_VIEW m_indexBufferView {};
        uint32_t m_indexCount {};
        ID3D12Resource* m_constantBuffer = nullptr;
        D3D12_CONSTANT_BUFFER_VIEW_DESC m_constantBufferDesc {};
        uint32_t m_constantBufferSize {};
        ID3D12Resource* m_constantBuffer2 = nullptr;
        D3D12_CONSTANT_BUFFER_VIEW_DESC m_constantBuffer2Desc {};
        uint32_t m_constantBuffer2Size {};

        uint32_t m_width { };
        uint32_t m_height { };
        const uint32_t m_frameCount { 3 };

        // Render m_device and resources
        ID3D12Device* m_device = nullptr;
        ID3D12CommandQueue* m_commandQueue = nullptr;
        IDXGISwapChain3* m_swapChain = nullptr;
        ID3D12Resource* m_renderTargets[3];
        ID3D12CommandAllocator* m_commandAlloc = nullptr;
        ID3D12GraphicsCommandList* m_commandList = nullptr;

        ID3D12Resource* m_depthStencilBuffer; // This is the memory for our depth buffer. it will also be used for a stencil buffer in a later tutorial

        // Pipeline state and root signature
        ID3D12PipelineState* m_pipelineState = nullptr;
        ID3D12RootSignature* m_rootSignature = nullptr;
        ShaderCompilerByteCode m_shaderCompiler {};

        // Set the m_viewport and scissor rect
        D3D12_VIEWPORT m_viewport = { };
        D3D12_RECT m_scissorRect = { };

        DescriptorHeap m_rtvDescriptorHeap {};  // This is a heap for our render target view descriptor
        DescriptorHeap m_dpvDescriptorHeap {};  // This is a heap for our depth/stencil buffer descriptor
        DescriptorHeap m_cbvDescriptorHeap {}; // This is a heap for our constant buffer view descriptor
        DescriptorHeap m_guiDescriptorHeap {}; // This is a heap for our ImGui descriptor heap

    GUI m_gui; // m_gui object for ImGui integration

        // Synchronization objects.
        UINT m_frameIndex;
        HANDLE m_fenceEvent;
        ID3D12Fence* m_fence;
        UINT64 m_fenceValue;

};

int main()
{
    WindowApp win = { 1280u, 720u, L"DX12 BasicTexture" };
    win.Initialize(GetModuleHandle(nullptr));

    Render render {};
    render.Initialize(win.GetHWND(), win.GetWidth(), win.GetHeight());


    win.SetOnUpdate([&render]
        {
        });




    win.SetOnRender([&render]
        {
            render.OnUpdate();
            render.OnRender();
        });


    win.SetOnResize([&render](UINT w, UINT h)
        {
            render.OnResize(w, h);
        });


    win.Run();

    return 0;
}
