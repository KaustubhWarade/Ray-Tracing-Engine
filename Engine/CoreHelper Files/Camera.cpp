#include "Camera.h"

using namespace DirectX;

Camera::Camera()
{
	m_VerticalFOV = 45.0f;
	m_NearClip = 0.1f;
	m_FarClip = 100.0f;
	m_ForwardDirection = XMFLOAT3(0, 0, -1);
	m_Position = XMFLOAT3(0.0f, 0.0f, 5.0f);
	m_ForwardDirection = XMFLOAT3(0.0f, 0.0f, -1.0f);
	m_UpDirection = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_ViewportWidth = 800;
	m_ViewportHeight = 600;
	XMVECTOR rightDirection = XMVector3Normalize(XMVector3Cross(XMVector3Normalize(XMLoadFloat3(&m_ForwardDirection)), XMVectorSet(0, 1, 0, 0)));
	m_RayDirections.push_back(XMFLOAT3(0, 0, 0)); // Initialize with a dummy value
	XMStoreFloat3(&m_RightDirection, rightDirection);
	RecalculateProjection();
	RecalculateView();
	//RecalculateRayDirections();
}

void Camera::OnResize(uint32_t width, uint32_t height)
{
	if (width == m_ViewportWidth && height == m_ViewportHeight)
		return;

	m_ViewportWidth = width;
	m_ViewportHeight = height;

	RecalculateProjection();
	//RecalculateRayDirections();
}

void Camera::OnMouseMove(float x, float y)
{
	if (!m_IsMouseDown)
	{
		return;
	}
	XMFLOAT2 mousePos = XMFLOAT2(x, y);
	XMFLOAT2 delta = XMFLOAT2(mousePos.x - m_LastMousePosition.x, mousePos.y - m_LastMousePosition.y);
	m_LastMousePosition = mousePos;
	if (delta.x == 0.0f && delta.y == 0.0f)
		return;

	float pitchDelta = -delta.y * GetRotationSpeed();
	float yawDelta = -delta.x * GetRotationSpeed();
	XMVECTOR forward = XMLoadFloat3(&m_ForwardDirection);
	XMVECTOR right = XMLoadFloat3(&m_RightDirection);
	XMVECTOR up = XMLoadFloat3(&m_UpDirection);

	XMVECTOR pitchQuat = XMQuaternionRotationAxis(right, pitchDelta);
	XMVECTOR yawQuat = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), yawDelta);

	XMVECTOR combinedQuat = XMQuaternionMultiply(pitchQuat, yawQuat);

	forward = XMVector3Rotate(forward, combinedQuat);
	XMStoreFloat3(&m_ForwardDirection, forward);
	right = XMVector3Cross(forward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMStoreFloat3(&m_RightDirection, XMVector3Normalize(right));
	up = XMVector3Rotate(up, combinedQuat);
	XMStoreFloat3(&m_UpDirection, XMVector3Normalize(up));
	RecalculateView();
	//RecalculateRayDirections();

}

void Camera::OnLeftButtonDown(float x, float y)
{
	m_LastMousePosition = XMFLOAT2(x, y);
	m_IsMouseDown = TRUE;

}

void Camera::OnLeftButtonUp(float x, float y)
{
	m_LastMousePosition = XMFLOAT2(x, y);
	m_IsMouseDown = FALSE;
}

float Camera::GetRotationSpeed()
{
	return 0.001f;
}

bool Camera::HandleWindowsMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	bool moved = false;
	switch (message)
	{
	case WM_MOUSEMOVE:
		if (static_cast<UINT8>(wParam) == MK_LBUTTON)
		{
			UINT x = LOWORD(lParam);
			UINT y = HIWORD(lParam);
			OnMouseMove(x, y);
			moved = true;
		}
		break;

	case WM_LBUTTONDOWN:
	{
		UINT x = LOWORD(lParam);
		UINT y = HIWORD(lParam);
		OnLeftButtonDown(x, y);
	}
	break;

	case WM_LBUTTONUP:
	{
		UINT x = LOWORD(lParam);
		UINT y = HIWORD(lParam);
		OnLeftButtonUp(x, y);
	}
	break;
	case WM_CHAR:
		switch (LOWORD(wParam))
		{
		case 'W':
		case 'w':
		{
			XMVECTOR tempPos = GetPosition();
			tempPos += GetForwardDirection() * 0.2f;
			SetPosition(tempPos);
			moved = true;
			break;
		}
		case 'S':
		case 's':
		{
			XMVECTOR tempPos = GetPosition();
			tempPos -= GetForwardDirection() * 0.2f;
			SetPosition(tempPos);
			moved = true;
			break;
		}
		case 'A':
		case 'a':
		{
			XMVECTOR tempPos = GetPosition();
			tempPos -= GetRightDirection() * 0.2f;
			SetPosition(tempPos);
			moved = true;
			break;
		}
		case 'D':
		case 'd':
		{
			XMVECTOR tempPos = GetPosition();
			tempPos += GetRightDirection() * 0.2f;
			SetPosition(tempPos);
			moved = true;
			break;
		}
		case 'Q':
		case 'q':
		{
			XMVECTOR tempPos = GetPosition();
			tempPos += GetUpDirection() * 0.2f;
			SetPosition(tempPos);
			moved = true;
			break;
		}
		case 'E':
		case 'e':
		{
			XMVECTOR tempPos = GetPosition();
			tempPos -= GetUpDirection() * 0.2f;
			SetPosition(tempPos);
			moved = true;
			break;
		}

		}

	}
	return moved;
}

void Camera::RecalculateProjection()
{
	m_Projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_VerticalFOV), (float)m_ViewportWidth / (float)m_ViewportHeight, m_NearClip, m_FarClip);
	m_InverseProjection = XMMatrixInverse(nullptr, m_Projection);
}

void Camera::RecalculateView()
{
	XMVECTOR pos = XMLoadFloat3(&m_Position);
	XMVECTOR target = XMVectorAdd(pos, XMLoadFloat3(&m_ForwardDirection));
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);

	m_View = XMMatrixLookAtLH(pos, target, up);

	m_InverseView = XMMatrixInverse(nullptr, m_View);

}

void Camera::RecalculateRayDirections()
{
	//log width and height
	std::wstring msg = L"[Camera] Width: " + std::to_wstring(m_ViewportWidth) + L", Height: " + std::to_wstring(m_ViewportHeight) + L"\n";
	OutputDebugStringW(msg.c_str());
	m_RayDirections.resize(m_ViewportWidth * m_ViewportHeight);

	float invW = 1.0f / (float)m_ViewportWidth;
	float invH = 1.0f / (float)m_ViewportHeight;
	assert(!XMMatrixIsNaN(m_InverseProjection));
	for (uint32_t y = 0; y < m_ViewportHeight; y++)
	{
		for (uint32_t x = 0; x < m_ViewportWidth; x++)
		{
			float px = 2.0f * ((float)x + 0.5f) * invW - 1.0f;
			float py = 2.0f * ((float)y + 0.5f) * invH - 1.0f;
			px = -px;

			XMVECTOR coord = XMVectorSet((float)px, (float)py, 1.0f, 1.0f);

			XMVECTOR viewSpace = XMVector4Transform(coord, m_InverseProjection);
			viewSpace = XMVectorDivide(viewSpace, XMVectorSplatW(viewSpace));

			XMVECTOR worldDirection = XMVector3TransformNormal(viewSpace, m_InverseView);
			worldDirection = XMVector3Normalize(worldDirection);

			XMStoreFloat3(&m_RayDirections[x + y * m_ViewportWidth], worldDirection);

		}
	}
}
