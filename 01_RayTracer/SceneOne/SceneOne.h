#pragma once

#include "../Application.h"

#include "CoreHelper Files/Image.h"
#include "CoreHelper Files/PipelineBuilderHelper.h"
#include "CoreHelper Files/ShaderHelper.h"
#include "CoreHelper Files/Helper.h"
#include "RenderEngine Files/global.h"

#include <vector>
#include <string>


struct Material
{
	XMFLOAT3 Albedo{ 1.0f, 1.0f, 1.0f };
	float Roughness = 1.0f;

	XMFLOAT3 EmissionColor{ 0.0f, 0.0f, 0.0f };
	float EmissionPower = 0.0f;

    XMFLOAT3 AbsorptionColor = { 0.0f, 0.0f, 0.0f };
	float Metallic = 0.0f;
    float Transmission = 0.0f;
    float IOR = 1.5f;


	XMVECTOR GetEmission() const { return XMVectorScale(XMLoadFloat3(&EmissionColor), EmissionPower); }
};

struct Sphere
{
	XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
	float Radius = 0.5f;

	int MaterialIndex = 0;
};

struct Triangle
{
    XMFLOAT3 v0, v1, v2;    // Vertex positions
    XMFLOAT3 n0, n1, n2;    // Vertex normals
    int MaterialIndex = 0;
};

struct HitData
{
	float HitDistance;
	XMFLOAT3 HitPosition;
	XMFLOAT3 HitNormal;

	int ObjectIndex;
};


namespace BVH
{
    struct Node
    {
        XMFLOAT3 min_aabb;
        int primitives_count; // Using count instead of indices array for simplicity first. -1 for inner node
        XMFLOAT3 max_aabb;
        int right_child_offset; // Offset to the right child node in the buffer
    };
}

class SceneOne : public IScene
{
public:
    SceneOne() = default;
    virtual ~SceneOne() = default;

    // --- Public IScene contract implementation ---
    virtual HRESULT Initialize(RenderEngine* pRenderEngine) override;
    virtual void OnResize(UINT width, UINT height) override;
    virtual bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;
    virtual void OnViewChanged() override;

    virtual void PopulateCommandList() override;
    virtual void Update() override;
    virtual void RenderImGui() override;
    
    virtual void OnDestroy() override;

private:

    HRESULT InitializeResources(void);
    HRESULT InitializeComputeResources(void);

    //compute Shader resources
	COMPUTE_SHADER_DATA m_computeShaderData;
    PipeLineStateObject m_computePSO;
    //ComPtr<ID3D12DescriptorHeap> m_computeDescriptorHeap;

    ComPtr<ID3D12Resource> m_sphereUpload;
    ComPtr<ID3D12Resource> m_sphereBuffer;
    ComPtr<ID3D12Resource> m_materialUpload;
    ComPtr<ID3D12Resource> m_materialBuffer;
    ComPtr<ID3D12Resource> m_accumulationTextureResource;
    ComPtr<ID3D12Resource> m_outputTextureResource;
    ComPtr<ID3D12Resource> m_ConstantBufferView;
    UINT8* m_pCbvDataBegin = nullptr; 
    Image m_accumulationTexture;
    Image m_outputTextureImage;
    Texture m_environmentTexture;

    DescriptorAllocation m_computeDescriptorHeapStart;
    DescriptorAllocation m_texQuadDescriptorHeapStart;

    // normal rendering image
    SHADER_DATA m_finalShader;
	PipeLineStateObject m_finalTexturePSO;

	GeometryMesh m_quadMesh;
    uint32_t m_frameIndex = 1;

    // reset accumulationtexture
    bool m_cameraMoved = true;
    bool m_sceneDataDirty = false;

    struct CBUFFER
    {
        XMFLOAT3 CameraPosition;
        UINT FrameIndex;
        XMMATRIX InverseProjection;
        XMMATRIX InverseView;

        UINT numBounces;
        UINT numRaysPerPixel;

        float exposure;

    };

    int mc_numBounces = 10;
    int mc_numRaysPerPixel = 10;
    float mc_exposure = 0.2f;

    RenderEngine* m_pRenderEngine = nullptr;

    struct SceneData
    {
        std::vector<Sphere> Spheres;
        std::vector<Material> Materials;
    };
    SceneData m_sceneData;
    


};
