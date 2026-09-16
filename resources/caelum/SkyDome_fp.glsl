// This file is part of the Caelum project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution.

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>
#include "SkyDome_common.glsl"

OGRE_UNIFORMS(
    SAMPLER2D(gradientsMap, 0);
    SAMPLER1D(atmRelativeDepth, 1);
    uniform vec4 hazeColour;
    uniform float offset;
)

MAIN_PARAMETERS
    IN(vec4 col, COLOR)
    IN(vec2 uv, TEXCOORD0)
    IN(float incidenceAngleCos, TEXCOORD1)
    IN(float y, TEXCOORD2)
    IN(vec3 normal, TEXCOORD3)
MAIN_DECLARATION
{
	vec4 sunColour = vec4(3.0, 3.0, 3.0, 1.0);

#ifdef HAZE
	float fogDensity = 15.0;
	// Haze amount calculation
	float invHazeHeight = 100.0;
	float haze = fogExp(pow(clamp(1.0 - normal.y, 0.0, 1.0), invHazeHeight), fogDensity);
#endif // HAZE

	// Pass the colour
	vec4 outCol = texture2D(gradientsMap, uv + vec2(offset, 0.0)) * col;

	// Sunlight inscatter
	if (incidenceAngleCos > 0.0)
	{
		float sunlightScatteringFactor = 0.05;
		float sunlightScatteringLossFactor = 0.1;
		float atmLightAbsorptionFactor = 0.1;

		outCol.rgb += sunlightInscatter(
                sunColour,
                clamp(atmLightAbsorptionFactor * (1.0 - texture1D(atmRelativeDepth, y).r), 0.0, 1.0),
                clamp(incidenceAngleCos, 0.0, 1.0),
                sunlightScatteringFactor).rgb * (1.0 - sunlightScatteringLossFactor);
	}

#ifdef HAZE
	// Haze pass. A local copy because the original wrote to its own uniform, which is
	// not assignable here.
	vec4 hazeCol = hazeColour;
	hazeCol.a = 1.0;
	outCol = outCol * (1.0 - haze) + hazeCol * haze;
#endif // HAZE

	gl_FragColor = outCol;
}
