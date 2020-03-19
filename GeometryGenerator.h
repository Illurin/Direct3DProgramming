#pragma once
#include "d3dUtil.h"

class GeometryGenerator {
public:
	struct MeshData {
		std::vector<Vertex> vertices;
		std::vector<UINT> indices;
	};

	MeshData CreatePlane(float width, float depth, UINT texCountX, UINT texCountY);
	MeshData CreateGeosphere(float radius, UINT numSubdivisions);

	void Subdivide(MeshData& meshData);
	Vertex GetMidPoint(const Vertex& v1, const Vertex& v2);
};