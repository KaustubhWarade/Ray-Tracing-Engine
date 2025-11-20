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
		EXECUTE_AND_LOG_RETURN(compileCSShader(L"DefaultScene/WorleyNoiseGenerator_CShader.hlsl", m_computeShader));

		// Root Signature: [CBV, SRV, UAV]
		D3D12_DESCRIPTOR_RANGE1 cbvRange = {};
		cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRange.NumDescriptors = 1;
		cbvRange.BaseShaderRegister = 0; // b0
		cbvRange.RegisterSpace = 0;
		cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 4;
		srvRange.BaseShaderRegister = 0; // t0
		srvRange.RegisterSpace = 0;
		srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 uavRange = {};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = 1;
		uavRange.BaseShaderRegister = 0; // u0
		uavRange.RegisterSpace = 0;
		uavRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 rootParameters[3];
		ZeroMemory(&rootParameters, sizeof(rootParameters));

		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[0].Constants.ShaderRegister = 0; // b0
		rootParameters[0].Constants.RegisterSpace = 0;
		rootParameters[0].Constants.Num32BitValues = 4;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[1].DescriptorTable.NumDescriptorRanges = 1; // t0 -t3
		rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = 1; // u0
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
		EXECUTE_AND_LOG_RETURN(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_computePSO.rootSignature)));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_computePSO.rootSignature.Get();
		psoDesc.CS = m_computeShader.computeShaderByteCode;
		EXECUTE_AND_LOG_RETURN(pDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_computePSO.PipelineState)));
	}

	// 2. --- Create Render Pipeline for Displaying Slices ---
	{
		EXECUTE_AND_LOG_RETURN(compileVSShader(L"DefaultScene/SliceRenderer_VShader.hlsl", m_renderShader));
		EXECUTE_AND_LOG_RETURN(compilePSShader(L"DefaultScene/SliceRenderer_PShader.hlsl", m_renderShader));

		// Root Signature: [CBV, SRV] + Static Sampler
		D3D12_DESCRIPTOR_RANGE1 cbvRange = {};
		cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRange.NumDescriptors = 1;
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
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
		EXECUTE_AND_LOG_RETURN(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_renderPSO.rootSignature)));

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_renderPSO.rootSignature.Get();
		psoDesc.VS = m_renderShader.vertexShaderByteCode;
		psoDesc.PS = m_renderShader.pixelShaderByteCode;
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		EXECUTE_AND_LOG_RETURN(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_renderPSO.PipelineState)));
	}
	// 3. --- Allocate Descriptor Tables ---
	m_highResTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(5);
	m_lowResTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(5);
	m_renderCBVTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(1);
	m_noiseTextureSRVTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(2);

	// 4. --- Create Render Constant Buffer ---
	{
		// worley noise Parameter CBV
	/*	CBV changed to use 32bit constants
		const UINT constantBufferSize = (sizeof(int) * 4 + 255) & ~255;
		D3D12_HEAP_PROPERTIES heapProps;
		CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = constantBufferSize;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		EXECUTE_AND_LOG_RETURN(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_noiseParametersResource)));

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_noiseParametersResource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = constantBufferSize;
		pDevice->CreateConstantBufferView(&cbvDesc, m_computeDescriptorTable.GetCpuHandle(0));

		m_noiseParametersResource->Map(0, nullptr, reinterpret_cast<void**>(&m_noiseParameterCBVStart));*/

		// Final visual RenderCBV
		const UINT sliceConstantBufferSize = (sizeof(SliceViewParams) + 255) & ~255;
		D3D12_HEAP_PROPERTIES heapProps;
		CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC sliceBufferDesc = {};
		sliceBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		sliceBufferDesc.Width = sliceConstantBufferSize;
		sliceBufferDesc.Height = 1;
		sliceBufferDesc.DepthOrArraySize = 1;
		sliceBufferDesc.MipLevels = 1;
		sliceBufferDesc.SampleDesc.Count = 1;
		sliceBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		EXECUTE_AND_LOG_RETURN(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &sliceBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_renderConstantBuffer)));

		D3D12_CONSTANT_BUFFER_VIEW_DESC slicecbvDesc = {};
		slicecbvDesc.BufferLocation = m_renderConstantBuffer->GetGPUVirtualAddress();
		slicecbvDesc.SizeInBytes = sliceConstantBufferSize;
		pDevice->CreateConstantBufferView(&slicecbvDesc, m_renderCBVTable.GetCpuHandle(0));

		m_renderConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_renderCBVStart));

	}

	return hr;
}

