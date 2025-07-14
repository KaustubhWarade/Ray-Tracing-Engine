#pragma once

#include "../RenderEngine Files/global.h"
using namespace DirectX;

class Camera
{
public:
	Camera();

	void OnResize(uint32_t width, uint32_t height);

	const XMMATRIX GetProjection() const { return m_Projection; }
	const XMMATRIX GetInverseProjection() const { return m_InverseProjection; }

	const XMMATRIX GetView() const { return m_View; }
	const XMMATRIX GetInverseView() const { return m_InverseView; }

	const XMVECTOR GetPosition() const { return XMLoadFloat3(&m_Position); }
	const XMFLOAT3 GetPosition3f() const { return m_Position; }

	const XMVECTOR GetForwardDirection() const { return XMLoadFloat3(&m_ForwardDirection); }
	const XMFLOAT3 GetForwardDirection3f() const { return m_ForwardDirection; }

	const XMFLOAT3 GetRightDirection3f() const { return m_RightDirection; }
	const XMVECTOR GetRightDirection() const { return XMLoadFloat3(&m_RightDirection); }

	const XMFLOAT3 GetUpDirection3f() const { return m_UpDirection; }
	const XMVECTOR GetUpDirection() const { return XMLoadFloat3(&m_UpDirection); }

	void SetPosition(const XMFLOAT3 &position)
	{
		m_Position = position;
		RecalculateView();
		RecalculateRayDirections();
	}
	void SetPosition(const XMVECTOR &position)
	{
		XMStoreFloat3(&m_Position, position);
		RecalculateView();
		RecalculateRayDirections();
	}
	void SetPosition(float x, float y, float z)
	{
		m_Position = XMFLOAT3(x, y, z);
		RecalculateView();
		RecalculateRayDirections();
	}


	const std::vector<XMFLOAT3> &GetRayDirection() const { return m_RayDirections; }
	

	void OnMouseMove(float x, float y);
	void OnLeftButtonDown(float x, float y);
	void OnLeftButtonUp(float x, float y);

	float GetRotationSpeed();

	bool HandleWindowsMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
	void RecalculateProjection();
	void RecalculateView();
	void RecalculateRayDirections();

private:
	XMMATRIX m_Projection = DirectX::XMMatrixIdentity();
	XMMATRIX m_View = DirectX::XMMatrixIdentity();
	XMMATRIX m_InverseProjection = DirectX::XMMatrixIdentity();
	XMMATRIX m_InverseView = DirectX::XMMatrixIdentity();

	float m_VerticalFOV = 45.0f;
	float m_NearClip = 0.1f;
	float m_FarClip = 100.0f;

	XMFLOAT3 m_Position{0.0f, 0.0f, 0.0f};
	XMFLOAT3 m_ForwardDirection{0.0f, 0.0f, 0.0f};
	XMFLOAT3 m_RightDirection{0.0f, 0.0f, 0.0f};
	XMFLOAT3 m_UpDirection{0.0f, 1.0f, 0.0f};

	float m_Sensitivity = 0.3f;

	// Cached ray directions
	std::vector<XMFLOAT3> m_RayDirections;

	BOOL m_IsMouseDown = FALSE;
	XMFLOAT2 m_LastMousePosition{0.0f, 0.0f};

	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
};
