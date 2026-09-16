// This file is part of the Caelum project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution.

// Helpers shared by the sky dome and haze shaders, kept together so the two fragment
// programs cannot drift apart.

float bias(float b, float x)
{
	return pow(x, log(b) / log(0.5));
}

vec4 sunlightInscatter(
    vec4 sunColour,
    float absorption,
    float incidenceAngleCos,
    float sunlightScatteringFactor)
{
	float scatteredSunlight = bias(sunlightScatteringFactor * 0.5, incidenceAngleCos);

	sunColour = sunColour * (1.0 - absorption) * vec4(0.9, 0.5, 0.09, 1.0);

	return sunColour * scatteredSunlight;
}

float fogExp(float z, float density)
{
	return 1.0 - clamp(pow(2.71828, -z * density), 0.0, 1.0);
}
