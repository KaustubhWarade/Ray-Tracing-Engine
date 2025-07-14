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
		D3D12_DESCRIPTOR_RANGE1 descriptorRangeCBV = {};
		ZeroMemory(&descriptorRangeCBV, sizeof(D3D12_DESCRIPTOR_RANGE1));
		descriptorRangeCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		descriptorRangeCBV.NumDescriptors = 1;
		descriptorRangeCBV.BaseShaderRegister = 0;
		descriptorRangeCBV.RegisterSpace = 0;
		descriptorRangeCBV.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		descriptorRangeCBV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 descriptorRangeUAV = {};
		ZeroMemory(&descriptorRangeUAV, sizeof(D3D12_DESCRIPTOR_RANGE1));
		descriptorRangeUAV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		descriptorRangeUAV.NumDescriptors = 2;
		descriptorRangeUAV.BaseShaderRegister = 0;
		descriptorRangeUAV.RegisterSpace = 0;
		descriptorRangeUAV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 descriptorRangeSRV = {};
		ZeroMemory(&descriptorRangeSRV, sizeof(D3D12_DESCRIPTOR_RANGE1));
		descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRangeSRV.NumDescriptors = 3; // 2structure biffers +  1 texture buffer
		descriptorRangeSRV.BaseShaderRegister = 0;
		descriptorRangeSRV.RegisterSpace = 0;
		descriptorRangeSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 rootParameter[3];
		ZeroMemory(&rootParameter[0], sizeof(D3D12_ROOT_PARAMETER1));
		rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParameter[0].DescriptorTable.pDescriptorRanges = &descriptorRangeCBV;
		rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		ZeroMemory(&rootParameter[1], sizeof(D3D12_ROOT_PARAMETER1));
		rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameter[1].DescriptorTable.pDescriptorRanges = &descriptorRangeUAV;
		rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		ZeroMemory(&rootParameter[2], sizeof(D3D12_ROOT_PARAMETER1));
		rootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParameter[2].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
		rootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		CreateStaticSampler(staticSampler, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,0,0,D3D12_SHADER_VISIBILITY_ALL);


		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
		ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
		rootSignatureDesc.NumParameters = 3;
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
		EXECUTE_AND_LOG_RETURN(D3D12SerializeVersionedRootSignature(&versionedRootSignatureDesc, &pID3DBlob_signature, &pID3DBlob_Error));

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
		D3D12_INPUT_ELEMENT_DESC d3dInputElementDesc[2];
		ZeroMemory(&d3dInputElementDesc, sizeof(D3D12_INPUT_ELEMENT_DESC) * _ARRAYSIZE(d3dInputElementDesc));
		d3dInputElementDesc[0].SemanticName = "POSITION";
		d3dInputElementDesc[0].SemanticIndex = 0;
		d3dInputElementDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		d3dInputElementDesc[0].InputSlot = 0;
		d3dInputElementDesc[0].AlignedByteOffset = 0;
		d3dInputElementDesc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		d3dInputElementDesc[0].InstanceDataStepRate = 0;

		d3dInputElementDesc[1].SemanticName = "TEXCOORD";
		d3dInputElementDesc[1].SemanticIndex = 0;
		d3dInputElementDesc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
		d3dInputElementDesc[1].InputSlot = 1;
		d3dInputElementDesc[1].AlignedByteOffset = 0;
		d3dInputElementDesc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		d3dInputElementDesc[1].InstanceDataStepRate = 0;

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
			2,
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
	m_quadMesh.name = L"Quad_Mesh";
	// Load quad position
	{
		const float quad_position[] = {
			-1.0f, +1.0f, 0.0f, // top-left of front
			+1.0f, +1.0f, 0.0f, // top-right of front
			-1.0f, -1.0f, 0.0f, // bottom-left of front

			-1.0f, -1.0f, 0.0f, // bottom-left of front
			+1.0f, +1.0f, 0.0f, // top-right of front
			+1.0f, -1.0f, 0.0f, // bottom-right of front
		};
		const UINT vertexPositionBufferSize = sizeof(quad_position);

		EXECUTE_AND_LOG_RETURN(createGeometryVertexResource(
			m_pRenderEngine->GetDevice(),
			m_pRenderEngine->m_commandList.Get(),
			m_quadMesh,
			quad_position,
			vertexPositionBufferSize));

		m_quadMesh.VertexByteStride = sizeof(float) * 3;
		m_quadMesh.VertexBufferByteSize = vertexPositionBufferSize;
		m_quadMesh.VertexBufferUploader->SetName(L"m_quadMesh Vertex Buffer Uploader");
		m_quadMesh.VertexBufferGPU->SetName(L"m_quadMesh Vertex Buffer GPU");
	}

	// Load quad texcoord
	{
		// Define the geometry for a triangle.
		const float quad_texcoord[] = {
			// front
			0.0f, 1.0f, // top-left of front
			1.0f, 1.0f, // top-right of front
			0.0f, 0.0f, // bottom-left of front

			0.0f, 0.0f, // bottom-left of front
			1.0f, 1.0f, // top-right of front
			1.0f, 0.0f, // bottom-right of front
		};

		const UINT vertexTexcoordBufferSize = sizeof(quad_texcoord);

		EXECUTE_AND_LOG_RETURN(createGeometryTexCoordResource(
			m_pRenderEngine->GetDevice(),
			m_pRenderEngine->m_commandList.Get(),
			m_quadMesh,
			quad_texcoord,
			vertexTexcoordBufferSize));

		// Initialize the vertex buffer view.
		m_quadMesh.TexCoordByteStride = sizeof(float) * 2;
		m_quadMesh.TexCoordBufferByteSize = vertexTexcoordBufferSize;
		m_quadMesh.TexCoordBufferUploader->SetName(L"Quad_Mesh TexCoord Buffer Uploader");
		m_quadMesh.TexCoordBufferGPU->SetName(L"Quad_Mesh TexCoord Buffer GPU");
	}

	// Load scene
	{
		//light
		Material tempMaterial;
		tempMaterial.Albedo = XMFLOAT3(1.0f, 1.0f, 1.0f);
		tempMaterial.Roughness = 1.0f;
		tempMaterial.EmissionColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
		tempMaterial.EmissionPower = 5.0f;
		m_sceneData.Materials.push_back(tempMaterial);
	}
	{
		//base
		Material tempMaterial;
		tempMaterial.Albedo = XMFLOAT3(0.6f, 0.4f, 0.8f);
		tempMaterial.Roughness = 1.0f;
		tempMaterial.EmissionColor = tempMaterial.Albedo;
		tempMaterial.EmissionPower = 0.0f;
		m_sceneData.Materials.push_back(tempMaterial);
	}
	{
		//material-1
		Material tempMaterial;
		tempMaterial.Albedo = XMFLOAT3(0.0f, 0.0f, 0.0f);
		tempMaterial.Roughness = 1.0f;
		m_sceneData.Materials.push_back(tempMaterial);
	}
	{
		//material-2
		Material tempMaterial;
		tempMaterial.Albedo = XMFLOAT3(0.0f, 0.0f, 1.0f);
		tempMaterial.Roughness = 1.0f;
		m_sceneData.Materials.push_back(tempMaterial);
	}
	{
		//material-3
		Material tempMaterial;
		tempMaterial.Albedo = XMFLOAT3(0.0f, 1.0f, 0.0f);
		tempMaterial.Roughness = 1.0f;
		//tempMaterial.EmissionColor = XMFLOAT3(0.0f, 0.0f, 0.0f);
		//tempMaterial.EmissionPower = 5.0f;
		m_sceneData.Materials.push_back(tempMaterial);
	}
	{
		//material-4
		Material tempMaterial;
		tempMaterial.Albedo = XMFLOAT3(1.0f, 0.0f, 0.0f);
		tempMaterial.Roughness = 1.0f;
		m_sceneData.Materials.push_back(tempMaterial);
	}
	{
		//material-5
		Material tempMaterial;
		tempMaterial.Albedo = XMFLOAT3(1.0f, 1.0f, 1.0f);
		tempMaterial.Roughness = 1.0f;
		m_sceneData.Materials.push_back(tempMaterial);
	}
	{
		//light
		Sphere tempSphere;
		tempSphere.Position = XMFLOAT3(-30.0f, 28.0f, -50.0f);
		tempSphere.Radius = 20.0f;
		tempSphere.MaterialIndex = 0;
		m_sceneData.Spheres.push_back(tempSphere);
	}
	{
		//base
		Sphere tempSphere;
		tempSphere.Position = XMFLOAT3(0.0f, -101.0f, 0.0f);
		tempSphere.Radius = 100.0f;
		tempSphere.MaterialIndex = 1;
		m_sceneData.Spheres.push_back(tempSphere);
	}
	{
		//sphere-1
		Sphere tempSphere;
		tempSphere.Position = XMFLOAT3(-6.0f, 0.0f, 0.0f);
		tempSphere.Radius = 0.5f;
		tempSphere.MaterialIndex = 2;
		m_sceneData.Spheres.push_back(tempSphere);
	}
	{
		//sphere-2
		Sphere tempSphere;
		tempSphere.Position = XMFLOAT3(-4.0f, 0.0f, 0.0f);
		tempSphere.Radius = 1.0f;
		tempSphere.MaterialIndex = 3;
		m_sceneData.Spheres.push_back(tempSphere);
	}
	{
		//sphere-3
		Sphere tempSphere;
		tempSphere.Position = XMFLOAT3(-1.0f, 0.5f, 0.0f);
		tempSphere.Radius = 1.5f;
		tempSphere.MaterialIndex = 4;
		m_sceneData.Spheres.push_back(tempSphere);
	}
	{
		//sphere-4
		Sphere tempSphere;
		tempSphere.Position = XMFLOAT3(3.5f, 1.0f, 0.0f);
		tempSphere.Radius = 2.0f;
		tempSphere.MaterialIndex = 5;
		m_sceneData.Spheres.push_back(tempSphere);
	}
	{
		//sphere-5
		Sphere tempSphere;
		tempSphere.Position = XMFLOAT3(9.0f, 1.5f, 0.0f);
		tempSphere.Radius = 2.5f;
		tempSphere.MaterialIndex = 6;
		m_sceneData.Spheres.push_back(tempSphere);
	}

	return hr;
}

