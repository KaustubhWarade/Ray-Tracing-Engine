#pragma once

#include "../DefaultApplication.h"

#include "CoreHelper Files/Image.h"
#include "CoreHelper Files/PipelineBuilderHelper.h"
#include "CoreHelper Files/ShaderHelper.h"
#include "CoreHelper Files/DescriptorAllocator.h"
#include "CoreHelper Files/GpuBuffer.h"
#include "CoreHelper Files/DescriptorTable.h"
#include "CoreHelper Files/Model Loader/ModelLoader.h"
#include "CoreHelper Files/Camera.h"
#include "RenderEngine Files/global.h"

#include <vector>
#include <string>
#include <random>

class WorleyNoiseScene : public IScene
{
public:
	WorleyNoiseScene() = default;
	virtual ~WorleyNoiseScene() = default;

	virtual HRESULT Initialize(RenderEngine* pRenderEngine) override;

	virtual void OnResize(UINT width, UINT height) override;

	virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;

	virtual void OnViewChanged() override;

	virtual void PopulateCommandList() override;
	virtual void Update() override;

	virtual void RenderImGui() override;

	virtual void OnDestroy() override;

	void GenerateNoise();

	static float randomFloat(float min = 0.0f, float max = 1.0f)
	{
		static std::mt19937 rng(std::random_device{}()); 
		static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		return min + (max - min) * dist(rng);
	}

private:
	// Camera for rendering
	Camera m_camera;

	struct CBUFFER
	{
		XMMATRIX worldMatrix;
		XMMATRIX viewMatrix;
		XMMATRIX projectionMatrix;
	};

	struct CameraData
	{
		XMMATRIX g_InverseView;
		XMMATRIX g_InverseProjection;
		XMFLOAT3 g_CameraPosition;

	};

	struct CloudConstants
	{
		XMFLOAT3 g_LightDirection = { 0.3f, -0.6f, 0.2f };
		float g_DensityMultiplier = 0.5f;

		XMFLOAT3 g_CloudColor = {0.8f,0.8f, 0.8f};
		float g_Absorption = 0.2f; 

		float g_StepSize = 0.05f; 
		int g_NumSteps = 8;
		float pad1, pad2;

		XMFLOAT3 g_VolumeMin = { -5.0f, 1.0f, -5.0f };
		float pad3;

		XMFLOAT3 g_VolumeMax = { 5.0f, 5.0f, 5.0f };
		float pad4;

	};

	// Worley Noise Data

	// Worley Noise Parameters
	int m_noiseResolution = 256;
	int m_numCellsPerAxis = 8;
	float m_currentSlice = 0.0f;
	bool m_regenerateNoise = true;

	int m_currentNoiseResolution = 0;
	int m_currentNumcellsPerAxis = 0;

	PipeLineStateObject m_worleyNoiseGeneratorPSO;
	COMPUTE_SHADER_DATA m_worleyNoiseGenComputeShader;

	ResizableBuffer m_featurePointsBuffer;
	ComPtr<ID3D12Resource> m_worleyNoiseTexture3D;

	DescriptorTable m_computeDescriptorTable;

	// --- Resources for Cloud Rendering ---
	PipeLineStateObject m_cloudRenderPSO;
	SHADER_DATA m_cloudRenderShader;

	// Camera data (b0)
	ComPtr<ID3D12Resource> m_cameraConstantBuffer;
	UINT8* m_cameraCBVStart = nullptr;
	CameraData m_cameraParameters;

	// Cloud parameters (b1)
	ComPtr<ID3D12Resource> m_cloudConstantBuffer;
	UINT8* m_cloudCBVStart = nullptr;
	CloudConstants m_cloudParameters;

	DescriptorTable m_cloudRenderDescriptorTable;

	// --- Engine Dependencies ---

	RenderEngine* m_pRenderEngine = nullptr;

};
