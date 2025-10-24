#include "ResourceManager.h"

#include "CommonFunction.h" 
#include "../RenderEngine Files/RenderEngine.h" 
#include "extraPackages/d3dx12.h"
#include "thirdParty/stb_image.h" 

// Static instance for the singleton
static std::unique_ptr<ResourceManager> s_instance;

ResourceManager* ResourceManager::Get()
{
	if (!s_instance) {
		s_instance = std::unique_ptr<ResourceManager>(new ResourceManager());
	}
	return s_instance.get();
}

ResourceManager::ResourceManager()
	: m_device(nullptr), m_cmdList(nullptr), m_renderEngine(nullptr)
{
}

ResourceManager::~ResourceManager()
{
	Shutdown();
}

HRESULT ResourceManager::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, RenderEngine* renderEngine)
{
	m_device = device;
	m_cmdList = cmdList;
	m_renderEngine = renderEngine;

	const UINT BINDLESS_CAPACITY = 1024; 
	const UINT GENERAL_PURPOSE_CAPACITY = 256; 
	const UINT TOTAL_CAPACITY = BINDLESS_CAPACITY + GENERAL_PURPOSE_CAPACITY;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = TOTAL_CAPACITY;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	EXECUTE_AND_LOG_RETURN(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_mainCbvSrvUavHeap)));
	GetMainCbvSrvUavHeap()->SetName(L"ResourceManager Main  CBV/SRV/UAVHeap");

	EXECUTE_AND_LOG_RETURN(m_bindlessTextureAllocator.Initialize(m_device,GetMainCbvSrvUavHeap(), 0, BINDLESS_CAPACITY));

	EXECUTE_AND_LOG_RETURN(m_generalPurposeHeapAllocator.Initialize(m_device,GetMainCbvSrvUavHeap(), BINDLESS_CAPACITY, GENERAL_PURPOSE_CAPACITY));


	return S_OK;
}

void ResourceManager::Shutdown()
{
	FlushUploadBuffers();
	for (auto const& [name, texture] : m_textures)
	{
		if (texture->SrvHandle.IsValid())
		{
			m_bindlessTextureAllocator.FreeSingle(texture->SrvHandle);
		}
	}
	m_models.clear();
	m_textures.clear();
	m_images.clear();

}

void ResourceManager::FlushUploadBuffers()
{
	m_uploadBuffersToRelease.clear();
}

void ResourceManager::StoreModel(const std::string& name, std::unique_ptr<Model> model)
{
	if (m_models.count(name) || !model) {
		return;
	}
	m_models[name] = std::move(model);
}

HRESULT ResourceManager::LoadModel(const std::string& name, const std::string& filename)
{
	if (m_models.count(name)) {
#ifdef _DEBUG
		fprintf(gpFile, "Model '%s' already loaded.\n", name.c_str());
#endif
		return S_OK; // Already loaded
	}

	auto model = std::make_unique<Model>();
	HRESULT hr = m_modelLoader.LoadGLTF(Get(), filename, *model);
	if (FAILED(hr)) {
#ifdef _DEBUG
		fprintf(gpFile, "Failed to load glTF model '%s' from '%s'.\n", name.c_str(), filename.c_str());
#endif
		return hr;
	}

	m_models[name] = std::move(model);
#ifdef _DEBUG
	fprintf(gpFile, "Successfully loaded model '%s'.\n", name.c_str());
#endif
	return S_OK;
}

