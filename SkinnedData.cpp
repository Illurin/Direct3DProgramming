#include "SkinnedData.h"

float BoneAnimation::GetStartTime()const {
	float t0 = 0.0f;
	float t1 = 0.0f;
	float t2 = 0.0f;
	if (translation.size() != 0) t0 = translation.front().timePos;
	if (scale.size() != 0) t1 = scale.front().timePos;
	if (rotationQuat.size() != 0) t2 = rotationQuat.front().timePos;

	float timePos = t0 < t1 ? t0 : t1;
	timePos = timePos < t2 ? timePos : t2;
	return timePos;
}

float BoneAnimation::GetEndTime()const {
	float t0 = 0.0f;
	float t1 = 0.0f;
	float t2 = 0.0f;
	if (translation.size() != 0) t0 = translation.back().timePos;
	if (scale.size() != 0) t1 = scale.back().timePos;
	if (rotationQuat.size() != 0) t2 = rotationQuat.back().timePos;

	float timePos = t0 > t1 ? t0 : t1;
	timePos = timePos > t2 ? timePos : t2;
	return timePos;
}

DirectX::XMVECTOR BoneAnimation::LerpKeys(float t, const std::vector<VectorKey>& keys) {
	DirectX::XMVECTOR res = DirectX::XMVectorZero();
	if (t <= keys.front().timePos) res = DirectX::XMLoadFloat3(&keys.front().value);
	else if (t >= keys.back().timePos) res = DirectX::XMLoadFloat3(&keys.back().value);
	else {
		for (size_t i = 0; i < keys.size() - 1; i++){
			if (t >= keys[i].timePos && t <= keys[i + 1].timePos){
				float lerpPercent = (t - keys[i].timePos) / (keys[i + 1].timePos - keys[i].timePos);
				DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&keys[i].value);
				DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&keys[i + 1].value);
				res = DirectX::XMVectorLerp(v0, v1, lerpPercent);
				break;
			}
		}
	}
	return res;
}

DirectX::XMVECTOR BoneAnimation::LerpKeys(float t, const std::vector<QuatKey>& keys) {
	DirectX::XMVECTOR res = DirectX::XMVectorZero();
	if (t <= keys.front().timePos) res = DirectX::XMLoadFloat4(&keys.front().value);
	else if (t >= keys.back().timePos) res = DirectX::XMLoadFloat4(&keys.back().value);
	else {
		for (size_t i = 0; i < keys.size() - 1; i++) {
			if (t >= keys[i].timePos && t <= keys[i + 1].timePos) {
				float lerpPercent = (t - keys[i].timePos) / (keys[i + 1].timePos - keys[i].timePos);
				DirectX::XMVECTOR v0 = DirectX::XMLoadFloat4(&keys[i].value);
				DirectX::XMVECTOR v1 = DirectX::XMLoadFloat4(&keys[i + 1].value);
				res = DirectX::XMQuaternionSlerp(v0, v1, lerpPercent);
				break;
			}
		}
	}
	return res;
}

void BoneAnimation::Interpolate(float t, DirectX::XMFLOAT4X4& M) {
	if (translation.size() == 0 && scale.size() == 0 && rotationQuat.size() == 0) {
		M = defaultTransform;
		return;
	}

	DirectX::XMVECTOR T = DirectX::XMVectorZero();
	DirectX::XMVECTOR S = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	DirectX::XMVECTOR Q = DirectX::XMVectorZero();
	DirectX::XMVECTOR zero = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	if (translation.size() != 0) T = LerpKeys(t, translation);
	if (scale.size() != 0) S = LerpKeys(t, scale);
	if (rotationQuat.size() != 0) Q = LerpKeys(t, rotationQuat);

	DirectX::XMMATRIX result = DirectX::XMMatrixAffineTransformation(S, zero, Q, T);
	DirectX::XMStoreFloat4x4(&M, result);
}

float AnimationClip::GetClipStartTime()const {
	float t = D3D12_FLOAT32_MAX;
	for (UINT i = 0; i < boneAnimations.size(); i++)
		t = t < boneAnimations[i].GetStartTime() ? t : boneAnimations[i].GetStartTime();
	return t;
}

float AnimationClip::GetClipEndTime()const {
	float t = 0.0f;
	for (UINT i = 0; i < boneAnimations.size(); i++)
		t = t > boneAnimations[i].GetEndTime() ? t : boneAnimations[i].GetEndTime();
	return t;
}

void AnimationClip::Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransform) {
	for (UINT i = 0; i < boneAnimations.size(); i++)
		boneAnimations[i].Interpolate(t, boneTransform[i]);
}

float SkinnedData::GetClipStartTime(const std::string& clipName)const {
	auto clip = animations.find(clipName);
	return clip->second.GetClipStartTime();
}

float SkinnedData::GetClipEndTime(const std::string& clipName)const {
	auto clip = animations.find(clipName);
	return clip->second.GetClipEndTime();
}

void SkinnedData::Set(std::vector<int>& boneHierarchy,
	std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
	std::unordered_map<std::string, AnimationClip>& animations) {
	this->boneHierarchy = boneHierarchy;
	this->boneOffsets = boneOffsets;
	this->animations = animations;
}

void SkinnedData::GetFinalTransform(const std::string& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) {
	UINT numBones = boneOffsets.size();
	std::vector<DirectX::XMFLOAT4X4> toParentTransforms(numBones);

	auto clip = animations.find(clipName);
	clip->second.Interpolate(timePos, toParentTransforms);

	std::vector<DirectX::XMFLOAT4X4> toRootTransforms(numBones);
	toRootTransforms[0] = toParentTransforms[0];

	for (UINT i = 1; i < numBones; i++){
		DirectX::XMMATRIX toParent = DirectX::XMLoadFloat4x4(&toParentTransforms[i]);

		int parentIndex = boneHierarchy[i];
		DirectX::XMMATRIX parentToRoot = DirectX::XMLoadFloat4x4(&toRootTransforms[parentIndex]);

		DirectX::XMStoreFloat4x4(&toRootTransforms[i], DirectX::XMMatrixMultiply(toParent, parentToRoot));
	}

	for (UINT i = 0; i < numBones; i++)
	{
		DirectX::XMMATRIX offset = DirectX::XMLoadFloat4x4(&boneOffsets[i]);
		DirectX::XMMATRIX toRoot = DirectX::XMLoadFloat4x4(&toRootTransforms[i]);
		DirectX::XMStoreFloat4x4(&finalTransforms[i], DirectX::XMMatrixMultiply(offset, toRoot));
	}
}