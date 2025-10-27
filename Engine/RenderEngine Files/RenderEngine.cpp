#include "RenderEngine.h"
#include "win32Window.h"
#include "ImGuiHelper.h"
#include "../IApplication.h"

#include "global.h"

RenderEngine* RenderEngine::Get()
{
	static RenderEngine instance; // Created once and guaranteed to be destroyed correctly.
	return &instance;
}

bool RenderEngine::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse)
	{
		return true;
	}

	if (m_pApplication)
	{
		if (m_pApplication->HandleMessage(message, wParam, lParam))
		{
			if (m_pApplication)
			{
				m_pApplication->OnViewChanged();
			}
			return true;
		}
	}
	return false;
	
}

void RenderEngine::SetApplication(std::unique_ptr<IApplication> pApp)
{
	m_pApplication = std::move(pApp);
}

HRESULT RenderEngine ::initialize()
{
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}


		ComPtr<ID3D12Debug1> debugController1;
		if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1))))
		{
			debugController1->SetEnableGPUBasedValidation(TRUE);
			debugController1->Release();
		}
	}
#endif 

	// create adapter get device and create command queue swapchain
	EXECUTE_AND_LOG_RETURN(CreateDeviceCommandQueueAndSwapChain());
	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	EXECUTE_AND_LOG_RETURN(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValue = 1;

	// create rtv and dsb heaps
	EXECUTE_AND_LOG_RETURN(CreateRenderBufferHeaps());

	EXECUTE_AND_LOG_RETURN(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

	// Create an event handle to use for frame synchronization.
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		errno_t err = fopen_s(&gpFile, gszLogFileName, "a+");
		fprintf(gpFile, "CreateFenceEvent() Failed.\n");
		fclose(gpFile);
		return 0;
	}

	EXECUTE_AND_LOG_RETURN(ResourceManager::Get()->Initialize(m_device.Get(), m_commandList.Get(), this));

	EXECUTE_AND_LOG_RETURN(AccelerationStructureManager::Get()->Initialize(m_device.Get(),RenderEngine::Get()));

	EXECUTE_AND_LOG_RETURN(m_pApplication->InitializeApplication(this));

	EXECUTE_AND_LOG_RETURN(initialize_imgui());

	EXECUTE_AND_LOG_RETURN(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	resize(WIN_WIDTH, WIN_HEIGHT);

	WaitForPreviousFrame();

	return S_OK;
}

HRESULT RenderEngine ::CreateDeviceCommandQueueAndSwapChain(void)
{
	HRESULT hr = S_OK;

	UINT dxgiFactoryFlags = 0;

	// create the DXGI factory
	Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory;
	EXECUTE_AND_LOG_RETURN(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&pFactory)));

	// Get the hardware adapter.
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter;
	EXECUTE_AND_LOG_RETURN(pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter)));

	// Create the D3D12 device.
	DXGI_ADAPTER_DESC1 adapterDesc = {};
	pAdapter->GetDesc1(&adapterDesc);

	//errno_t err = fopen_s(&gpFile, gszLogFileName, "a+");
	if (gpFile != NULL)
	{
		fprintf(gpFile, "Description: %ws\n", adapterDesc.Description);
		fprintf(gpFile, "Adapter Vendor Id: %u\n", adapterDesc.VendorId);
		fprintf(gpFile, "Dedicated Video Memory: %llu bytes\n", adapterDesc.DedicatedVideoMemory);

	}
	EXECUTE_AND_LOG_RETURN(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

	// Create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.NodeMask = 0;
	EXECUTE_AND_LOG_RETURN(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Memory Allocation for the command list
	EXECUTE_AND_LOG_RETURN(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC1));
	swapChainDesc.BufferCount = FRAMECOUNT;
	swapChainDesc.Width = WIN_WIDTH;
	swapChainDesc.Height = WIN_HEIGHT;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	swapChainDesc.SampleDesc.Count = 1;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
	EXECUTE_AND_LOG_RETURN(pFactory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),
		ghwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain));

	EXECUTE_AND_LOG_RETURN(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	return S_OK;
}

