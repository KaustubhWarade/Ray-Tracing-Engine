#include "SceneOne.h"

#include "CoreHelper Files/Helper.h"
#include "RenderEngine Files/RenderEngine.h" 
#include "RenderEngine Files/ImGuiHelper.h"
#include <omp.h>

using namespace DirectX;

HRESULT SceneOne::Initialize(RenderEngine* pRenderEngine)
{
	m_pRenderEngine = pRenderEngine;
	auto pDevice = m_pRenderEngine->GetDevice();
	HRESULT hr = S_OK;
	//Create Compute shader pipeline state 
	{
		D3D12_DESCRIPTOR_RANGE1 descriptorRangeBindlessSRV = {};
		descriptorRangeBindlessSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRangeBindlessSRV.NumDescriptors = -1;
		descriptorRangeBindlessSRV.BaseShaderRegister = 0;
		descriptorRangeBindlessSRV.RegisterSpace = 1;
		descriptorRangeBindlessSRV.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
		descriptorRangeBindlessSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 descriptorRangeSpace0[4];
		descriptorRangeSpace0[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC };
		descriptorRangeSpace0[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
		descriptorRangeSpace0[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
		descriptorRangeSpace0[3] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };

		D3D12_ROOT_PARAMETER1 rootParameter[2];
		ZeroMemory(&rootParameter[0], sizeof(D3D12_ROOT_PARAMETER1));
		rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeSpace0);
		rootParameter[0].DescriptorTable.pDescriptorRanges = descriptorRangeSpace0;
		rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		ZeroMemory(&rootParameter[1], sizeof(D3D12_ROOT_PARAMETER1));
		rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameter[1].DescriptorTable.pDescriptorRanges = &descriptorRangeBindlessSRV;
		rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		CreateStaticSampler(staticSampler, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
		ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = rootParameter;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &staticSampler;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSignatureDesc = {};
		versionedRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		versionedRootSignatureDesc.Desc_1_1 = rootSignatureDesc;

		// Serialize the root signature
		Microsoft::WRL::ComPtr<ID3DBlob> pID3DBlob_signature;
		Microsoft::WRL::ComPtr<ID3DBlob> pID3DBlob_Error;
		COMPILE_AND_LOG_RETURN(D3D12SerializeVersionedRootSignature(&versionedRootSignatureDesc, &pID3DBlob_signature, &pID3DBlob_Error), pID3DBlob_Error.Get());

		// Create the root signature
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		EXECUTE_AND_LOG_RETURN(m_pRenderEngine->GetDevice()->CreateRootSignature(0, pID3DBlob_signature->GetBufferPointer(), pID3DBlob_signature->GetBufferSize(), IID_PPV_ARGS(&m_computePSO.rootSignature)));

		EXECUTE_AND_LOG_RETURN(compileCSShader(L"SceneOne/RayTracerCS.hlsl", m_computeShaderData));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_computePSO.rootSignature.Get();
		psoDesc.CS = m_computeShaderData.computeShaderByteCode;
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		psoDesc.NodeMask = 0;

		EXECUTE_AND_LOG_RETURN(pDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_computePSO.PipelineState)));
	}

	// create normal texture pipeline state object
	{
		EXECUTE_AND_LOG_RETURN(compileVSShader(L"SceneOne/FinalTextureVShader.hlsl", m_finalShader));
		EXECUTE_AND_LOG_RETURN(compilePSShader(L"SceneOne/FinalTexturePShader.hlsl", m_finalShader));

		D3D12_DESCRIPTOR_RANGE1 descriptorRangeSRV = {};
		ZeroMemory(&descriptorRangeSRV, sizeof(D3D12_DESCRIPTOR_RANGE1));
		descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRangeSRV.NumDescriptors = 1;
		descriptorRangeSRV.BaseShaderRegister = 0;
		descriptorRangeSRV.RegisterSpace = 0;
		descriptorRangeSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 rootParameter[1];
		ZeroMemory(&rootParameter[0], sizeof(D3D12_ROOT_PARAMETER1));
		rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParameter[0].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
		rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		CreateStaticSampler(staticSampler, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
		ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
		rootSignatureDesc.NumParameters = 1;
		rootSignatureDesc.pParameters = &rootParameter[0];
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &staticSampler;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSignatureDesc = {};
		versionedRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		versionedRootSignatureDesc.Desc_1_1 = rootSignatureDesc;

		// Serialize the root signature
		Microsoft::WRL::ComPtr<ID3DBlob> pID3DBlob_signature;
		Microsoft::WRL::ComPtr<ID3DBlob> pID3DBlob_Error;
		EXECUTE_AND_LOG_RETURN(D3D12SerializeVersionedRootSignature(&versionedRootSignatureDesc, &pID3DBlob_signature, &pID3DBlob_Error));

		// Create the root signature
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		EXECUTE_AND_LOG_RETURN(m_pRenderEngine->GetDevice()->CreateRootSignature(0, pID3DBlob_signature->GetBufferPointer(), pID3DBlob_signature->GetBufferSize(), IID_PPV_ARGS(&m_finalTexturePSO.rootSignature)));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC d3dInputElementDesc[3];
		ZeroMemory(&d3dInputElementDesc, sizeof(D3D12_INPUT_ELEMENT_DESC) * _ARRAYSIZE(d3dInputElementDesc));
		d3dInputElementDesc[0].SemanticName = "POSITION";
		d3dInputElementDesc[0].SemanticIndex = 0;
		d3dInputElementDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		d3dInputElementDesc[0].InputSlot = 0;
		d3dInputElementDesc[0].AlignedByteOffset = 0;
		d3dInputElementDesc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		d3dInputElementDesc[0].InstanceDataStepRate = 0;

		d3dInputElementDesc[1].SemanticName = "NORMAL";
		d3dInputElementDesc[1].SemanticIndex = 0;
		d3dInputElementDesc[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		d3dInputElementDesc[1].InputSlot = 0;
		d3dInputElementDesc[1].AlignedByteOffset = 12;
		d3dInputElementDesc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		d3dInputElementDesc[1].InstanceDataStepRate = 0;

		d3dInputElementDesc[2].SemanticName = "TEXCOORD";
		d3dInputElementDesc[2].SemanticIndex = 0;
		d3dInputElementDesc[2].Format = DXGI_FORMAT_R32G32_FLOAT;
		d3dInputElementDesc[2].InputSlot = 0;
		d3dInputElementDesc[2].AlignedByteOffset = 24;
		d3dInputElementDesc[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		d3dInputElementDesc[2].InstanceDataStepRate = 0;

		D3D12_RASTERIZER_DESC rasterizerDesc = {};
		createRasterizerDesc(rasterizerDesc);

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
		createDepthStencilDesc(depthStencilDesc);

		D3D12_BLEND_DESC blendDesc;
		createBlendDesc(blendDesc);

		// Set up blending for the first render target (RT[0])
		D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc;
		createRenderTargetBlendDesc(rtBlendDesc);

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		createPipelineStateDesc(psoDesc,
			d3dInputElementDesc,
			_countof(d3dInputElementDesc),
			m_finalTexturePSO.rootSignature.Get(),
			m_finalShader,
			rasterizerDesc,
			blendDesc,
			depthStencilDesc);

		EXECUTE_AND_LOG_RETURN(m_pRenderEngine->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_finalTexturePSO.PipelineState)));

	}
	EXECUTE_AND_LOG_RETURN(InitializeResources());
	EXECUTE_AND_LOG_RETURN(InitializeAccelerationStructures());
	auto resourceManager = m_pRenderEngine->GetResourceManager();
	m_computeDescriptorTable = resourceManager->m_generalPurposeHeapAllocator.AllocateTable(10);
	m_finalPassSrvTable = resourceManager->m_generalPurposeHeapAllocator.AllocateTable(1);
	EXECUTE_AND_LOG_RETURN(InitializeComputeResources());
	EXECUTE_AND_LOG_RETURN(InitializeDescriptorTables());


	m_pRenderEngine->m_camera.SetPosition(0.0f, 3.0f, 10.0f);

	return hr;
}

