#pragma once
#include "../RenderEngine Files/global.h"
#include "GeoMetryHelper.h"
#include "DDSTextureLoader12.h"
#include "DescriptorAllocator.h"


// A manager class for a dynamic, CPU-writable image resource.
// Use this for render targets, procedural generation, ray tracing output, etc.
class Image
{
public:
	Image();
	~Image();

	HRESULT Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, DescriptorAllocator* allocator,
		UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	void CommitChanges();

	void Resize(UINT width, UINT height);

	void ClearAccumulationData();

	ID3D12Resource* GetResource() const { return m_gpuResource.Get(); }
	DescriptorHandle GetSrvHandle() const { return m_srvHandle; }
	DescriptorHandle GetUavHandle() const { return m_uavHandle; }
	DescriptorHandle GetRtvHandle() const { return m_rtvHandle; }

	uint32_t* GetPixelBuffer() { return m_pixelData; }
	XMFLOAT4* GetAccumulationBuffer() { return m_accumulateData; }
	UINT GetWidth() const { return m_width; }
	UINT GetHeight() const { return m_height; }

private:
	ID3D12Device* m_device;
	ID3D12GraphicsCommandList* m_cmdList;
	DescriptorAllocator* m_allocator = nullptr;

	uint32_t* m_pixelData = nullptr;
	XMFLOAT4* m_accumulateData = nullptr;
	UINT m_width = 0, m_height = 0;
	D3D12_RESOURCE_FLAGS m_flags;
	DXGI_FORMAT m_format;

	void Internal_CreateViews();
	void Internal_FreeViews();
	void Shutdown();

	Microsoft::WRL::ComPtr<ID3D12Resource> m_gpuResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	DescriptorHandle m_rtvHandle;
	DescriptorHandle m_srvHandle;
	DescriptorHandle m_uavHandle;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_footprint;
};