HRESULT ResourceManager::CreateModel(const std::string& name, const void* vertexData, UINT vertexDataSize, UINT vertexStride, const void* indexData, UINT indexDataSize, DXGI_FORMAT indexFormat)
{
	if (m_models.count(name)) {
#ifdef _DEBUG
		fprintf(gpFile, "Model '%s' already exists.\n", name.c_str());
#endif
		return S_OK; // Already exists
	}

	auto model = std::make_unique<Model>();
	ModelMesh mesh = {};
	mesh.Name = std::wstring(name.begin(), name.end());

	const ModelVertex* vertices = static_cast<const ModelVertex*>(vertexData);
	const size_t vertexCount = vertexDataSize / vertexStride;
	mesh.Vertices.assign(vertices, vertices + vertexCount);

	EXECUTE_AND_LOG_RETURN(createGeometryVertexResource(
		m_device, m_cmdList, mesh, vertexData, vertexDataSize));

	if (indexData && indexDataSize > 0)
	{
		const size_t indexSize = (indexFormat == DXGI_FORMAT_R16_UINT) ? sizeof(uint16_t) : sizeof(uint32_t);
		const size_t indexCount = indexDataSize / indexSize;

		// We need to convert uint16_t indices to uint32_t for our vector.
		if (indexFormat == DXGI_FORMAT_R16_UINT)
		{
			const uint16_t* indices16 = static_cast<const uint16_t*>(indexData);
			mesh.Indices.resize(indexCount);
			for (size_t i = 0; i < indexCount; ++i)
			{
				mesh.Indices[i] = indices16[i];
			}
		}
		else
		{
			const uint32_t* indices32 = static_cast<const uint32_t*>(indexData);
			mesh.Indices.assign(indices32, indices32 + indexCount);
		}
	}

	if (indexData && indexDataSize > 0)
	{
		EXECUTE_AND_LOG_RETURN(createGeometryIndexResource(
			m_device, m_cmdList, mesh, indexData, indexDataSize, indexFormat));
	}

	model->EnsureDefaultMaterial();
	// A simple procedural mesh has one primitive that covers all indices/vertices
	ModelPrimitive primitive = {};
	primitive.IndexCount = (UINT)mesh.Indices.size();
	primitive.StartIndexLocation = 0;
	primitive.BaseVertexLocation = 0;
	primitive.MaterialIndex = 0;
	mesh.Primitives.push_back(primitive);

	model->Meshes.push_back(std::move(mesh));

	m_models[name] = std::move(model);
#ifdef _DEBUG
	fprintf(gpFile, "Successfully created procedural model '%s'.\n", name.c_str());
#endif
	return S_OK;
}

Model* ResourceManager::GetModel(const std::string& name)
{
	auto it = m_models.find(name);
	if (it == m_models.end()) {
#ifdef _DEBUG
		fprintf(gpFile, "Model '%s' not found.\n", name.c_str());
#endif
		return nullptr;
	}
	return it->second.get();
}

void ResourceManager::UnloadModel(const std::string& name)
{
	if (m_models.count(name)) {
		m_models.erase(name); // unique_ptr releases resources
		fprintf(gpFile, "Model '%s' unloaded.\n", name.c_str());
	}
}

HRESULT LoadTextureWithStb(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const wchar_t* filename, ComPtr<ID3D12Resource>& textureResource, ComPtr<ID3D12Resource>& uploadBuffer)
{
	int texWidth, texHeight, texChannels;
	char filename_char[260];
	size_t i;
	wcstombs_s(&i, filename_char, 260, filename, 260);

	stbi_uc* pixels = stbi_load(filename_char, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (pixels == nullptr)
	{
		if (gpFile) fprintf(gpFile, "STB Failed to load image: %s. Reason: %s\n", filename_char, stbi_failure_reason());
		return E_FAIL;
	}

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = texWidth;
	texDesc.Height = texHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; 
	texDesc.SampleDesc.Count = 1;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_HEAP_PROPERTIES heapProps = {};
	CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_DEFAULT);
	EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&textureResource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource.Get(), 0, 1);

	CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = uploadBufferSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)));

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = pixels;
	textureData.RowPitch = (long long)texWidth * STBI_rgb_alpha;
	textureData.SlicePitch = textureData.RowPitch * texHeight;

	UpdateSubresources(cmdList, textureResource.Get(), uploadBuffer.Get(), 0, 0, 1, &textureData);
	stbi_image_free(pixels);
	return S_OK;
}

