struct Vertex
{
  vec3 pos;
  vec3 normal;
  vec2 uv;
  vec4 color;
  vec4 joint0; 
  vec4 weight0;
  vec4 tangent;
  int materialIndex;
};

struct Triangle {
	Vertex vertices[3];
	vec3 normal;
	vec4 tangent;
	vec2 uv;
	vec4 color;
	int materialIndex;
};