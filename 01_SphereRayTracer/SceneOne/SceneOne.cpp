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
		D3D12_DESCRIPTOR_RANGE1 ranges[4];
		// Range 0: Constant Buffer (b0)
		ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC };
		// Range 1: UAVs (u0, u1)
		ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
		// Range 2: Structured Buffer SRVs (t0, t1, t2)
		ranges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
		// Range 3: Texture SRV (t3)
		ranges[3] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };

		D3D12_ROOT_PARAMETER1 rootParameter[1];
		rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter[0].DescriptorTable.NumDescriptorRanges = _countof(ranges);
		rootParameter[0].DescriptorTable.pDescriptorRanges = ranges;
		rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		CreateStaticSampler(staticSampler, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,0,0,D3D12_SHADER_VISIBILITY_ALL);


		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
		ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
		rootSignatureDesc.NumParameters = 1;
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
	EXECUTE_AND_LOG_RETURN(InitializeComputeResources());

	m_pRenderEngine->m_camera.SetPosition(-10.0f, 5.5f, 7.2f);

	return hr;
}

HRESULT SceneOne::InitializeResources(void)
{
	HRESULT hr = S_OK;
	auto pDevice = m_pRenderEngine->GetDevice();
	auto pCmdList = m_pRenderEngine->m_commandList.Get();
	auto resourceManager = ResourceManager::Get();
	// Load quad position
	{
		ModelVertex quadVertices[] = {
	{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
	{ XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
	{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
	{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) }
		};
		uint16_t quadIndices[] = { 0, 1, 2, 2, 1, 3 };

		// Ask the ResourceManager to create the model from our data
		EXECUTE_AND_LOG_RETURN(resourceManager->CreateModel(
			"FullscreenQuad", // Give it a unique name
			quadVertices, sizeof(quadVertices), sizeof(ModelVertex),
			quadIndices, sizeof(quadIndices), DXGI_FORMAT_R16_UINT
		));
		m_pQuadModel = resourceManager->GetModel("FullscreenQuad");
		if (!m_pQuadModel)
		{
			return E_FAIL; // Failed to create or retrieve the model
		}
	}

	// Load scene
	{
		Material mat;

		// 0: Light
		mat = Material();
		mat.BaseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
		mat.RoughnessFactor = 1.0f;
		mat.EmissiveFactor = { 5.0f, 5.0f, 5.0f }; // Color * Power
		m_materials.push_back(mat);

		// 1: Base
		mat = Material();
		mat.BaseColorFactor = { 0.6f, 0.4f, 0.8f, 1.0f };
		mat.RoughnessFactor = 1.0f;
		m_materials.push_back(mat);

		// 2: Material 1 (Black)
		mat = Material();
		mat.BaseColorFactor = { 0.0f, 0.0f, 0.0f, 1.0f };
		mat.RoughnessFactor = 1.0f;
		m_materials.push_back(mat);

		// 3: Material 2 (Blue)
		mat = Material();
		mat.BaseColorFactor = { 0.0f, 0.0f, 1.0f, 1.0f };
		mat.RoughnessFactor = 1.0f;
		m_materials.push_back(mat);

		// 4: Material 3 (Green)
		mat = Material();
		mat.BaseColorFactor = { 0.0f, 1.0f, 0.0f, 1.0f };
		mat.RoughnessFactor = 1.0f;
		m_materials.push_back(mat);

		// 5: Material 4 (Red)
		mat = Material();
		mat.BaseColorFactor = { 1.0f, 0.0f, 0.0f, 1.0f };
		mat.RoughnessFactor = 1.0f;
		m_materials.push_back(mat);

		// 6: Material 5 (White)
		mat = Material();
		mat.BaseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
		mat.RoughnessFactor = 1.0f;
		m_materials.push_back(mat);
		m_spheres = {
			{ {-30.0f, 28.0f, -50.0f}, 20.0f, 0},
			{ {0.0f, -101.0f, 0.0f}, 100.0f, 1},
			{ {-6.0f, 0.0f, 0.0f}, 0.5f, 2},
			{ {-4.0f, 0.0f, 0.0f}, 1.0f, 3},
			{ {-1.0f, 0.5f, 0.0f}, 1.5f, 4},
			{ {3.5f, 1.0f, 0.0f}, 2.0f, 5},
			{ {9.0f, 1.5f, 0.0f}, 2.5f, 6},
		};

		m_triangles = {
			{ {-1.0f, +1.0f, -1.0f}, {+1.0f, +1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, 6},
		};
	}

	return hr;
}

HRESULT SceneOne::InitializeComputeResources(void)
{
	HRESULT hr = S_OK;
	auto pDevice = m_pRenderEngine->GetDevice();
	auto pCommandList = m_pRenderEngine->m_commandList.Get();

	auto resourceManager = ResourceManager::Get();

	// Create resources using the ResourceManager
	UINT width = 800;
	UINT height = 700;

	EXECUTE_AND_LOG_RETURN(resourceManager->CreateImage("Accumulation", width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
	EXECUTE_AND_LOG_RETURN(resourceManager->CreateImage("Output", width, height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
	EXECUTE_AND_LOG_RETURN(resourceManager->LoadTextureFromFile("Environment", L"SceneOne/env.dds"));

	m_pAccumulationImage = resourceManager->GetImage("Accumulation");
	m_pOutputImage = resourceManager->GetImage("Output");
	m_pEnvironmentTexture = resourceManager->GetTexture("Environment");

	{

		m_sphereBuffer.Stride = sizeof(Sphere);
		m_sphereBuffer.Sync(m_pRenderEngine, m_pRenderEngine->m_commandList.Get(), m_spheres.data(), (UINT)m_spheres.size());
		if (m_sphereBuffer.Resource) m_sphereBuffer.Resource->SetName(L"Sphere Buffer");

		m_triangleBuffer.Stride = sizeof(Triangle);
		m_triangleBuffer.Sync(m_pRenderEngine, m_pRenderEngine->m_commandList.Get(), m_triangles.data(), (UINT)m_triangles.size());
		if (m_triangleBuffer.Resource) m_triangleBuffer.Resource->SetName(L"Triangle Buffer");

		m_materialBuffer.Stride = sizeof(Material);
		m_materialBuffer.Sync(m_pRenderEngine, m_pRenderEngine->m_commandList.Get(), m_materials.data(), (UINT)m_materials.size());
		if (m_materialBuffer.Resource) m_materialBuffer.Resource->SetName(L"Material Buffer");

	}

	EXECUTE_AND_LOG_RETURN(CreateDescriptorTables());

	return hr;
}

HRESULT SceneOne::CreateDescriptorTables()
{
	auto pDevice = m_pRenderEngine->GetDevice();
	auto resourceManager = ResourceManager::Get();
	// Allocate a contiguous block of 7 descriptors for the compute pass
	{

		UINT cbvSize = (sizeof(CBUFFER) + 255) & ~255;
		D3D12_HEAP_PROPERTIES heapProperties;
		CreateHeapProperties(heapProperties, D3D12_HEAP_TYPE_UPLOAD);

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = cbvSize;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


		EXECUTE_AND_LOG_RETURN(pDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_ConstantBufferView)));
		m_ConstantBufferView->SetName(L"Per Frame Constant Buffer");

		// Map the buffer and keep the pointer
		D3D12_RANGE readRange = {};
		readRange.Begin = 0;
		readRange.End = 0;
		m_ConstantBufferView->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin));

	}
	m_computeDescriptorTable = resourceManager->m_generalPurposeHeapAllocator.AllocateTable(7);
	m_finalPassSrvTable = resourceManager->m_generalPurposeHeapAllocator.AllocateTable(1);

	// Slot 0: CBV (b0)
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_ConstantBufferView->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (sizeof(CBUFFER) + 255) & ~255;
	pDevice->CreateConstantBufferView(&cbvDesc, m_computeDescriptorTable.GetCpuHandle(0));

	// Slot 1: Accumulation UAV (u0)
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	pDevice->CreateUnorderedAccessView(m_pAccumulationImage->GetResource(), nullptr, &uavDesc, m_computeDescriptorTable.GetCpuHandle(1));

	// Slot 2: Output UAV (u1)
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	pDevice->CreateUnorderedAccessView(m_pOutputImage->GetResource(), nullptr, &uavDesc, m_computeDescriptorTable.GetCpuHandle(2));

	// Slot 3: Sphere SRV (t0)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = m_sphereBuffer.Size;
	srvDesc.Buffer.StructureByteStride = m_sphereBuffer.Stride;
	pDevice->CreateShaderResourceView(m_sphereBuffer.Resource.Get(), &srvDesc, m_computeDescriptorTable.GetCpuHandle(3));

	// Slot 4: Triangle SRV (t1)
	srvDesc.Buffer.NumElements = m_triangleBuffer.Size;
	srvDesc.Buffer.StructureByteStride = m_triangleBuffer.Stride;
	pDevice->CreateShaderResourceView(m_triangleBuffer.Resource.Get(), &srvDesc, m_computeDescriptorTable.GetCpuHandle(4));

	// Slot 5: Material SRV (t2)
	srvDesc.Buffer.NumElements = m_materialBuffer.Size;
	srvDesc.Buffer.StructureByteStride = m_materialBuffer.Stride;
	pDevice->CreateShaderResourceView(m_materialBuffer.Resource.Get(), &srvDesc, m_computeDescriptorTable.GetCpuHandle(5));

	// Slot 6: Environment SRV (t3)
	D3D12_SHADER_RESOURCE_VIEW_DESC envSrvDesc = {};
	envSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	envSrvDesc.Format = m_pEnvironmentTexture->m_ResourceGPU->GetDesc().Format;
	envSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	envSrvDesc.Texture2D.MipLevels = m_pEnvironmentTexture->m_ResourceGPU->GetDesc().MipLevels;
	pDevice->CreateShaderResourceView(m_pEnvironmentTexture->m_ResourceGPU.Get(), &envSrvDesc, m_computeDescriptorTable.GetCpuHandle(6));

	// Populate Final Pass Descriptor Table
	D3D12_SHADER_RESOURCE_VIEW_DESC finalSrvDesc = {};
	finalSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	finalSrvDesc.Format = m_pOutputImage->GetResource()->GetDesc().Format;
	finalSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	finalSrvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(m_pOutputImage->GetResource(), &finalSrvDesc, m_finalPassSrvTable.GetCpuHandle(0));

	return S_OK;
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

		auto pDevice = m_pRenderEngine->GetDevice();

		// Slot 1: Accumulation UAV (u0)
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		pDevice->CreateUnorderedAccessView(m_pAccumulationImage->GetResource(), nullptr, &uavDesc, m_computeDescriptorTable.GetCpuHandle(1));

		// Slot 2: Output UAV (u1)
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		pDevice->CreateUnorderedAccessView(m_pOutputImage->GetResource(), nullptr, &uavDesc, m_computeDescriptorTable.GetCpuHandle(2));

		// Re-create SRV for the final pass
		D3D12_SHADER_RESOURCE_VIEW_DESC quadSrvDesc = {};
		quadSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		quadSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		quadSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		quadSrvDesc.Texture2D.MipLevels = 1;
		pDevice->CreateShaderResourceView(m_pOutputImage->GetResource(), &quadSrvDesc, m_finalPassSrvTable.GetCpuHandle(0));
	}
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
	UINT descriptorSize = m_pRenderEngine->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	ID3D12DescriptorHeap* ppHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	if (m_sceneDataDirty == true)
	{
		
		m_sphereBuffer.Sync(m_pRenderEngine, cmdList, m_spheres.data(), (UINT)m_spheres.size());
		m_triangleBuffer.Sync(m_pRenderEngine, cmdList, m_triangles.data(), (UINT)m_triangles.size());
		m_materialBuffer.Sync(m_pRenderEngine, cmdList, m_materials.data(), (UINT)m_materials.size());

		if (m_sphereBuffer.GpuResourceDirty) {
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.NumElements = m_sphereBuffer.Size;
			srvDesc.Buffer.StructureByteStride = m_sphereBuffer.Stride;
			m_pRenderEngine->GetDevice()->CreateShaderResourceView(m_sphereBuffer.Resource.Get(), &srvDesc, m_computeDescriptorTable.GetCpuHandle(3));
		}
		if (m_triangleBuffer.GpuResourceDirty) {
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.NumElements = m_triangleBuffer.Size;
			srvDesc.Buffer.StructureByteStride = m_triangleBuffer.Stride;
			m_pRenderEngine->GetDevice()->CreateShaderResourceView(m_triangleBuffer.Resource.Get(), &srvDesc, m_computeDescriptorTable.GetCpuHandle(4));
		}
		if (m_materialBuffer.GpuResourceDirty) {
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.NumElements = m_materialBuffer.Size;
			srvDesc.Buffer.StructureByteStride = m_materialBuffer.Stride;
			m_pRenderEngine->GetDevice()->CreateShaderResourceView(m_materialBuffer.Resource.Get(), &srvDesc, m_computeDescriptorTable.GetCpuHandle(5));
		}
		m_frameIndex = 0;
		m_sceneDataDirty = false;
	}

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
	memcpy(m_pCbvDataBegin, &constants, sizeof(CBUFFER));

	cmdList->SetComputeRootDescriptorTable(0, m_computeDescriptorTable.GetGpuHandle());

	cmdList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

	m_pRenderEngine->TransitionResource(cmdList, m_pOutputImage->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Render Pass
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
	m_frameIndex++;

}

void SceneOne::RenderImGui()
{
	ImGui::Begin("RayTracer Specifications", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	m_sceneDataDirty |= ImGui::DragInt("Number Of Bounces", &mc_numBounces, 1.0f, 1, 100);
	m_sceneDataDirty |= ImGui::DragInt("Rays Per Pixel", &mc_numRaysPerPixel, 1.0f, 1, 100);
	m_sceneDataDirty |= ImGui::DragFloat("Exposure", &mc_exposure, 0.05f, 0.0f, 10.0f);
	ImGui::End();


	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Scene Controles");
	if (ImGui::TreeNode("Spheres"))
	{
		for (size_t i = 0; i < m_spheres.size(); i++)
		{
			ImGui::PushID((int)i);
			Sphere& sphere = m_spheres[i];
			if (ImGui::TreeNode((std::string("Sphere ") + std::to_string(i)).c_str()))
			{
				m_sceneDataDirty |= ImGui::DragFloat3("Position", reinterpret_cast<float*>(&sphere.Position), 0.1f);
				m_sceneDataDirty |= ImGui::DragFloat("Radius", &sphere.Radius, 0.1f);
				m_sceneDataDirty |= ImGui::DragInt("Material ID", &sphere.MaterialIndex, 1.0f, 0, (int)m_materials.size() - 1);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	for (size_t i = 0; i < m_materials.size(); i++)
	{
		ImGui::PushID(1000 + (int)i); // Ensure unique ID from spheres
		Material& material = m_materials[i];
		m_sceneDataDirty |= ImGui::ColorEdit4("Base Color", reinterpret_cast<float*>(&material.BaseColorFactor));
		m_sceneDataDirty |= ImGui::DragFloat("Roughness", &material.RoughnessFactor, 0.05f, 0.0f, 1.0f);
		m_sceneDataDirty |= ImGui::DragFloat("Metallic", &material.MetallicFactor, 0.05f, 0.0f, 1.0f);
		m_sceneDataDirty |= ImGui::ColorEdit3("Emission Color", reinterpret_cast<float*>(&material.EmissiveFactor));
		m_sceneDataDirty |= ImGui::DragFloat("Transmission", &material.Transmission, 0.05f, 0.0f, 1.0f);
		m_sceneDataDirty |= ImGui::DragFloat("IOR", &material.IOR, 0.01f, 0.0f, 20.0f);
		ImGui::Separator();
		ImGui::PopID();
	}
	ImGui::End();

	if (m_sceneDataDirty)
	{
		OnViewChanged();
	}

}

void SceneOne::OnDestroy()
{
}
