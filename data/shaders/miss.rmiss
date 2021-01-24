#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "includes/raypayload.glsl"
#include "includes/ubo.glsl"

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };

// The miss shader is used to render a simple sky background gradient (if enabled)

void main()
{
	if (ubo.sky == 1) {
		const float t = 0.5 * (normalize(gl_WorldRayDirectionEXT).y + 1.0);
		const vec3 gradientStart = vec3(0.5, 0.6, 1.0);
		const vec3 gradientEnd = vec3(1.0);
		const vec3 skyColor = mix(gradientEnd, gradientStart, t);
		rayPayload.color = skyColor * ubo.skyIntensity;
	} else {
		rayPayload.color = vec3(0.0);
	}
	rayPayload.distance = -1.0;
}