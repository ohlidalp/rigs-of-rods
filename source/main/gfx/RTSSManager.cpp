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

#include "RTSSManager.h"

#include "Actor.h"
#include "CameraManager.h"
#include "GfxScene.h"
#include "RTSSNiceMetal.h"

#include <Ogre.h>
#include <OgreCodec.h>
#include <Terrain/OgreTerrain.h>
#include <Overlay/OgreOverlayManager.h>
#include <Overlay/OgreOverlayContainer.h>
#include <Overlay/OgreOverlay.h>
#include <OgreMaterialManager.h>
#include <OGRE/RTShaderSystem/OgreRTShaderSystem.h>

using namespace Ogre;
using namespace RoR;

RTSSManager::RTSSManager()
{
    // The factory outlives every RTSSManager - one is constructed per terrain load,
    // but RTSS keeps sub render state factories for the lifetime of the process.
    static NiceMetalSubRenderStateFactory nicemetal_factory;
    static bool nicemetal_factory_registered = false;
    if (!nicemetal_factory_registered)
    {
        Ogre::RTShader::ShaderGenerator::getSingleton().addSubRenderStateFactory(&nicemetal_factory);
        nicemetal_factory_registered = true;
    }
}

RTSSManager::~RTSSManager()
{
}

void RTSSManager::SetupRTSS()
{
    // RTSS sizes its light arrays from the number of lights in the frustum, which is
    // unbounded - one vehicle's flares alone push it past 70. OGRE never binds more than
    // OGRE_MAX_SIMULTANEOUS_LIGHTS to a pass, so everything above that is dead weight, and
    // on D3D9 the oversized arrays overrun the ps_3_0 constant register budget and the
    // shader fails to assemble.
    auto* scheme_render_state = Ogre::RTShader::ShaderGenerator::getSingleton()
        .getRenderState(Ogre::MSN_SHADERGEN);
    scheme_render_state->setLightCountAutoUpdate(false);
    scheme_render_state->setLightCount(OGRE_MAX_SIMULTANEOUS_LIGHTS);

    // RTSS PSSM3
    if (App::gfx_shadow_type->getEnum<GfxShadowType>() == GfxShadowType::PSSM)
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTechnique(SHADOWTYPE_TEXTURE_MODULATIVE_INTEGRATED);
        App::GetGfxScene()->GetSceneManager()->setShadowFarDistance(350);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_DIRECTIONAL, 3);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_POINT, 3);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_SPOTLIGHT, 3);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureSettings(2048, 3, PF_DEPTH16);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureSelfShadow(true);


        pssmSetup = new PSSMShadowCameraSetup();
        pssmSetup->calculateSplitPoints(3, 1, 500, 1);
        pssmSetup->setSplitPadding(App::GetCameraManager()->GetCamera()->getNearClipDistance());
        pssmSetup->setOptimalAdjustFactor(0, 2);
        pssmSetup->setOptimalAdjustFactor(1, 1);
        pssmSetup->setOptimalAdjustFactor(2, 0.5);

        auto* mShaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
        auto* schemRenderState = mShaderGenerator->getRenderState(Ogre::MSN_SHADERGEN);

        App::GetGfxScene()->GetSceneManager()->setShadowCameraSetup(ShadowCameraSetupPtr(pssmSetup));
        auto subRenderState = mShaderGenerator->createSubRenderState(RTShader::SRS_INTEGRATED_PSSM3);
        schemRenderState->addTemplateSubRenderState(subRenderState);
    }
}

