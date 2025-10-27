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
	auto mySphereRaytracingScene = std::make_unique<SceneOne>();
	AddScene("CornellBoxRayTracer", std::move(mySphereRaytracingScene));
	m_CurrentSceneName = "CornellBoxRayTracer";
	EXECUTE_AND_LOG_RETURN(ResourceManager::Get()->Initialize(m_pRenderer->GetDevice(), m_pRenderer->m_commandList.Get(), m_pRenderer));
	return SwitchToScene("CornellBoxRayTracer");
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
	ImGui::Begin("Application", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	const char* currentSceneNameCStr = m_CurrentSceneName.c_str();

	if (ImGui::BeginCombo("Select Scene", currentSceneNameCStr))
	{
		for (auto const& [name, scene] : m_scenes) 
		{
			const bool is_selected = (m_pCurrentScene == scene.get()); 

			if (ImGui::Selectable(name.c_str(), is_selected))
			{
				SwitchToScene(name);
			}

			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo(); 
	}

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
	m_CurrentSceneName = name;

	return m_pCurrentScene->Initialize(m_pRenderer);
}


