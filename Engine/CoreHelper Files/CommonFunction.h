#pragma once
#include "GeoMetryHelper.h"


HRESULT CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	UINT64 dataSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer);

void TransitionResource(
	ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES oldState,
	D3D12_RESOURCE_STATES newState);


void CreateHeapProperties(D3D12_HEAP_PROPERTIES& heap, D3D12_HEAP_TYPE heapType);