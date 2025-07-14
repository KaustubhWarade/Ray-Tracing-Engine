#pragma once
#include "RenderEngine Files/global.h"

class RenderEngine;

// The interface that all scenes must implement.
class IApplication
{
public:
    virtual ~IApplication() = default;

    virtual HRESULT InitializeApplication(RenderEngine* pRenderEngine) = 0;
    virtual void OnResize(UINT width, UINT height) = 0;
    virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) = 0;
    virtual void OnViewChanged() = 0;

    virtual void PopulateCommandList() = 0;
    virtual void Update() = 0;
    virtual void RenderImGui() = 0;
    
    virtual void OnDestroy() = 0;
};

std::unique_ptr<IApplication> CreateApplication(); 

class IScene
{
public:
    virtual ~IScene() = default;
    virtual HRESULT Initialize(RenderEngine* pRenderEngine) = 0;
    virtual void OnResize(UINT width, UINT height) = 0;
    virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) = 0;
    virtual void OnViewChanged() = 0;

    virtual void Update() = 0;
    virtual void PopulateCommandList() = 0;
    virtual void RenderImGui() = 0;
    
    virtual void OnDestroy() = 0;
};
