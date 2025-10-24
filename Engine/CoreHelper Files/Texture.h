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
	DescriptorHandle SrvHandle;
	std::wstring FilePath;
	UINT BindlessSrvIndex = -1;

};

