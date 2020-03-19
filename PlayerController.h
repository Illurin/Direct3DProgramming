#pragma once
#include "d3dUtil.h"
#include "Camera.h"

class ActorController {
public:
	//Set Function
	void SetCamera(DirectX::XMVECTOR cameraPosition, DirectX::XMVECTOR modelOrigin, float horizontalSpeed, float verticalSpeed);
	void SetModel(float modelRotate, float handleRotate, float scale, float velocity);

	//Update Function
	void Update(char* keyBuffers, float deltaTime);

	//Get Function
	void GetWorldMatrix(DirectX::XMFLOAT4X4& world)const;
	void GetViewProjMatrix(DirectX::XMFLOAT4X4& view, DirectX::XMFLOAT4X4& proj);
	DirectX::XMFLOAT3 GetCameraPosition();

	bool isWalk = false;

private:
	float velocity = 0.0f;
	float cameraHorizontalSpeed = 0.0f;
	float cameraVerticalSpeed = 0.0f;

	DirectX::XMVECTOR rotateOrigin;

	DirectX::XMVECTOR cameraPosition;
	DirectX::XMVECTOR modelOrigin;
	DirectX::XMVECTOR cameraPositionWorld;
	DirectX::XMVECTOR modelOriginWorld;

	float modelEulerAngle = 0.0f;
	float cameraRotateX = 0.0f;
	float handleRotateY = 0.0f;

	DirectX::XMVECTOR translation;
	DirectX::XMMATRIX scaling;
	DirectX::XMMATRIX toWorld;
	DirectX::XMMATRIX centerToHandle;

	Camera camera;
};