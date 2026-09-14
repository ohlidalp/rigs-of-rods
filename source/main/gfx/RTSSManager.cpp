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

void RTSSManager::ApplyActorShading(const MaterialPtr& mat, bool transparent)
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
        this->ApplyPbrShading(render_state, pass);
    }
    else
    {
        this->ApplyClassicShading(render_state, pass, transparent);
    }

    Ogre::RTShader::ShaderGenerator::getSingleton().invalidateMaterial(Ogre::MSN_SHADERGEN, *mat);
}

void RTSSManager::ApplyClassicShading(Ogre::RTShader::RenderState* render_state, Pass* pass, bool transparent)
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
    render_state->addTemplateSubRenderState(srs);
}

void RTSSManager::ApplyPbrShading(Ogre::RTShader::RenderState* render_state, Pass* pass)
{
    // NOTE: CookTorrance reads roughness from the green channel and metalness from the
    // blue one, whereas a legacy specular map is a single reflectivity mask. Feeding it
    // in directly therefore makes reflective areas read as rough. Deciding how to map
    // the old masks onto metal-roughness is still open.
    const String spec_tex_name = pass->getTextureUnitState("Specular_Map")->getTextureName();

    for (TextureUnitState* tus : pass->getTextureUnitStates())
    {
        Ogre::RTShader::ShaderGenerator::_markNonFFP(tus);
    }

    auto* shader_gen = Ogre::RTShader::ShaderGenerator::getSingletonPtr();

    // Reuse the classic surface blend so that damage textures and the wetness darkening
    // survive; Cook-Torrance then treats its result as the base colour.
    auto* surface = shader_gen->createSubRenderState(NiceMetalSubRenderState::Type);
    surface->setParameter("surface_only", "true");
    render_state->addTemplateSubRenderState(surface);

    auto* cook_torrance = shader_gen->createSubRenderState(Ogre::RTShader::SRS_COOK_TORRANCE_LIGHTING);
    cook_torrance->setParameter("texture", spec_tex_name);
    render_state->addTemplateSubRenderState(cook_torrance);

    auto* ibl = shader_gen->createSubRenderState(Ogre::RTShader::SRS_IMAGE_BASED_LIGHTING);
    ibl->setParameter("texture", "EnvironmentTexture");
    render_state->addTemplateSubRenderState(ibl);
}