HRESULT ResourceManager::LoadTextureFromFile(const std::string& name, const std::wstring& filename)
{
	if (m_textures.count(name)) {
		return S_OK;
	}

	auto texture = std::make_unique<Texture>();
	texture->FilePath = filename;
	ComPtr<ID3D12Resource> uploader;
	HRESULT hr = S_OK;

	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;

	std::wstring ext;
	size_t pos = filename.find_last_of(L".");
	if (pos != std::wstring::npos) {
		ext = filename.substr(pos + 1);
		std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
	}

	if (ext == L"dds")
	{
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		hr = DirectX::LoadDDSTextureFromFile(
			m_device,
			filename.c_str(),
			texture->m_ResourceGPU.GetAddressOf(),
			ddsData,
			subresources);

		if (FAILED(hr) || !texture->m_ResourceGPU || subresources.empty())
		{
			fprintf(gpFile, "LoadDDSTextureFromFile() Failed. HRESULT: 0x%08X\n", hr);
			return hr;
		}
		D3D12_RESOURCE_DESC desc = texture->m_ResourceGPU->GetDesc();
		fprintf(gpFile, "Format: %d, Width: %llu, Height: %u, MipLevels: %d, Dimension: %d\n",
			desc.Format, desc.Width, desc.Height, desc.MipLevels, desc.Dimension);

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture->m_ResourceGPU.Get(), 0, static_cast<UINT>(subresources.size()));

		D3D12_HEAP_PROPERTIES heapProps = {};
		CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufferDesc = {};
		ZeroMemory(&bufferDesc, sizeof(D3D12_RESOURCE_DESC));
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Alignment = 0;
		bufferDesc.Width = uploadBufferSize;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.SampleDesc.Quality = 0;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		EXECUTE_AND_LOG_RETURN(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploader)
		));

		UpdateSubresources(m_cmdList, texture->m_ResourceGPU.Get(), uploader.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

	}
	else
	{
		// Use our new STB helper for other formats
		EXECUTE_AND_LOG_RETURN(LoadTextureWithStb(m_device, m_cmdList, filename.c_str(), texture->m_ResourceGPU, uploader));
	}
	m_renderEngine->TrackResource(texture->m_ResourceGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
	
	m_renderEngine->TransitionResource(
		m_cmdList,
		texture->m_ResourceGPU.Get(),
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	texture->SrvHandle = m_bindlessTextureAllocator.AllocateSingle();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texture->m_ResourceGPU->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = texture->m_ResourceGPU->GetDesc().MipLevels;
	m_device->CreateShaderResourceView(texture->m_ResourceGPU.Get(), &srvDesc, texture->SrvHandle.CpuHandle);

	//allocate in bindless buffer
	texture->BindlessSrvIndex = texture->SrvHandle.HeapIndex;

	m_textures[name] = std::move(texture);
	m_uploadBuffersToRelease.push_back(uploader);

	return S_OK;
}

Texture* ResourceManager::GetTexture(const std::string& name)
{
	auto it = m_textures.find(name);
	if (it == m_textures.end()) {
		fprintf(gpFile, "Texture '%s' not found.\n", name.c_str());
		return nullptr;
	}
	return it->second.get();
}

void ResourceManager::UnloadTexture(const std::string& name)
{
	auto it = m_textures.find(name);
	if (it != m_textures.end()) {
		Texture* texture = it->second.get();

		if (texture->SrvHandle.IsValid())
		{
			m_bindlessTextureAllocator.FreeSingle(texture->SrvHandle);
		}

		m_textures.erase(it);
		fprintf(gpFile, "Texture '%s' unloaded.\n", name.c_str());
	}
}

Texture* ResourceManager::CreateTextureFromMemory(const std::string& name, int width, int height, DXGI_FORMAT format, int bytesPerPixel, const unsigned char* pixelData)
{
	auto it = m_textures.find(name);
	if (it != m_textures.end()) {
		return it->second.get();
	}

	if (!pixelData) {
		return nullptr;
	}
	auto texture = std::make_unique<Texture>();
	ComPtr<ID3D12Resource> uploader;

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = format;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProps = {};
	CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_DEFAULT);
	HRESULT hr = m_device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr, IID_PPV_ARGS(&texture->m_ResourceGPU));
	if (FAILED(hr)) return nullptr;

	texture->m_ResourceGPU->SetName(std::wstring(name.begin(), name.end()).c_str());

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture->m_ResourceGPU.Get(), 0, 1);

	CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = uploadBufferSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;

	hr = m_device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&uploader));
	if (FAILED(hr)) return nullptr;

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = pixelData;
	textureData.RowPitch = (long long)width * bytesPerPixel;
	textureData.SlicePitch = textureData.RowPitch * height;

	UpdateSubresources(m_cmdList, texture->m_ResourceGPU.Get(), uploader.Get(), 0, 0, 1, &textureData);

	m_renderEngine->TrackResource(texture->m_ResourceGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST);

	m_renderEngine->TransitionResource(
		m_cmdList,
		texture->m_ResourceGPU.Get(),
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	texture->SrvHandle = m_bindlessTextureAllocator.AllocateSingle();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texture->m_ResourceGPU->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	m_device->CreateShaderResourceView(texture->m_ResourceGPU.Get(), &srvDesc, texture->SrvHandle.CpuHandle);

	//allocate in bindless buffer
	texture->BindlessSrvIndex = texture->SrvHandle.HeapIndex;

	Texture* resultPtr = texture.get();
	m_textures[name] = std::move(texture);
	m_uploadBuffersToRelease.push_back(uploader);

	return resultPtr;
}

