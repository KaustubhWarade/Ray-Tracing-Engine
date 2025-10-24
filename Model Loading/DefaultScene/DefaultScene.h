#pragma once

#include "../DefaultApplication.h"

#include "CoreHelper Files/Image.h"
#include "CoreHelper Files/PipelineBuilderHelper.h"
#include "CoreHelper Files/ShaderHelper.h"
#include "CoreHelper Files/DescriptorAllocator.h"
#include "CoreHelper Files/DescriptorTable.h"
#include "RenderEngine Files/global.h"
#include "CoreHelper Files/Model Loader/ModelLoader.h"
#include "CoreHelper Files/AccelerationStructureManager.h"

#include <vector>
#include <string>

enum class MaterialDebugView
{
	PBR,
	BaseColor,
	Normals,
	Metallic,
	Roughness,
	Emissive,
	AmbientOcclusion
};

enum class PreviewMeshType
{
	Sphere,
	Quad
};

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
		DirectX::XMMATRIX worldMatrix;
		DirectX::XMMATRIX viewMatrix;
		DirectX::XMMATRIX projectionMatrix;
		DirectX::XMFLOAT4 lD;
		DirectX::XMFLOAT4 kD;
		DirectX::XMFLOAT4 lightPosition;
		UINT              lightingMode;
		DirectX::XMFLOAT3 cameraPosition;
	};

	ComPtr<ID3D12Resource> m_constantBufferResource;
	DescriptorTable m_constantBufferTable;
	DescriptorTable m_materialSrvTable;
	UINT8* m_CBVHeapStartPointer;
	CBUFFER m_constantBufferData;

	ComPtr<ID3D12Resource> m_previewConstantBufferResource;
	DescriptorTable m_previewConstantBufferTable;
	UINT8 * m_previewCBVHeapStartPointer;
	CBUFFER m_previewConstantBufferData;

	//Add variables here to control your scene's state, like animation timers or object positions.
	XMFLOAT3 m_modelPosition = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 m_modelScale = { 0.5f,0.5f,0.5f };
	XMFLOAT3 m_rotationAngle = { -90.0f, 0.0f, 0.0f };

	SHADER_DATA m_finalShader;
	std::string m_currentModelName;
	PipeLineStateObject m_finalTexturePSO;

	Model* m_pModel;
	BLASStats m_blasStats;

	ResizableBuffer m_materialBuffer;
	std::vector<Texture*> m_pModelTextures;

	float m_modelLoadTimeMs = 0.0f;
	float m_blasBuildTimeMs = 0.0f;

	int m_selectedMaterialIndex = 0;
	bool m_materialsDirty = false;
	MaterialDebugView m_debugView = MaterialDebugView::PBR;
	PreviewMeshType m_previewMeshType = PreviewMeshType::Sphere;

	// Material preview
	std::unique_ptr<Image> m_materialPreviewImage;
	// material preview shader 
	ComPtr<ID3D12Resource> m_previewDepthBuffer;
	ComPtr<ID3D12DescriptorHeap> m_previewDsvHeap;
	D3D12_VIEWPORT m_previewViewport;
	D3D12_RECT m_previewScissorRect;
	Model* m_pSphereModel = nullptr;
	Model* m_pQuadModel = nullptr;
	void RenderMaterialPreview(void);

	// --- Engine Dependencies ---
	// A pointer to the main RenderEngine, providing access to the device, command list, etc.

	RenderEngine* m_pRenderEngine = nullptr;

};