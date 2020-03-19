#include "Camera.h"

Camera::Camera() {
	SetLens(0.25f * DirectX::XM_PI, 2.0f, 1.0f, 1000.0f);
}

DirectX::XMFLOAT4X4 Camera::GetViewMatrix4x4() {
	return viewMatrix;
}

DirectX::XMFLOAT4X4 Camera::GetProjMatrix4x4() {
	return projMatrix;
}

void Camera::SetLens(float fovY, float aspect, float nearZ, float farZ) {
	this->fovY = fovY;
	this->aspect = aspect;
	this->nearZ = nearZ;
	this->farZ = farZ;

	nearWindowHeight = 2.0f * nearZ * tanf(0.5f * fovY);
	farWindowHeight = 2.0f * farZ * tanf(0.5f * fovY);

	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
	DirectX::XMStoreFloat4x4(&projMatrix, P);
}

void Camera::LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp) {
	DirectX::XMVECTOR L = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, pos));
	DirectX::XMVECTOR R = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, L));
	DirectX::XMVECTOR U = DirectX::XMVector3Cross(L, R);

	DirectX::XMStoreFloat3(&position, pos);
	DirectX::XMStoreFloat3(&right, R);
	DirectX::XMStoreFloat3(&up, U);
	DirectX::XMStoreFloat3(&look, L);

	viewDirty = true;
}

void Camera::UpdataViewMatrix() {
	if (viewDirty) {
		DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&right);
		DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&up);
		DirectX::XMVECTOR L = DirectX::XMLoadFloat3(&look);
		DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&position);

		L = DirectX::XMVector3Normalize(L);
		U = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(L, R));
		R = DirectX::XMVector3Cross(U, L);

		float x = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, R));
		float y = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, U));
		float z = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, L));

		DirectX::XMStoreFloat3(&right, R);
		DirectX::XMStoreFloat3(&up, U);
		DirectX::XMStoreFloat3(&look, L);

		viewMatrix = {
			right.x, up.x, look.x, 0.0f,
			right.y, up.y, look.y, 0.0f,
			right.z, up.z, look.z, 0.0f,
			x      , y   , z     , 1.0f
		};

		viewDirty = false;
	}
}

void Camera::Walk(float distance) {
	DirectX::XMVECTOR S = DirectX::XMVectorReplicate(distance);
	DirectX::XMVECTOR L = DirectX::XMLoadFloat3(&look);
	DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&position);
	DirectX::XMStoreFloat3(&position, DirectX::XMVectorMultiplyAdd(S, L, P));

	viewDirty = true;
}

void Camera::Strafe(float distance) {
	DirectX::XMVECTOR S = DirectX::XMVectorReplicate(distance);
	DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&right);
	DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&position);
	DirectX::XMStoreFloat3(&position, DirectX::XMVectorMultiplyAdd(S, R, P));

	viewDirty = true;
}

void Camera::Pitch(float angle) {
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&right), angle);
	DirectX::XMStoreFloat3(&up, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&up), R));
	DirectX::XMStoreFloat3(&look, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&look), R));

	viewDirty = true;
}

void Camera::RotateY(float angle) {
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationY(angle);
	DirectX::XMStoreFloat3(&up, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&up), R));
	DirectX::XMStoreFloat3(&look, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&look), R));
	DirectX::XMStoreFloat3(&right, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&right), R));
	
	viewDirty = true;
}