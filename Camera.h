#pragma once
#include "d3dUtil.h"

class Camera {
public:
	Camera();
	~Camera(){}

	DirectX::XMFLOAT3 GetPosition3f() { return position; }
	DirectX::XMFLOAT4X4 GetViewMatrix4x4();
	DirectX::XMFLOAT4X4 GetProjMatrix4x4();

	void SetLens(float fovY, float aspect, float nearZ, float farZ);
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void UpdataViewMatrix();

	void Walk(float distance);
	void Strafe(float distance);
	void Pitch(float angle);
	void RotateY(float angle);

private:
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 right = { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 look = { 0.0f, 0.0f, 1.0f };

	DirectX::XMFLOAT4X4 projMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;

	float nearZ = 0.0f;
	float farZ = 0.0f;
	float aspect = 0.0f;
	float fovY = 0.0f;
	float nearWindowHeight = 0.0f;
	float farWindowHeight = 0.0f;

	bool viewDirty = false;
};