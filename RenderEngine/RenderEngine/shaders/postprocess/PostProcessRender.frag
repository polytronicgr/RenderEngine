#version 430 core

out vec4 outColor;

layout (location=0) in vec2 texCoord;

//uniform sampler3D postProcessing_0;
uniform sampler2D postProcessing_0;
//uniform sampler2D postProcessing_1;

void main()
{
	//float r = texture(postProcessing_0, vec3(texCoord, 0.0)).r;
	//outColor = vec4(r, r, r, 1.0);
	outColor = texture(postProcessing_0, texCoord);
	//gl_FragDepth = texture(postProcessing_1, texCoord).r;
}