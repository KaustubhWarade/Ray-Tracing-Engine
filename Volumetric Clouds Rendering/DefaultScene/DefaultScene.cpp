#include "DefaultScene.h"

#include "CoreHelper Files/Helper.h"
#include "RenderEngine Files/RenderEngine.h" 
#include "RenderEngine Files/ImGuiHelper.h"

struct Vertex_Pos_Tex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Texcoord;
};

using namespace DirectX;


HRESULT WorleyNoiseScene::Initialize(RenderEngine* pRenderEngine)
{

	m_pRenderEngine = pRenderEngine;
	auto pDevice = m_pRenderEngine->GetDevice();
	HRESULT hr = S_OK;

	// 1. --- Create Compute Pipeline for Noise Generation ---
	{
		EXECUTE_AND_LOG_RETURN(compileCSShader(L"DefaultScene/WorleyNoiseGenerator_CShader.hlsl", m_worleyNoiseGenComputeShader));

		// Root Signature: [CBV, SRV, UAV]
		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 1;
		srvRange.BaseShaderRegister = 0; // t0
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 uavRange = {};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = 1;
		uavRange.BaseShaderRegister = 0; // u0
		uavRange.RegisterSpace = 0;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 rootParameters[3];
		ZeroMemory(&rootParameters, sizeof(rootParameters));

		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[0].Constants.ShaderRegister = 0; // b0
		rootParameters[0].Constants.RegisterSpace = 0;
		rootParameters[0].Constants.Num32BitValues = 1;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[2].DescriptorTable.pDescriptorRanges = &uavRange;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
		ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
		rootSignatureDesc.NumParameters = _countof(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSignatureDesc = {};
		versionedRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		versionedRootSignatureDesc.Desc_1_1 = rootSignatureDesc;

		ComPtr<ID3DBlob> signature, error;
		EXECUTE_AND_LOG_RETURN(D3D12SerializeVersionedRootSignature(&versionedRootSignatureDesc, &signature, &error));
		EXECUTE_AND_LOG_RETURN(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_worleyNoiseGeneratorPSO.rootSignature)));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_worleyNoiseGeneratorPSO.rootSignature.Get();
		psoDesc.CS = m_worleyNoiseGenComputeShader.computeShaderByteCode;
		EXECUTE_AND_LOG_RETURN(pDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_worleyNoiseGeneratorPSO.PipelineState)));
	}

	// 2. --- Create Render Pipeline for rendering clouds  ---
	{
		EXECUTE_AND_LOG_RETURN(compileVSShader(L"DefaultScene/CloudRendererVShader.hlsl", m_cloudRenderShader));
		EXECUTE_AND_LOG_RETURN(compilePSShader(L"DefaultScene/CloudRendererPShader.hlsl", m_cloudRenderShader));

		// Root Signature: [CBV, SRV] + Static Sampler
		D3D12_DESCRIPTOR_RANGE1 cbvRange = {};
		cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRange.NumDescriptors = 2;
		cbvRange.BaseShaderRegister = 0; // b0
		cbvRange.RegisterSpace = 0;
		cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 1;
		srvRange.BaseShaderRegister = 0; // t0
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 rootParameters[2];
		ZeroMemory(&rootParameters, sizeof(rootParameters));

		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[0].DescriptorTable.pDescriptorRanges = &cbvRange;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		CreateStaticSampler(staticSampler, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
		ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
		rootSignatureDesc.NumParameters = _countof(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &staticSampler;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSignatureDesc = {};
		versionedRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		versionedRootSignatureDesc.Desc_1_1 = rootSignatureDesc;

		ComPtr<ID3DBlob> signature, error;
		EXECUTE_AND_LOG_RETURN(D3D12SerializeVersionedRootSignature(&versionedRootSignatureDesc, &signature, &error));
		EXECUTE_AND_LOG_RETURN(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_cloudRenderPSO.rootSignature)));

		D3D12_RASTERIZER_DESC rasterizerDesc = {};
		createRasterizerDesc(rasterizerDesc);
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
		createDepthStencilDesc(depthStencilDesc);
		depthStencilDesc.DepthEnable = FALSE;

		D3D12_BLEND_DESC blendDesc;
		createBlendDesc(blendDesc);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		createPipelineStateDesc(psoDesc,
			nullptr,
			0,
			m_cloudRenderPSO.rootSignature.Get(),
			m_cloudRenderShader,
			rasterizerDesc,
			blendDesc,
			depthStencilDesc);

		EXECUTE_AND_LOG_RETURN(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_cloudRenderPSO.PipelineState)));
	}
	// 3. --- Allocate Descriptor Tables ---
	m_computeDescriptorTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(3);
	m_cloudRenderDescriptorTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(3);

	// 4. --- Create Render Constant Buffer ---
	{
		// --- Camera Constant Buffer (b0) ---
		const UINT cameraBufferSize = (sizeof(CameraData) + 255) & ~255;
		D3D12_HEAP_PROPERTIES heapProps;
		CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = cameraBufferSize;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		EXECUTE_AND_LOG_RETURN(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_cameraConstantBuffer)));
		m_cameraConstantBuffer->SetName(L"Camera Constant Buffer");

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_cameraConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = cameraBufferSize;
		// Slot 0 of our table is for the Camera CBV
		pDevice->CreateConstantBufferView(&cbvDesc, m_cloudRenderDescriptorTable.GetCpuHandle(0));
		m_cameraConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cameraCBVStart));

		// --- Cloud Constant Buffer (b1) ---
		const UINT cloudBufferSize = (sizeof(CloudConstants) + 255) & ~255;
		bufferDesc.Width = cloudBufferSize;
		EXECUTE_AND_LOG_RETURN(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_cloudConstantBuffer)));
		m_cloudConstantBuffer->SetName(L"Cloud Constant Buffer");

		cbvDesc.BufferLocation = m_cloudConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = cloudBufferSize;
		// Slot 1 of our table is for the Cloud CBV
		pDevice->CreateConstantBufferView(&cbvDesc, m_cloudRenderDescriptorTable.GetCpuHandle(1));
		m_cloudConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cloudCBVStart));

	}

	return hr;
}

