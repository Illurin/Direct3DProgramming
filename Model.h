#pragma once

#include "assimp/scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/mesh.h"
#include "assimp/texture.h"
#include "FrameResource.h"
#include "Texture.h"
#include "SkinnedData.h"

struct MaterialInfo {
	UINT diffuseMaps;
};

struct RenderInfo {
	std::vector<SkinnedVertex> vertices;
	std::vector<UINT> indices;
};

class Mesh {
public:
	std::vector<SkinnedVertex> vertices;
	std::vector<UINT> indices;
	UINT materialIndex;
	
	Mesh(std::vector<SkinnedVertex> vertices, std::vector<UINT> indices, UINT materialIndex) {
		this->vertices = vertices;
		this->indices = indices;
		this->materialIndex = materialIndex;
	}
};

class Model {
public:
	struct BoneData {
		UINT boneIndex[NUM_BONES_PER_VERTEX];
		float weights[NUM_BONES_PER_VERTEX];
		void Add(UINT boneID, float weight) {
			for (size_t i = 0; i < NUM_BONES_PER_VERTEX; i++) {
				if(weights[i] == 0.0f) {
					boneIndex[i] = boneID;
					weights[i] = weight;
					return;
				}
			}
			//insert error program
			MessageBox(NULL, L"bone index out of size", L"Error", NULL);
		}
	};
	struct BoneInfo {
		bool isSkinned = false;
		DirectX::XMFLOAT4X4 boneOffset;
		DirectX::XMFLOAT4X4 defaultOffset;
		int parentIndex;
	};

	Model(std::string path);
	std::vector<Mesh> meshes;
	std::vector<MaterialInfo> materials;
	std::vector<RenderInfo> renderInfo;
	std::vector<Texture> textureHasLoaded;

	void GetBoneMapping(std::unordered_map<std::string, UINT>& boneMapping) {
		boneMapping = this->boneMapping;
	}
	void GetBoneOffsets(std::vector<DirectX::XMFLOAT4X4>& boneOffsets) {
		for (size_t i = 0; i < boneInfo.size(); i++)
			boneOffsets.push_back(boneInfo[i].boneOffset);
	}
	void GetNodeOffsets(std::vector<DirectX::XMFLOAT4X4>& nodeOffsets) {
		for (size_t i = 0; i < boneInfo.size(); i++)
			nodeOffsets.push_back(boneInfo[i].defaultOffset);
	}
	void GetBoneHierarchy(std::vector<int>& boneHierarchy) {
		for (size_t i = 0; i < boneInfo.size(); i++)
			boneHierarchy.push_back(boneInfo[i].parentIndex);
	}
	void GetAnimations(std::unordered_map<std::string, AnimationClip>& animations) {
		animations.insert(this->animations.begin(), this->animations.end());
	}
	void LoadTexturesFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

private:
	std::string directory;
	std::vector<std::string> texturePath;

	void ProcessNode(const aiScene* scene, aiNode* node);
	Mesh ProcessMesh(const aiScene* scene, aiMesh* mesh);
	UINT SetupMaterial(std::vector<UINT> diffuseMaps);
	void SetupRenderInfo();
	std::vector<UINT> LoadMaterialTextures(aiMaterial* mat, aiTextureType type);

	//Bone/Animation Information
	std::vector<BoneInfo> boneInfo;
	std::unordered_map<std::string, UINT> boneMapping;
	std::unordered_map<std::string, AnimationClip> animations;

	void LoadBones(const aiMesh* mesh, std::vector<BoneData>& bones);
	void ReadNodeHierarchy(const aiNode* node, int parentIndex);
	void LoadAnimations(const aiScene* scene);
};

bool CompareMaterial(MaterialInfo dest, MaterialInfo source);
wchar_t* StringToWideChar(const std::string& str);