#version 330 core

layout (location=0) out vec4 outColor;
layout (location=1) out vec4 outNormal;
layout (location=2) out vec4 outSpecular;
layout (location=3) out vec4 outEmissive;
layout (location=4) out vec4 outPos;

in vec3 color;
in vec3 pos;
in vec3 norm;
in vec2 texCoord;
in vec3 tangent;

uniform sampler2D colorTex;
uniform sampler2D emiTex;
uniform sampler2D specTex;
uniform sampler2D normalTex;

uniform bool computeShading;

uniform vec3 backgroundColor;

//Propiedades del objeto
vec3 Ka;
vec3 Kd;
vec3 Ks;
vec3 Ke;
vec3 N;
float alpha = 30.0;

// Point light
uniform vec3 Ia;
uniform vec3 Id;
uniform vec3 Is;
uniform vec3 lpos; 
uniform vec3 PLattenuation;

// Spot light
uniform vec3 SLIa;
uniform vec3 SLId;
uniform vec3 SLIs;
uniform vec3 SLpos;
uniform vec3 SLdir;
uniform float SLapperture;
uniform float SLm;
uniform vec3 SLattenuation;

// Directional light
uniform vec3 DLIa;
uniform vec3 DLId;
uniform vec3 DLIs;
uniform vec3 DLdir; 

vec3 shade();

void main()
{
	Ka = texture(colorTex, texCoord).rgb;
	Kd = texture(colorTex, texCoord).rgb;
	Ke = texture(emiTex, texCoord).rgb;
	Ks = texture(specTex, texCoord).rgb;

	// ===============================================
	// BUMP MAPPING
	// ===============================================
	vec3 normalizedTangent = normalize(tangent);

	vec3 normalizedNormal = normalize(norm);

	// Tagent must be perpendicular to normal
	normalizedTangent = normalize(normalizedTangent - (dot(normalizedTangent, normalizedNormal) * normalizedNormal));

	vec3 bitangent = normalize(cross(normalizedNormal, normalizedTangent));

	// Matrix from tangent space to world space (inverse of tangent basis)
	// since the vectors are normalized, the inverse equals the transpose
	// since glsl is column major, we just create a matrix with the vectors
	mat3 tangentBasis = mat3(normalizedTangent, bitangent, normalizedNormal);

	// Get the normal from the map and normalize it in the [-1, 1] range
	vec3 bumpMapNormal = (((texture(normalTex, texCoord).rgb) * 2) - 1);

	N = normalize(tangentBasis * bumpMapNormal);

	vec3 resultColor = Kd;

	if(computeShading)
	{
		resultColor = shade();
	}

	outColor = vec4(resultColor, 1.0);
	outNormal = vec4(N, 1.0);
	outSpecular = vec4(Ks, 1.0);
	outEmissive = vec4(Ke, 1.0);
	outPos = vec4(pos, 1.0);
}

vec3 shade()
{
	vec3 c = vec3(0.0);

	// ============================================
	// Point light
	// ============================================
	c = Ia * Ka;

	vec3 PL_L = lpos - pos;
	float PL_dist = length(PL_L);

	float PL_fatt = min(1/(PLattenuation.x + PLattenuation.y * PL_dist + PLattenuation.z * PL_dist * PL_dist), 1);

	vec3 L = normalize (PL_L);
	vec3 diffuse = Id * Kd * dot (L,N) * PL_fatt;
	c += clamp(diffuse, 0.0, 1.0);
	
	vec3 V = normalize (-pos);
	vec3 R = normalize (reflect (-L,N));
	float PL_factor = max (dot (R,V), 0.01);
	vec3 specular = Is*Ks*pow(PL_factor,alpha);
	c += clamp(specular, 0.0, 1.0);
	
	// ============================================
	// Spot light
	// ============================================
	c+= SLIa * Ka;

	vec3 SL_L = SLpos - pos;
	float SL_dist = length(SL_L);
	SL_L = normalize(SL_L);

	// Factor de atenuación
	float SL_fatt = min(1/(SLattenuation.x + SLattenuation.y * SL_dist + SLattenuation.z * SL_dist * SL_dist),1);

	vec3 normDir = normalize(SLdir);

	float cosApertura = cos(radians(SLapperture));
	float point = dot(normDir , -SL_L);

	float factor = max(pow(max(point - cosApertura, 0) / (1 - cosApertura), SLm), 0);

	c += clamp(factor * Kd * SLId * dot(SL_L, N) * SL_fatt, 0, 1);

	vec3 SL_R = normalize(reflect(-SL_L, N));
	float SL_Factor = max(dot(SL_R, V),0.01);
	c += SLIs * Ks * SL_Factor * factor * SL_fatt;

	// ============================================
	// Directional light
	// ============================================
	c += DLIa * Ka;

	vec3 D_L = DLdir; // normalized in the client (see StandarProgram::onDirectionalLightRender)
	
	// LUZ DIFUSA
	c += (DLId * Kd * max(dot(N,D_L), 0));

	vec3 DL_R = normalize(reflect(-D_L, N));
	vec3 DL_V = normalize(-pos);
	float DL_Factor = max(dot(R,DL_V), 0.01);
	c += DLIs * Ks * DL_Factor;

	c += Ke;

	float d = length(pos);
	float alfa = 1/exp(0.005*d*d);

	vec3 finalColor = mix(backgroundColor, c, alfa);//alfa*I+(1-alfa)*cf;

	return finalColor;
}
