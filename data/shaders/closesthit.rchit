#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable

#include "includes/geometryTypes.glsl"

struct ObjBuffers
{
	uint64_t vertices;
	uint64_t indices;
	uint64_t materials;
};

#include "includes/material.glsl"

layout(buffer_reference, scalar) buffer Vertices {vec4 v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices {uint i[]; }; // Triangle indices
layout(buffer_reference, scalar) buffer Materials {Material m[]; }; // Array of all materials on an object

#include "includes/random.glsl"
#include "includes/raypayload.glsl"
#include "includes/ubo.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };
layout(binding = 4, set = 0) buffer _scene_desc { ObjBuffers i[]; } scene_desc;
layout(binding = 5, set = 0) uniform sampler2D[] textures;

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec3 attribs;

RayPayload scatter(Material material, vec3 vertexColor, vec3 direction, vec3 normal, vec2 uv, float t, uint seed)
{
	RayPayload payload;
	payload.distance = t;

	if (material.type == 0) {
		// Lambertian
		vec4 color = material.baseColorTextureIndex > -1 ? texture(textures[material.baseColorTextureIndex], uv) : vec4(1.0);
		payload.color = color.rgb * vertexColor * material.baseColor.rgb;
		payload.scatterDir = normal + RandomInUnitSphere(seed);
		payload.doScatter = true;
	}
	if (material.type == 1) {
		// Light source
		const vec3 lightColor = vec3(255.0) / 255.0;
		payload.color = lightColor * 15.0;
		payload.scatterDir = vec3(1.0, 0.0, 0.0);
		payload.doScatter = false;
	}
	payload.randomSeed = seed;
	return payload;
}

#include "includes/geometry.glsl"

void main()
{
	Triangle tri = unpackTriangle(gl_PrimitiveID, ubo.vertexSize);

	ObjBuffers objResource = scene_desc.i[gl_InstanceCustomIndexEXT];
	Materials materials = Materials(objResource.materials);
	Material mat = materials.m[tri.materialIndex];
	vec3 normal = tri.normal;
	if (mat.normalTextureIndex > -1) {
		// Apply normal mapping
		if (length(tri.tangent) != 0) {
			vec3 T = normalize(tri.tangent.xyz);
			vec3 B = cross(tri.normal, tri.tangent.xyz) * tri.tangent.w;
			vec3 N = normalize(tri.normal);
			mat3 TBN = mat3(T, B, N);
			normal = TBN * normalize(texture(textures[mat.normalTextureIndex], tri.uv).rgb * 2.0 - vec3(1.0));
		}
	}
	normal = tri.normal;
	rayPayload = scatter(mat, tri.color.rgb, gl_WorldRayDirectionEXT, normal, tri.uv, gl_HitTEXT, rayPayload.randomSeed);
}