void WorleyNoiseScene::GenerateSingleNoiseTexture(NoiseParam& params, DescriptorTable& descriptorTable, ComPtr<ID3D12Resource>& targetTexture, int bufferBaseIndex)
{
	auto pDevice = m_pRenderEngine->GetDevice();
	auto cmdList = m_pRenderEngine->m_commandList.Get();

	if (params.dirtyResolution || !targetTexture)
	{
		// If a resource already exists, untrack it before releasing.
		if (targetTexture) m_pRenderEngine->UnTrackResource(targetTexture.Get());

		// 1. --- Create 3D Texture Resource ---
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		texDesc.Width = params.resolution;
		texDesc.Height = params.resolution;
		texDesc.DepthOrArraySize = params.resolution;
		texDesc.MipLevels = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_HEAP_PROPERTIES heapProps;
		CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_DEFAULT);
		EXECUTE_AND_LOG(pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&targetTexture)));
		targetTexture->SetName(bufferBaseIndex == 1 ? L"LowRes Noise Texture" : L"HighRes Noise Texture");
		m_pRenderEngine->TrackResource(targetTexture.Get(), D3D12_RESOURCE_STATE_COMMON);

		// 3. --- Create Descriptor Views for the new resources ---
		// UAV for output texture
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.Texture3D.WSize = params.resolution;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.MipSlice = 0;
		pDevice->CreateUnorderedAccessView(targetTexture.Get(), nullptr, &uavDesc, descriptorTable.GetCpuHandle(4));

		// SRV for noise texture (for rendering)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture3D.MipLevels = 1;
		srvDesc.Texture3D.MostDetailedMip = 0;
		pDevice->CreateShaderResourceView(targetTexture.Get(), &srvDesc, m_noiseTextureSRVTable.GetCpuHandle(bufferBaseIndex));

		params.dirtyResolution = false;

		OutputDebugStringA("Texture Recreated due to Dirty Flag.\n");

	}

	for (int i = 0; i < 4; ++i)
	{
		int bufferIndex = bufferBaseIndex + i;

		// Check specific channel flag
		if (params.dirtyChannel[i] || !m_featurePointsBuffer[bufferIndex].Resource)
		{
			int numCells = params.numPoints[i];

			std::vector<XMFLOAT3> points;
			points.resize(numCells * numCells * numCells);
			float cellSize = 1.0f / static_cast<float>(numCells);

			for (int z = 0; z < numCells; z++)
			{
				for (int y = 0; y < numCells; y++)
				{
					for (int x = 0; x < numCells; x++)
					{
						// Random offset within the current cell
						float rx = randomFloat();
						float ry = randomFloat();
						float rz = randomFloat();

						// Points are in [0, 1] texture space
						float px = (x + rx) * cellSize;
						float py = (y + ry) * cellSize;
						float pz = (z + rz) * cellSize;

						int index = x + numCells * (y + z * numCells);
						points[index] = { px, py, pz };
					}
				}
			}

			m_featurePointsBuffer[i].Stride = sizeof(XMFLOAT3);
			m_featurePointsBuffer[i].Sync(m_pRenderEngine, cmdList, points.data(), points.size());
			if (m_featurePointsBuffer[bufferIndex].Resource)
			{
				wchar_t debugName[128];

				const wchar_t* channelNames[] = { L"R", L"G", L"B", L"A" };
				const wchar_t* resName = (bufferBaseIndex == 0) ? L"HighRes" : L"LowRes";

				swprintf_s(debugName, L"FeaturePts_%s_%s", resName, channelNames[i]);

				m_featurePointsBuffer[bufferIndex].Resource->SetName(debugName);
			}


			// 3. --- Create Descriptor Views for the new resources ---
			// SRV for feature points
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = params.numPoints[i] * params.numPoints[i] * params.numPoints[i];
			srvDesc.Buffer.StructureByteStride = sizeof(XMFLOAT3);
			pDevice->CreateShaderResourceView(m_featurePointsBuffer[bufferIndex].Resource.Get(), &srvDesc, descriptorTable.GetCpuHandle(i));

			params.dirtyChannel[i] = false;

			char msg[128];
			sprintf_s(msg, "Buffer %d Updated due to Dirty Flag.\n", i);
			OutputDebugStringA(msg);
		}
	}

	// 4. --- Dispatch Compute Shader ---
	m_pRenderEngine->TransitionResource(cmdList, targetTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmdList->SetPipelineState(m_computePSO.PipelineState.Get());
	cmdList->SetComputeRootSignature(m_computePSO.rootSignature.Get());

	ID3D12DescriptorHeap* pHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
	cmdList->SetDescriptorHeaps(1, pHeaps);

	cmdList->SetComputeRoot32BitConstants(0, 4, &params.numPoints, 0);
	cmdList->SetComputeRootDescriptorTable(1, descriptorTable.GetGpuHandle(0));
	cmdList->SetComputeRootDescriptorTable(2, descriptorTable.GetGpuHandle(4));

	UINT threads = 8;
	UINT dispatchX = (params.resolution + threads - 1) / threads;
	UINT dispatchY = (params.resolution + threads - 1) / threads;
	UINT dispatchZ = (params.resolution + threads - 1) / threads;
	cmdList->Dispatch(dispatchX, dispatchY, dispatchZ);

	// 5. --- Barrier ---
	// Wait for the compute shader to finish writing to the texture before we try to render with it.
	m_pRenderEngine->TransitionResource(cmdList, targetTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

}

void WorleyNoiseScene::GenerateNoise()
{
	m_regenerateNoise = false;

	// Generate High Res (Base Index 0)
	GenerateSingleNoiseTexture(m_highresConstantBuffer, m_highResTable, m_highResNoiseTexture, 0);

	// Generate Low Res (Base Index 4)
	GenerateSingleNoiseTexture(m_lowresConstantBuffer, m_lowResTable, m_lowResNoiseTexture, 1);
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
	const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	cmdList->SetPipelineState(m_renderPSO.PipelineState.Get());
	cmdList->SetGraphicsRootSignature(m_renderPSO.rootSignature.Get());

	ID3D12DescriptorHeap* pHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
	cmdList->SetDescriptorHeaps(1, pHeaps);

	// Update constant buffer with current slice depth
	int currentResolution = 128; // Default safety
	if (m_vizTextureSelection == HighRes)
	{
		currentResolution = m_highresConstantBuffer.resolution;
	}
	else
	{
		currentResolution = m_lowresConstantBuffer.resolution;
	}
	m_renderConstantData.sliceDepth = (m_currentSlice + 0.5f) / (float)currentResolution;
	m_renderConstantData.vizMode = (int)m_vizMode;
	memcpy(m_renderCBVStart, &m_renderConstantData, sizeof(SliceViewParams));

	cmdList->SetGraphicsRootDescriptorTable(0, m_renderCBVTable.GetGpuHandle(0));
	if (m_vizTextureSelection == HighRes)
	{
		cmdList->SetGraphicsRootDescriptorTable(1, m_noiseTextureSRVTable.GetGpuHandle(0));
	}
	else
	{
		cmdList->SetGraphicsRootDescriptorTable(1, m_noiseTextureSRVTable.GetGpuHandle(1));
	}

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0); // Draw full-screen triangle

}

