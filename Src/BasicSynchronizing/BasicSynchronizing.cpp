// BasicSynchronizing.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

//#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <d3dcompiler.h>
#include "ShaderCompiler.h"
#include "WindowApp.h"
#include "Graphics/DescriptorHeap.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


using namespace Graphics;

class Render
{
public:

    class VertexBuffer
    {
    public:
        VertexBuffer() = default;

        ID3D12Resource* m_vertexBuffer = nullptr;
        D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

        uint32_t stride = { };
        uint32_t size = { };

        void Destroy()
        {
            if (m_vertexBuffer)
            {
                m_vertexBuffer->Release();
                m_vertexBuffer = nullptr;
            }
        }
    } vertexBuffer;

    class IndexBuffer
    {
    public:
        IndexBuffer() = default;
        ID3D12Resource* m_indexBuffer = nullptr;
        D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
        uint32_t stride = { };
        uint32_t size = { };
        uint32_t m_indexCount { };

        void Destroy()
        {
            if (m_indexBuffer)
            {
                m_indexBuffer->Release();
                m_indexBuffer = nullptr;
            }
        }

    } indexBuffer;

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


        tempSwapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain));
        factory->Release();


        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAlloc));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAlloc, nullptr, IID_PPV_ARGS(&m_commandList));

        m_commandList->Close();

        CreateSynchronizationObjects();
        CreateRenderTargetViews();
        CreateDepthBuffer();
        CreatePipeline();
        CreateVertexBuffer();
        CreateIndexBuffer();
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
        auto vertexShaderBlob = m_shaderCompiler.Compile(L"../../../../Assets/Shaders/BasicSync/VertexShader.hlsl", L"VS", L"vs_6_0");
        auto pixelShaderBlob = m_shaderCompiler.Compile(L"../../../../Assets/Shaders/BasicSync/PixelShader.hlsl", L"PS", L"ps_6_0");


        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = 0;
        rootSigDesc.pParameters = nullptr;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* sigBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;

        D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlob);
        m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));

        // --- PIPELINE STATE ---
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_rootSignature;

        psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
        psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };



        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        psoDesc.InputLayout.NumElements = _countof(inputElementDescs);
        psoDesc.InputLayout.pInputElementDescs = inputElementDescs;

        // Rasterizer state manual
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        rasterizerDesc.FrontCounterClockwise = false;
        rasterizerDesc.DepthClipEnable = true;
        psoDesc.RasterizerState = rasterizerDesc;

        // Blend state manual
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = false;
        blendDesc.IndependentBlendEnable = false;
        blendDesc.RenderTarget[0].BlendEnable = false;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.BlendState = blendDesc;


        // Depth stencil
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable = true;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable = false;


		// Set the depth stencil state in the pipeline state object (PSO) description
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    }


    void CreateVertexBuffer()
    {
        // Define vertices for a triangle
        struct Vertex
        {
            float position[4];
            float color[4];
        };

        Vertex vertices[] =
        {
            { -0.20f, -0.55f, 0.0f, 1.0f,   0.8f, 0.2f, 0.0f, 1.0f },
            { -0.55f,  0.55f, 0.0f, 1.0f,   0.8f, 0.2f, 0.0f, 1.0f },
            {  0.15f,  0.55f, 0.0f, 1.0f,   0.8f, 0.2f, 0.0f, 1.0f },

            {  0.00f, -0.40f, 0.0f, 1.0f,   0.3f, 1.0f, 0.3f, 1.0f },
            { -0.35f,  0.70f, 0.0f, 1.0f,   0.3f, 1.0f, 0.3f, 1.0f },
            {  0.35f,  0.70f, 0.0f, 1.0f,   0.3f, 1.0f, 0.3f, 1.0f },

            {  0.20f, -0.55f, 0.0f, 1.0f,   0.3f, 0.3f, 1.0f, 1.0f },
            { -0.15f,  0.55f, 0.0f, 1.0f,   0.3f, 0.3f, 1.0f, 1.0f },
            {  0.55f,  0.55f, 0.0f, 1.0f,   0.3f, 0.3f, 1.0f, 1.0f },
        };

        vertexBuffer.size = sizeof(vertices);
        vertexBuffer.stride = sizeof(Vertex);

        // Create vertex buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = vertexBuffer.size;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer.m_vertexBuffer));


        // Copy vertex data to the vertex buffer
        void* pData;
        vertexBuffer.m_vertexBuffer->Map(0, nullptr, &pData);
        memcpy(pData, vertices, sizeof(vertices));
        vertexBuffer.m_vertexBuffer->Unmap(0, nullptr);


        // Initialize the vertex buffer view.
        vertexBuffer.m_vertexBufferView.BufferLocation = vertexBuffer.m_vertexBuffer->GetGPUVirtualAddress();
        vertexBuffer.m_vertexBufferView.StrideInBytes = vertexBuffer.stride;
        vertexBuffer.m_vertexBufferView.SizeInBytes = vertexBuffer.size;

    }

    void CreateIndexBuffer()
    {
        // Define indices for a triangle
        uint32_t indices[] =
        {
            0, 1, 2, 
            3, 4, 5, 
            6, 7, 8 
        };
        indexBuffer.size = sizeof(indices);
        indexBuffer.stride = sizeof(uint32_t);
        indexBuffer.m_indexCount = _countof(indices);

        // Create index buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = indexBuffer.size;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer.m_indexBuffer));


        // Copy index data to the index buffer
        void* pData;
        indexBuffer.m_indexBuffer->Map(0, nullptr, &pData);
        memcpy(pData, indices, sizeof(indices));
        indexBuffer.m_indexBuffer->Unmap(0, nullptr);


        // Initialize the index buffer view.
        indexBuffer.m_indexBufferView.BufferLocation = indexBuffer.m_indexBuffer->GetGPUVirtualAddress();
        indexBuffer.m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        indexBuffer.m_indexBufferView.SizeInBytes = indexBuffer.size;
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

        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);



        // Clear the render target
        float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);



        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        // draw the triangle
        m_commandList->SetPipelineState(m_pipelineState);
        m_commandList->IASetVertexBuffers(0, 1, &vertexBuffer.m_vertexBufferView);
        m_commandList->IASetIndexBuffer(&indexBuffer.m_indexBufferView);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->DrawIndexedInstanced(indexBuffer.m_indexCount, 1, 0, 0, 0);


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

    }


    void Cleanup()
    {
        if (indexBuffer.m_indexBuffer)
            indexBuffer.Destroy();

        if (vertexBuffer.m_vertexBuffer)
            vertexBuffer.Destroy();

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
    uint32_t m_width { };
    uint32_t m_height { };
    uint32_t m_frameCount { 2 };

    // Render m_device and resources
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12Resource* m_renderTargets[2];
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



    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ID3D12Fence* m_fence;
    UINT64 m_fenceValue;

};





int main()
{
    WindowApp win = { 1280u, 720u, L"DX12 Basic Sync" };
	win.Initialize(GetModuleHandle(nullptr));

    Render render {};
	render.Initialize(win.GetHWND(), win.GetWidth(), win.GetHeight());


    win.SetOnUpdate([&render]
    {

    });


    win.SetOnRender([&render]
    {
        render.OnRender();
    });


    win.SetOnResize([&render](UINT w, UINT h)
    {
        std::wcout << L"[RESIZE] -> " << w << L"x" << h << std::endl;
        render.OnResize(w, h);
    });


    win.Run();

    return 0;
}





