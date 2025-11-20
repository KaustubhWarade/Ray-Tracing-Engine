#include "DefaultScene.h"

#include "CoreHelper Files/Helper.h"
#include "RenderEngine Files/RenderEngine.h" 
#include "RenderEngine Files/ImGuiHelper.h"
#include <shobjidl.h> 
#include <stack>
#include <algorithm>

using namespace DirectX;


HRESULT DefaultScene::Initialize(RenderEngine* pRenderEngine)
{
	m_pRenderEngine = pRenderEngine;
	auto pDevice = m_pRenderEngine->GetDevice();
	HRESULT hr = S_OK;

	// 1. Compile Shaders
	EXECUTE_AND_LOG_RETURN(compileVSShader(L"DefaultScene/FinalTextureVShader.hlsl", m_finalShader));
	EXECUTE_AND_LOG_RETURN(compilePSShader(L"DefaultScene/FinalTexturePShader.hlsl", m_finalShader));

	// 2. Create Root Signature (1 CBV table, 1 static sampler (not used))

	// Descriptor range for a single Constant Buffer View (CBV) at register b0.
	D3D12_DESCRIPTOR_RANGE1 descriptorRangeCBV;
	ZeroMemory(&descriptorRangeCBV, sizeof(D3D12_DESCRIPTOR_RANGE1));
	descriptorRangeCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorRangeCBV.NumDescriptors = 1;
	descriptorRangeCBV.BaseShaderRegister = 0;
	descriptorRangeCBV.RegisterSpace = 0;
	descriptorRangeCBV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE1 descriptorRangeBindlessSRV = {};
	descriptorRangeBindlessSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeBindlessSRV.NumDescriptors = -1;
	descriptorRangeBindlessSRV.BaseShaderRegister = 0;
	descriptorRangeBindlessSRV.RegisterSpace = 1;
	descriptorRangeBindlessSRV.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
	descriptorRangeBindlessSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE1 materialSrvRange = {};
	materialSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	materialSrvRange.NumDescriptors = 1;
	materialSrvRange.BaseShaderRegister = 0;
	materialSrvRange.RegisterSpace = 0;
	materialSrvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	materialSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Root parameter using the CBV descriptor table.
	D3D12_ROOT_PARAMETER1 rootParameters[4];

	// Root Param [0]: Per-Frame CBV
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRangeCBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// Root Param [1]: Bindless Texture Array (a pointer to the start of the heap)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRangeBindlessSRV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// Root Param [2]: Material Buffer SRV
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &materialSrvRange;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// Root Param [3]: Per-Draw Material Index (Root Constant)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[3].Constants.ShaderRegister = 1; // b1
	rootParameters[3].Constants.RegisterSpace = 0;
	rootParameters[3].Constants.Num32BitValues = 1;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	CreateStaticSampler(staticSampler, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	staticSampler.MaxAnisotropy = 16;

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
	ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &staticSampler;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Serialize and create the root signature.
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

	// 3. Create Pipeline State Object (PSO)

	// Define the vertex input layout to match the Vertex_Pos_Tex struct.
	D3D12_INPUT_ELEMENT_DESC d3dInputElementDesc[4] = {};
	d3dInputElementDesc[0].SemanticName = "POSITION";
	d3dInputElementDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	d3dInputElementDesc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	d3dInputElementDesc[1].SemanticName = "NORMAL";
	d3dInputElementDesc[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	d3dInputElementDesc[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	d3dInputElementDesc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	d3dInputElementDesc[2].SemanticName = "TEXCOORD";
	d3dInputElementDesc[2].Format = DXGI_FORMAT_R32G32_FLOAT;
	d3dInputElementDesc[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	d3dInputElementDesc[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	d3dInputElementDesc[3].SemanticName = "TANGENT";
	d3dInputElementDesc[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	d3dInputElementDesc[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	d3dInputElementDesc[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;


	// Create standard rasterizer, depth-stencil, and blend states via helper functions.
	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	createRasterizerDesc(rasterizerDesc);

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	createDepthStencilDesc(depthStencilDesc);

	D3D12_BLEND_DESC blendDesc;
	createBlendDesc(blendDesc);

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

	m_constantBufferTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(1);
	m_materialSrvTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(1);


	// --- Initialize resources for the picking pass ---

	EXECUTE_AND_LOG_RETURN(compileVSShader(L"DefaultScene/PickingVShader.hlsl", m_pickingShader));
	EXECUTE_AND_LOG_RETURN(compilePSShader(L"DefaultScene/PickingPShader.hlsl", m_pickingShader));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC picker_psoDesc = {};
	createPipelineStateDesc(picker_psoDesc, d3dInputElementDesc, _countof(d3dInputElementDesc), m_finalTexturePSO.rootSignature.Get(), m_pickingShader, rasterizerDesc, blendDesc, depthStencilDesc);
	picker_psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	EXECUTE_AND_LOG_RETURN(pDevice->CreateGraphicsPipelineState(&picker_psoDesc, IID_PPV_ARGS(&m_pickerPSO.PipelineState)));


	//CBV
	{
		// Constant buffers must be 256-byte aligned.
		const UINT constantBufferSize = (sizeof(CBUFFER) + 255) & ~255;

		// Create the constant buffer resource in an UPLOAD heap for frequent CPU updates.
		D3D12_HEAP_PROPERTIES cbvHeapProp;
		CreateHeapProperties(cbvHeapProp, D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC constantBufferResourceDesc = {};
		constantBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		constantBufferResourceDesc.Alignment = 0;
		constantBufferResourceDesc.Width = constantBufferSize;
		constantBufferResourceDesc.Height = 1;
		constantBufferResourceDesc.DepthOrArraySize = 1;
		constantBufferResourceDesc.MipLevels = 1;
		constantBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		constantBufferResourceDesc.SampleDesc.Count = 1;
		constantBufferResourceDesc.SampleDesc.Quality = 0;
		constantBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		constantBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		EXECUTE_AND_LOG_RETURN(m_pRenderEngine->GetDevice()->CreateCommittedResource(
			&cbvHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&constantBufferResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBufferResource)));

		// Create the CBV in the descriptor heap.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_constantBufferResource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(CBUFFER) + 255) & ~255;
		m_pRenderEngine->GetDevice()->CreateConstantBufferView(&cbvDesc, m_constantBufferTable.GetCpuHandle(0));

		// Map the buffer for persistent CPU access; we won't unmap it.
		D3D12_RANGE readRange;
		readRange.Begin = 0;
		readRange.End = 0;
		EXECUTE_AND_LOG_RETURN(m_constantBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&m_CBVHeapStartPointer)));
		ZeroMemory(&m_constantBufferData, sizeof(CBUFFER));
		m_constantBufferData.lightPosition = XMFLOAT4(0.0f, 15.0f, -15.0f, 1.0f);
		m_constantBufferData.lD = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		m_constantBufferData.kD = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		m_constantBufferData.lightingMode = 0;
		memcpy(m_CBVHeapStartPointer, &m_constantBufferData, sizeof(m_constantBufferData));

	}
	m_previewConstantBufferTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(1);
	{
		const UINT constantBufferSize = (sizeof(CBUFFER) + 255) & ~255;

		D3D12_HEAP_PROPERTIES cbvHeapProp;
		CreateHeapProperties(cbvHeapProp, D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC constantBufferResourceDesc = {};
		constantBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		constantBufferResourceDesc.Width = constantBufferSize;
		constantBufferResourceDesc.Height = 1;
		constantBufferResourceDesc.DepthOrArraySize = 1;
		constantBufferResourceDesc.MipLevels = 1;
		constantBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		constantBufferResourceDesc.SampleDesc.Count = 1;
		constantBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		EXECUTE_AND_LOG_RETURN(m_pRenderEngine->GetDevice()->CreateCommittedResource(
			&cbvHeapProp, D3D12_HEAP_FLAG_NONE, &constantBufferResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_previewConstantBufferResource)));

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_previewConstantBufferResource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = constantBufferSize;
		m_pRenderEngine->GetDevice()->CreateConstantBufferView(&cbvDesc, m_previewConstantBufferTable.GetCpuHandle(0));

		D3D12_RANGE readRange = { 0, 0 };
		EXECUTE_AND_LOG_RETURN(m_previewConstantBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&m_previewCBVHeapStartPointer)));

		// Initialize with default values
		ZeroMemory(&m_previewConstantBufferData, sizeof(CBUFFER));
		m_previewConstantBufferData.lightPosition = XMFLOAT4(2.0f, 2.0f, -2.0f, 1.0f);
		m_previewConstantBufferData.lD = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		m_previewConstantBufferData.kD = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		m_previewConstantBufferData.lightingMode = 0;
		memcpy(m_previewCBVHeapStartPointer, &m_previewConstantBufferData, sizeof(m_previewConstantBufferData));
	}

	// texture preview
	// create sphere mesh
	CreateSphereMesh(1.0f, 32, 32);
	m_pSphereModel = ResourceManager::Get()->GetModel("material_preview_sphere");
	CreateQuadMesh(1.5f);
	m_pQuadModel = ResourceManager::Get()->GetModel("material_preview_quad");
	// create image for render target
	m_materialPreviewImage = std::make_unique<Image>();
	UINT previewSize = 256;
	EXECUTE_AND_LOG_RETURN(m_materialPreviewImage->Initialize(pDevice, m_pRenderEngine->m_commandList.Get(), &ResourceManager::Get()->m_generalPurposeHeapAllocator, previewSize, previewSize, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));
	m_pRenderEngine->TrackResource(m_materialPreviewImage->GetResource(), D3D12_RESOURCE_STATE_COMMON);

	// Create the depth buffer for the preview
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	EXECUTE_AND_LOG_RETURN(pDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_previewDsvHeap)));

	D3D12_RESOURCE_DESC depthStencilBufferDesc = {};
	depthStencilBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilBufferDesc.Width = previewSize;
	depthStencilBufferDesc.Height = previewSize;
	depthStencilBufferDesc.DepthOrArraySize = 1;
	depthStencilBufferDesc.MipLevels = 1;
	depthStencilBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilBufferDesc.SampleDesc.Count = 1;
	depthStencilBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthClearValue.DepthStencil.Depth = 1.0f;

	D3D12_HEAP_PROPERTIES heapProps;
	CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_DEFAULT);
	EXECUTE_AND_LOG_RETURN(pDevice->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &depthStencilBufferDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue, IID_PPV_ARGS(&m_previewDepthBuffer)));

	pDevice->CreateDepthStencilView(m_previewDepthBuffer.Get(), nullptr, m_previewDsvHeap->GetCPUDescriptorHandleForHeapStart());

	// Create viewport and scissor rect for the preview
	m_previewViewport = { 0.0f, 0.0f, (float)previewSize, (float)previewSize, 0.0f, 1.0f };
	m_previewScissorRect = { 0, 0, (LONG)previewSize, (LONG)previewSize };

	// ***** VizualizeTexture ******
	EXECUTE_AND_LOG_RETURN(compileVSShader(L"DefaultScene/VisualizeTextureVShader.hlsl", m_visualizeShader));
	EXECUTE_AND_LOG_RETURN(compilePSShader(L"DefaultScene/VisualizeTexturePShader.hlsl", m_visualizeShader));

	D3D12_DESCRIPTOR_RANGE1 srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0; 
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER1 visRootParam = {};
	visRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	visRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	visRootParam.DescriptorTable.NumDescriptorRanges = 1;
	visRootParam.DescriptorTable.pDescriptorRanges = &srvRange;

	D3D12_STATIC_SAMPLER_DESC pointSampler = {};
	CreateStaticSampler(pointSampler, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_ROOT_SIGNATURE_DESC1 visRootSigDesc = {};
	visRootSigDesc.NumParameters = 1;
	visRootSigDesc.pParameters = &visRootParam;
	visRootSigDesc.NumStaticSamplers = 1;
	visRootSigDesc.pStaticSamplers = &pointSampler;
	visRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> pVisSigBlob, pVisErrorBlob;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC visVersionedRootSig = {};
	visVersionedRootSig.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	visVersionedRootSig.Desc_1_1 = visRootSigDesc;
	EXECUTE_AND_LOG_RETURN(D3D12SerializeVersionedRootSignature(&visVersionedRootSig, &pVisSigBlob, &pVisErrorBlob));
	EXECUTE_AND_LOG_RETURN(pDevice->CreateRootSignature(0, pVisSigBlob->GetBufferPointer(), pVisSigBlob->GetBufferSize(), IID_PPV_ARGS(&m_visualizePSO.rootSignature)));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC visPsoDesc = {};
	visPsoDesc.pRootSignature = m_visualizePSO.rootSignature.Get();
	visPsoDesc.VS = m_visualizeShader.vertexShaderByteCode;
	visPsoDesc.PS = m_visualizeShader.pixelShaderByteCode;
	visPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	visPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	visPsoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	visPsoDesc.DepthStencilState.DepthEnable = FALSE; // We don't need depth testing
	visPsoDesc.SampleMask = UINT_MAX;
	visPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	visPsoDesc.NumRenderTargets = 1;
	visPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // It will draw to the main back buffer
	visPsoDesc.SampleDesc.Count = 1;

	EXECUTE_AND_LOG_RETURN(pDevice->CreateGraphicsPipelineState(&visPsoDesc, IID_PPV_ARGS(&m_visualizePSO.PipelineState)));


	return hr;
}

void DefaultScene::OnResize(UINT width, UINT height)
{
	if (m_pRenderEngine)
	{
		m_camera.OnResize(width, height);
	}

	// --- Create/recreate picking resources whenever the window size changes ---
	if (!m_pRenderEngine) return;
	auto pDevice = m_pRenderEngine->GetDevice();

	// Create Picking Texture for storing IDs
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; 
	texDesc.SampleDesc.Count = 1;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 0.0f;

	D3D12_HEAP_PROPERTIES defaultHeap = {};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

	if (m_pickingTexture)
	{
		m_pRenderEngine->UnTrackResource(m_pickingTexture.Get());
	}

	EXECUTE_AND_LOG(pDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&m_pickingTexture)));
	m_pickingTexture->SetName(L"Picking ID Texture");

	m_pRenderEngine->TrackResource(m_pickingTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	// Create Render Target View for the Picking Texture
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 1;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	EXECUTE_AND_LOG(pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_pickingRtvHeap)));
	m_pickingRtvHeap->SetName(L"Picking RTV Heap");
	pDevice->CreateRenderTargetView(m_pickingTexture.Get(), nullptr, m_pickingRtvHeap->GetCPUDescriptorHandleForHeapStart());

	// Create Readback Buffer to get the ID back to the CPU
	D3D12_HEAP_PROPERTIES readbackHeap = {};
	readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = 256; // We only need to read one pixel's data
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	EXECUTE_AND_LOG(pDevice->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_pickingReadbackBuffer)));
	m_pickingReadbackBuffer->SetName(L"Picking Readback Buffer");


	// visualize texture
	if (m_pickingSrvHandle.IsValid())
	{
		ResourceManager::Get()->m_generalPurposeHeapAllocator.FreeSingle(m_pickingSrvHandle);
	}
	m_pickingSrvHandle = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateSingle();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	pDevice->CreateShaderResourceView(m_pickingTexture.Get(), &srvDesc, m_pickingSrvHandle.CpuHandle);

}

