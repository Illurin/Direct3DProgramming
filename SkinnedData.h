#pragma once
#include "d3dUtil.h"

struct VectorKey {
	float timePos;
	DirectX::XMFLOAT3 value;
};

struct QuatKey {
	float timePos;
	DirectX::XMFLOAT4 value;
};

class BoneAnimation {
public:
	float GetStartTime()const;
	float GetEndTime()const;
	void Interpolate(float t, DirectX::XMFLOAT4X4& M);
	
	std::vector<VectorKey> translation;
	std::vector<VectorKey> scale;
	std::vector<QuatKey> rotationQuat;

	DirectX::XMFLOAT4X4 defaultTransform;

private:
	DirectX::XMVECTOR LerpKeys(float t, const std::vector<VectorKey>& keys);
	DirectX::XMVECTOR LerpKeys(float t, const std::vector<QuatKey>& keys);
};

class AnimationClip {
public:
	float GetClipStartTime()const;
	float GetClipEndTime()const;
	void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransform);

	std::vector<BoneAnimation> boneAnimations;
};

class SkinnedData {
public:
	UINT GetBoneCount()const { return boneHierarchy.size(); }
	float GetClipStartTime(const std::string& clipName)const;
	float GetClipEndTime(const std::string& clipName)const;
	void Set(std::vector<int>& boneHierarchy,
		std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
		std::unordered_map<std::string, AnimationClip>& animations);
	void GetFinalTransform(const std::string& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms);

private:
	std::vector<int> boneHierarchy;
	std::vector<DirectX::XMFLOAT4X4> boneOffsets;
	std::unordered_map<std::string, AnimationClip> animations;
};

struct SkinnedModelInstance {
	SkinnedData* skinnedInfo = nullptr;
	std::vector<DirectX::XMFLOAT4X4> finalTransforms;
	std::string clipName;
	float timePos = 0.0f;

	void UpdateSkinnedAnimation(float deltaTime) {
		timePos += deltaTime;
		skinnedInfo;

		//Loop
		if (timePos > skinnedInfo->GetClipEndTime(clipName))
			timePos = 0.0f;

		skinnedInfo->GetFinalTransform(clipName, timePos, finalTransforms);
	}
};