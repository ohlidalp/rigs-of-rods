/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

#include "RTSSNiceMetal.h"

#include <OgreLogManager.h>
#include <OgrePass.h>
#include <OgreShaderFFPRenderState.h>
#include <OgreShaderGenerator.h>
#include <OgreShaderProgram.h>
#include <OgreShaderProgramSet.h>
#include <OgreTextureUnitState.h>

using namespace Ogre;
using namespace Ogre::RTShader;
using namespace RoR;

/*static*/ const String NiceMetalSubRenderState::Type = "RoR/NiceMetal";

// Varyings private to this effect; the reserved custom range avoids clashing
// with anything the stock sub render states allocate.
static const int NICEMETAL_VERTEX_FLAGS  = Parameter::SPC_CUSTOM_CONTENT_BEGIN + 1;
static const int NICEMETAL_WORLD_NORMAL  = Parameter::SPC_CUSTOM_CONTENT_BEGIN + 2;
static const int NICEMETAL_WORLD_EYE_DIR = Parameter::SPC_CUSTOM_CONTENT_BEGIN + 3;

int NiceMetalSubRenderState::getExecutionOrder() const
{
    return FFP_TEXTURING;
}

void NiceMetalSubRenderState::copyFrom(const SubRenderState& rhs)
{
    const NiceMetalSubRenderState& other = static_cast<const NiceMetalSubRenderState&>(rhs);

    m_env_map_name = other.m_env_map_name;
    m_transparent = other.m_transparent;
    m_surface_only = other.m_surface_only;
}

bool NiceMetalSubRenderState::setParameter(const String& name, const String& value)
{
    if (name == "env_map")
    {
        m_env_map_name = value;
        return true;
    }
    else if (name == "transparent")
    {
        return StringConverter::parse(value, m_transparent);
    }
    else if (name == "surface_only")
    {
        return StringConverter::parse(value, m_surface_only);
    }

    return false;
}

bool NiceMetalSubRenderState::preAddToRenderState(const RenderState* renderState, Pass* srcPass, Pass* dstPass)
{
    // The texture units are sampled by this sub render state rather than by FFPTexturing,
    // which `RTSSManager::ApplyActorShading()` has already blacklisted them for.
    for (unsigned short i = 0; i < srcPass->getNumTextureUnitStates(); ++i)
    {
        const String& tu_name = srcPass->getTextureUnitState(i)->getName();
        if (tu_name == "Diffuse_Map")          { m_diffuse_sampler = i; }
        else if (tu_name == "Dmg_Diffuse_Map") { m_damage_sampler = i; }
        else if (tu_name == "Specular_Map" && !m_surface_only) { m_specular_sampler = i; }
    }

    if (m_diffuse_sampler < 0)
    {
        LogManager::getSingleton().logError(
            "RoR/NiceMetal: pass '" + srcPass->getName() + "' has no 'Diffuse_Map' texture unit");
        return false;
    }

    if (m_specular_sampler >= 0)
    {
        TextureUnitState* env_tus = dstPass->createTextureUnitState();
        env_tus->setTextureName(m_env_map_name, TEX_TYPE_CUBE_MAP);
        env_tus->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);
        m_env_sampler = dstPass->getNumTextureUnitStates() - 1;
    }

    return true;
}

