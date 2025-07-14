#pragma once
#include "../RenderEngine Files/global.h"
#include "GeoMetryHelper.h"
#include "DDSTextureLoader12.h"

class Image
{
	//~Image() noexcept;
public:
	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

	HRESULT CreateImageResource(
		UINT width,
		UINT height,
		D3D12_RESOURCE_STATES startState,
		DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flags);
	void SetData(const uint32_t* data);

	void Resize(UINT width, UINT height);

	UINT m_width = 800;
	UINT m_height = 600;
	uint32_t* m_data = nullptr;
	XMFLOAT4* m_accumulateData = nullptr;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_footprint;
	ComPtr<ID3D12Resource> m_imageGPU;
	ComPtr<ID3D12Resource> m_imageUpload;
	D3D12_RESOURCE_STATES m_currentResourceState;
private:
	ID3D12Device* m_device;
	ID3D12GraphicsCommandList* m_cmdList;
	D3D12_RESOURCE_STATES m_startState = D3D12_RESOURCE_STATE_COMMON;
	DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
	D3D12_RESOURCE_FLAGS m_flags = D3D12_RESOURCE_FLAG_NONE;
};

class Texture
{
public:
	HRESULT loadTexture(ID3D12Device* pDevice, 
		ID3D12GraphicsCommandList* pCmdList, 
		const wchar_t* filename);
	HRESULT UpdateSubresources(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		UINT64 intermediateOffset,
		UINT firstSubresource);

	ComPtr<ID3D12Resource> m_ResourceGPU;
	ComPtr<ID3D12Resource> m_ResourceUpload;
	std::unique_ptr<uint8_t[]> m_textureddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> m_texturesubresources;
private:

};