bool DefaultScene::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_camera.HandleWindowsMessage(message, wParam, lParam))
	{
		OnViewChanged(); 
		return true;
	}

	switch (message)
	{
	case WM_LBUTTONDOWN:
	{
		if (!ImGui::GetIO().WantCaptureMouse)
		{
			m_mouseDownPos.x = LOWORD(lParam);
			m_mouseDownPos.y = HIWORD(lParam);
			return true;
		}
		break;
	}
	case WM_LBUTTONUP:
	{
		if (!ImGui::GetIO().WantCaptureMouse)
		{
			int mouseUpX = LOWORD(lParam);
			int mouseUpY = HIWORD(lParam);

			int deltaX = abs(mouseUpX - m_mouseDownPos.x);
			int deltaY = abs(mouseUpY - m_mouseDownPos.y);

			const int clickThreshold = 5; 

			if (deltaX < clickThreshold && deltaY < clickThreshold)
			{
				m_performPicking = true;
				m_mouseClickPos.x = mouseUpX;
				m_mouseClickPos.y = mouseUpY;
			}
			return true;
		}
		break;
	}
	}
	return false;
}

void DefaultScene::OnViewChanged()
{
	/* Stub for camera-change events */
}

void DefaultScene::PopulateCommandList(void)
{
	ID3D12GraphicsCommandList* cmdList = m_pRenderEngine->m_commandList.Get();

	// --- Pass 1: Always render the material preview (independent off-screen pass) ---
	RenderMaterialPreview();

	// --- Pass 2: Always render the picking ID buffer (off-screen pass) ---
	{
		// Set render target to the picking texture
		D3D12_CPU_DESCRIPTOR_HANDLE pickingRtvHandle = m_pickingRtvHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pRenderEngine->m_dsbHeap->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &pickingRtvHandle, FALSE, &dsvHandle);

		// Clear the picking texture to black (ID 0) and clear the depth buffer for this pass
		const float clearID[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		cmdList->ClearRenderTargetView(pickingRtvHandle, clearID, 0, nullptr);
		cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// Set common state (viewport and scissor must match the picking texture size)
		cmdList->RSSetViewports(1, &m_pRenderEngine->m_viewport);
		cmdList->RSSetScissorRects(1, &m_pRenderEngine->m_scissorRect);

		ID3D12DescriptorHeap* ppHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
		cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		// Set the picking pipeline state
		cmdList->SetPipelineState(m_pickerPSO.PipelineState.Get());
		cmdList->SetGraphicsRootSignature(m_finalTexturePSO.rootSignature.Get()); // The picker PSO uses a compatible root sig
		cmdList->SetGraphicsRootDescriptorTable(0, m_constantBufferTable.GetGpuHandle(0));

		// Update constant buffer for the picking pass
		if (m_pModel)
		{
			XMMATRIX scaleMatrix = XMMatrixScaling(m_modelScale.x, m_modelScale.y, m_modelScale.z);
			XMMATRIX rotationMatrix = XMMatrixRotationX(XMConvertToRadians(m_rotationAngle.x)) * XMMatrixRotationY(XMConvertToRadians(m_rotationAngle.y)) * XMMatrixRotationZ(XMConvertToRadians(m_rotationAngle.z));
			XMMATRIX translationMatrix = XMMatrixTranslation(m_modelPosition.x, m_modelPosition.y, m_modelPosition.z);
			m_constantBufferData.worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_constantBufferData.viewMatrix = m_camera.GetView();
			m_constantBufferData.projectionMatrix = m_camera.GetProjection();
			memcpy(m_CBVHeapStartPointer, &m_constantBufferData, sizeof(m_constantBufferData));

			RenderModel(cmdList, m_pModel, 3);
		}
	}

	// --- Pass 3: Render to the screen (the final back buffer) ---

	// Set render target to the main back buffer
	m_pRenderEngine->TransitionResource(cmdList, m_pRenderEngine->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRenderEngine->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += m_pRenderEngine->m_frameIndex * m_pRenderEngine->m_rtvDescriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pRenderEngine->m_dsbHeap->GetCPUDescriptorHandleForHeapStart();
	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	if (m_bVisualizePicking)
	{
		// --- Option A: Visualize the Picking Buffer ---
		const float clearColor[] = { 1.0f, 0.0f, 1.0f, 1.0f }; // Clear screen to magenta for debugging
		cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		m_pRenderEngine->TransitionResource(cmdList, m_pickingTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		cmdList->SetPipelineState(m_visualizePSO.PipelineState.Get());
		cmdList->SetGraphicsRootSignature(m_visualizePSO.rootSignature.Get());

		ID3D12DescriptorHeap* ppHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
		cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		cmdList->SetGraphicsRootDescriptorTable(0, m_pickingSrvHandle.GpuHandle);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(3, 1, 0, 0); // Draw the full-screen triangle

		m_pRenderEngine->TransitionResource(cmdList, m_pickingTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	else
	{
		// --- Option B: Render the normal 3D scene ---
		const float clearColor[] = { 0.48f, 0.71f, 0.91f, 1.0f };
		cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		cmdList->SetPipelineState(m_finalTexturePSO.PipelineState.Get());
		cmdList->SetGraphicsRootSignature(m_finalTexturePSO.rootSignature.Get());

		ID3D12DescriptorHeap* ppHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
		cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		cmdList->SetGraphicsRootDescriptorTable(0, m_constantBufferTable.GetGpuHandle(0));
		cmdList->SetGraphicsRootDescriptorTable(1, ResourceManager::Get()->m_bindlessTextureAllocator.GetHeap()->GetGPUDescriptorHandleForHeapStart());
		if (m_pModel && m_materialBuffer.Resource)
		{
			cmdList->SetGraphicsRootDescriptorTable(2, m_materialSrvTable.GetGpuHandle(0));
		}

		RenderModel(cmdList, m_pModel, 3);
	}

	// --- Final Step: Schedule the GPU readback if a click happened this frame ---
	if (m_performPicking)
	{
		m_pRenderEngine->TransitionResource(cmdList, m_pickingTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);

		D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
		srcLoc.pResource = m_pickingTexture.Get();
		srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
		dstLoc.pResource = m_pickingReadbackBuffer.Get();
		dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dstLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstLoc.PlacedFootprint.Footprint.Width = 1;
		dstLoc.PlacedFootprint.Footprint.Height = 1;
		dstLoc.PlacedFootprint.Footprint.Depth = 1;
		dstLoc.PlacedFootprint.Footprint.RowPitch = 256;

		D3D12_BOX srcBox = { (UINT)m_mouseClickPos.x, (UINT)m_mouseClickPos.y, 0, (UINT)m_mouseClickPos.x + 1, (UINT)m_mouseClickPos.y + 1, 1 };
		cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

		m_pRenderEngine->TransitionResource(cmdList, m_pickingTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

}

void DefaultScene::RenderMaterialPreview()
{
	ID3D12GraphicsCommandList* cmdList = m_pRenderEngine->m_commandList.Get();

	// Don't render if nothing is selected
	if (m_selectedMaterialIndex < 0 || !m_pSphereModel)
	{
		// Just clear the texture so it's not garbage
		m_pRenderEngine->TransitionResource(cmdList, m_materialPreviewImage->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		const float clearColor[] = { 1.0f, 0.0f, 0.0f, 1.0f };
		cmdList->ClearRenderTargetView(m_materialPreviewImage->GetRtvHandle().CpuHandle, clearColor, 0, nullptr);
		m_pRenderEngine->TransitionResource(cmdList, m_materialPreviewImage->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		return;
	}

	// 1. Transition resources
	m_pRenderEngine->TransitionResource(cmdList, m_materialPreviewImage->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	// 2. Set render target, clear it
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_materialPreviewImage->GetRtvHandle().CpuHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_previewDsvHeap->GetCPUDescriptorHandleForHeapStart();
	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 3. Set viewport, PSO, root signature
	cmdList->RSSetViewports(1, &m_previewViewport);
	cmdList->RSSetScissorRects(1, &m_previewScissorRect);
	cmdList->SetPipelineState(m_finalTexturePSO.PipelineState.Get());
	cmdList->SetGraphicsRootSignature(m_finalTexturePSO.rootSignature.Get());

	// Set descriptor heaps (same as main pass)
	ID3D12DescriptorHeap* ppHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// 4. Update constant buffer for the sphere
	m_previewConstantBufferData.worldMatrix = XMMatrixRotationY(XMConvertToRadians(0.0f)); // Fixed rotation

	// Simple fixed camera for the preview
	XMVECTOR eye = XMVectorSet(0.0f, 0.0f, -2.5f, 0.0f);
	XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_previewConstantBufferData.viewMatrix = XMMatrixLookAtLH(eye, at, up);
	m_previewConstantBufferData.projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1.0f, 0.1f, 10.0f);
	m_previewConstantBufferData.lightPosition = XMFLOAT4(-2.0f, 2.0f, -5.0f, 1.0f);
	XMStoreFloat3(&m_previewConstantBufferData.cameraPosition, eye);

	memcpy(m_previewCBVHeapStartPointer, &m_previewConstantBufferData, sizeof(m_previewConstantBufferData));

	// 5. Set root arguments
	cmdList->SetGraphicsRootDescriptorTable(0, m_previewConstantBufferTable.GetGpuHandle(0));
	cmdList->SetGraphicsRootDescriptorTable(1, ResourceManager::Get()->m_bindlessTextureAllocator.GetHeap()->GetGPUDescriptorHandleForHeapStart());
	if (m_pModel && m_materialBuffer.Resource)
	{
		cmdList->SetGraphicsRootDescriptorTable(2, m_materialSrvTable.GetGpuHandle(0));
	}
	// IMPORTANT: Set the root constant to the selected material index
	cmdList->SetGraphicsRoot32BitConstant(3, m_selectedMaterialIndex, 0);

	// 6. render draw object
	Model* pModelToRender = nullptr;
	switch (m_previewMeshType)
	{
	case PreviewMeshType::Sphere: pModelToRender = m_pSphereModel; break;
	case PreviewMeshType::Quad:   pModelToRender = m_pQuadModel;   break;
	}
	RenderModel(cmdList, pModelToRender);

	// 7. Transition texture back to be readable by ImGui
	m_pRenderEngine->TransitionResource(cmdList, m_materialPreviewImage->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DefaultScene::Update(void)
{
	if (m_performPicking)
	{
		void* pData;
		D3D12_RANGE readRange = { 0, sizeof(uint32_t) };
		if (SUCCEEDED(m_pickingReadbackBuffer->Map(0, &readRange, &pData)))
		{
			uint32_t packedColor = *static_cast<uint32_t*>(pData);

			uint8_t r = (packedColor >> 0) & 0xFF;
			uint8_t g = (packedColor >> 8) & 0xFF;
			uint8_t b = (packedColor >> 16) & 0xFF;

			m_pickingReadbackBuffer->Unmap(0, nullptr);

			uint32_t hundreds = (uint32_t)round(r / 25.0f);
			uint32_t tens = (uint32_t)round(g / 25.0f);
			uint32_t ones = (uint32_t)round(b / 25.0f);

			uint32_t pickedId = (hundreds * 100) + (tens * 10) + ones;

			if (pickedId > 0)
			{
				m_selectedMaterialIndex = pickedId - 1;
			}
			else
			{
				m_selectedMaterialIndex = -1;
			}
		}

		m_performPicking = false;
	}
}

void DefaultScene::RenderImGui()
{
	ImGui::Begin("Welcome to My Renddering Engine", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Render Time : %.3fms", m_LastRenderTime);
	XMFLOAT3 pos = m_camera.GetPosition3f();
	ImGui::Text("Camera Position : (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
	ImGui::Text("Camera Forward Direction : (%.2f, %.2f, %.2f)", m_camera.GetForwardDirection3f().x, m_camera.GetForwardDirection3f().y, m_camera.GetForwardDirection3f().z);
	ImGui::Checkbox("Visualize Picking Buffer", &m_bVisualizePicking);
	ImGui::End();

	ImGui::Begin("Scene One", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::Button("Load Model"))
	{
		std::wstring filePath = OpenFileDialog();
		if (!filePath.empty())
		{
			std::filesystem::path path(filePath);
			std::string modelKey = path.stem().string() + "_model";
			auto startTime = std::chrono::high_resolution_clock::now();
			ResourceManager::Get()->LoadModel(modelKey, WStringToString(filePath));
			auto endTime = std::chrono::high_resolution_clock::now();
			auto loadDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
			m_modelLoadTimeMs = static_cast<float>(loadDuration) / 1000.0f;
			m_pModel = ResourceManager::Get()->GetModel(modelKey);
			if (m_pModel)
			{
				m_currentModelName = modelKey;
				m_selectedMaterialIndex = 0;
				m_modelPosition = { 0.0f, 0.0f, 0.0f };
				m_modelScale = { 1.0f, 1.0f, 1.0f };
				m_rotationAngle = { -90.0f, 0.0f, 0.0f };

				m_materialBuffer.Stride = sizeof(Material);
				m_materialBuffer.Sync(m_pRenderEngine, m_pRenderEngine->m_commandList.Get(),
					m_pModel->Materials.data(), m_pModel->Materials.size());
				if (m_materialBuffer.Resource)
					m_materialBuffer.Resource->SetName(L"Model Material Buffer");
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = m_materialBuffer.Size;
				srvDesc.Buffer.StructureByteStride = m_materialBuffer.Stride;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				m_pRenderEngine->GetDevice()->CreateShaderResourceView(
					m_materialBuffer.Resource.Get(),
					&srvDesc,
					m_materialSrvTable.GetCpuHandle(0)
				);

				auto accelManager = m_pRenderEngine->GetAccelManager();
				startTime = std::chrono::high_resolution_clock::now();
				const BuiltBLAS* blas = accelManager->GetOrBuildBLAS(m_pRenderEngine->m_commandList.Get(), m_pModel);
				endTime = std::chrono::high_resolution_clock::now();
				auto buildDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
				m_blasBuildTimeMs = static_cast<float>(buildDuration) / 1000.0f;
				if (blas)
				{
					m_blasStats = accelManager->AnalyzeBLAS(blas);
				}
			}
		}
	}
	ImGui::Text("Model Controls");
	ImGui::DragFloat3("Position", &m_modelPosition.x, 0.1f);
	ImGui::DragFloat3("Rotation", &m_rotationAngle.x, 0.1f);
	static bool uniformScale = false;
	ImGui::Checkbox("Uniform Scale", &uniformScale);

	if (uniformScale)
	{
		float scale = m_modelScale.x;
		if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.001f, 20.0f))
		{
			m_modelScale = { scale, scale, scale };
		}
	}
	else
	{
		ImGui::DragFloat3("Scale", &m_modelScale.x, 0.01f);
	}
	ImGui::Separator();

	ImGui::Text("Debug View:");

	static const char* debugViewNames[] = {
		"PBR",
		"Base Color",
		"Normals",
		"Metallic",
		"Roughness",
		"Emissive",
		"Ambient Occlusion"
	};

	int currentView = static_cast<int>(m_debugView);

	if (ImGui::Combo("##DebugView", &currentView, debugViewNames, IM_ARRAYSIZE(debugViewNames)))
	{
		m_debugView = static_cast<MaterialDebugView>(currentView);
		m_constantBufferData.lightingMode = static_cast<int>(m_debugView);
		m_previewConstantBufferData.lightingMode = m_constantBufferData.lightingMode;
	}

	ImGui::Separator();

	ImGui::DragFloat3("Light Position", &m_constantBufferData.lightPosition.x, 0.1f);
	ImGui::ColorEdit3("Light Color", &m_constantBufferData.lD.x);
	ImGui::ColorEdit3("Material Color", &m_constantBufferData.kD.x);
	ImGui::End();

	ImGui::Begin("Material Editor");
	if (m_pModel)
	{
		// Combo box to select the material to edit
		const char* current_material_name = (m_selectedMaterialIndex >= 0) ? m_pModel->MaterialNames[m_selectedMaterialIndex].c_str() : "None";
		if (ImGui::BeginCombo("Select Material", current_material_name))
		{
			for (int i = 0; i < m_pModel->MaterialNames.size(); ++i)
			{
				const bool isSelected = (m_selectedMaterialIndex == i);
				if (ImGui::Selectable(m_pModel->MaterialNames[i].c_str(), isSelected))
				{
					m_selectedMaterialIndex = i;
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();

		// If a material is selected, show its properties
		if (m_selectedMaterialIndex >= 0 && m_selectedMaterialIndex < m_pModel->Materials.size())
		{
			Material& mat = m_pModel->Materials[m_selectedMaterialIndex];

			ImGui::Text("Preview:");
			float regionWidth = ImGui::GetContentRegionAvail().x;
			ImTextureID previewTextureID = (ImTextureID)m_materialPreviewImage->GetSrvHandle().GpuHandle.ptr;
			ImGui::Image(previewTextureID, ImVec2(regionWidth, regionWidth), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0.5, 0.5, 0.5, 1));
			ImGui::Separator();
			ImGui::Text("Preview Mesh:");
			ImGui::RadioButton("Sphere", (int*)&m_previewMeshType, (int)PreviewMeshType::Sphere); ImGui::SameLine();
			ImGui::RadioButton("Quad", (int*)&m_previewMeshType, (int)PreviewMeshType::Quad); ImGui::SameLine();
			ImGui::Separator();

			ImGui::Text("Editing: %s", m_pModel->MaterialNames[m_selectedMaterialIndex].c_str());

			if (ImGui::ColorEdit4("Base Color Factor", &mat.BaseColorFactor.x)) m_materialsDirty = true;
			if (ImGui::ColorEdit3("Emissive Factor", &mat.EmissiveFactor.x)) m_materialsDirty = true;
			if (ImGui::SliderFloat("Metallic Factor", &mat.MetallicFactor, 0.0f, 1.0f)) m_materialsDirty = true;
			if (ImGui::SliderFloat("Roughness Factor", &mat.RoughnessFactor, 0.0f, 1.0f)) m_materialsDirty = true;
			if (ImGui::SliderFloat("Refraction Index", &mat.IOR, 1.0f, 2.5f)) m_materialsDirty = true;

			// Casting to a bool pointer is safe here for ImGui's Checkbox
			if (ImGui::Checkbox("Double Sided", (bool*)&mat.DoubleSided)) m_materialsDirty = true;
			if (ImGui::Checkbox("Unlit", (bool*)&mat.Unlit)) m_materialsDirty = true;


			// Display texture indices (read-only for simplicity)
			ImGui::Separator();
			ImGui::Text("Texture Indices:");
			ImGui::Text("Base Color: %d", mat.BaseColorTextureIndex);
			ImGui::Text("Metallic/Roughness: %d", mat.MetallicRoughnessTextureIndex);
			ImGui::Text("Normal: %d", mat.NormalTextureIndex);
			ImGui::Text("Occlusion: %d", mat.OcclusionTextureIndex);
			ImGui::Text("Emissive: %d", mat.EmissiveTextureIndex);
		}
		else
		{
			ImGui::Text("No material selected.");
		}
	}
	else
	{
		ImGui::Text("Load a model to edit its materials.");
	}
	ImGui::End();


	ImGui::Begin("BLAS Inspector");
	if (m_pModel)
	{
		ImGui::Text("Statistics for: %s", m_currentModelName.c_str());
		ImGui::Separator();
		ImGui::Text("Total Nodes: %u", m_blasStats.nodeCount);
		ImGui::Text("Internal Nodes: %u", m_blasStats.internalNodeCount);
		ImGui::Text("Leaf Nodes: %u", m_blasStats.leafNodeCount);
		ImGui::Text("Max BVH Depth: %u", m_blasStats.maxDepth);
		ImGui::Separator();
		ImGui::Text("Triangles per Leaf Node:");
		ImGui::Text("Min: %u", m_blasStats.minTrianglesPerLeaf);
		ImGui::Text("Max: %u", m_blasStats.maxTrianglesPerLeaf);
		ImGui::Text("Average: %.2f", m_blasStats.averageTrianglesPerLeaf);
		ImGui::Separator();
		ImGui::Text("Build Performance:");
		ImGui::Text("Model Load Time: %.3f ms", m_modelLoadTimeMs);
		ImGui::Text("BLAS Build Time: %.3f ms", m_blasBuildTimeMs);
	}
	else
	{
		ImGui::Text("No model loaded.");
	}
	ImGui::End();
}

void DefaultScene::OnDestroy()
{
	// ComPtrs handle automatic cleanup of D3D resources.
}