HRESULT RenderEngine ::CreateRenderBufferHeaps()
{
	HRESULT hr = S_OK;
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	ZeroMemory(&rtvHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	rtvHeapDesc.NumDescriptors = FRAMECOUNT;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	EXECUTE_AND_LOG_RETURN(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	// Create a RTV for each frame.
	for (UINT n = 0; n < FRAMECOUNT; n++)
	{
		EXECUTE_AND_LOG_RETURN(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));

		m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += m_rtvDescriptorSize;
		m_resourceStates[m_renderTargets[n].Get()] = D3D12_RESOURCE_STATE_COMMON;
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	ZeroMemory(&dsvHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	EXECUTE_AND_LOG_RETURN(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsbHeap)));
	return hr;
}

HRESULT RenderEngine ::resize(UINT width, UINT height)
{
	HRESULT hr = S_OK;
	// Wait for the GPU to finish executing the command list.
	WaitForPreviousFrame();

	// Reset the command allocator and command list.
	EXECUTE_AND_LOG_RETURN(m_commandAllocator->Reset());
	EXECUTE_AND_LOG_RETURN(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

	// Release the previous render target views.
	for (UINT n = 0; n < FRAMECOUNT; n++)
	{
		m_renderTargets[n].Reset();
	}
	m_depthStencilBuffer.Reset();
	// Resize the swap chain.
	EXECUTE_AND_LOG_RETURN(m_swapChain->ResizeBuffers(FRAMECOUNT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING));

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	// Create a RTV for each frame.
	for (UINT n = 0; n < FRAMECOUNT; n++)
	{
		EXECUTE_AND_LOG_RETURN(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));

		m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += m_rtvDescriptorSize;
		m_resourceStates[m_renderTargets[n].Get()] = D3D12_RESOURCE_STATE_PRESENT;
	}

	CreateDepthStencilBuffer(width, height);

	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Width = static_cast<float>(width);
	m_viewport.Height = static_cast<float>(height);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissorRect.left = 0;
	m_scissorRect.top = 0;
	m_scissorRect.right = static_cast<LONG>(width);
	m_scissorRect.bottom = static_cast<LONG>(height);
	if (m_pApplication)
	{
		m_pApplication->OnResize(width, height);
	}
	EXECUTE_AND_LOG_RETURN(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForPreviousFrame();

	return hr;
}

void RenderEngine ::WaitForPreviousFrame()
{
	const UINT64 fence = m_fenceValue;
	// Signal and increment the fence value.
	EXECUTE_AND_LOG(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		EXECUTE_AND_LOG(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

HRESULT RenderEngine ::CreateDepthStencilBuffer(UINT width, UINT height)
{
	m_depthStencilBuffer.Reset();
	
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // typless so that is can be read in shaders
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES depthHeapProps = {};
	ZeroMemory(&depthHeapProps, sizeof(D3D12_HEAP_PROPERTIES));
	depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	depthHeapProps.CreationNodeMask = 1;
	depthHeapProps.VisibleNodeMask = 1;

	EXECUTE_AND_LOG_RETURN(m_device->CreateCommittedResource(
		&depthHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&depthClearValue,
		IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.Texture2D.MipSlice = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsbHeap->GetCPUDescriptorHandleForHeapStart();
	m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, dsvHandle);
	m_resourceStates[m_depthStencilBuffer.Get()] = D3D12_RESOURCE_STATE_COMMON;

	TransitionResource(m_commandList.Get(), m_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void RenderEngine::TrackResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON)
{
	if (resource)
	{
		m_resourceStates[resource] = initialState;
	}
}

void RenderEngine::UnTrackResource(ID3D12Resource* resource)
{
	if (resource)
	{
		auto it = m_resourceStates.find(resource);
		if (it != m_resourceStates.end())
		{
			m_resourceStates.erase(it);
		}
	}
}

void RenderEngine::TransitionResource(
	ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES newState)
{
	if (m_resourceStates.count(resource) && m_resourceStates[resource] == newState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = m_resourceStates[resource];
	barrier.Transition.StateAfter = newState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	cmdList->ResourceBarrier(1, &barrier);

	// Update state
	m_resourceStates[resource] = newState;
}

HRESULT RenderEngine ::uninitialize()
{
	WaitForPreviousFrame();
	m_pApplication->OnDestroy();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
	if (m_swapChain)
	{
		m_swapChain.Reset();
	}
	if (m_commandQueue)
	{
		m_commandQueue.Reset();
	}
	if (m_device)
	{
		m_device.Reset();
	}
	if (m_commandAllocator)
	{
		m_commandAllocator.Reset();
	}
	if (m_commandList)
	{
		m_commandList.Reset();
	}
	for (UINT n = 0; n < FRAMECOUNT; n++)
	{
		if (m_renderTargets[n])
		{
			m_renderTargets[n].Reset();
		}
	}
	if (m_rtvHeap)
	{
		m_rtvHeap.Reset();
	}
	if (m_depthStencilBuffer)
	{
		m_depthStencilBuffer.Reset();
	}
	if (m_dsbHeap)
	{
		m_dsbHeap.Reset();
	}
	return S_OK;
}