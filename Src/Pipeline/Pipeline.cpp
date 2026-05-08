// Pipeline.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include "ShaderCompiler.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


class RenderSystem
{
public:

	// This class encapsulates a descriptor heap and provides utility functions to manage it.
    struct DescriptorHeap
    {
        ID3D12DescriptorHeap* heap = nullptr;
        uint32_t descriptorSize = 0;
    };

    RenderSystem() = default;

    void Initialize(HWND hwnd, uint32_t width, uint32_t Heigh)
    {
        m_width = width;
        m_height = Heigh;

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

        // Create RTV descriptor heap
        InitializeDescriptorHeap(&m_rtvHeap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_frameCount);

        for (uint32_t i = 0; i < m_frameCount; ++i)
        {
            ID3D12Resource* backBuffer = nullptr;
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetDescriptorCpuHandle(&m_rtvHeap, i);
            m_device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);

            m_renderTargets[i] = backBuffer;
        }

        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAlloc));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAlloc, nullptr, IID_PPV_ARGS(&m_commandList));
        m_commandList->Close();

        CreatePipeline();
    }

    void InitializeDescriptorHeap(DescriptorHeap* heap, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = type;
        heapDesc.NumDescriptors = numDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.NodeMask = 0;
        m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap->heap));

        heap->descriptorSize = m_device->GetDescriptorHandleIncrementSize(type);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorCpuHandle(DescriptorHeap* heap, uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * heap->descriptorSize;
        return handle;
    }

    void DestroyDescriptorHeap(DescriptorHeap* heap)
    {
        if (heap->heap)
        {
            heap->heap->Release();
            heap->heap = nullptr;
            heap->descriptorSize = 0;
        }
	}

    void CreatePipeline()
    {
        auto vertexShaderBlob = m_shaderCompiler.Compile(L"../../../../Assets/Shaders/Pipeline/VertexShader.hlsl", L"VS", L"vs_6_0");
        auto pixelShaderBlob = m_shaderCompiler.Compile(L"../../../../Assets/Shaders/Pipeline/PixelShader.hlsl", L"PS", L"ps_6_0");


        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = 0;
        rootSigDesc.pParameters = nullptr;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* sigBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;

        D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlob);
        m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));


        // Rasterizer state manual
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthClipEnable = TRUE;

        // Blend state manual
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


		// --- PIPELINE STATE [PSO]---
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_rootSignature;
        psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
        psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
        psoDesc.BlendState = blendDesc;
		psoDesc.DepthStencilState.DepthEnable = FALSE; // We are not using depth testing for this simple triangle example
		psoDesc.DepthStencilState.StencilEnable = FALSE; // We are not using stencil testing either
        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipeline));
	}

    void Loop()
    {
        // get the current back buffer index
        uint32_t backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

        // Reset the command allocator and command list for the current frame
        m_commandAlloc->Reset();
        m_commandList->Reset(m_commandAlloc, nullptr);

        // Set the render target view (RTV) for the current back bufferq
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetDescriptorCpuHandle(&m_rtvHeap, backBufferIndex);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

        // Clear the render target
        float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		// Set the viewport and scissor rect
        D3D12_VIEWPORT view = { 0, 0, m_width, m_height, 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, m_width, m_height };

        m_commandList->RSSetViewports(1, &view);
        m_commandList->RSSetScissorRects(1, &scissorRect);

		// draw the triangle
        m_commandList->SetPipelineState(m_pipeline);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->DrawInstanced(3, 1, 0, 0);

		// Close the command list to prepare it for execution
        m_commandList->Close();

        // Execute the command list
        ID3D12CommandList* ppCommandLists[] = { m_commandList };
        m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

        // Present the frame
        m_swapChain->Present(1, 0);
    }

    void Cleanup()
    {
        for (uint32_t i = 0; i < 2; ++i)
            if (m_renderTargets[i])
                m_renderTargets[i]->Release();

		DestroyDescriptorHeap(&m_rtvHeap);

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

        // Render device and resources
        ID3D12Device* m_device = nullptr;
        ID3D12CommandQueue* m_commandQueue = nullptr;
        IDXGISwapChain3* m_swapChain = nullptr;
        ID3D12Resource* m_renderTargets[2] { };
        ID3D12CommandAllocator* m_commandAlloc = nullptr;
        ID3D12GraphicsCommandList* m_commandList = nullptr;

        // Pipeline state and root signature
        ID3D12PipelineState* m_pipeline = nullptr;
        ID3D12RootSignature* m_rootSignature = nullptr;
        
        ShaderCompilerByteCode m_shaderCompiler { };
        DescriptorHeap m_rtvHeap{};
};

int main()
{
    uint32_t width = 1280;
    uint32_t height = 820;
    const wchar_t title[] = L"DX12 Pipeline";

    RenderSystem render {};

    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, title, nullptr };
    RegisterClassEx(&wcex);

    // Create a window
    HWND hWnd = CreateWindow(title, title, WS_OVERLAPPEDWINDOW, 100, 100, width, height, nullptr, nullptr, wcex.hInstance, nullptr);
    ShowWindow(hWnd, SW_SHOW);

    render.Initialize(hWnd, width, height);

    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            render.Loop();
        }
    }

    render.Cleanup();
    return 0;
}
