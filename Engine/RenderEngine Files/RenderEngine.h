#pragma once

#include "global.h"
#include "../CoreHelper Files/Camera.h"
#include "../IApplication.h"
#include "../CoreHelper Files/DescriptorAllocator.h"
#include "../CoreHelper Files/ResourceManager.h"
#include "../CoreHelper Files/AccelerationStructureManager.h" 


#define FRAMECOUNT 2

class RenderEngine 
{
public:
    std::string name[256];

    static RenderEngine* Get();
    RenderEngine(const RenderEngine&) = delete;
    void operator=(const RenderEngine&) = delete;


    bool HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HRESULT initialize();
    HRESULT uninitialize();
    HRESULT initialize_imgui();
    HRESULT CreateDeviceCommandQueueAndSwapChain(void);
    HRESULT CreateRenderBufferHeaps();

    HRESULT resize(UINT width, UINT height);
    HRESULT CreateDepthStencilBuffer(UINT width, UINT height);

    void SetApplication(std::unique_ptr<IApplication> pApp);

    void WaitForPreviousFrame(void);

    void display(void);
    void update(void);

    ID3D12Device *GetDevice(void) const { return m_device.Get(); };
    ID3D12Device **GetDeviceAddress(void) { return m_device.GetAddressOf(); };
    ID3D12CommandQueue *GetCommandQueue(void) const { return m_commandQueue.Get(); };
    ID3D12CommandQueue **GetCommandQueueAddress(void) { return m_commandQueue.GetAddressOf(); };
    IDXGISwapChain3 *GetSwapChain(void) const { return m_swapChain.Get(); };
    IDXGISwapChain3 **GetSwapChainAddress(void) { return m_swapChain.GetAddressOf(); };
    ID3D12Resource *GetRenderTarget() const
    {
        return m_renderTargets[m_frameIndex].Get();
    };
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // viewport scissr rect
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    // Render Targets
    ComPtr<ID3D12Resource> m_renderTargets[FRAMECOUNT];
    std::unordered_map<ID3D12Resource *, D3D12_RESOURCE_STATES> m_resourceStates;
    void TrackResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState);
    void UnTrackResource(ID3D12Resource* resource);
    void TransitionResource(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES newState);

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    ComPtr<ID3D12DescriptorHeap> m_dsbHeap;

    // Descriptor Sizes
    UINT m_rtvDescriptorSize;

    ComPtr<ID3D12DescriptorHeap> m_imguiSrvHeap;

    ResourceManager* GetResourceManager() { return ResourceManager::Get(); }
    AccelerationStructureManager* GetAccelManager() { return AccelerationStructureManager::Get(); }

    UINT m_frameIndex;

    RenderEngine() = default;
private:

    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;

    // Fences
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 1;
    HANDLE m_fenceEvent;

    std::unique_ptr<IApplication> m_pApplication;
};
