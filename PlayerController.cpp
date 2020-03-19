#include "PlayerController.h"

void ActorController::SetCamera(DirectX::XMVECTOR cameraPosition, DirectX::XMVECTOR modelOrigin, float horizontalSpeed, float verticalSpeed) {
	this->cameraPosition = cameraPosition;
	this->modelOrigin = modelOrigin;
	cameraHorizontalSpeed = horizontalSpeed;
	cameraVerticalSpeed = verticalSpeed;

	DirectX::XMVECTOR centerTranslation = modelOrigin;
	centerToHandle = DirectX::XMMatrixTranslationFromVector(centerTranslation);
}

void ActorController::SetModel(float modelRotate, float handleRotate, float scale, float velocity) {
	handleRotateY = handleRotate;
	modelEulerAngle = modelRotate;
	scaling = DirectX::XMMatrixScaling(scale, scale, scale);
	this->velocity = velocity;

	translation = DirectX::XMVectorZero();
	rotateOrigin = DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
}

void ActorController::Update(char* keyBuffers, float deltaTime) {
	float Dup = (keyBuffers[DIK_UP] & 0x80 ? 1.0f : 0.0f) - (keyBuffers[DIK_DOWN] & 0x80 ? 1.0f : 0.0f);
	float Dright = (keyBuffers[DIK_RIGHT] & 0x80 ? 1.0f : 0.0f) - (keyBuffers[DIK_LEFT] & 0x80 ? 1.0f : 0.0f);

	float Jup = (keyBuffers[DIK_W] & 0x80 ? 1.0f : 0.0f) - (keyBuffers[DIK_S] & 0x80 ? 1.0f : 0.0f);
	float Jright = (keyBuffers[DIK_D] & 0x80 ? 1.0f : 0.0f) - (keyBuffers[DIK_A] & 0x80 ? 1.0f : 0.0f);

	float Dmag = sqrt(Dup * Dup + Dright * Dright);

	isWalk = false;

	//Update World Matrix
	if (Dmag > 0.1f) {
		float offsetAngle = 0.0f;
		if (Dup < 0.0f) {
			offsetAngle -= Dright * DirectX::XM_PI / 4.0f;
		}
		else if (Dup > 0.0f) {
			offsetAngle = DirectX::XM_PI;
			offsetAngle += Dright * DirectX::XM_PI / 4.0f;
		}
		else {
			offsetAngle = -Dright * DirectX::XM_PI / 2.0f;
		}
		float targetAngle = handleRotateY + offsetAngle;
		float lerpPercent = 0.3f;
		modelEulerAngle = modelEulerAngle * (1.0f - lerpPercent) + targetAngle * lerpPercent;

		DirectX::XMVECTOR forward = DirectX::XMVector3Transform(rotateOrigin, DirectX::XMMatrixRotationY(-modelEulerAngle));

		DirectX::XMVECTOR moveDistance = DirectX::XMVectorScale(forward, Dmag * velocity * deltaTime);
		translation = DirectX::XMVectorSetX(translation, DirectX::XMVectorGetX(translation) - DirectX::XMVectorGetX(moveDistance));
		translation = DirectX::XMVectorSetZ(translation, DirectX::XMVectorGetZ(translation) + DirectX::XMVectorGetZ(moveDistance));
	
		isWalk = true;
	}

	//Update Camera
	cameraRotateX += Jup * cameraVerticalSpeed * deltaTime;
	handleRotateY += Jright * cameraHorizontalSpeed * deltaTime;

	//Recalc World Position
	//playerHandle
	//  -model
	//  -center
	//    -camera

	DirectX::XMMATRIX handleTranslate = DirectX::XMMatrixTranslationFromVector(translation);
	DirectX::XMMATRIX handleRotate = DirectX::XMMatrixRotationY(handleRotateY);
	DirectX::XMMATRIX cameraToCenter = DirectX::XMMatrixRotationX(cameraRotateX);
	DirectX::XMMATRIX modelRotate = DirectX::XMMatrixRotationY(modelEulerAngle);

	DirectX::XMMATRIX cameraToWorld = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(cameraToCenter, centerToHandle), handleRotate), handleTranslate);
	DirectX::XMMATRIX centerToWorld = DirectX::XMMatrixMultiply(centerToHandle, DirectX::XMMatrixMultiply(handleRotate, handleTranslate));

	cameraPositionWorld = DirectX::XMVector4Transform(cameraPosition, cameraToWorld);
	modelOriginWorld = DirectX::XMVector4Transform(modelOrigin, centerToWorld);

	camera.LookAt(cameraPositionWorld, modelOriginWorld, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	camera.UpdataViewMatrix();

	toWorld = DirectX::XMMatrixMultiply(scaling, DirectX::XMMatrixMultiply(modelRotate, handleTranslate));
}

void ActorController::GetWorldMatrix(DirectX::XMFLOAT4X4& world)const {
	DirectX::XMStoreFloat4x4(&world, toWorld);
}

void ActorController::GetViewProjMatrix(DirectX::XMFLOAT4X4& view, DirectX::XMFLOAT4X4& proj) {
	view = camera.GetViewMatrix4x4();
	proj = camera.GetProjMatrix4x4();
}

DirectX::XMFLOAT3 ActorController::GetCameraPosition() {
	return camera.GetPosition3f();
}