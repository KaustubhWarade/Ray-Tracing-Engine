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

	m_constantBufferTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(1);
	m_materialSrvTable = ResourceManager::Get()->m_generalPurposeHeapAllocator.AllocateTable(1);

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

	// hardcode model loading
	//ResourceManager::Get()->LoadModel("my_model", "assets/model.glb");
	//EXECUTE_AND_LOG_RETURN(ResourceManager::Get()->LoadModel("Knight_model", "DefaultScene/Fox.glb"));
	//m_pModel = ResourceManager::Get()->GetModel("Knight_model");
	//if (!m_pModel)
	//{
	//	fprintf(gpFile, "Failed to get model 'Knight_model' from ResourceManager.\n");
	//	return E_FAIL; // Failed to load the model, abort initialization.
	//}

	// texture preview
	// 1. create sphere mesh
	CreateSphereMesh(1.0f, 32, 32);
	m_pSphereModel = ResourceManager::Get()->GetModel("material_preview_sphere");
	CreateQuadMesh(1.5f);
	m_pQuadModel = ResourceManager::Get()->GetModel("material_preview_quad");
	//create image for render target
	m_materialPreviewImage = std::make_unique<Image>();
	UINT previewSize = 256;
	EXECUTE_AND_LOG_RETURN(m_materialPreviewImage->Initialize(pDevice, m_pRenderEngine->m_commandList.Get(), &ResourceManager::Get()->m_generalPurposeHeapAllocator, previewSize, previewSize, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));
	m_pRenderEngine->TrackResource(m_materialPreviewImage->GetResource(), D3D12_RESOURCE_STATE_COMMON);

	// 3. Create the depth buffer for the preview
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

	// 4. Create viewport and scissor rect for the preview
	m_previewViewport = { 0.0f, 0.0f, (float)previewSize, (float)previewSize, 0.0f, 1.0f };
	m_previewScissorRect = { 0, 0, (LONG)previewSize, (LONG)previewSize };


	return hr;
}

void DefaultScene::OnResize(UINT width, UINT height)
{
	if (m_pRenderEngine)
	{
		m_pRenderEngine->m_camera.OnResize(width, height);
	}

}

bool DefaultScene::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	// Scene-specific message handling (e.g., input) would go here.
	//following Win32 style
	return false;
}

void DefaultScene::OnViewChanged()
{
	/* Stub for camera-change events */
}

void DefaultScene::PopulateCommandList(void)
{
	ID3D12GraphicsCommandList* cmdList = m_pRenderEngine->m_commandList.Get();

	RenderMaterialPreview();

	if (m_materialsDirty == true)
	{
		// Re-uploads the entire vector of materials to the GPU buffer.
		m_materialBuffer.Sync(m_pRenderEngine, cmdList, m_pModel->Materials.data(), m_pModel->Materials.size());
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = m_materialBuffer.Size;
		srvDesc.Buffer.StructureByteStride = m_materialBuffer.Stride;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		// If the buffer had to be re-sized, its SRV needs to be re-created.
		if (m_materialBuffer.GpuResourceDirty)
		{
			m_pRenderEngine->GetDevice()->CreateShaderResourceView(
				m_materialBuffer.Resource.Get(),
				&srvDesc,
				m_materialSrvTable.GetCpuHandle(0)
			);
		}
		m_materialsDirty = false; // Reset the flag
		OnViewChanged(); // Reset accumulation
	}

	//1. Transition the back buffer to a render target state.
	m_pRenderEngine->TransitionResource(cmdList, m_pRenderEngine->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	//2. Get current RTV and DSV handles.
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRenderEngine->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += m_pRenderEngine->m_frameIndex * m_pRenderEngine->m_rtvDescriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pRenderEngine->m_dsbHeap->GetCPUDescriptorHandleForHeapStart();

	// Bind the render target.
	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Clear render target and depth stencil.
	const float clearColor[] = { 0.48f, 0.71f, 0.91f, 1.0f };
	cmdList->RSSetViewports(1, &m_pRenderEngine->m_viewport);
	cmdList->RSSetScissorRects(1, &m_pRenderEngine->m_scissorRect);
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// Set the main PSO and root signature for the scene.
	cmdList->SetPipelineState(m_finalTexturePSO.PipelineState.Get());
	cmdList->SetGraphicsRootSignature(m_finalTexturePSO.rootSignature.Get());

	// Set the CBV descriptor heap.
	ID3D12DescriptorHeap* ppHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	cmdList->SetGraphicsRootDescriptorTable(0, m_constantBufferTable.GetGpuHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, ResourceManager::Get()->m_bindlessTextureAllocator.GetHeap()->GetGPUDescriptorHandleForHeapStart());
	if (m_pModel && m_materialBuffer.Resource)
	{
		cmdList->SetGraphicsRootDescriptorTable(2, m_materialSrvTable.GetGpuHandle());
	}

	m_constantBufferData.worldMatrix = XMMatrixIdentity();
	XMMATRIX scaleMatrix = XMMatrixScaling(m_modelScale.x, m_modelScale.y, m_modelScale.z);
	XMMATRIX rotationMatrixX = XMMatrixRotationX(XMConvertToRadians(m_rotationAngle.x));
	XMMATRIX rotationMatrixY = XMMatrixRotationY(XMConvertToRadians(m_rotationAngle.y));
	XMMATRIX rotationMatrixZ = XMMatrixRotationZ(XMConvertToRadians(m_rotationAngle.z));
	XMMATRIX rotationMatrix = rotationMatrixZ * rotationMatrixX * rotationMatrixY;
	XMMATRIX translationMatrix = XMMatrixTranslation(m_modelPosition.x, m_modelPosition.y, m_modelPosition.z);
	m_constantBufferData.worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	m_constantBufferData.viewMatrix = m_pRenderEngine->m_camera.GetView();
	m_constantBufferData.projectionMatrix = m_pRenderEngine->m_camera.GetProjection();
	m_constantBufferData.cameraPosition = m_pRenderEngine->m_camera.GetPosition3f();

	memcpy(m_CBVHeapStartPointer, &m_constantBufferData, sizeof(m_constantBufferData));

	// Set vertex buffer and draw the model.
	RenderModel(cmdList, m_pModel, 3);
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
	m_previewConstantBufferData.lightPosition = XMFLOAT4(-2.0f, 2.0f, -2.0f, 1.0f);
	XMStoreFloat3(&m_previewConstantBufferData.cameraPosition, eye);

	memcpy(m_previewCBVHeapStartPointer, &m_previewConstantBufferData, sizeof(m_previewConstantBufferData));

	// 5. Set root arguments
	cmdList->SetGraphicsRootDescriptorTable(0, m_previewConstantBufferTable.GetGpuHandle());
	cmdList->SetGraphicsRootDescriptorTable(1, ResourceManager::Get()->m_bindlessTextureAllocator.GetHeap()->GetGPUDescriptorHandleForHeapStart());
	if (m_pModel && m_materialBuffer.Resource)
	{
		cmdList->SetGraphicsRootDescriptorTable(2, m_materialSrvTable.GetGpuHandle());
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

}

void DefaultScene::RenderImGui()
{
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
