#include "Application.h"

#include "RenderEngine Files/ImGuiHelper.h"
#include "RenderEngine Files/RenderEngine.h"
#include "CoreHelper Files/Helper.h"

using namespace DirectX;

std::unique_ptr<IApplication> CreateApplication()
{
	return std::make_unique<Application>();
}

HRESULT Application::InitializeApplication(RenderEngine* pRenderEngine)
{
	m_pRenderer = pRenderEngine;
	auto myRaytracingScene = std::make_unique<SceneOne>();
	AddScene("SphereRayTracer", std::move(myRaytracingScene));
	
	return SwitchToScene("SphereRayTracer");
}

void Application::OnResize(UINT width, UINT height)
{
	if (m_pCurrentScene)
	{
		m_pCurrentScene->OnResize(width, height);
	}
}

bool Application::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_pCurrentScene)
	{
		if (m_pCurrentScene->HandleMessage(message, wParam, lParam))
		{
			return true; 
		}
	}

	return false;
}

void Application::OnViewChanged()
{
	if (m_pCurrentScene)
	{
		m_pCurrentScene->OnViewChanged();
	}
}


void Application::PopulateCommandList(void)
{
	m_pRenderer->m_commandList->RSSetViewports(1, &m_pRenderer->m_viewport);
	m_pRenderer->m_commandList->RSSetScissorRects(1, &m_pRenderer->m_scissorRect);

	if (m_pCurrentScene)
	{
		m_pCurrentScene->PopulateCommandList();
	}
}

void Application::Update(void)
{
	if (m_pCurrentScene)
	{
		m_pCurrentScene->Update();
	}
}

void Application::RenderImGui()
{
	ImGui::Begin("Ray Tracer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Render Time : %.3fms", m_LastRenderTime);
	XMFLOAT3 pos = m_pRenderer->m_camera.GetPosition3f();
	ImGui::Text("Camera Position : (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
	ImGui::Text("Camera Forward Direction : (%.2f, %.2f, %.2f)", m_pRenderer->m_camera.GetForwardDirection3f().x, m_pRenderer->m_camera.GetForwardDirection3f().y, m_pRenderer->m_camera.GetForwardDirection3f().z);

	ImGui::End();

	if (m_pCurrentScene)
	{
		m_pCurrentScene->RenderImGui();
	}
}

void Application::OnDestroy(void)
{

	for (auto const& [name, scene] : m_scenes)
	{
		if (scene)
		{
			scene->OnDestroy();
		}
	}
	m_scenes.clear();
	m_pCurrentScene = nullptr;
}

void Application::AddScene(const std::string& name, std::unique_ptr<IScene> scene)
{
	if (scene)
	{
		m_scenes[name] = std::move(scene);
	}
}

HRESULT Application::SwitchToScene(const std::string& name)
{
	auto it = m_scenes.find(name);
	if (it == m_scenes.end())
	{
		return E_FAIL;
	}

	m_pCurrentScene = it->second.get();

	return m_pCurrentScene->Initialize(m_pRenderer);
}


