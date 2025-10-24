#pragma once
#include "GeoMetryHelper.h"
#include "Model Loader/ModelLoader.h"


HRESULT CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	UINT64 dataSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer,
	UINT64 bufferSize = 0);

void TransitionResource(
	ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES oldState,
	D3D12_RESOURCE_STATES newState);


void CreateHeapProperties(D3D12_HEAP_PROPERTIES& heap, D3D12_HEAP_TYPE heapType);

std::wstring OpenFileDialog();

void RenderModel(ID3D12GraphicsCommandList* cmdList, Model* model);
void RenderModel(ID3D12GraphicsCommandList* cmdList, Model* model, UINT materialRootParamIndex);

inline std::string WStringToString(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	// WideCharToMultiByte 
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	// WideCharToMultiByte 
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

	return strTo;
}

inline std::wstring StringToWString(const std::string& str)
{
	if (str.empty()) return {};
	int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(),nullptr, 0);
	std::wstring wstrTo(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(),&wstrTo[0], sizeNeeded);
	return wstrTo;
}