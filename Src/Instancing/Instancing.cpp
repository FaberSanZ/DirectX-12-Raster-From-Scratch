// Instancing.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <random> // std::random_device, std::mt19937, std::uniform_real_distribution
#include <d3dcompiler.h>
#include "ShaderCompiler.h"
#include "Desktop/Window.h"
#include "Graphics/DescriptorHeap.h"
#include <DirectXMath.h>
#include <Graphics/GUI.h>

// Link necessary d3d12 libraries.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


// Using namespaces for convenience.
using namespace Graphics;
using namespace Desktop;
using namespace DirectX;

class Render
{
public:
    Render() = default;


    struct CameraBuffer
    {
        XMMATRIX view;
        XMMATRIX projection;
    } cameraBuffer;

    struct Camera
    {
        float m_positionX, m_positionY, m_positionZ;
        float m_rotationX, m_rotationY, m_rotationZ;

        XMMATRIX m_viewMatrix;

        Camera()
        {
            m_positionX = 0.0f;
            m_positionY = 0.0f;
            m_positionZ = 0.0f;

            m_rotationX = 0.0f;
            m_rotationY = 0.0f;
            m_rotationZ = 0.0f;

            m_viewMatrix = XMMatrixIdentity();
        }

        void SetPosition(float x, float y, float z)
        {
            m_positionX = x;
            m_positionY = y;
            m_positionZ = z;
            return;
        }


        void SetRotation(float x, float y, float z)
        {
            m_rotationX = x;
            m_rotationY = y;
            m_rotationZ = z;
            return;
        }


        XMFLOAT3 GetPosition()
        {
            return XMFLOAT3(m_positionX, m_positionY, m_positionZ);
        }


        XMFLOAT3 GetRotation()
        {
            return XMFLOAT3(m_rotationX, m_rotationY, m_rotationZ);
        }


        void Render()
        {
            XMFLOAT3 up, position, lookAt;
            XMVECTOR upVector, positionVector, lookAtVector;
            float yaw, pitch, roll;
            XMMATRIX rotationMatrix;


            // Setup the vector that points upwards.
            up.x = 0.0f;
            up.y = 1.0f;
            up.z = 0.0f;

            // Load it into a XMVECTOR structure.
            upVector = XMLoadFloat3(&up);

            // Setup the position of the camera in the world.
            position.x = m_positionX;
            position.y = m_positionY;
            position.z = m_positionZ;

            // Load it into a XMVECTOR structure.
            positionVector = XMLoadFloat3(&position);

            // Setup where the camera is looking by default.
            lookAt.x = 0.0f;
            lookAt.y = 0.0f;
            lookAt.z = 1.0f;

            // Load it into a XMVECTOR structure.
            lookAtVector = XMLoadFloat3(&lookAt);

            // Set the yaw (Y axis), pitch (X axis), and roll (Z axis) rotations in radians.
            pitch = m_rotationX * 0.0174532925f;
            yaw = m_rotationY * 0.0174532925f;
            roll = m_rotationZ * 0.0174532925f;

            // Create the rotation matrix from the yaw, pitch, and roll values.
            rotationMatrix = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

            // Transform the lookAt and up vector by the rotation matrix so the view is correctly rotated at the origin.
            lookAtVector = XMVector3TransformCoord(lookAtVector, rotationMatrix);
            upVector = XMVector3TransformCoord(upVector, rotationMatrix);

            // Translate the rotated camera position to the location of the viewer.
            lookAtVector = XMVectorAdd(positionVector, lookAtVector);

            // Finally create the view matrix from the three updated vectors.
            m_viewMatrix = XMMatrixLookAtLH(positionVector, lookAtVector, upVector);

            return;
        }


        XMMATRIX GetViewMatrix() const { return m_viewMatrix; }


    };


