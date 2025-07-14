#include "global.h"
#include "win32Window.h"
#include "ImGuiHelper.h"
#include "RenderEngine.h"

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_ImGuisrvHeap;
DescriptorHeapAllocator g_ImGuiSrvHeapAllocator;

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


	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 10;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	EXECUTE_AND_LOG_RETURN(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_ImGuisrvHeap)));

	g_ImGuiSrvHeapAllocator.Create(m_device.Get(), g_ImGuisrvHeap.Get());
	g_ImGuiSrvHeapAllocator.Heap->SetName(L"Imgui descheap");
	ImGui_ImplWin32_Init(ghwnd);
	// Setup Platform/Renderer backends
	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = m_device.Get();
	init_info.CommandQueue = m_commandQueue.Get();
	init_info.NumFramesInFlight = FRAMECOUNT;
	init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Or your render target format.
	DescriptorAllocation imguiFontHandle = m_cbvSrvUavAllocator.Allocate(1);
	init_info.SrvDescriptorHeap = m_cbvSrvUavAllocator.GetHeap();
	init_info.LegacySingleSrvCpuDescriptor = imguiFontHandle.CpuHandle;
	init_info.LegacySingleSrvGpuDescriptor = imguiFontHandle.GpuHandle;
	ImGui_ImplDX12_Init(&init_info);
	return hr;
}
