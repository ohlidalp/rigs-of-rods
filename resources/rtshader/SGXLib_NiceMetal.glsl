//-----------------------------------------------------------------------------
// Program Name: SGXLib_NiceMetal
// Program Desc: Rigs of Rods vehicle surface shading.
// Program Type: Pixel shader
// Language: GLSL / HLSL (unified shader syntax)
//
// Reproduces the look of the legacy 'nicemetal.cg' shader, where the vertex
// colour is not a colour at all but a pair of per-node simulation flags written
// by FlexBody::updateBlend(): alpha = surface is damaged, blue = surface is wet.
// Wet surfaces get shinier, damaged ones get duller and fade to the damage
// texture. The specular map is a reflectivity mask selecting between the lit
// diffuse and a mirror highlight -- it is NOT a conventional gloss map.
//-----------------------------------------------------------------------------

// Damage/wetness weights, kept identical to the original Cg shader so that
// existing content keeps its authored appearance.
#define NICEMETAL_WET_DARKEN  0.333333
#define NICEMETAL_WET_SHINE   0.333333
#define NICEMETAL_DAMAGE_DULL 0.5

void SGX_NiceMetal_Surface(in vec4 diffuseSample,
                           in vec4 damageSample,
                           in vec4 vertexFlags,
                           out vec4 surface)
{
	surface = mix(diffuseSample, damageSample, vertexFlags.a);
	surface.rgb *= 1.0 - vertexFlags.b * NICEMETAL_WET_DARKEN;
}

// Deliberately per-channel and unclamped, as the original was: a coloured specular map
// tints the blend, and a wet surface pushes the weight above 1 so that mix() extrapolates
// past the highlight and brightens the reflection.
void SGX_NiceMetal_Mask(in vec4 specularSample,
                        in vec4 vertexFlags,
                        out vec3 mask)
{
	mask = specularSample.rgb
	     + vertexFlags.b * NICEMETAL_WET_SHINE
	     - vertexFlags.a * NICEMETAL_DAMAGE_DULL;
}

void SGX_NiceMetal_WorldSpace(in mat4 mWorld,
                              in mat4 mWorldIT,
                              in vec3 vCamPos,
                              in vec3 vNormal,
                              in vec4 vPos,
                              out vec3 worldNormal,
                              out vec3 worldEyeDir)
{
	worldNormal = mul(mWorldIT, vec4(vNormal, 0.0)).xyz;
	worldEyeDir = mul(mWorld, vPos).xyz - vCamPos;
}

void SGX_NiceMetal_Reflect(in vec3 worldNormal,
                           in vec3 worldEyeDir,
                           out vec3 reflectDir)
{
	reflectDir = reflect(normalize(worldEyeDir), normalize(worldNormal));
	// Matches OGRE's own FFP_GenerateTexCoord_EnvMap_Reflect handedness fixup.
	reflectDir.z *= -1.0;
}

// Combines everything into the FFP colour slots. 'litDiffuse' arrives holding
// ambient + diffuse lighting, 'specular' the specular term that FFPColour will
// add back in at FFP_PS_COLOUR_END -- so the reflection is written there to ride
// along on that same add, replacing the legacy additive second pass.
void SGX_NiceMetal_Combine(in vec4 surface,
                           in vec3 mask,
                           in vec4 reflectionSample,
                           inout vec4 litDiffuse,
                           inout vec4 specular)
{
	vec3 base = litDiffuse.rgb * surface.rgb;
	litDiffuse.rgb = mix(base, specular.rgb, mask);
	specular.rgb = reflectionSample.rgb * mask;
}
