#pragma once
#include "../RenderEngine Files/global.h"
#include "GeoMetryHelper.h"
#include "DDSTextureLoader12.h"
#include "DescriptorAllocator.h"

// A simple data container for a static, file-based texture asset.
// This is the single, authoritative struct for all textures loaded from disk
struct Texture
{
	ComPtr<ID3D12Resource> m_ResourceGPU;
	DescriptorAllocation SrvAllocation;
	std::wstring FilePath;

	std::unique_ptr<uint8_t[]> m_textureddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> m_texturesubresources;
	UINT MipLevels = 1;
};

