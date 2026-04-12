/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2016 Petr Ohlidal

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

#include "HydraxWater.h"

#include "Actor.h"
#include "AppContext.h"
#include "CameraManager.h"
#include "GameContext.h"
#include "GfxScene.h"
#include "SkyManager.h"
#include "Sun.h"
#include "Terrain.h"

using namespace Ogre;
using namespace RoR;

// HydraxWater
HydraxWater::HydraxWater(float water_height, Ogre::String conf_file):
    waternoise(0)
    , mHydrax(0)
    , waterHeight(water_height)
    , waveHeight(water_height)
    , CurrentConfigFile(conf_file)
{
    App::GetCameraManager()->GetCamera()->setNearClipDistance(0.1f);

    InitHydrax();
}

HydraxWater::~HydraxWater()
{
    mHydrax->remove();
    mHydrax = nullptr;
}

void HydraxWater::InitHydrax()
{
    mHydrax = new Hydrax::Hydrax(App::GetGfxScene()->GetSceneManager(), App::GetCameraManager()->GetCamera(), RoR::App::GetAppContext()->GetViewport());

    waternoise = new Hydrax::Noise::Perlin();
    mModule = new Hydrax::Module::ProjectedGrid(// Hydrax parent pointer
        mHydrax,
        // Noise module
        waternoise,
        // Base plane
        Ogre::Plane(Ogre::Vector3::UNIT_Y, 0),
        // Normal mode
        Hydrax::MaterialManager::NM_VERTEX,
        // Projected grid options
        Hydrax::Module::ProjectedGrid::Options());

    mHydrax->setModule(static_cast<Hydrax::Module::Module*>(mModule));

    mHydrax->loadCfg(CurrentConfigFile);

    // Choose shader language based on renderer (HLSL=0, CG=1, GLSL=2)
    if (Root::getSingleton().getRenderSystem()->getName() == "Direct3D9 Rendering Subsystem" || Root::getSingleton().getRenderSystem()->getName() == "Direct3D11 Rendering Subsystem")
    {
        mHydrax->setShaderMode(static_cast<Hydrax::MaterialManager::ShaderMode>(0));
    }
    else
    {
        mHydrax->setShaderMode(static_cast<Hydrax::MaterialManager::ShaderMode>(2));
    }

    mHydrax->create();
    mHydrax->setPosition(Ogre::Vector3(0, waterHeight, 0));
}

void HydraxWater::UpdateWater()
{
    SkyManager* sky = RoR::App::GetGameContext()->GetTerrain()->getSkyManager();
    if (sky != nullptr)
    {
        Caelum::BaseSkyLight* sun = sky->GetCaelumSys()->getSun();
        Ogre::Vector3 sunPosition = App::GetCameraManager()->GetCameraNode()->_getDerivedPosition();
        sunPosition -= sun->getLightDirection() * 80000;
        mHydrax->setSunPosition(sunPosition);
        mHydrax->setSunColor(Ogre::Vector3(sun->getBodyColour().r, sun->getBodyColour().g, sun->getBodyColour().b));
    }
}

void HydraxWater::SetWaterVisible(bool value)
{
    if (mHydrax)
        mHydrax->setVisible(value);
}

void HydraxWater::WaterSetSunPosition(Ogre::Vector3 pos)
{
    if (mHydrax)
        mHydrax->setSunPosition(pos);
}

void HydraxWater::FrameStepWater(float dt)
{
    const float curWaterHeight = App::GetGameContext()->GetTerrain()->getWater()->GetStaticWaterHeight();
    if (waterHeight != curWaterHeight)
    {
        waterHeight = curWaterHeight;
    }
    if (mHydrax)
    {
        mHydrax->update(dt);
    }
    this->UpdateWater();
}

