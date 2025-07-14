#pragma once

#include "../DefaultApplication.h"

#include "CoreHelper Files/Image.h"
#include "CoreHelper Files/PipelineBuilderHelper.h"
#include "CoreHelper Files/ShaderHelper.h"
#include "RenderEngine Files/global.h"

#include <vector>
#include <string>

//User is free to make changes as he pleases
// Add You structs variables here...
class DefaultScene : public IScene
{
public:
	DefaultScene() = default;
	virtual ~DefaultScene() = default;

	// --- IScene Interface Implementation ---
// These methods are called by the main application loop (RenderEngine).
// DO NOT CHANGE THEIR SIGNATURES. Implement your scene's logic inside them.
// ------------------------------------------------------------------------

	//Load all scene - specific resources(shaders, PSOs, models, textures).
	virtual HRESULT Initialize(RenderEngine* pRenderEngine) override;
	//Called when the window is resized to update camera projection, etc.
	virtual void OnResize(UINT width, UINT height) override;
	//Forwards window messages to the scene for input handling.
	virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
	//Called when the camera's view matrix has changed.
	virtual void OnViewChanged() override;

	//Called once per frame to update scene logic(animation, physics, etc.).
	virtual void PopulateCommandList() override;
	virtual void Update() override;
	//Draw any ImGui windows or controls for this scene.
	virtual void RenderImGui() override;
	//Release all resources created in Initialize().
	virtual void OnDestroy() override;

	//Feel free to make changes here

private:

	struct CBUFFER
	{
		XMMATRIX worldMatrix;
		XMMATRIX viewMatrix;
		XMMATRIX projectionMatrix;
	};

	ComPtr<ID3D12Resource> m_constantBufferResource;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	UINT8* m_CBVHeapStartPointer;
	CBUFFER m_constantBufferData;

	//Add variables here to control your scene's state, like animation timers or object positions.

	float m_rotationAngle = 0.0f;

	SHADER_DATA m_finalShader;
	PipeLineStateObject m_finalTexturePSO;
	GeometryMesh m_cube;

	// --- Engine Dependencies ---
	// A pointer to the main RenderEngine, providing access to the device, command list, etc.

	RenderEngine* m_pRenderEngine = nullptr;

};
