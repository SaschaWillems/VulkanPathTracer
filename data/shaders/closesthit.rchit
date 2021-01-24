#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require

#include "includes/random.glsl"
#include "includes/raypayload.glsl"
#include "includes/ubo.glsl"
#include "includes/material.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };
layout(binding = 4, set = 0) readonly buffer Vertices { vec4 vertices[]; };
layout(binding = 5, set = 0) readonly buffer Indices { uint indices[]; };
layout(binding = 6, set = 0) readonly buffer Materials { Material materials[]; };
layout(binding = 7, set = 0) uniform sampler2D[] textures;

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
	Material mat = materials[tri.materialIndex];
	vec3 normal = tri.normal;
	if (mat.normalTextureIndex > -1) {
		// Apply normal mapping
		vec3 T = normalize(tri.tangent.xyz);
		vec3 B = cross(tri.normal, tri.tangent.xyz) * tri.tangent.w;
		vec3 N = normalize(tri.normal);
		mat3 TBN = mat3(T, B, N);
		normal = TBN * normalize(texture(textures[mat.normalTextureIndex], tri.uv).rgb * 2.0 - vec3(1.0));
	}
	rayPayload = scatter(mat, tri.color.rgb, gl_WorldRayDirectionEXT, normal, tri.uv, gl_HitTEXT, rayPayload.randomSeed);
}