void WorleyNoiseScene::GenerateNoise()
{
	auto pDevice = m_pRenderEngine->GetDevice();
	auto cmdList = m_pRenderEngine->m_commandList.Get();

	m_regenerateNoise = false; // Reset the flag

	if (m_currentNoiseResolution != m_noiseResolution)
	{
		// Update our trackers
		m_currentNoiseResolution = m_noiseResolution;

		// If a resource already exists, untrack it before releasing.
		if (m_worleyNoiseTexture3D) m_pRenderEngine->UnTrackResource(m_worleyNoiseTexture3D.Get());

		// 1. --- Create 3D Texture Resource ---
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		texDesc.Width = m_noiseResolution;
		texDesc.Height = m_noiseResolution;
		texDesc.DepthOrArraySize = m_noiseResolution;
		texDesc.MipLevels = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_HEAP_PROPERTIES heapProps;
		CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_DEFAULT);
		pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_worleyNoiseTexture3D));
		m_worleyNoiseTexture3D->SetName(L"3D Noise Texture");
		m_pRenderEngine->TrackResource(m_worleyNoiseTexture3D.Get(), D3D12_RESOURCE_STATE_COMMON);

		// 3. --- Create Descriptor Views for the new resources ---
		// UAV for output texture
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.Texture3D.WSize = m_noiseResolution;
		pDevice->CreateUnorderedAccessView(m_worleyNoiseTexture3D.Get(), nullptr, &uavDesc, m_computeDescriptorTable.GetCpuHandle(2));

		// SRV for noise texture (for rendering)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture3D.MipLevels = 1;
		pDevice->CreateShaderResourceView(m_worleyNoiseTexture3D.Get(), &srvDesc, m_cloudRenderDescriptorTable.GetCpuHandle(2));
	}

	if (m_currentNumcellsPerAxis != m_numCellsPerAxis)
	{
		// Update our trackers
		m_currentNumcellsPerAxis = m_numCellsPerAxis;

		// 2. --- Create Feature Point Buffer ---
		std::vector<XMFLOAT3> points;
		points.resize(m_numCellsPerAxis * m_numCellsPerAxis * m_numCellsPerAxis);

		float cellSize = 1.0f / static_cast<float>(m_numCellsPerAxis);

		for (int z = 0; z < m_numCellsPerAxis; z++)
		{
			for (int y = 0; y < m_numCellsPerAxis; y++)
			{
				for (int x = 0; x < m_numCellsPerAxis; x++)
				{
					// Random offset within the current cell
					float rx = randomFloat();
					float ry = randomFloat();
					float rz = randomFloat();

					float px = (x + rx) * cellSize;
					float py = (y + ry) * cellSize;
					float pz = (z + rz) * cellSize;

					int index = x + m_numCellsPerAxis * (y + z * m_numCellsPerAxis);
					points[index] = { px, py, pz };
				}
			}
		}

		m_featurePointsBuffer.Stride = sizeof(XMFLOAT3);
		m_featurePointsBuffer.Sync(m_pRenderEngine, cmdList, points.data(), points.size());
		if (m_featurePointsBuffer.Resource) m_featurePointsBuffer.Resource->SetName(L"Feature Points Buffer");


		// 3. --- Create Descriptor Views for the new resources ---
		// SRV for feature points
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = points.size();
		srvDesc.Buffer.StructureByteStride = sizeof(XMFLOAT3);
		pDevice->CreateShaderResourceView(m_featurePointsBuffer.Resource.Get(), &srvDesc, m_computeDescriptorTable.GetCpuHandle(1));
	}

	// 4. --- Dispatch Compute Shader ---
	m_pRenderEngine->TransitionResource(cmdList, m_worleyNoiseTexture3D.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmdList->SetPipelineState(m_worleyNoiseGeneratorPSO.PipelineState.Get());
	cmdList->SetComputeRootSignature(m_worleyNoiseGeneratorPSO.rootSignature.Get());

	ID3D12DescriptorHeap* pHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
	cmdList->SetDescriptorHeaps(1, pHeaps);
	cmdList->SetComputeRoot32BitConstant(0, m_numCellsPerAxis, 0);
	cmdList->SetComputeRootDescriptorTable(1, m_computeDescriptorTable.GetGpuHandle(1));
	cmdList->SetComputeRootDescriptorTable(2, m_computeDescriptorTable.GetGpuHandle(2));

	UINT threads = 8;
	cmdList->Dispatch(m_noiseResolution / threads, m_noiseResolution / threads, m_noiseResolution / threads);

	// 5. --- Barrier ---
	// Wait for the compute shader to finish writing to the texture before we try to render with it.
	m_pRenderEngine->TransitionResource(cmdList, m_worleyNoiseTexture3D.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void WorleyNoiseScene::OnResize(UINT width, UINT height)
{
	if (m_pRenderEngine)
	{
		m_camera.OnResize(width, height);
	}

}

bool WorleyNoiseScene::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	// Scene-specific message handling (e.g., input) would go here.
	//following Win32 style
	if (m_camera.HandleWindowsMessage(message, wParam, lParam))
	{
		OnViewChanged();
		return true;
	}

	return false;
}

void WorleyNoiseScene::OnViewChanged()
{
	/* Stub for camera-change events */
}

void WorleyNoiseScene::PopulateCommandList(void)
{
	if (m_regenerateNoise)
	{
		GenerateNoise();
	}

	ID3D12GraphicsCommandList* cmdList = m_pRenderEngine->m_commandList.Get();
	// --- Render the Slice to the Main Back Buffer ---
	m_pRenderEngine->TransitionResource(cmdList, m_pRenderEngine->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRenderEngine->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += m_pRenderEngine->m_frameIndex * m_pRenderEngine->m_rtvDescriptorSize;

	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
	const float clearColor[] = { 0.39f, 0.58f, 0.92f, 1.0f };
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	cmdList->SetPipelineState(m_cloudRenderPSO.PipelineState.Get());
	cmdList->SetGraphicsRootSignature(m_cloudRenderPSO.rootSignature.Get());

	ID3D12DescriptorHeap* pHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
	cmdList->SetDescriptorHeaps(1, pHeaps);

	CameraData camData;
	camData.g_InverseView = m_camera.GetInverseView();
	camData.g_InverseProjection = m_camera.GetInverseProjection();
	camData.g_CameraPosition = m_camera.GetPosition3f();
	memcpy(m_cameraCBVStart, &camData, sizeof(CameraData));

	memcpy(m_cloudCBVStart, &m_cloudParameters, sizeof(CloudConstants));

	cmdList->SetGraphicsRootDescriptorTable(0, m_cloudRenderDescriptorTable.GetGpuHandle(0));
	cmdList->SetGraphicsRootDescriptorTable(1, m_cloudRenderDescriptorTable.GetGpuHandle(2));

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0); // Draw full-screen triangle

}

void WorleyNoiseScene::Update(void)
{
	// Application update call

}

void WorleyNoiseScene::RenderImGui()
{

	ImGui::Begin("Cloud Renderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Render Time: %.3f ms", m_LastRenderTime);
	ImGui::Separator();

	ImGui::Text("Noise Generation");
	ImGui::SliderInt("Resolution", &m_noiseResolution, 32, 128);
	ImGui::SliderInt("Cells Per Axis", &m_numCellsPerAxis, 2, 16);
	if (ImGui::Button("Generate Noise"))
	{
		m_regenerateNoise = true;
	}

	ImGui::Separator();
	ImGui::Text("Ray Marching");
	ImGui::SliderInt("Steps", &m_cloudParameters.g_NumSteps, 4, 64);
	ImGui::SliderFloat("Step Size", &m_cloudParameters.g_StepSize, 0.01f, 2.0f);

	ImGui::Separator();
	ImGui::Text("Cloud Appearance");
	ImGui::SliderFloat("Density", &m_cloudParameters.g_DensityMultiplier, 0.1f, 5.0f);
	ImGui::SliderFloat("Absorption", &m_cloudParameters.g_Absorption, 0.01f, 1.0f);
	ImGui::ColorEdit3("Cloud Color", &m_cloudParameters.g_CloudColor.x);
	ImGui::SliderFloat3("Light Direction", &m_cloudParameters.g_LightDirection.x, -1.0f, 1.0f);

	ImGui::Separator();
	ImGui::Text("Cloud Volume Bounds");
	ImGui::SliderFloat3("Volume Min", &m_cloudParameters.g_VolumeMin.x, -10.0f, 10.0f);
	ImGui::SliderFloat3("Volume Max", &m_cloudParameters.g_VolumeMax.x, -10.0f, 10.0f);

	ImGui::End();

}

void WorleyNoiseScene::OnDestroy()
{
	// ComPtrs handle automatic cleanup of D3D resources.
}
