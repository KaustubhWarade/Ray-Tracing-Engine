#include "RenderEngine.h"
#include "ImGuiHelper.h"

#include "global.h"
#include "../IApplication.h"

//#include "../../Applications/Application.h"

void RenderEngine::display(void)
{
	// log
	m_commandAllocator->Reset();
	m_commandList->Reset(m_commandAllocator.Get(), nullptr);
	// populate command list here do not close command list.
	m_pApplication->PopulateCommandList();

	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX12_NewFrame();
	ImGui::NewFrame();
	m_pApplication->RenderImGui();
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

	TransitionResource(m_commandList.Get(), GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->Close();

	ID3D12CommandList *ppCommandLists[] = {m_commandList.Get()};
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_swapChain->Present(1, 0);
	WaitForPreviousFrame();
}

void RenderEngine::update(void)
{
	m_pApplication->Update();
}
