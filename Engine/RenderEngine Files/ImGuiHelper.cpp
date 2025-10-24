#include "global.h"
#include "win32Window.h"
#include "ImGuiHelper.h"
#include "RenderEngine.h"
#include "../CoreHelper Files/ResourceManager.h"

float rendertime = 0.0f;

HRESULT RenderEngine::initialize_imgui()
{
	HRESULT hr = S_OK;
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	//D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	//srvHeapDesc.NumDescriptors = 100; // More than enough for UI
	//srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	//srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	//EXECUTE_AND_LOG_RETURN(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_imguiSrvHeap)));
	//m_imguiSrvHeap->SetName(L"ImGui SRV Heap");

	ImGui_ImplWin32_Init(ghwnd);
	// Setup Platform/Renderer backends
	//ImGui_ImplDX12_InitInfo init_info = {};
	//init_info.Device = m_device.Get();
	//init_info.CommandQueue = m_commandQueue.Get();
	//init_info.NumFramesInFlight = FRAMECOUNT;
	//init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Or your render target format.
	//init_info.SrvDescriptorHeap = ResourceManager::Get()->GetMainCbvSrvUavHeap();
	//DescriptorAllocation imguiFontHandle = ResourceManager::Get()->m_cbvSrvUavAllocator.Allocate(1);
	//init_info.LegacySingleSrvCpuDescriptor = imguiFontHandle.CpuHandle;
	//init_info.LegacySingleSrvGpuDescriptor = imguiFontHandle.GpuHandle;
	//ImGui_ImplDX12_Init(&init_info);

	ResourceManager* pResourceManager = ResourceManager::Get();
	pResourceManager->m_imguiFontSrvHandle = pResourceManager->m_generalPurposeHeapAllocator.AllocateSingle();

	ImGui_ImplDX12_Init(
		m_device.Get(),
		FRAMECOUNT,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		pResourceManager->GetMainCbvSrvUavHeap(),
		pResourceManager->m_imguiFontSrvHandle.CpuHandle,
		pResourceManager->m_imguiFontSrvHandle.GpuHandle
	);

	io.Fonts->GetTexDataAsRGBA32(nullptr, nullptr, nullptr);
	return hr;
}