HRESULT SceneOne::InitializeResources(void)
{
	HRESULT hr = S_OK;
	auto resourceManager = m_pRenderEngine->GetResourceManager();
	auto pCommandList = m_pRenderEngine->m_commandList.Get();
	// Load quad model for final render pass
	{
		ModelVertex quadVertices[] = {
			{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
			{ XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
			{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
			{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) }
		};
		uint16_t quadIndices[] = { 0, 1, 2, 2, 1, 3 };

		EXECUTE_AND_LOG_RETURN(resourceManager->CreateModel(
			"FullscreenQuad",
			quadVertices, sizeof(quadVertices), sizeof(ModelVertex),
			quadIndices, sizeof(quadIndices), DXGI_FORMAT_R16_UINT
		));
		m_pQuadModel = resourceManager->GetModel("FullscreenQuad");
		if (!m_pQuadModel)
		{
			return E_FAIL;
		}
	}
		
	m_materialBuffer.Stride = sizeof(Material);
	m_materialBuffer.Sync(m_pRenderEngine, pCommandList, m_materials.data(), m_materials.size());
	if (m_materialBuffer.Resource) m_materialBuffer.Resource->SetName(L"Scene Material Buffer");

	return hr;
}

HRESULT SceneOne::InitializeAccelerationStructures()
{
	auto accelManager = m_pRenderEngine->GetAccelManager();
	auto cmdList = m_pRenderEngine->m_commandList.Get();

	// 1. Update instance transforms just like we do in the Update() loop.
	for (auto& inst : m_ModelInstances)
	{
		inst.Transform = XMMatrixScaling(inst.Scale.x, inst.Scale.y, inst.Scale.z) *
			XMMatrixRotationRollPitchYaw(XMConvertToRadians(inst.Rotation.x), XMConvertToRadians(inst.Rotation.y), XMConvertToRadians(inst.Rotation.z)) *
			XMMatrixTranslation(inst.Position.x, inst.Position.y, inst.Position.z);
		inst.InverseTransform = XMMatrixInverse(nullptr, inst.Transform);

	}

	// 2. Perform the initial CPU-side build.
	accelManager->BuildTLAS(m_ModelInstances);

	// 3. Record the commands to create the GPU buffers.
	accelManager->UpdateGpuBuffers(cmdList, m_ModelInstances);

	// 4. Execute the commands and wait for the GPU to finish.
	EXECUTE_AND_LOG_RETURN(cmdList->Close());
	ID3D12CommandList* ppCommandLists[] = { cmdList };
	m_pRenderEngine->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_pRenderEngine->WaitForPreviousFrame();

	// 5. Reset the command list so it's ready for the first frame.
	EXECUTE_AND_LOG_RETURN(m_pRenderEngine->m_commandAllocator->Reset());
	EXECUTE_AND_LOG_RETURN(cmdList->Reset(m_pRenderEngine->m_commandAllocator.Get(), nullptr));

	return S_OK;
}

HRESULT SceneOne::InitializeComputeResources(void)
{
	HRESULT hr = S_OK;
	auto pDevice = m_pRenderEngine->GetDevice();
	auto pCommandList = m_pRenderEngine->m_commandList.Get();
	auto resourceManager = ResourceManager::Get();

	UINT width = 800;
	UINT height = 700;

	EXECUTE_AND_LOG_RETURN(resourceManager->CreateImage("Accumulation", width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
	EXECUTE_AND_LOG_RETURN(resourceManager->CreateImage("Output", width, height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
	EXECUTE_AND_LOG_RETURN(resourceManager->LoadTextureFromFile("Environment", L"SceneOne/env.dds"));

	m_pAccumulationImage = resourceManager->GetImage("Accumulation");
	m_pOutputImage = resourceManager->GetImage("Output");
	m_pEnvironmentTexture = resourceManager->GetTexture("Environment");

	return hr;
}

HRESULT SceneOne::InitializeDescriptorTables()
{
	auto pDevice = m_pRenderEngine->GetDevice();
	auto resourceManager = ResourceManager::Get();

	m_computeDescriptorTable = resourceManager->m_generalPurposeHeapAllocator.AllocateTable(10);
	m_finalPassSrvTable = resourceManager->m_generalPurposeHeapAllocator.AllocateTable(1);

	const UINT CBV_SLOT = 0;
	{
		UINT cbvSize = (sizeof(CBUFFER) + 255) & ~255;
		D3D12_HEAP_PROPERTIES heapProperties;
		CreateHeapProperties(heapProperties, D3D12_HEAP_TYPE_UPLOAD);

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = cbvSize;
		// ... (rest of resourceDesc setup)
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


		EXECUTE_AND_LOG_RETURN(pDevice->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_ConstantBufferView)));
		m_ConstantBufferView->SetName(L"Per Frame Constant Buffer");

		// Map it once and keep it mapped for the lifetime of the scene
		D3D12_RANGE readRange = { 0, 0 };
		m_ConstantBufferView->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin));
	}

	// Create the view for the constant buffer
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_ConstantBufferView->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (sizeof(CBUFFER) + 255) & ~255;
	pDevice->CreateConstantBufferView(&cbvDesc, m_computeDescriptorTable.GetCpuHandle(CBV_SLOT));

	UpdateImageDescriptors();
	UpdateStaticGeometryDescriptors();
	UpdateInstanceDescriptors();

	const UINT ENV_SRV_SLOT = 8;
	// SRV for Environment texture (t5)
	D3D12_SHADER_RESOURCE_VIEW_DESC envSrvDesc = {};
	envSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	envSrvDesc.Format = m_pEnvironmentTexture->m_ResourceGPU->GetDesc().Format;
	envSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	envSrvDesc.Texture2D.MostDetailedMip = 0;
	envSrvDesc.Texture2D.MipLevels = m_pEnvironmentTexture->m_ResourceGPU->GetDesc().MipLevels;
	pDevice->CreateShaderResourceView(m_pEnvironmentTexture->m_ResourceGPU.Get(), &envSrvDesc, m_computeDescriptorTable.GetCpuHandle(ENV_SRV_SLOT));

	return S_OK;
}

HRESULT SceneOne::UpdateImageDescriptors()
{
	auto pDevice = m_pRenderEngine->GetDevice();
	auto accelManager = m_pRenderEngine->GetAccelManager();

	const UINT ACCUM_UAV_SLOT = 1;
	const UINT OUTPUT_UAV_SLOT = 2;

	// UAV for Accumulation (u0)
	D3D12_UNORDERED_ACCESS_VIEW_DESC accumUavDesc = {};
	accumUavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	accumUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	pDevice->CreateUnorderedAccessView(m_pAccumulationImage->GetResource(), nullptr, &accumUavDesc, m_computeDescriptorTable.GetCpuHandle(ACCUM_UAV_SLOT));


	// UAV for Output (u1)
	D3D12_UNORDERED_ACCESS_VIEW_DESC outputUavDesc = {};
	outputUavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	outputUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	pDevice->CreateUnorderedAccessView(m_pOutputImage->GetResource(), nullptr, &outputUavDesc, m_computeDescriptorTable.GetCpuHandle(OUTPUT_UAV_SLOT));

	D3D12_SHADER_RESOURCE_VIEW_DESC finalSrvDesc = {};
	finalSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	finalSrvDesc.Format = m_pOutputImage->GetResource()->GetDesc().Format;
	finalSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	finalSrvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(m_pOutputImage->GetResource(), &finalSrvDesc, m_finalPassSrvTable.GetCpuHandle(0));

	return S_OK;
}

HRESULT SceneOne::UpdateStaticGeometryDescriptors()
{
	auto pDevice = m_pRenderEngine->GetDevice();
	auto accelManager = m_pRenderEngine->GetAccelManager();

	const UINT MATERIAL_SRV_SLOT = 3;
	const UINT TRIANGLE_SRV_SLOT = 4;
	const UINT BLAS_SRV_SLOT = 5;


	// SRV for Material Buffer (t0)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.StructureByteStride = m_materialBuffer.Stride;
	srvDesc.Buffer.NumElements = m_materialBuffer.Size;
	pDevice->CreateShaderResourceView(m_materialBuffer.Resource.Get(), &srvDesc, m_computeDescriptorTable.GetCpuHandle(MATERIAL_SRV_SLOT));

	// SRV for Triangle Buffer (t1)
	ID3D12Resource* uberTriangles = accelManager->GetUberTriangleBuffer();
	srvDesc.Buffer.StructureByteStride = accelManager->GetUberTriangleBufferStride();
	srvDesc.Buffer.NumElements = accelManager->GetUberTriangleCount();
	pDevice->CreateShaderResourceView(uberTriangles, &srvDesc, m_computeDescriptorTable.GetCpuHandle(TRIANGLE_SRV_SLOT));

	// SRV for BLAS Buffer (t2)
	ID3D12Resource* uberBlas = accelManager->GetUberBlasNodeBuffer();
	srvDesc.Buffer.StructureByteStride = accelManager->GetUberBlasNodeBufferStride();
	srvDesc.Buffer.NumElements = accelManager->GetUberBlasNodeCount();
	pDevice->CreateShaderResourceView(uberBlas, &srvDesc, m_computeDescriptorTable.GetCpuHandle(BLAS_SRV_SLOT));

	return S_OK;
	
}

HRESULT SceneOne::UpdateInstanceDescriptors()
{
	auto pDevice = m_pRenderEngine->GetDevice();
	auto accelManager = m_pRenderEngine->GetAccelManager();

	const UINT TLAS_SRV_SLOT = 6;
	const UINT INSTANCE_SRV_SLOT = 7;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	// SRV for TLAS Buffer (t3)
	ID3D12Resource* tlas = accelManager->GetTLASBuffer();
	srvDesc.Buffer.StructureByteStride = accelManager->GetTLASBufferStride();
	srvDesc.Buffer.NumElements = accelManager->GetTLASNodeCount();
	pDevice->CreateShaderResourceView(tlas, &srvDesc, m_computeDescriptorTable.GetCpuHandle(TLAS_SRV_SLOT));

	// SRV for Instance Data Buffer (t4)
	ID3D12Resource* instances = accelManager->GetInstanceBuffer();
	srvDesc.Buffer.StructureByteStride = accelManager->GetInstanceBufferStride();
	srvDesc.Buffer.NumElements = accelManager->GetInstanceCount();
	pDevice->CreateShaderResourceView(instances, &srvDesc, m_computeDescriptorTable.GetCpuHandle(INSTANCE_SRV_SLOT));

	return S_OK;
}

void SceneOne::AddModelInstance(Model* model, const std::string& modelKey)
{
	if (!model) return;

	auto accelManager = m_pRenderEngine->GetAccelManager();

	// 1. Create a new instance
	ModelInstance inst = {};
	inst.SourceModel = model;
	inst.Name = modelKey + "_" + std::to_string(m_ModelInstances.size());
	inst.Position = { 0.0f, 0.0f, 0.0f };
	inst.Rotation = { 0.0f, 0.0f, 0.0f };
	inst.Scale = { 1.0f, 1.0f, 1.0f };

	// 2. Unify materials
	inst.MaterialOffset = static_cast<uint32_t>(m_materials.size());
	m_materials.insert(m_materials.end(), model->Materials.begin(), model->Materials.end());

	m_ModelInstances.push_back(inst);

	// 3. Build the BLAS for the model's geometry
	accelManager->GetOrBuildBLAS(m_pRenderEngine->m_commandList.Get(), model);

	// 4. Mark scene as dirty to trigger buffer and TLAS updates
	m_sceneDataDirty = true;
	m_tlasDirty = true;

	// 5. Select the newly added instance in the UI
	m_selectedInstanceIndex = static_cast<int>(m_ModelInstances.size() - 1);
}

void SceneOne::OnResize(UINT width, UINT height)
{
	if (m_pRenderEngine)
	{
		m_pRenderEngine->m_camera.OnResize(width, height);
	}

	if (m_pOutputImage->GetWidth() != width || m_pOutputImage->GetHeight() != height)
	{
		ResourceManager::Get()->ResizeImage("Accumulation", width, height);
		ResourceManager::Get()->ResizeImage("Output", width, height);

		UpdateImageDescriptors();
	}

	OnViewChanged();
}

bool SceneOne::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{

	return false;
}

void SceneOne::OnViewChanged()
{
	m_frameIndex = 0;
}

void SceneOne::PopulateCommandList(void)
{
	ID3D12GraphicsCommandList* cmdList = m_pRenderEngine->m_commandList.Get();
	UINT width = m_pRenderEngine->m_viewport.Width;
	UINT height = m_pRenderEngine->m_viewport.Height;
	auto resourceManager = ResourceManager::Get();
	auto pDevice = m_pRenderEngine->GetDevice();
	UINT descriptorSize = m_pRenderEngine->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	auto accelManager = m_pRenderEngine->GetAccelManager();

	ID3D12DescriptorHeap* ppHeaps[] = {
		resourceManager->GetMainCbvSrvUavHeap(),
	};
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	if (m_sceneDataDirty)
	{
		m_materialBuffer.Sync(m_pRenderEngine, cmdList, m_materials.data(), m_materials.size());

		// If the sync caused a reallocation, we must update the SRVs.
		if (m_materialBuffer.GpuResourceDirty)
		{
			UpdateStaticGeometryDescriptors();
		}
		m_sceneDataDirty = false;
		OnViewChanged();
	}

	if (m_tlasDirty)
	{
		accelManager->UpdateGpuBuffers(cmdList, m_ModelInstances);
		if (accelManager->InstanceSrvsNeedUpdate())
		{
			UpdateInstanceDescriptors();
		}
		m_tlasDirty = false;
		OnViewChanged();
	}

	// --- Compute Pass ---
	m_pRenderEngine->TransitionResource(cmdList, m_pOutputImage->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmdList->SetPipelineState(m_computePSO.PipelineState.Get());
	cmdList->SetComputeRootSignature(m_computePSO.rootSignature.Get());

	CBUFFER constants;
	constants.CameraPosition = m_pRenderEngine->m_camera.GetPosition3f();
	constants.FrameIndex = m_frameIndex;
	constants.InverseView = m_pRenderEngine->m_camera.GetInverseView();
	constants.InverseProjection = m_pRenderEngine->m_camera.GetInverseProjection();
	constants.numBounces = mc_numBounces;
	constants.numRaysPerPixel = mc_numRaysPerPixel;
	constants.exposure = mc_exposure;
	constants.UseEnvMap = m_useEnvMap;
	memcpy(m_pCbvDataBegin, &constants, sizeof(CBUFFER));

	D3D12_GPU_DESCRIPTOR_HANDLE bindfulTableHandle = m_computeDescriptorTable.GetGpuHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE bindlessTableHandle = resourceManager->m_bindlessTextureAllocator.GetHeap()->GetGPUDescriptorHandleForHeapStart();

	cmdList->SetComputeRootDescriptorTable(0, bindfulTableHandle);  // For our per-pass resources
	cmdList->SetComputeRootDescriptorTable(1, bindlessTableHandle); // For the global texture library


	cmdList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

	m_pRenderEngine->TransitionResource(cmdList, m_pOutputImage->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// --- Graphics Pass ---
	m_pRenderEngine->TransitionResource(cmdList, m_pRenderEngine->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetPipelineState(m_finalTexturePSO.PipelineState.Get());
	cmdList->SetGraphicsRootSignature(m_finalTexturePSO.rootSignature.Get());

	cmdList->SetGraphicsRootDescriptorTable(0, m_finalPassSrvTable.GetGpuHandle());

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRenderEngine->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += m_pRenderEngine->m_frameIndex * m_pRenderEngine->m_rtvDescriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pRenderEngine->m_dsbHeap->GetCPUDescriptorHandleForHeapStart();

	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	RenderModel(cmdList, m_pQuadModel);

	m_pRenderEngine->TransitionResource(cmdList, m_pOutputImage->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void SceneOne::Update(void)
{
	if (m_tlasDirty)
	{
		auto accelManager = m_pRenderEngine->GetAccelManager();

		// Update instance transforms
		for (auto& inst : m_ModelInstances)
		{
			inst.Transform = XMMatrixScaling(inst.Scale.x, inst.Scale.y, inst.Scale.z) *
				XMMatrixRotationRollPitchYaw(XMConvertToRadians(inst.Rotation.x), XMConvertToRadians(inst.Rotation.y), XMConvertToRadians(inst.Rotation.z)) *
				XMMatrixTranslation(inst.Position.x, inst.Position.y, inst.Position.z);
			inst.InverseTransform = XMMatrixInverse(nullptr, inst.Transform);
		}

		//accelManager->RefitTLAS(m_ModelInstances);
		accelManager->BuildTLAS(m_ModelInstances);
		OnViewChanged();
	}

	if (m_sceneDataDirty)
	{
		OnViewChanged();
	}

	m_frameIndex++;
}

void SceneOne::RenderImGui()
{
	ImGui::Begin("RayTracer Specifications", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	m_sceneDataDirty |= ImGui::DragInt("Number Of Bounces", &mc_numBounces, 1.0f, 1, 100);
	m_sceneDataDirty |= ImGui::DragInt("Rays Per Pixel", &mc_numRaysPerPixel, 1.0f, 1, 100);
	m_sceneDataDirty |= ImGui::DragFloat("Exposure", &mc_exposure, 0.05f, 0.0f, 10.0f);

	ImGui::Separator();

	if (ImGui::Button("Load and Add Model to Scene"))
	{
		std::wstring filePath = OpenFileDialog();
		if (!filePath.empty())
		{
			std::filesystem::path path(filePath);
			std::string modelKey = path.stem().string();

			ResourceManager::Get()->LoadModel(modelKey, WStringToString(filePath));
			Model* loadedModel = ResourceManager::Get()->GetModel(modelKey);

			if (loadedModel)
			{
				AddModelInstance(loadedModel, modelKey);
			}
		}
	}
	ImGui::Separator();
	if (ImGui::Button("Load Environment Map"))
	{
		std::wstring filePath = OpenFileDialog();
		if (!filePath.empty())
		{
			std::filesystem::path path(filePath);
			std::string envMapKey = "env_" + path.stem().string();

			HRESULT hr = ResourceManager::Get()->LoadTextureFromFile(envMapKey, filePath);
			if (SUCCEEDED(hr))
			{
				Texture* loadedTexture = ResourceManager::Get()->GetTexture(envMapKey);
				if (loadedTexture)
				{
					m_pEnvironmentTexture = loadedTexture;
					m_currentEnvMapName = path.filename().string();

					// Re-create the SRV for the new texture in our descriptor table
					auto pDevice = m_pRenderEngine->GetDevice();
					const UINT ENV_SRV_SLOT = 8;
					D3D12_SHADER_RESOURCE_VIEW_DESC envSrvDesc = {};
					envSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					envSrvDesc.Format = m_pEnvironmentTexture->m_ResourceGPU->GetDesc().Format;
					envSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					envSrvDesc.Texture2D.MostDetailedMip = 0;
					envSrvDesc.Texture2D.MipLevels = m_pEnvironmentTexture->m_ResourceGPU->GetDesc().MipLevels;
					pDevice->CreateShaderResourceView(m_pEnvironmentTexture->m_ResourceGPU.Get(), &envSrvDesc, m_computeDescriptorTable.GetCpuHandle(ENV_SRV_SLOT));

					m_useEnvMap = 1; 
					OnViewChanged(); 
				}
			}
		}
	}
	ImGui::Separator();

	ImGui::Text("Environment: %s", m_currentEnvMapName.c_str());
	m_sceneDataDirty |= ImGui::Checkbox("Use Environment Map", reinterpret_cast<bool*>(&m_useEnvMap));
	m_useEnvMap = m_useEnvMap ? 1 : 0;
	ImGui::End();

	ImGui::Begin("Scene Hierarchy");
	for (size_t i = 0; i < m_ModelInstances.size(); ++i)
	{
		auto& inst = m_ModelInstances[i];

		bool isSelected = (m_selectedInstanceIndex == i);
		if (ImGui::Selectable((inst.Name).c_str(), isSelected))
		{
			m_selectedInstanceIndex = i;
		}

		if (isSelected)
		{
			ImGui::SetItemDefaultFocus();
		}
	}
	ImGui::End();

	ImGui::Begin("Properties");
	if (m_selectedInstanceIndex >= 0 && m_selectedInstanceIndex < m_ModelInstances.size())
	{
		auto& inst = m_ModelInstances[m_selectedInstanceIndex];

		char nameBuffer[256];
		strncpy_s(nameBuffer, sizeof(nameBuffer), inst.Name.c_str(), sizeof(nameBuffer) - 1);

		if (ImGui::InputText("Instance Name", nameBuffer, sizeof(nameBuffer)))
		{
			inst.Name = nameBuffer;
		}
		ImGui::Separator();

		// --- Transform Widgets ---
		ImGui::Text("Transform");
		m_tlasDirty |= ImGui::DragFloat3("Position", &inst.Position.x, 0.1f);
		m_tlasDirty |= ImGui::DragFloat3("Rotation", &inst.Rotation.x, 1.0f);
		static bool uniformScale = false; 
		ImGui::Checkbox("Uniform Scale", &uniformScale);

		if (uniformScale)
		{
			float scale = inst.Scale.x; 
			if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.001f, 10.0f))
			{
				inst.Scale = { scale, scale, scale };
				m_tlasDirty = true;
			}
		}
		else
		{
			m_tlasDirty |= ImGui::DragFloat3("Scale", &inst.Scale.x, 0.05f);
		}
		ImGui::Separator();

		// --- Material Widgets ---
		ImGui::Text("Materials");
		uint32_t materialOffset = inst.MaterialOffset;
		for (size_t i = 0; i < inst.SourceModel->Materials.size(); ++i)
		{
			ImGui::PushID((int)i); // Ensure unique IDs for each material widget set.
			Material& mat = m_materials[materialOffset + i];

			std::string matLabel = "Material " + std::to_string(i);
			if (ImGui::TreeNode(matLabel.c_str()))
			{
				m_sceneDataDirty |= ImGui::ColorEdit4("Base Color", reinterpret_cast<float*>(&mat.BaseColorFactor));
				m_sceneDataDirty |= ImGui::ColorEdit3("Emissive Factor", reinterpret_cast<float*>(&mat.EmissiveFactor));
				m_sceneDataDirty |= ImGui::DragFloat("Metallic Factor", &mat.MetallicFactor, 0.05f, 0.0f, 1.0f);
				m_sceneDataDirty |= ImGui::DragFloat("Roughness Factor", &mat.RoughnessFactor, 0.05f, 0.0f, 1.0f);
				m_sceneDataDirty |= ImGui::DragFloat("Transmission", &mat.Transmission, 0.05f, 0.0f, 1.0f);
				m_sceneDataDirty |= ImGui::DragFloat("Index of Refraction (IOR)", &mat.IOR, 0.01f, 0.0f, 5.0f);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
	else
	{
		ImGui::Text("No instance selected.");
	}
	ImGui::End();

	if (m_sceneDataDirty || m_tlasDirty)
	{
		OnViewChanged();
	}
}

void SceneOne::OnDestroy()
{
}