HRESULT SceneOne::InitializeComputeResources(void)
{
	HRESULT hr = S_OK;
	auto pDevice = m_pRenderEngine->GetDevice();
	auto pCommandList = m_pRenderEngine->m_commandList.Get();

	m_accumulationTexture.Initialize(pDevice, pCommandList);
	m_outputTextureImage.Initialize(pDevice, pCommandList);

	m_accumulationTexture.CreateImageResource(WIN_WIDTH, WIN_HEIGHT,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_accumulationTexture.m_imageGPU->SetName(L"Accumulation Texture");

	m_outputTextureImage.CreateImageResource(WIN_WIDTH, WIN_HEIGHT,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_outputTextureImage.m_imageGPU->SetName(L"Output Texture");

	EXECUTE_AND_LOG_RETURN(m_environmentTexture.loadTexture(pDevice, pCommandList, L"SceneOne/env.dds"));

	{

		CreateDefaultBuffer(pDevice, pCommandList, m_sceneData.Spheres.data(),
			sizeof(Sphere) * m_sceneData.Spheres.size(), m_sphereUpload, m_sphereBuffer);
		m_sphereBuffer->SetName(L"Sphere Buffer");

		CreateDefaultBuffer(pDevice, pCommandList, m_sceneData.Materials.data(),
			sizeof(Material) * m_sceneData.Materials.size(), m_materialUpload, m_materialBuffer);
		m_materialBuffer->SetName(L"Material Buffer");

	}

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
	{

		m_computeDescriptorHeapStart = m_pRenderEngine->GetCbvSrvUavAllocator().Allocate(6);
		m_texQuadDescriptorHeapStart = m_pRenderEngine->GetCbvSrvUavAllocator().Allocate(1);

		UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE computeHeapHandler = m_computeDescriptorHeapStart.CpuHandle;

		// CBV for Constant Buffer (b0)
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_ConstantBufferView->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(CBUFFER) + 255) & ~255;
		pDevice->CreateConstantBufferView(&cbvDesc, computeHeapHandler);
		computeHeapHandler.ptr += descriptorSize;

		// UAV for Accumulation Texture (u0)
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		pDevice->CreateUnorderedAccessView(m_accumulationTexture.m_imageGPU.Get(), nullptr, &uavDesc, computeHeapHandler);
		computeHeapHandler.ptr += descriptorSize;

		// UAV for Output Texture (u1)
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		pDevice->CreateUnorderedAccessView(m_outputTextureImage.m_imageGPU.Get(), nullptr, &uavDesc, computeHeapHandler);
		computeHeapHandler.ptr += descriptorSize;

		// SRV for Sphere Buffer (t0)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.NumElements = m_sceneData.Spheres.size();
		srvDesc.Buffer.StructureByteStride = sizeof(Sphere);
		pDevice->CreateShaderResourceView(m_sphereBuffer.Get(), &srvDesc, computeHeapHandler);
		computeHeapHandler.ptr += descriptorSize;

		// SRV for Material Buffer (t1)
		srvDesc.Buffer.NumElements = m_sceneData.Materials.size();
		srvDesc.Buffer.StructureByteStride = sizeof(Material);
		pDevice->CreateShaderResourceView(m_materialBuffer.Get(), &srvDesc, computeHeapHandler);
		computeHeapHandler.ptr += descriptorSize;

		D3D12_SHADER_RESOURCE_VIEW_DESC textureSrvDesc = {};
		textureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		textureSrvDesc.Format = m_environmentTexture.m_ResourceGPU->GetDesc().Format;
		textureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		textureSrvDesc.Texture2D.MipLevels = m_environmentTexture.m_ResourceGPU->GetDesc().MipLevels;
		textureSrvDesc.Texture2D.MostDetailedMip = 0;
		textureSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		pDevice->CreateShaderResourceView(m_environmentTexture.m_ResourceGPU.Get(), &textureSrvDesc, computeHeapHandler);

		D3D12_CPU_DESCRIPTOR_HANDLE graphicsSrvHandle = m_texQuadDescriptorHeapStart.CpuHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC quadSrvDesc = {};
		quadSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		quadSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		quadSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		quadSrvDesc.Texture2D.MipLevels = 1;
		pDevice->CreateShaderResourceView(m_outputTextureImage.m_imageGPU.Get(), &quadSrvDesc, graphicsSrvHandle);
	}
	return hr;

}

void SceneOne::OnResize(UINT width, UINT height)
{
	if (m_pRenderEngine)
	{
		m_pRenderEngine->m_camera.OnResize(width, height);
	}

	if (m_outputTextureImage.m_width != width || m_outputTextureImage.m_height != height)
	{
		m_accumulationTexture.Resize(width, height);
		m_outputTextureImage.Resize(width, height);
	}

	m_frameIndex = 0;
	m_cameraMoved = true;

	auto pDevice = m_pRenderEngine->GetDevice();
	UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE computeHeapHandle = m_computeDescriptorHeapStart.CpuHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE accumulationUavHandle = computeHeapHandle;
	//U0 accumulate textuer
	accumulationUavHandle.ptr += 1 * descriptorSize;
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	pDevice->CreateUnorderedAccessView(m_accumulationTexture.m_imageGPU.Get(), nullptr, &uavDesc, accumulationUavHandle);
	//U1 Output texture
	D3D12_CPU_DESCRIPTOR_HANDLE outputUavHandle = computeHeapHandle;
	outputUavHandle.ptr += 2 * descriptorSize;
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	pDevice->CreateUnorderedAccessView(m_outputTextureImage.m_imageGPU.Get(), nullptr, &uavDesc, outputUavHandle);


	//assign outputtexture to output texture heap location heap
	D3D12_CPU_DESCRIPTOR_HANDLE graphicsSrvHandle = m_texQuadDescriptorHeapStart.CpuHandle;
	D3D12_SHADER_RESOURCE_VIEW_DESC quadSrvDesc = {};
	quadSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	quadSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	quadSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	quadSrvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(m_outputTextureImage.m_imageGPU.Get(), &quadSrvDesc, graphicsSrvHandle);

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


	ID3D12DescriptorHeap* ppHeaps[] = { m_pRenderEngine->GetCbvSrvUavAllocator().GetHeap() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	if (m_sceneDataDirty == true)
	{
		
		{
			UINT8* pData;
			const UINT bufferSize = sizeof(Sphere) * m_sceneData.Spheres.size();
			D3D12_RANGE readRange = { 0, 0 };
			m_sphereUpload->Map(0, &readRange, reinterpret_cast<void**>(&pData));
			memcpy(pData, m_sceneData.Spheres.data(), bufferSize);
			m_sphereUpload->Unmap(0, nullptr);
			TransitionResource(cmdList, m_sphereBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
			cmdList->CopyBufferRegion(m_sphereBuffer.Get(), 0, m_sphereUpload.Get(), 0, bufferSize);
			TransitionResource(cmdList, m_sphereBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
		{
			UINT8* pData;
			const UINT bufferSize = sizeof(Material) * m_sceneData.Materials.size();
			D3D12_RANGE readRange = { 0, 0 };
			m_materialUpload->Map(0, &readRange, reinterpret_cast<void**>(&pData));
			memcpy(pData, m_sceneData.Materials.data(), bufferSize);
			m_materialUpload->Unmap(0, nullptr);
			TransitionResource(cmdList, m_materialBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
			cmdList->CopyBufferRegion(m_materialBuffer.Get(), 0, m_materialUpload.Get(), 0, bufferSize);
			TransitionResource(cmdList, m_materialBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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

	D3D12_GPU_DESCRIPTOR_HANDLE computeHeapHandle = m_computeDescriptorHeapStart.GpuHandle;
	// CBV
	cmdList->SetComputeRootDescriptorTable(0, computeHeapHandle);
	// UAV
	computeHeapHandle.ptr += descriptorSize;	// 1 CBV
	cmdList->SetComputeRootDescriptorTable(1, computeHeapHandle);
	// SRV
	computeHeapHandle.ptr += 2 * descriptorSize;	 //2 UAV
	cmdList->SetComputeRootDescriptorTable(2, computeHeapHandle);

	cmdList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

	TransitionResource(cmdList, m_outputTextureImage.m_imageGPU.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


	// Render Pass
	m_pRenderEngine->TransitionResource(cmdList, m_pRenderEngine->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetPipelineState(m_finalTexturePSO.PipelineState.Get());
	cmdList->SetGraphicsRootSignature(m_finalTexturePSO.rootSignature.Get());


	cmdList->SetGraphicsRootDescriptorTable(0, m_texQuadDescriptorHeapStart.GpuHandle);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRenderEngine->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += m_pRenderEngine->m_frameIndex * m_pRenderEngine->m_rtvDescriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pRenderEngine->m_dsbHeap->GetCPUDescriptorHandleForHeapStart();

	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = m_quadMesh.VertexBufferView();
	cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
	D3D12_VERTEX_BUFFER_VIEW texCoordBufferView = m_quadMesh.TexCoordBufferView();
	cmdList->IASetVertexBuffers(1, 1, &texCoordBufferView);
	cmdList->DrawInstanced(6, 1, 0, 0);

	TransitionResource(cmdList, m_outputTextureImage.m_imageGPU.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
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
	for (size_t i = 0; i < m_sceneData.Spheres.size(); i++)
	{
		ImGui::PushID(i);

		Sphere& sphere = m_sceneData.Spheres[i];

		m_sceneDataDirty |= ImGui::DragFloat3("Position", reinterpret_cast<float*>(&sphere.Position), 0.1f);
		m_sceneDataDirty |= ImGui::DragFloat("Radius", &sphere.Radius, 0.1f);
		m_sceneDataDirty |= ImGui::DragInt("Material", &sphere.MaterialIndex, 1.0f, 0, (int)m_sceneData.Materials.size() - 1);

		ImGui::Separator();

		ImGui::PopID();
	}
	for (size_t i = 0; i < m_sceneData.Materials.size(); i++)
	{
		ImGui::PushID(i);

		Material& material = m_sceneData.Materials[i];

		m_sceneDataDirty |= ImGui::ColorEdit3("Albedo", reinterpret_cast<float*>(&material.Albedo));
		m_sceneDataDirty |= ImGui::DragFloat("Roughness", &material.Roughness, 0.05f, 0.0f, 1.0f);
		m_sceneDataDirty |= ImGui::DragFloat("Metallic", &material.Metallic, 0.05f, 0.0f, 1.0f);
		m_sceneDataDirty |= ImGui::ColorEdit3("Emission Color", reinterpret_cast<float*>(&material.EmissionColor));
		m_sceneDataDirty |= ImGui::DragFloat("Emission Power", &material.EmissionPower, 0.05f, 0.0f, FLT_MAX);
		m_sceneDataDirty |= ImGui::ColorEdit3("Absorption Color", reinterpret_cast<float*>(&material.AbsorptionColor));
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
