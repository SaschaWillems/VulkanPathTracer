struct RayPayload {
	vec3 color;
	float distance;
	vec3 scatterDir;
	bool doScatter; 
	uint randomSeed;
};