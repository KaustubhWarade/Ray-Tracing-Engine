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
	
	enum NoiseQuality { Low = 64, Medium = 128, High = 256 };
	enum TextureSelection { HighRes, LowRes};
	enum VisualizationMode { Grayscale, Red, Green, Blue, Alpha,All };

	struct NoiseParam
	{
		int resolution = 128;
		int numPoints[4] = { 8,16,24,32 };
		
		bool dirtyResolution = true;
		bool dirtyChannel[4] = { true, true, true, true };
	};

private:
	// Camera for rendering
	void GenerateSingleNoiseTexture(NoiseParam& params, DescriptorTable& descriptorTable, ComPtr<ID3D12Resource>& targetTexture, int bufferBaseIndex);
	Camera m_camera;

	ComPtr<ID3D12Resource> m_constantBufferResource;
	UINT8* m_CBVHeapStartPointer;

	// Worley Noise generator
	PipeLineStateObject m_computePSO;
	COMPUTE_SHADER_DATA m_computeShader;

	DescriptorTable m_highResTable;
	DescriptorTable m_lowResTable;

	ResizableBuffer m_featurePointsBuffer[8];
	ComPtr<ID3D12Resource> m_highResNoiseTexture;
	ComPtr<ID3D12Resource> m_lowResNoiseTexture;
	ComPtr<ID3D12Resource> m_noiseParametersResource;
	UINT8* m_noiseParameterCBVStart = nullptr;

	// worley cbv
	NoiseParam m_highresConstantBuffer;
	NoiseParam m_lowresConstantBuffer;

	bool m_regenerateNoise = true;

	// Output display
	PipeLineStateObject m_renderPSO;
	SHADER_DATA m_renderShader;

	DescriptorTable m_renderCBVTable;
	DescriptorTable m_noiseTextureSRVTable;

	ComPtr<ID3D12Resource> m_renderConstantBuffer;
	UINT8* m_renderCBVStart = nullptr;

	struct SliceViewParams
	{
		float sliceDepth = 0;
		int vizMode = 0;
		float pad1, pad2;
	};

	SliceViewParams m_renderConstantData;
	int m_vizTextureSelection= 0;
	int m_vizMode = 0;
	int m_currentSlice = 0;


	// --- Engine Dependencies ---
	// A pointer to the main RenderEngine, providing access to the device,
	// command list, etc.

	RenderEngine* m_pRenderEngine = nullptr;

};