HRESULT ResourceManager::CreateImage(const std::string& name, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
{
	if (m_images.count(name)) {
		fprintf(gpFile, "Image '%s' already exists.\n", name.c_str());
		m_images[name]->Resize(width, height);
		return S_OK;
	}

	auto image = std::make_unique<Image>();
	HRESULT hr = image->Initialize(m_device, m_cmdList, &m_generalPurposeHeapAllocator, width, height, format, flags);
	if (FAILED(hr))
	{
		fprintf(gpFile, "Failed to create Image resource '%s'.\n", name.c_str());
		return hr;
	}

	m_renderEngine->TrackResource(image->GetResource(), D3D12_RESOURCE_STATE_COMMON);

	m_images[name] = std::move(image);
	fprintf(gpFile, "Successfully created Image '%s'.\n", name.c_str());
	return S_OK;
}

Image* ResourceManager::GetImage(const std::string& name)
{
	auto it = m_images.find(name);
	if (it == m_images.end()) {
		fprintf(gpFile, "Image '%s' not found.\n", name.c_str());
		return nullptr;
	}
	return it->second.get();
}

HRESULT ResourceManager::ResizeImage(const std::string& name, UINT width, UINT height)
{
	auto it = m_images.find(name);
	if (it == m_images.end()) {
		return E_INVALIDARG;
	}

	Image* image = it->second.get();

	// Check if a resize is actually needed
	if (image->GetWidth() == width && image->GetHeight() == height)
	{
		return S_OK;
	}
	m_renderEngine->UnTrackResource(image->GetResource());

	image->Resize(width, height);

	m_renderEngine->TrackResource(image->GetResource(), D3D12_RESOURCE_STATE_COMMON);

}

void ResourceManager::UpdateImage(const std::string& name, const uint32_t* data)
{
	Image* img = GetImage(name);
	if (img) {
		//img->SetData(data);
	}
}

void ResourceManager::UnloadImage(const std::string& name)
{
	auto it = m_images.find(name);
	if (it != m_images.end()) {
		m_images.erase(it); // unique_ptr releases resources
		fprintf(gpFile, "Image '%s' unloaded.\n", name.c_str());
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::GetGpuDescriptorHandle(const std::string& assetName)
{
	// Check if it's a texture from LoadTexture
	auto tex_it = m_textures.find(assetName);
	if (tex_it != m_textures.end()) {
		return tex_it->second->SrvHandle.GpuHandle;
	}
	// Check if it's an image from CreateImage (if SRV was allocated)
	auto img_it = m_images.find(assetName);
	if (img_it != m_images.end()) {
		return img_it->second->GetSrvHandle().GpuHandle;
	}
	return D3D12_GPU_DESCRIPTOR_HANDLE{}; // Return invalid handle if not found
}
