
#pragma once

#include "../Application.h"
#include "CoreHelper Files/Image.h"
#include "CoreHelper Files/PipelineBuilderHelper.h"
#include "CoreHelper Files/ShaderHelper.h"
#include "CoreHelper Files/GpuBuffer.h"
#include "CoreHelper Files/Helper.h"
#include "CoreHelper Files/DescriptorTable.h"
#include "CoreHelper Files/Camera.h"
#include "RenderEngine Files/global.h"
#include "../basicStructs.h"

#include <vector>
#include <string>


class SceneOne : public IScene
{
public:
	SceneOne() = default;
	virtual ~SceneOne() = default;

	// --- Public IScene contract implementation ---
	virtual HRESULT Initialize(RenderEngine* pRenderEngine) override;
	virtual void OnResize(UINT width, UINT height) override;
	virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
	virtual void OnViewChanged() override;

	virtual void PopulateCommandList() override;
	virtual void Update() override;
	virtual void RenderImGui() override;

	virtual void OnDestroy() override;

private:
	// Camera for rendering
	Camera m_camera;

	HRESULT InitializeResources(void);
	HRESULT InitializeComputeResources(void);
	HRESULT CreateDescriptorTables();

	//compute Shader resources
	COMPUTE_SHADER_DATA m_computeShaderData;
	PipeLineStateObject m_computePSO;

	// normal rendering image
	SHADER_DATA m_finalShader;
	PipeLineStateObject m_finalTexturePSO;

	uint32_t m_frameIndex = 1;

	bool m_sceneDataDirty = false;

	// --- Scene and Compute Data ---
	struct CBUFFER
	{
		XMFLOAT3 CameraPosition;
		UINT FrameIndex;
		XMMATRIX InverseProjection;
		XMMATRIX InverseView;
		UINT numBounces;
		UINT numRaysPerPixel;
		float exposure;
		float padding; // ensure 16-byte alignment
	};

	ComPtr<ID3D12Resource> m_ConstantBufferView;
	UINT8* m_pCbvDataBegin = nullptr;

	// Scene geometry buffers (managed by the scene)
	ResizableBuffer m_sphereBuffer;
	ResizableBuffer m_triangleBuffer;
	ResizableBuffer m_materialBuffer;

	// Pointers to resources managed by ResourceManager
	Image* m_pAccumulationImage;
	Image* m_pOutputImage;
	Texture* m_pEnvironmentTexture;
	Model* m_pQuadModel;

	// Descriptors for our specific render passes
	DescriptorTable m_computeDescriptorTable;
	DescriptorTable m_finalPassSrvTable;

	RenderEngine* m_pRenderEngine = nullptr;

	// --- ImGui Controlled Parameters ---
	int mc_numBounces = 10;
	int mc_numRaysPerPixel = 1;
	float mc_exposure = 0.5f;

	// --- CPU-side scene data ---
	std::vector<Sphere> m_spheres;
	std::vector<Triangle> m_triangles;
	std::vector<Material> m_materials;
};