void WorleyNoiseScene::Update(void)
{
	// Application update call

}

void WorleyNoiseScene::RenderImGui()
{
	ImGui::Begin("Worley Noise Generator", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Render Time: %.3f ms", m_LastRenderTime);
	ImGui::Separator();

	// --- Generation Controls ---
	if (ImGui::Button("Generate Noise", ImVec2(-1, 0)))
	{
		m_regenerateNoise = true;
	}

	// --- High Res Settings ---
	if (ImGui::CollapsingHeader("High Res Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::SliderInt("Resolution##High", &m_highresConstantBuffer.resolution, 32, 256))
		{
			m_highresConstantBuffer.dirtyResolution = true;
		}

		ImGui::Text("Cells Per Axis (Frequency):");

		if (ImGui::SliderInt("Red##High", &m_highresConstantBuffer.numPoints[0], 1, 64))
			m_highresConstantBuffer.dirtyChannel[0] = true;

		if (ImGui::SliderInt("Green##High", &m_highresConstantBuffer.numPoints[1], 1, 64))
			m_highresConstantBuffer.dirtyChannel[1] = true;

		if (ImGui::SliderInt("Blue##High", &m_highresConstantBuffer.numPoints[2], 1, 64))
			m_highresConstantBuffer.dirtyChannel[2] = true;

		if (ImGui::SliderInt("Alpha##High", &m_highresConstantBuffer.numPoints[3], 1, 64))
			m_highresConstantBuffer.dirtyChannel[3] = true;
	}

	// --- Low Res Settings ---
	if (ImGui::CollapsingHeader("Low Res Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::SliderInt("Resolution##Low", &m_lowresConstantBuffer.resolution, 4, 128))
		{
			m_lowresConstantBuffer.dirtyResolution = true;
		}

		ImGui::Text("Cells Per Axis (Frequency):");

		if (ImGui::SliderInt("Red##Low", &m_lowresConstantBuffer.numPoints[0], 1, 32))
			m_lowresConstantBuffer.dirtyChannel[0] = true;

		if (ImGui::SliderInt("Green##Low", &m_lowresConstantBuffer.numPoints[1], 1, 32))
			m_lowresConstantBuffer.dirtyChannel[1] = true;

		if (ImGui::SliderInt("Blue##Low", &m_lowresConstantBuffer.numPoints[2], 1, 32))
			m_lowresConstantBuffer.dirtyChannel[2] = true;

		if (ImGui::SliderInt("Alpha##Low", &m_lowresConstantBuffer.numPoints[3], 1, 32))
			m_lowresConstantBuffer.dirtyChannel[3] = true;
	}
	ImGui::Separator();
	ImGui::Text("Visualization");

	const char* textureItems[] = { "High Res", "Low Res" };
	ImGui::Combo("Target Texture", (int*)&m_vizTextureSelection, textureItems, IM_ARRAYSIZE(textureItems));

	int currentMaxRes = (m_vizTextureSelection == HighRes) ? m_highresConstantBuffer.resolution : m_lowresConstantBuffer.resolution;
	if (m_currentSlice >= currentMaxRes) m_currentSlice = currentMaxRes - 1;

	ImGui::SliderInt("Z-Slice", &m_currentSlice, 0, currentMaxRes - 1);

	const char* viewModeItems[] = { "Grayscale", "Red", "Green", "Blue", "Alpha", "All" };
	ImGui::Combo("View Mode", (int*)&m_vizMode, viewModeItems, IM_ARRAYSIZE(viewModeItems));

	ImGui::End();
}
void WorleyNoiseScene::OnDestroy()
{
	// ComPtrs handle automatic cleanup of D3D resources.
}