void RTSSManager::EnableRTSS(const MaterialPtr& mat)
{
    Ogre::RTShader::ShaderGenerator* mShaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    mShaderGenerator->createShaderBasedTechnique(*mat, Ogre::MaterialManager::DEFAULT_SCHEME_NAME, Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
}

void RTSSManager::ApplyActorShading(const MaterialPtr& mat, bool transparent, bool is_flexbody)
{
    Pass* pass = mat->getTechnique("BaseTechnique")->getPass("BaseRender");

    // A plain diffuse-only material needs nothing special - FFP texturing, which RTSS
    // turns into a per-pixel shader anyway, already does the right thing.
    const bool has_specular = pass->getTextureUnitState("Specular_Map") != nullptr;
    if (!has_specular && !pass->getTextureUnitState("Dmg_Diffuse_Map"))
    {
        return;
    }

    // The per-pass render state the sub render states attach to only exists once the material
    // has a shader-generated technique; until then it is merely queued for lazy registration
    // by SGTechniqueResolverListener at first render, and the lookup below returns null.
    this->EnableRTSS(mat);

    auto* render_state = Ogre::RTShader::ShaderGenerator::getSingleton()
        .getRenderState(Ogre::MSN_SHADERGEN, *mat, 0);
    if (!render_state)
    {
        Ogre::LogManager::getSingleton().logError(
            "RTSSManager: no shader render state for material '" + mat->getName() + "', leaving it unshaded");
        return;
    }

    if (has_specular && App::gfx_actor_shading->getEnum<GfxActorShading>() == GfxActorShading::PBR)
    {
        this->ApplyPbrShading(render_state, pass, is_flexbody);
    }
    else
    {
        this->ApplyClassicShading(render_state, pass, transparent, is_flexbody);
    }

    Ogre::RTShader::ShaderGenerator::getSingleton().invalidateMaterial(Ogre::MSN_SHADERGEN, *mat);
}

void RTSSManager::ApplyClassicShading(Ogre::RTShader::RenderState* render_state, Pass* pass, bool transparent, bool is_flexbody)
{
    // The sub render state samples these itself, so keep FFPTexturing off them. This is
    // also what keeps the vertex colour out of the lighting stage: it carries damage and
    // wetness flags rather than a colour, so it must not tint the surface.
    for (TextureUnitState* tus : pass->getTextureUnitStates())
    {
        Ogre::RTShader::ShaderGenerator::_markNonFFP(tus);
    }

    auto* srs = Ogre::RTShader::ShaderGenerator::getSingleton().createSubRenderState(NiceMetalSubRenderState::Type);
    srs->setParameter("transparent", transparent ? "true" : "false");
    srs->setParameter("is_flexbody", is_flexbody ? "true" : "false");
    render_state->addTemplateSubRenderState(srs);
}

/// Cook-Torrance wants roughness in the green channel and metalness in blue, but a legacy
/// specular map is a single reflectivity mask - handing it over unchanged makes reflective
/// areas read as fully rough. This rewrites the mask into both channels, inverted for
/// roughness, so that what the artist marked as reflective comes out as smooth metal.
static TexturePtr DeriveMetalRoughnessTexture(const TexturePtr& spec_tex)
{
    const String mr_name = spec_tex->getName() + "/RoR_metalrough";
    TexturePtr existing = TextureManager::getSingleton().getByName(mr_name, spec_tex->getGroup());
    if (existing)
    {
        return existing;
    }

    // Specular maps are often DXT compressed, which cannot be sampled on the CPU, so ask the
    // DDS codec to decode rather than hand the compressed blocks straight to the GPU.
    Image spec;
    Codec* dds_codec = Codec::getCodec("dds");
    if (dds_codec) { dds_codec->setParameter("decode_enforce", "true"); }
    try
    {
        spec.load(spec_tex->getName(), spec_tex->getGroup());
    }
    catch (Ogre::Exception& e)
    {
        if (dds_codec) { dds_codec->setParameter("decode_enforce", "false"); }
        LogManager::getSingleton().logError(
            "RTSSManager: cannot derive metal-roughness from '" + spec_tex->getName() + "': " + e.getDescription());
        return TexturePtr();
    }
    if (dds_codec) { dds_codec->setParameter("decode_enforce", "false"); }

    const uint32 width = spec.getWidth();
    const uint32 height = spec.getHeight();
    std::vector<uint8> pixels(size_t(width) * height * 4);
    for (uint32 y = 0; y < height; ++y)
    {
        for (uint32 x = 0; x < width; ++x)
        {
            const float mask = spec.getColourAt(x, y, 0).r;
            uint8* texel = &pixels[(size_t(y) * width + x) * 4];
            texel[0] = 255;                                             // occlusion, unused
            texel[1] = static_cast<uint8>((1.0f - mask) * 255.0f);      // roughness
            texel[2] = static_cast<uint8>(mask * 255.0f);               // metalness
            texel[3] = 255;
        }
    }

    TexturePtr mr_tex = TextureManager::getSingleton().createManual(
        mr_name, spec_tex->getGroup(), TEX_TYPE_2D, width, height, MIP_DEFAULT, PF_BYTE_RGBA);
    PixelBox box(width, height, 1, PF_BYTE_RGBA, pixels.data());
    mr_tex->getBuffer()->blitFromMemory(box);
    return mr_tex;
}

void RTSSManager::ApplyPbrShading(Ogre::RTShader::RenderState* render_state, Pass* pass, bool is_flexbody)
{
    const TexturePtr& spec_tex = pass->getTextureUnitState("Specular_Map")->_getTexturePtr();
    TexturePtr mr_tex = (spec_tex) ? DeriveMetalRoughnessTexture(spec_tex) : TexturePtr();
    const String metal_roughness_name = (mr_tex) ? mr_tex->getName() : String();

    for (TextureUnitState* tus : pass->getTextureUnitStates())
    {
        Ogre::RTShader::ShaderGenerator::_markNonFFP(tus);
    }

    auto* shader_gen = Ogre::RTShader::ShaderGenerator::getSingletonPtr();

    // Reuse the classic surface blend so that damage textures and the wetness darkening
    // survive; Cook-Torrance then treats its result as the base colour.
    auto* surface = shader_gen->createSubRenderState(NiceMetalSubRenderState::Type);
    surface->setParameter("surface_only", "true");
    surface->setParameter("is_flexbody", is_flexbody ? "true" : "false");
    render_state->addTemplateSubRenderState(surface);

    auto* cook_torrance = shader_gen->createSubRenderState(Ogre::RTShader::SRS_COOK_TORRANCE_LIGHTING);
    if (!metal_roughness_name.empty())
    {
        // Without a map Cook-Torrance falls back to the pass's specular colour, which at
        // least keeps the vehicle lit rather than leaving it black.
        cook_torrance->setParameter("texture", metal_roughness_name);
    }
    render_state->addTemplateSubRenderState(cook_torrance);

    auto* ibl = shader_gen->createSubRenderState(Ogre::RTShader::SRS_IMAGE_BASED_LIGHTING);
    ibl->setParameter("texture", "EnvironmentTexture");
    render_state->addTemplateSubRenderState(ibl);
}