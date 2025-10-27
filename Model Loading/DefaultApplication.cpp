#include "DefaultApplication.h"

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
	// Assign Renderng Engine to class pointer for easy access
	m_pRenderer = pRenderEngine;

	EXECUTE_AND_LOG_RETURN(ResourceManager::Get()->Initialize(m_pRenderer->GetDevice(), m_pRenderer->m_commandList.Get(), m_pRenderer));
	//create scene and add to application
	auto myRaytracingScene = std::make_unique<DefaultScene>();
	AddScene("RayTracer", std::move(myRaytracingScene));
	
	return SwitchToScene("RayTracer");
}

void Application::OnResize(UINT width, UINT height)
{
	//Add Scene resize operateion
	//by default depthbuffer and RTVs are resized
	if (m_pCurrentScene)
	{
		m_pCurrentScene->OnResize(width, height);
	}
}

bool Application::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse)
	{
		return true;
	}

	// this is Fir handling any user input by default campera and imgui is handled
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
	// this is called if there is any operation to be carried out on changing camera
	if (m_pCurrentScene)
	{
		m_pCurrentScene->OnViewChanged();
	}
}


void Application::PopulateCommandList(void)
{
	// by default commandAllocator and commandList is reeset rest has to be done
	m_pRenderer->m_commandList->RSSetViewports(1, &m_pRenderer->m_viewport);
	m_pRenderer->m_commandList->RSSetScissorRects(1, &m_pRenderer->m_scissorRect);

	if (m_pCurrentScene)
	{
		m_pCurrentScene->PopulateCommandList();
	}

	//Do not close the command list that will be done by the engine
}

void Application::Update(void)
{
	//any scene updates
	if (m_pCurrentScene)
	{
		m_pCurrentScene->Update();
	}
}

void Application::RenderImGui()
{
	//add scene-wise ImGui
	if (m_pCurrentScene)
	{
		m_pCurrentScene->RenderImGui();
	}
}

void Application::OnDestroy(void)
{
	//Add uninitialize statements
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

//Thius function is to add any and all scene to the application and ins called in the Application Initialize
void Application::AddScene(const std::string& name, std::unique_ptr<IScene> scene)
{
	if (scene)
	{
		m_scenes[name] = std::move(scene);
	}
}

//Use this function in case you want to change scenes
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


