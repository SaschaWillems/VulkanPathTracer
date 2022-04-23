// Wraps access to the unpacked data at the current barycentric position of a single triangle

Triangle unpackTriangle(uint index, int vertexSize) {
	Triangle tri;
	const uint triIndex = index * 3;

	ObjBuffers objResource = scene_desc.i[gl_InstanceCustomIndexEXT];
	Indices    indices     = Indices(objResource.indices);
	Vertices   vertices    = Vertices(objResource.vertices);

	// Unpack vertices
	// Data is packed as vec4 so we can map to the glTF vertex structure from the host side
	for (uint i = 0; i < 3; i++) {
		const uint offset = indices.i[triIndex + i] * (vertexSize / 16);

		vec4 d0 = vertices.v[offset + 0]; // pos.xyz, n.x
		vec4 d1 = vertices.v[offset + 1]; // n.yz, uv.xy
		vec4 d2 = vertices.v[offset + 2]; // color
		vec4 d3 = vertices.v[offset + 3]; // joint0
		vec4 d4 = vertices.v[offset + 4]; // weight0
		vec4 d5 = vertices.v[offset + 5]; // tangent
		vec4 d6 = vertices.v[offset + 6]; // material

		tri.vertices[i].pos = d0.xyz;
		tri.vertices[i].uv = d1.zw;
		tri.vertices[i].normal = vec3(d0.w, d1.x, d1.y);
		tri.vertices[i].color = vec4(d2.x, d2.y, d2.z, 1.0);
		tri.vertices[i].tangent = d5;
		tri.vertices[i].materialIndex = floatBitsToInt(d6.x);
	}

	// Calculate values at barycentric coordinates
	vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	tri.normal = normalize(tri.vertices[0].normal * barycentricCoords.x + tri.vertices[1].normal * barycentricCoords.y + tri.vertices[2].normal * barycentricCoords.z);
	tri.tangent = tri.vertices[0].tangent * barycentricCoords.x + tri.vertices[1].tangent * barycentricCoords.y + tri.vertices[2].tangent * barycentricCoords.z;
	tri.uv = tri.vertices[0].uv * barycentricCoords.x + tri.vertices[1].uv * barycentricCoords.y + tri.vertices[2].uv * barycentricCoords.z;
	tri.color = tri.vertices[0].color * barycentricCoords.x + tri.vertices[1].color * barycentricCoords.y + tri.vertices[2].color * barycentricCoords.z;

	// Fixed values
	tri.materialIndex = tri.vertices[0].materialIndex;

	return tri;
}