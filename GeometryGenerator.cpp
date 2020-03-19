#include "GeometryGenerator.h"

GeometryGenerator::MeshData GeometryGenerator::CreatePlane(float width, float depth, UINT texCountX, UINT texCountY) {
	float halfWidth = 0.5f * width;
	float halfDepth = 0.5f * depth;
	float texCoordX = 1.0f * static_cast<float>(texCountX);
	float texCoordY = 1.0f * static_cast<float>(texCountY);

	std::vector<Vertex> vertices(4);
	vertices[0].position = { -halfWidth, 0, -halfDepth };
	vertices[1].position = { halfWidth, 0, -halfDepth };
	vertices[2].position = { -halfWidth, 0, halfDepth };
	vertices[3].position = { halfWidth, 0, halfDepth };
	vertices[0].texCoord = { 0.0f, 0.0f };
	vertices[1].texCoord = { texCoordX, 0.0f };
	vertices[2].texCoord = { 0.0f, texCoordY };
	vertices[3].texCoord = { texCoordX, texCoordY };
	vertices[0].normal = { 0.0f, 1.0f, 0.0f };
	vertices[1].normal = { 0.0f, 1.0f, 0.0f };
	vertices[2].normal = { 0.0f, 1.0f, 0.0f };
	vertices[3].normal = { 0.0f, 1.0f, 0.0f };

	std::vector<UINT> indices = {
		0, 1, 3, 0, 3, 2
	};

	MeshData meshData;
	meshData.vertices = vertices;
	meshData.indices = indices;

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateGeosphere(float radius, UINT numSubdivisions) {
	MeshData meshData;
	numSubdivisions = (numSubdivisions > 6) ? 6 : numSubdivisions;

	//对一个正二十面体进行曲面细分
	const float X = 0.525731f;
	const float Z = 0.850651f;
	std::vector<DirectX::XMFLOAT3> pos = {
		DirectX::XMFLOAT3(-X, 0.0f, Z),  DirectX::XMFLOAT3(X, 0.0f, Z),
		DirectX::XMFLOAT3(-X, 0.0f, -Z), DirectX::XMFLOAT3(X, 0.0f, -Z),
		DirectX::XMFLOAT3(0.0f, Z, X),   DirectX::XMFLOAT3(0.0f, Z, -X),
		DirectX::XMFLOAT3(0.0f, -Z, X),  DirectX::XMFLOAT3(0.0f, -Z, -X),
		DirectX::XMFLOAT3(Z, X, 0.0f),   DirectX::XMFLOAT3(-Z, X, 0.0f),
		DirectX::XMFLOAT3(Z, -X, 0.0f),  DirectX::XMFLOAT3(-Z, -X, 0.0f)
	};
	std::vector<UINT> indices = {
		1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
		1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
		3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
		10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
	};

	meshData.vertices.resize(12);
	meshData.indices.insert(meshData.indices.end(), indices.begin(), indices.end());

	//细分
	for (UINT i = 0; i < 12; i++)
		meshData.vertices[i].position = pos[i];
	for (UINT i = 0; i < numSubdivisions; i++)
		Subdivide(meshData);

	//为每个顶点计算纹理坐标
	for (UINT i = 0; i < meshData.vertices.size(); i++)
	{
		//将每个顶点投射到球面上
		DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&meshData.vertices[i].position));
		DirectX::XMVECTOR p = DirectX::XMVectorScale(n, radius);

		DirectX::XMStoreFloat3(&meshData.vertices[i].position, p);
		DirectX::XMStoreFloat3(&meshData.vertices[i].normal, n);

		//根据球面坐标推导纹理坐标
		float theta = atan2f/*反正切*/(meshData.vertices[i].position.z, meshData.vertices[i].position.x);
		if (theta < 0.0f)
			theta += DirectX::XM_PI;
		float phi = acosf/*反余弦*/(meshData.vertices[i].position.y / radius);

		meshData.vertices[i].texCoord.x = theta / DirectX::XM_2PI;
		meshData.vertices[i].texCoord.y = phi / DirectX::XM_PI;
	}
	return meshData;
}

Vertex GeometryGenerator::GetMidPoint(const Vertex& v1, const Vertex& v2) {
	DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&v1.position);
	DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&v2.position);

	DirectX::XMVECTOR n1 = DirectX::XMLoadFloat3(&v1.normal);
	DirectX::XMVECTOR n2 = DirectX::XMLoadFloat3(&v2.normal);

	DirectX::XMVECTOR tex1 = DirectX::XMLoadFloat2(&v1.texCoord);
	DirectX::XMVECTOR tex2 = DirectX::XMLoadFloat2(&v2.texCoord);

	DirectX::XMVECTOR pos = DirectX::XMVectorScale(DirectX::XMVectorAdd(p1, p2), 0.5f);
	DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(n1, n2), 0.5f));
	DirectX::XMVECTOR texCoord = DirectX::XMVectorScale(DirectX::XMVectorAdd(tex1, tex2), 0.5f);

	Vertex vertex;
	DirectX::XMStoreFloat3(&vertex.position, pos);
	DirectX::XMStoreFloat3(&vertex.normal, normal);
	DirectX::XMStoreFloat2(&vertex.texCoord, texCoord);
	return vertex;
}

void GeometryGenerator::Subdivide(MeshData& meshData) {
	MeshData inputCopy = meshData;

	meshData.vertices.resize(0);
	meshData.indices.resize(0);

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2

	UINT numTris = (UINT)inputCopy.indices.size() / 3;
	for (UINT i = 0; i < numTris; i++)
	{
		Vertex v0 = inputCopy.vertices[inputCopy.indices[i * 3]];
		Vertex v1 = inputCopy.vertices[inputCopy.indices[i * 3 + 1]];
		Vertex v2 = inputCopy.vertices[inputCopy.indices[i * 3 + 2]];

		//生成三个顶点的中点
		Vertex m0 = GetMidPoint(v0, v1);
		Vertex m1 = GetMidPoint(v1, v2);
		Vertex m2 = GetMidPoint(v0, v2);

		//添加新的几何体
		meshData.vertices.push_back(v0);		//0
		meshData.vertices.push_back(v1);		//1
		meshData.vertices.push_back(v2);		//2
		meshData.vertices.push_back(m0);		//3
		meshData.vertices.push_back(m1);		//4
		meshData.vertices.push_back(m2);		//5

		meshData.indices.push_back(i * 6 + 0);
		meshData.indices.push_back(i * 6 + 3);
		meshData.indices.push_back(i * 6 + 5);

		meshData.indices.push_back(i * 6 + 3);
		meshData.indices.push_back(i * 6 + 4);
		meshData.indices.push_back(i * 6 + 5);

		meshData.indices.push_back(i * 6 + 5);
		meshData.indices.push_back(i * 6 + 4);
		meshData.indices.push_back(i * 6 + 2);

		meshData.indices.push_back(i * 6 + 3);
		meshData.indices.push_back(i * 6 + 1);
		meshData.indices.push_back(i * 6 + 4);
	}
}