bool NiceMetalSubRenderState::createCpuSubPrograms(ProgramSet* programSet)
{
    Program* vsProgram = programSet->getCpuProgram(GPT_VERTEX_PROGRAM);
    Function* vsMain = vsProgram->getEntryPointFunction();
    Program* psProgram = programSet->getCpuProgram(GPT_FRAGMENT_PROGRAM);
    Function* psMain = psProgram->getEntryPointFunction();

    psProgram->addDependency("SGXLib_NiceMetal");

    // --- texture coordinates ---------------------------------------------------
    auto vsOutTexcoord = vsMain->getOutputParameter(Parameter::SPC_TEXTURE_COORDINATE0, GCT_FLOAT2);
    ParameterPtr vsInTexcoord;
    if (!vsOutTexcoord)
    {
        vsInTexcoord = vsMain->resolveInputParameter(Parameter::SPC_TEXTURE_COORDINATE0, GCT_FLOAT2);
        vsOutTexcoord = vsMain->resolveOutputParameter(Parameter::SPC_TEXTURE_COORDINATE0, GCT_FLOAT2);
    }
    auto psInTexcoord = psMain->resolveInputParameter(vsOutTexcoord);

    // --- vertex colour ---------------------------------------------------------
    // Deliberately forwarded on a private varying: these are simulation flags, so
    // they must not reach the lighting stage the way tracked vertex colour would.
    auto vsInVertexFlags = vsMain->resolveInputParameter(Parameter::SPC_COLOR_DIFFUSE, GCT_FLOAT4);
    auto vsOutVertexFlags = vsMain->resolveOutputParameter(NICEMETAL_VERTEX_FLAGS, GCT_FLOAT4);
    auto psInVertexFlags = psMain->resolveInputParameter(vsOutVertexFlags);

    auto vstage = vsMain->getStage(FFP_VS_TEXTURING);
    if (vsInTexcoord)
    {
        vstage.assign(vsInTexcoord, vsOutTexcoord);
    }
    vstage.assign(vsInVertexFlags, vsOutVertexFlags);

    // --- world space normal and eye direction, for the reflection --------------
    ParameterPtr psInWorldNormal, psInWorldEyeDir;
    if (m_env_sampler >= 0)
    {
        auto vsInPosition = vsMain->resolveInputParameter(Parameter::SPC_POSITION_OBJECT_SPACE);
        auto vsInNormal = vsMain->resolveInputParameter(Parameter::SPC_NORMAL_OBJECT_SPACE);
        auto worldMatrix = vsProgram->resolveParameter(GpuProgramParameters::ACT_WORLD_MATRIX);
        auto worldITMatrix = vsProgram->resolveParameter(GpuProgramParameters::ACT_INVERSE_TRANSPOSE_WORLD_MATRIX);
        auto camPosition = vsProgram->resolveParameter(GpuProgramParameters::ACT_CAMERA_POSITION);

        auto vsOutWorldNormal = vsMain->resolveOutputParameter(NICEMETAL_WORLD_NORMAL, GCT_FLOAT3);
        auto vsOutWorldEyeDir = vsMain->resolveOutputParameter(NICEMETAL_WORLD_EYE_DIR, GCT_FLOAT3);

        vsProgram->addDependency("SGXLib_NiceMetal");
        vstage.callFunction("SGX_NiceMetal_WorldSpace",
            {In(worldMatrix), In(worldITMatrix), In(camPosition).xyz(), In(vsInNormal), In(vsInPosition),
             Out(vsOutWorldNormal), Out(vsOutWorldEyeDir)});

        psInWorldNormal = psMain->resolveInputParameter(vsOutWorldNormal);
        psInWorldEyeDir = psMain->resolveInputParameter(vsOutWorldEyeDir);
    }

    // --- fragment stage --------------------------------------------------------
    auto outDiffuse = psMain->resolveOutputParameter(Parameter::SPC_COLOR_DIFFUSE);
    auto outSpecular = psMain->resolveLocalParameter(Parameter::SPC_COLOR_SPECULAR);

    auto diffuseSampler = psProgram->resolveParameter(GCT_SAMPLER2D, "nicemetalDiffuseSampler", m_diffuse_sampler);
    auto diffuseSample = psMain->resolveLocalParameter(GCT_FLOAT4, "nicemetalDiffuseSample");
    auto damageSample = psMain->resolveLocalParameter(GCT_FLOAT4, "nicemetalDamageSample");
    auto surface = psMain->resolveLocalParameter(GCT_FLOAT4, "nicemetalSurface");

    auto fstage = psMain->getStage(FFP_PS_TEXTURING + 10);

    fstage.sampleTexture(diffuseSampler, psInTexcoord, diffuseSample);

    if (m_damage_sampler >= 0)
    {
        auto damageSampler = psProgram->resolveParameter(GCT_SAMPLER2D, "nicemetalDamageSampler", m_damage_sampler);
        fstage.sampleTexture(damageSampler, psInTexcoord, damageSample);
    }
    else
    {
        fstage.assign(diffuseSample, damageSample);
    }

    fstage.callFunction("SGX_NiceMetal_Surface", {In(diffuseSample), In(damageSample), In(psInVertexFlags), Out(surface)});

    if (m_specular_sampler >= 0)
    {
        auto specularSampler = psProgram->resolveParameter(GCT_SAMPLER2D, "nicemetalSpecularSampler", m_specular_sampler);
        auto envSampler = psProgram->resolveParameter(GCT_SAMPLERCUBE, "nicemetalEnvSampler", m_env_sampler);
        auto specularSample = psMain->resolveLocalParameter(GCT_FLOAT4, "nicemetalSpecularSample");
        auto mask = psMain->resolveLocalParameter(GCT_FLOAT1, "nicemetalMask");
        auto reflectDir = psMain->resolveLocalParameter(GCT_FLOAT3, "nicemetalReflectDir");
        auto reflectionSample = psMain->resolveLocalParameter(GCT_FLOAT4, "nicemetalReflectionSample");

        fstage.sampleTexture(specularSampler, psInTexcoord, specularSample);
        fstage.callFunction("SGX_NiceMetal_Mask", {In(specularSample), In(psInVertexFlags), Out(mask)});
        fstage.callFunction("SGX_NiceMetal_Reflect", {In(psInWorldNormal), In(psInWorldEyeDir), Out(reflectDir)});
        fstage.sampleTexture(envSampler, reflectDir, reflectionSample);
        fstage.callFunction("SGX_NiceMetal_Combine",
            {In(surface), In(mask), In(reflectionSample), InOut(outDiffuse), InOut(outSpecular)});
    }
    else
    {
        fstage.mul(In(outDiffuse).xyz(), In(surface).xyz(), Out(outDiffuse).xyz());
    }

    if (m_transparent)
    {
        fstage.assign(In(surface).w(), Out(outDiffuse).w());
    }
    else
    {
        fstage.assign(1.0f, Out(outDiffuse).w());
    }

    return true;
}

SubRenderState* NiceMetalSubRenderStateFactory::createInstanceImpl()
{
    return new NiceMetalSubRenderState;
}