    void Initialize(HWND hwnd, uint32_t width, uint32_t Heigh)
    {


		// Set screen width and height
        m_width = width;
        m_height = Heigh;


        m_viewport = { 0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f };
        m_scissorRect = { 0, 0, (long)m_width, (long)m_height };


        IDXGIFactory4* factory = nullptr;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));



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
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        IDXGISwapChain1* tempSwapChain = nullptr;
        factory->CreateSwapChainForHwnd(m_commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, &tempSwapChain);

        // 
        tempSwapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain));
        factory->Release();

        // Create command allocator and command list
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAlloc));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAlloc, nullptr, IID_PPV_ARGS(&m_commandList));

        m_commandList->Close();


        // Create ImGui context
        m_gui.Initialize(m_device, m_commandQueue, m_frameCount, hwnd);


        CreateSynchronizationObjects();
        CreateRenderTargetViews();
        CreateDepthBuffer();
        CreatePipeline();
        CreateMesh();
        CreateCamera();


        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

		m_rotationSpeeds.resize(m_drawCallCount);
        for (int i = 0; i < m_drawCallCount; i++)
            m_rotationSpeeds[i] = { dist(gen), dist(gen), dist(gen) };

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
        auto vertexShaderBlob = m_shaderCompiler.Compile(L"../../../../Assets/Shaders/Instancing/VertexShader.hlsl", L"VS", L"vs_6_0");
        auto pixelShaderBlob = m_shaderCompiler.Compile(L"../../../../Assets/Shaders/Instancing/PixelShader.hlsl", L"PS", L"ps_6_0");


        D3D12_ROOT_PARAMETER pixelConstantRootParam = {};
        pixelConstantRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        pixelConstantRootParam.Constants.Num32BitValues = 4;
        pixelConstantRootParam.Constants.ShaderRegister = 0; // b0
        pixelConstantRootParam.Constants.RegisterSpace = 0;
        pixelConstantRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_PARAMETER vertexBufferRootParam = {};
        vertexBufferRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        vertexBufferRootParam.Descriptor.ShaderRegister = 1; // t1
        vertexBufferRootParam.Descriptor.RegisterSpace = 0;
        vertexBufferRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_PARAMETER cameraRootParam = {};
        cameraRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        cameraRootParam.Descriptor.ShaderRegister = 2; // b2
        cameraRootParam.Descriptor.RegisterSpace = 0;
        cameraRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_PARAMETER instanceRootParam = {};
        instanceRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        instanceRootParam.Descriptor.ShaderRegister = 3; // t3
        instanceRootParam.Descriptor.RegisterSpace = 0;
        instanceRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_PARAMETER rootParams[] =
        {
            pixelConstantRootParam,
            vertexBufferRootParam,
            cameraRootParam,
            instanceRootParam
        };

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = _countof(rootParams);
        rootSigDesc.pParameters = rootParams;
        rootSigDesc.NumStaticSamplers = 0;
        rootSigDesc.pStaticSamplers = nullptr;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;




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

        m_vertexBuffer = CreateBuffer(vertices, sizeof(vertices), D3D12_RESOURCE_FLAG_NONE);
        m_indexBuffer = CreateBuffer(indices, sizeof(indices), D3D12_RESOURCE_FLAG_NONE);

        // Initialize the index buffer view.
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_indexBufferView.SizeInBytes = sizeof(indices);
        m_indexCount = _countof(indices);


        m_constantBuffer = CreateBuffer(nullptr, (sizeof(CameraBuffer) + 255) & ~255, D3D12_RESOURCE_FLAG_NONE);

		m_instanceBuffer = CreateBuffer(nullptr, m_drawCallCount * sizeof(XMMATRIX), D3D12_RESOURCE_FLAG_NONE);
    }



    ID3D12Resource* CreateBuffer(const void* data, uint32_t size, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
    {
        ID3D12Resource* buffer = nullptr;
        // Create a committed resource for the buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment = 0;
        bufferDesc.Width = size;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.SampleDesc.Quality = 0;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags = flags;
        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));

        // Copy data to the buffer
        if (data)
        {
            void* mappedData = nullptr;
            D3D12_RANGE readRange = { 0, 0 }; // We do not intend to read from this resource on the CPU.
            buffer->Map(0, &readRange, &mappedData);
            memcpy(mappedData, data, size);
            buffer->Unmap(0, nullptr);
        }
        return buffer;
    }

    void CreateCamera()
    {
        m_camera.SetPosition(0.0f, 0.0f, -5.0f);
        m_camera.SetRotation(0.0f, 0.0f, 0.0f);
        m_camera.Render();


        // Setup the projection matrix.
        auto fieldOfView = 3.141592654f / 4.0f;
        float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);

        // Create the projection matrix for 3D rendering.
        cameraBuffer.projection = DirectX::XMMatrixTranspose(XMMatrixPerspectiveFovLH(fieldOfView, aspect, m_screenNear, m_screenDepth));
        cameraBuffer.view = DirectX::XMMatrixTranspose(m_camera.GetViewMatrix());
    }

    void OnUpdate()
    {
        m_camera.Render();
        cameraBuffer.view = DirectX::XMMatrixTranspose(m_camera.GetViewMatrix());


        void* mappedData = nullptr;
        m_constantBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, &cameraBuffer, sizeof(CameraBuffer));
        m_constantBuffer->Unmap(0, nullptr);


        GenerateCubes();



        // Update instance buffer with the world matrix for the current cube
        void* mappedData2 = nullptr;
        m_instanceBuffer->Map(0, nullptr, &mappedData2);
        memcpy(mappedData2, m_instanceData.data() , sizeof(XMMATRIX) * m_instanceData.size());
        m_instanceBuffer->Unmap(0, nullptr);
    }


    void OnRenderGui()
    {
        uint32_t totaltriangles = m_drawCall * (m_indexCount / 3);
        ImGui::Begin("Camera Control");
		ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
		ImGui::Text("Screen Size: %ux%u", m_width, m_height);
        ImGui::Text("Draw Calls: %u", m_drawCallCount);
		ImGui::Text("Total Triangles: %u", totaltriangles);

        m_drawCall = 0;
		totaltriangles = 0;

        ImGui::NewLine();
        ImGui::Checkbox("VSync", &m_vSync);
        ImGui::NewLine();


		// Sliders for camera position and rotation
		ImGui::Text("x: %.2f,     y: %.2f,     z: %.2f", m_camera.GetPosition().x, m_camera.GetPosition().y, m_camera.GetPosition().z);
		ImGui::SliderFloat3("Camera Position", &m_camera.m_positionX, -30.0f, 30.0f);
        ImGui::NewLine();
        ImGui::Text("x: %.2f,     y: %.2f,     z: %.2f", m_camera.GetRotation().x, m_camera.GetRotation().y, m_camera.GetRotation().z);
		ImGui::SliderFloat3("Camera Rotation", &m_camera.m_rotationX, -180.0f, 180.0f);

        ImGui::NewLine();
        // Slider for dimension
		ImGui::Text("Dimension: %.2f", m_dimension);
        ImGui::SliderFloat("Dimension", &m_dimension, -10.0f, 10.0f);

		// Button to update camera
        if (ImGui::Button("Update Camera"))
        {
            m_camera.SetPosition(0.0f, 0, -25.0f);
            m_camera.SetRotation(0.0f, 0.0f, 0.0f);
        }

		// Button to reset rotation
        if (ImGui::Button("Reset"))
        {
			m_rotation = 0.0f;
        }

        ImGui::End();
    }


	// Generate cubes in a grid pattern
    void GenerateCubes()
    {
        m_instanceData.clear();
        m_instanceData.reserve(m_drawCallCount);

        m_rotation += 0.02f;

        uint32_t dim = static_cast<uint32_t>(std::cbrt(m_drawCallCount)); // using cube root to determine the dimension of the grid
        XMFLOAT3 offset = { m_dimension, m_dimension, m_dimension };

        float halfDimOffsetX = (dim * offset.x) / 2.0f;
        float halfDimOffsetY = (dim * offset.y) / 2.0f;
        float halfDimOffsetZ = (dim * offset.z) / 2.0f;

        for (uint32_t x = 0; x < dim; ++x)
        {
            for (uint32_t y = 0; y < dim; ++y)
            {
                for (uint32_t z = 0; z < dim; ++z)
                {
                    uint32_t index = x * dim * dim + y * dim + z;


                    XMFLOAT3 position =
                    {
                        -halfDimOffsetX + offset.x / 2.0f + x * offset.x,
                        -halfDimOffsetY + offset.y / 2.0f + y * offset.y,
                        -halfDimOffsetZ + offset.z / 2.0f + z * offset.z
                    };


                    XMFLOAT3 rotation =
                    {
                        m_rotation * m_rotationSpeeds[index].x,
                        m_rotation * m_rotationSpeeds[index].y,
                        m_rotation * m_rotationSpeeds[index].z
                    };


                    XMMATRIX trans = XMMatrixTranslation(position.x, position.y, position.z);
                    XMMATRIX rot = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
                    XMMATRIX world = XMMatrixTranspose(rot * trans);

                    m_instanceData.push_back(world);
                }
            }
        }
    }



    void OnRender()
    {
        // get the current back buffer index
        uint32_t backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

        // Reset the command allocator and command list for the current frame
        m_commandAlloc->Reset();
        m_commandList->Reset(m_commandAlloc, nullptr);


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


        // Tender the scene
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
        m_commandList->SetGraphicsRootSignature(m_rootSignature);
        m_commandList->SetPipelineState(m_pipelineState);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_commandList->SetGraphicsRoot32BitConstants(0, 4, &color, 0);
        m_commandList->SetGraphicsRootShaderResourceView(1, m_vertexBuffer->GetGPUVirtualAddress());
        m_commandList->SetGraphicsRootConstantBufferView(2, m_constantBuffer->GetGPUVirtualAddress());
        m_commandList->SetGraphicsRootShaderResourceView(3, m_instanceBuffer->GetGPUVirtualAddress());

        m_commandList->IASetIndexBuffer(&m_indexBufferView);
        m_commandList->DrawIndexedInstanced(m_indexCount, m_instanceData.size(), 0, 0, 0);

        // Start the Dear ImGui frame
        m_gui.NewFrame();
        // Call the function to render the m_gui
        OnRenderGui();
        // Render ImGui
        m_gui.Render(m_commandList);


        // Close the command list
        m_commandList->Close();

        // Execute the command list
        ID3D12CommandList* ppCommandLists[] = { m_commandList };
        m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

        // Present the frame
        m_vSync ? m_swapChain->Present(1, 0) : m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
        

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
        if (m_depthStencilBuffer) 
            m_depthStencilBuffer->Release();

        // Resize swap chain
        m_swapChain->ResizeBuffers(m_frameCount, m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

        CreateRenderTargetViews();

        CreateDepthBuffer();

        // m_viewport & Scissor Rect
        m_viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
        m_scissorRect = { 0, 0, static_cast<long>(m_width), static_cast<long>(m_height) };


        // Setup the projection matrix.
        auto fieldOfView = 3.141592654f / 4.0f;
        float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);

        // Create the projection matrix for 3D rendering.
        cameraBuffer.projection = DirectX::XMMatrixTranspose(XMMatrixPerspectiveFovLH(fieldOfView, aspect, m_screenNear, m_screenDepth));

    }


    void Cleanup()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (m_indexBuffer)
            m_indexBuffer->Release();

        if(m_vertexBuffer)
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


    uint32_t m_width{ };
    uint32_t m_height{ };
    uint32_t m_frameCount{ 2 };

    // Render m_device and resources
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12Resource* m_renderTargets[2];
    ID3D12CommandAllocator* m_commandAlloc = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;

    // vertex pulling
    ID3D12Resource* m_vertexBuffer = nullptr;

    // index buffer and view
    ID3D12Resource* m_indexBuffer = nullptr;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    uint32_t m_indexCount{ };

    // constant buffer
    ID3D12Resource* m_constantBuffer = nullptr;

	ID3D12Resource* m_instanceBuffer = nullptr;


    // This is the memory for our depth buffer. it will also be used for a stencil buffer in a later tutorial
    ID3D12Resource* m_depthStencilBuffer;

        // Pipeline state and root signature
    ID3D12PipelineState* m_pipelineState = nullptr;
    ID3D12RootSignature* m_rootSignature = nullptr;
    ShaderCompilerByteCode m_shaderCompiler{};

    // using m_viewport and scissor rect to define the area we will render to
    D3D12_VIEWPORT m_viewport = { };
    D3D12_RECT m_scissorRect = { };

    // This is a heap for our render target view descriptor
    DescriptorHeap m_rtvDescriptorHeap{};

    // This is a heap for our depth/stencil buffer descriptor
    DescriptorHeap m_dpvDescriptorHeap{};

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ID3D12Fence* m_fence;
    UINT64 m_fenceValue;

    // m_gui object for ImGui integration
    GUI m_gui;


    // Camera
    Camera m_camera{};
    float m_screenDepth = 1000.0f;
    float m_screenNear = 0.1f;


    // Real draw-call stress sample.
    // Not using instancing on purpose.
    // 4k keeps the tutorial sane and easy to debug.
    uint32_t m_drawCallCount = 256 * 256 * 8;
    std::vector<DirectX::XMFLOAT3> m_rotationSpeeds;
	std::vector<XMMATRIX> m_instanceData;


    float m_rotation = 0.0f;
    float m_dimension = 0;
    uint32_t m_drawCall = 0;

    bool m_vSync = false;


};


int main()
{
    WindowApp win = { 1280u, 720u, L"DX12 Instancing" };
    win.Initialize(GetModuleHandle(nullptr));

    Render render {};
    render.Initialize(win.GetHWND(), win.GetWidth(), win.GetHeight());

	// Set the update and render callbacks
    win.SetOnUpdate([&render] { render.OnUpdate(); });
    win.SetOnRender([&render] { render.OnRender(); });

	// Set the resize callback
    win.SetOnResize([&render](UINT w, UINT h) { render.OnResize(w, h); });


    win.Run();

    return 0;
}





