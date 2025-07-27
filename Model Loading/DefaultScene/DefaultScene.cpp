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

	// Root parameter using the CBV descriptor table.
	D3D12_ROOT_PARAMETER1 rootParameter;
	ZeroMemory(&rootParameter, sizeof(D3D12_ROOT_PARAMETER1));
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter.DescriptorTable.NumDescriptorRanges = 1;
	rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRangeCBV;
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//just showcasing.
	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	CreateStaticSampler(staticSampler, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc;
	ZeroMemory(&rootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
	rootSignatureDesc.NumParameters = 1;
	rootSignatureDesc.pParameters = &rootParameter;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
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
	D3D12_INPUT_ELEMENT_DESC d3dInputElementDesc[3] = {};
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

	m_DescriptorHeapStart = ResourceManager::Get()->m_cbvSrvUavAllocator.Allocate(1);

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
		m_pRenderEngine->GetDevice()->CreateConstantBufferView(&cbvDesc, m_DescriptorHeapStart.CpuHandle);

		// Map the buffer for persistent CPU access; we won't unmap it.
		D3D12_RANGE readRange;
		readRange.Begin = 0;
		readRange.End = 0;
		EXECUTE_AND_LOG_RETURN(m_constantBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&m_CBVHeapStartPointer)));
		ZeroMemory(&m_constantBufferData, sizeof(CBUFFER));
		m_constantBufferData.lightPosition = XMFLOAT4(0.0f, 15.0f, -15.0f, 1.0f);
		m_constantBufferData.lD = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		m_constantBufferData.kD = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		m_constantBufferData.keyPressed = 1;
		memcpy(m_CBVHeapStartPointer, &m_constantBufferData, sizeof(m_constantBufferData));
	}
	
	//ResourceManager::Get()->LoadModel("my_model", "assets/model.glb");
	EXECUTE_AND_LOG_RETURN(ResourceManager::Get()->LoadModel("Knight_model", "DefaultScene/knightglb.glb"));
	m_pModel = ResourceManager::Get()->GetModel("Knight_model");
	if (!m_pModel)
	{
		fprintf(gpFile, "Failed to get model 'Knight_model' from ResourceManager.\n");
		return E_FAIL; // Failed to load the model, abort initialization.
	}

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

	//1. Transition the back buffer to a render target state.
	m_pRenderEngine->TransitionResource(cmdList, m_pRenderEngine->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	//2. Get current RTV and DSV handles.
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRenderEngine->m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += m_pRenderEngine->m_frameIndex * m_pRenderEngine->m_rtvDescriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pRenderEngine->m_dsbHeap->GetCPUDescriptorHandleForHeapStart();

	// Bind the render target.
	cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Clear render target and depth stencil.
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// Set the main PSO and root signature for the scene.
	cmdList->SetPipelineState(m_finalTexturePSO.PipelineState.Get());
	cmdList->SetGraphicsRootSignature(m_finalTexturePSO.rootSignature.Get());

	// Set the CBV descriptor heap.
	ID3D12DescriptorHeap* ppHeaps[] = { ResourceManager::Get()->GetMainCbvSrvUavHeap() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	cmdList->SetGraphicsRootDescriptorTable(0, m_DescriptorHeapStart.GpuHandle);

	m_constantBufferData.worldMatrix = XMMatrixIdentity();
	XMMATRIX rotationMatrixY = XMMatrixRotationY(XMConvertToRadians(m_rotationAngle));
	//XMMATRIX scaleMatrix = XMMatrixScaling(0.01f, 0.1f, 0.01f);
	m_constantBufferData.worldMatrix = rotationMatrixY;
	
	m_constantBufferData.viewMatrix = m_pRenderEngine->m_camera.GetView();
	m_constantBufferData.projectionMatrix = m_pRenderEngine->m_camera.GetProjection();

	memcpy(m_CBVHeapStartPointer, &m_constantBufferData, sizeof(m_constantBufferData));

	// Set vertex buffer and draw the model.
	RenderModel(cmdList, m_pModel);
}

void DefaultScene::Update(void)
{
	// Application update call
	//m_rotationAngle += 0.05f;
	if (m_rotationAngle > 360.0f) {
		m_rotationAngle -= 360.0f;
	}
}

void DefaultScene::RenderImGui()
{
	ImGui::Begin("Scene One", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Model Controls");
	ImGui::SliderFloat("Rotation", &m_rotationAngle, 0.0f, 360.0f);

	bool isLightingEnabled = (m_constantBufferData.keyPressed == 1);
	if (ImGui::Checkbox("Enable Lighting", &isLightingEnabled))
	{
		m_constantBufferData.keyPressed = isLightingEnabled ? 1 : 0;
	}

	ImGui::DragFloat3("Light Position", &m_constantBufferData.lightPosition.x, 0.1f);
	ImGui::ColorEdit3("Light Color", &m_constantBufferData.lD.x);
	ImGui::ColorEdit3("Material Color", &m_constantBufferData.kD.x);
	ImGui::End();


}

void DefaultScene::OnDestroy()
{
	// ComPtrs handle automatic cleanup of D3D resources.
}
