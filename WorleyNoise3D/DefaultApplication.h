#pragma once

#include "IApplication.h"

#include "DefaultScene/DefaultScene.h"

#include <map>
#include <string>
#include <memory>

class Application : public IApplication
{
public:
	Application() = default;
	// --- IApplication Interface Implementation ---
	// These methods are the entry points from the main application loop.
	// They delegate the calls to the currently active scene.
	// ----------------------------------------------------------------

	// DO NOT CHANGE THESE FUNCTIONS.


	virtual HRESULT InitializeApplication(RenderEngine* pRenderEngine) override;
	virtual void OnResize(UINT width, UINT height) override;
	virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
	virtual void OnViewChanged() override;

	virtual void PopulateCommandList() override;
	virtual void Update() override;
	virtual void RenderImGui() override;
	
	virtual void OnDestroy() override;

	void AddScene(const std::string& name, std::unique_ptr<IScene> scene);
	HRESULT SwitchToScene(const std::string& name);

private:
	RenderEngine* m_pRenderer = nullptr;

	std::map<std::string, std::unique_ptr<IScene>> m_scenes;

	IScene* m_pCurrentScene = nullptr;
};

