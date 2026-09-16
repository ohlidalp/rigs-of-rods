// This file is part of the Caelum project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution.

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>

OGRE_UNIFORMS(
    uniform float lightAbsorption;
    uniform mat4 worldViewProj;
    uniform vec3 sunDirection;
)

MAIN_PARAMETERS
    IN(vec4 position, POSITION)
    IN(vec4 normal, NORMAL)
    IN(vec2 uv, TEXCOORD0)
    OUT(vec4 oCol, COLOR)
    OUT(vec2 oUv, TEXCOORD0)
    OUT(float incidenceAngleCos, TEXCOORD1)
    OUT(float y, TEXCOORD2)
    OUT(vec3 oNormal, TEXCOORD3)
MAIN_DECLARATION
{
	vec3 sunDir = normalize(sunDirection);
	// Normalising the full 4-vector, as the original did - the incoming normal has w = 1,
	// so this is not the same as normalising just the xyz.
	vec4 nrm = normalize(normal);
	float cosine = dot(-sunDir, nrm.xyz);
	incidenceAngleCos = -cosine;

	y = -sunDir.y;

	gl_Position = mul(worldViewProj, position);
	oCol = vec4(1.0, 1.0, 1.0, 1.0);
	oUv = uv;
	oNormal = -nrm.xyz;
}
