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
		descriptorRangeUAV.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		descriptorRangeUAV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 descriptorRangeSRV = {};
		ZeroMemory(&descriptorRangeSRV, sizeof(D3D12_DESCRIPTOR_RANGE1));
		descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRangeSRV.NumDescriptors = 3; // 3structure buffers
		descriptorRangeSRV.BaseShaderRegister = 0;
		descriptorRangeSRV.RegisterSpace = 0;
		descriptorRangeSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 descriptorRangeTextureSRV = {};
		ZeroMemory(&descriptorRangeTextureSRV, sizeof(D3D12_DESCRIPTOR_RANGE1));
		descriptorRangeTextureSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRangeTextureSRV.NumDescriptors = 1; // 1 texture
		descriptorRangeTextureSRV.BaseShaderRegister = 3;
		descriptorRangeTextureSRV.RegisterSpace = 0;
		descriptorRangeTextureSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 rootParameter[4];
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

		ZeroMemory(&rootParameter[3], sizeof(D3D12_ROOT_PARAMETER1));
		rootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter[3].DescriptorTable.NumDescriptorRanges = 1;
		rootParameter[3].DescriptorTable.pDescriptorRanges = &descriptorRangeTextureSRV;
		rootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		CreateStaticSampler(staticSampler, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,0,0,D3D12_SHADER_VISIBILITY_ALL);


		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
		ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
		rootSignatureDesc.NumParameters = 4;
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
	{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
	{ XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
	{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
	{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) }
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
	//box
	{
		const XMFLOAT3 cube_position[] = {
						XMFLOAT3(-1.0f, +1.0f, -1.0f), // top-left of front
						XMFLOAT3(+1.0f, +1.0f, -1.0f), // top-right of front
						XMFLOAT3(-1.0f, -1.0f, -1.0f), // bottom-left of front
						XMFLOAT3(-1.0f, -1.0f, -1.0f), // bottom-left of front
						XMFLOAT3(+1.0f, +1.0f, -1.0f), // top-right of front
						XMFLOAT3(+1.0f, -1.0f, -1.0f), // bottom-right of fron

						XMFLOAT3(+1.0f, +1.0f, -1.0f), // top-left of right
						XMFLOAT3(+1.0f, +1.0f, +1.0f), // top-right of right
						XMFLOAT3(+1.0f, -1.0f, -1.0f), // bottom-left of right
						XMFLOAT3(+1.0f, -1.0f, -1.0f), // bottom-left of right
						XMFLOAT3(+1.0f, +1.0f, +1.0f), // top-right of right
						XMFLOAT3(+1.0f, -1.0f, +1.0f), // bottom-right of right

						XMFLOAT3(+1.0f, +1.0f, +1.0f), // top-left of back
						XMFLOAT3(-1.0f, +1.0f, +1.0f), // top-right of back
						XMFLOAT3(+1.0f, -1.0f, +1.0f), // bottom-left of back
						XMFLOAT3(+1.0f, -1.0f, +1.0f), // bottom-left of back
						XMFLOAT3(-1.0f, +1.0f, +1.0f), // top-right of back
						XMFLOAT3(-1.0f, -1.0f, +1.0f), // bottom-right of back

						XMFLOAT3(-1.0f, +1.0f, +1.0f), // top-left of left
						XMFLOAT3(-1.0f, +1.0f, -1.0f), // top-right of left
						XMFLOAT3(-1.0f, -1.0f, +1.0f), // bottom-left of left
						XMFLOAT3(-1.0f, -1.0f, +1.0f), // bottom-left of left
						XMFLOAT3(-1.0f, +1.0f, -1.0f), // top-right of left
						XMFLOAT3(-1.0f, -1.0f, -1.0f), // bottom-right of left

						XMFLOAT3(-1.0f, +1.0f, +1.0f), // top-left of top
						XMFLOAT3(+1.0f, +1.0f, +1.0f), // top-right of top
						XMFLOAT3(-1.0f, +1.0f, -1.0f), // bottom-left of top
						XMFLOAT3(-1.0f, +1.0f, -1.0f), // bottom-left of top
						XMFLOAT3(+1.0f, +1.0f, +1.0f), // top-right of top
						XMFLOAT3(+1.0f, +1.0f, -1.0f), // bottom-right of top

						XMFLOAT3(-1.0f, -1.0f, -1.0f), // top-left of bottom
						XMFLOAT3(+1.0f, -1.0f, -1.0f), // top-right of bottom
						XMFLOAT3(-1.0f, -1.0f, +1.0f), // bottom-left of bottom
						XMFLOAT3(-1.0f, -1.0f, +1.0f), // bottom-left of bottom
						XMFLOAT3(+1.0f, -1.0f, -1.0f), // top-right of bottom
						XMFLOAT3(+1.0f, -1.0f, +1.0f), // bottom-right of bottom
		};
		{
			//triangle1
			Triangle tempTriangle;
				
			tempTriangle.v0 = XMFLOAT3(-1.0f, +1.0f, -1.0f), // top-left of front
			tempTriangle.v1 = XMFLOAT3(+1.0f, +1.0f, -1.0f), // top-right of front
			tempTriangle.v2 = XMFLOAT3(-1.0f, -1.0f, -1.0f), // bottom-left of front
			tempTriangle.MaterialIndex = 6;
			m_sceneData.Triangles.push_back(tempTriangle);
		}

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

		CreateDefaultBuffer(pDevice, pCommandList, m_sceneData.Spheres.data(),
			sizeof(Sphere) * m_sceneData.Spheres.size(), m_sphereUpload, m_sphereBuffer);
		m_sphereBuffer->SetName(L"Sphere Buffer");

		CreateDefaultBuffer(pDevice, pCommandList, m_sceneData.Triangles.data(),
			sizeof(Triangle) * m_sceneData.Triangles.size(), m_triangleUpload, m_triangleBuffer);
		m_triangleBuffer->SetName(L"Triangle Buffer");

		CreateDefaultBuffer(pDevice, pCommandList, m_sceneData.Materials.data(),
			sizeof(Material) * m_sceneData.Materials.size(), m_materialUpload, m_materialBuffer);
		m_materialBuffer->SetName(L"Material Buffer");

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
	m_computeDescriptorTable = resourceManager->m_cbvSrvUavAllocator.Allocate(7);
	D3D12_CPU_DESCRIPTOR_HANDLE computeHeapHandler = m_computeDescriptorTable.CpuHandle;
	UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//1. CBV for Constant Buffer (b0)
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_ConstantBufferView->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (sizeof(CBUFFER) + 255) & ~255;
	pDevice->CreateConstantBufferView(&cbvDesc, computeHeapHandler);
	computeHeapHandler.ptr += descriptorSize;

	// 2. UAV for Accumulation Texture (u0) - Create the view directly
	D3D12_UNORDERED_ACCESS_VIEW_DESC accumUavDesc = {};
	accumUavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // Format for accumulation
	accumUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	pDevice->CreateUnorderedAccessView(m_pAccumulationImage->GetResource(), nullptr, &accumUavDesc, computeHeapHandler);
	computeHeapHandler.ptr += descriptorSize;

	// 3. UAV for Output Texture (u1) - Create the view directly
	D3D12_UNORDERED_ACCESS_VIEW_DESC outputUavDesc = {};
	outputUavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Final display format
	outputUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	pDevice->CreateUnorderedAccessView(m_pOutputImage->GetResource(), nullptr, &outputUavDesc, computeHeapHandler);
	computeHeapHandler.ptr += descriptorSize;

	//4. SRV for Sphere Buffer (t0)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = m_sceneData.Spheres.size();
	srvDesc.Buffer.StructureByteStride = sizeof(Sphere);
	pDevice->CreateShaderResourceView(m_sphereBuffer.Get(), &srvDesc, computeHeapHandler);
	computeHeapHandler.ptr += descriptorSize;

	//5.SRV for triangle Buffer (t1)
	srvDesc.Buffer.NumElements = m_sceneData.Triangles.size();
	srvDesc.Buffer.StructureByteStride = sizeof(Triangle);
	pDevice->CreateShaderResourceView(m_triangleBuffer.Get(), &srvDesc, computeHeapHandler);
	computeHeapHandler.ptr += descriptorSize;

	//6. SRV for Material Buffer (t2)
	srvDesc.Buffer.NumElements = m_sceneData.Materials.size();
	srvDesc.Buffer.StructureByteStride = sizeof(Material);
	pDevice->CreateShaderResourceView(m_materialBuffer.Get(), &srvDesc, computeHeapHandler);
	computeHeapHandler.ptr += descriptorSize;

	//7. SRV for Environment texture (t3)
	D3D12_SHADER_RESOURCE_VIEW_DESC envSrvDesc = {};
	envSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	envSrvDesc.Format = m_pEnvironmentTexture->m_ResourceGPU->GetDesc().Format;
	envSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	envSrvDesc.Texture2D.MostDetailedMip = 0;
	envSrvDesc.Texture2D.MipLevels = m_pEnvironmentTexture->m_ResourceGPU->GetDesc().MipLevels;
	pDevice->CreateShaderResourceView(m_pEnvironmentTexture->m_ResourceGPU.Get(), &envSrvDesc, computeHeapHandler);

	// --- Create descriptor table for the final graphics pass ---
	m_finalPassSrvTable = resourceManager->m_cbvSrvUavAllocator.Allocate(1);
	D3D12_SHADER_RESOURCE_VIEW_DESC finalSrvDesc = {};
	finalSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	finalSrvDesc.Format = m_pOutputImage->GetResource()->GetDesc().Format; // DXGI_FORMAT_R8G8B8A8_UNORM
	finalSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	finalSrvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(m_pOutputImage->GetResource(), &finalSrvDesc, m_finalPassSrvTable.CpuHandle);

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
		m_pAccumulationImage->Resize(width, height);
		m_pOutputImage->Resize(width, height);
	}

	m_frameIndex = 0;
	m_cameraMoved = true;

	auto pDevice = m_pRenderEngine->GetDevice();
	UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE computeHeapHandle = m_computeDescriptorTable.CpuHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE accumulationUavHandle = computeHeapHandle;
	//U0 accumulate textuer
	accumulationUavHandle.ptr += 1 * descriptorSize;
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	pDevice->CreateUnorderedAccessView(m_pAccumulationImage->GetResource(), nullptr, &uavDesc, accumulationUavHandle);
	//U1 Output texture
	D3D12_CPU_DESCRIPTOR_HANDLE outputUavHandle = computeHeapHandle;
	outputUavHandle.ptr += 2 * descriptorSize;
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	pDevice->CreateUnorderedAccessView(m_pOutputImage->GetResource(), nullptr, &uavDesc, outputUavHandle);


	//assign outputtexture to output texture heap location heap
	D3D12_CPU_DESCRIPTOR_HANDLE graphicsSrvHandle = m_finalPassSrvTable.CpuHandle;
	D3D12_SHADER_RESOURCE_VIEW_DESC quadSrvDesc = {};
	quadSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	quadSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	quadSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	quadSrvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(m_pOutputImage->GetResource(), &quadSrvDesc, graphicsSrvHandle);

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
			const UINT bufferSize = sizeof(Triangle) * m_sceneData.Triangles.size();
			D3D12_RANGE readRange = { 0, 0 };
			m_triangleUpload->Map(0, &readRange, reinterpret_cast<void**>(&pData));
			memcpy(pData, m_sceneData.Triangles.data(), bufferSize);
			m_triangleUpload->Unmap(0, nullptr);
			TransitionResource(cmdList, m_triangleBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
			cmdList->CopyBufferRegion(m_triangleBuffer.Get(), 0, m_triangleUpload.Get(), 0, bufferSize);
			TransitionResource(cmdList, m_triangleBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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

	D3D12_GPU_DESCRIPTOR_HANDLE computeHeapHandle = m_computeDescriptorTable.GpuHandle;
	// CBV
	cmdList->SetComputeRootDescriptorTable(0, computeHeapHandle);
	computeHeapHandle.ptr += descriptorSize;	// 1 CBV
	// UAV
	cmdList->SetComputeRootDescriptorTable(1, computeHeapHandle);
	computeHeapHandle.ptr += 2 * descriptorSize;	 //2 UAV
	// SRV
	cmdList->SetComputeRootDescriptorTable(2, computeHeapHandle);
	computeHeapHandle.ptr += 3 * descriptorSize;	 //3SRV 

	cmdList->SetComputeRootDescriptorTable(3, computeHeapHandle);

	cmdList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

	TransitionResource(cmdList, m_pOutputImage->GetResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


	// Render Pass
	m_pRenderEngine->TransitionResource(cmdList, m_pRenderEngine->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetPipelineState(m_finalTexturePSO.PipelineState.Get());
	cmdList->SetGraphicsRootSignature(m_finalTexturePSO.rootSignature.Get());


	cmdList->SetGraphicsRootDescriptorTable(0, m_finalPassSrvTable.GpuHandle);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRenderEngine->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += m_pRenderEngine->m_frameIndex * m_pRenderEngine->m_rtvDescriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pRenderEngine->m_dsbHeap->GetCPUDescriptorHandleForHeapStart();

	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	RenderModel(cmdList, m_pQuadModel);

	TransitionResource(cmdList, m_pOutputImage->GetResource(),
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
