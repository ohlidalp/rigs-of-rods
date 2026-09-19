/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2018 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

#include "SkyManager.h"

#include "Actor.h"
#include "AppContext.h"
#include "CameraManager.h"
#include "GameContext.h"
#include "GfxScene.h"
#include "Terrain.h"
#include "TerrainGeometryManager.h"
#include "CaelumPlugin.h"

#include <Caelum.h>

using namespace RoR;

SkyManager::SkyManager() : m_caelum_system(nullptr), m_last_clock(0.0)
{
    // Initialise CaelumSystem.
    m_caelum_system = new Caelum::CaelumSystem(
        RoR::App::GetAppContext()->GetOgreRoot(),
        App::GetGfxScene()->GetSceneManager(),
        Caelum::CaelumSystem::CAELUM_COMPONENTS_DEFAULT
    );

    m_caelum_system->attachViewport(RoR::App::GetAppContext()->GetViewport());

    // Register caelum as a listener.
    RoR::App::GetAppContext()->GetRenderWindow()->addListener(m_caelum_system);
}

SkyManager::~SkyManager()
{
    RoR::App::GetAppContext()->GetRenderWindow()->removeListener(m_caelum_system);
    m_caelum_system->shutdown();
    m_caelum_system = nullptr;
}

void SkyManager::NotifySkyCameraChanged(Ogre::Camera* cam)
{
    if (m_caelum_system)
        m_caelum_system->notifyCameraChanged(cam);
}

void SkyManager::UpdateSky(float dt_sim)
{
    if (!m_caelum_system || !App::GetGameContext()->GetTerrain())
    {
        return;
    }

    m_caelum_system->frameStepSubcomponents(dt_sim);

    Caelum::LongReal c = m_caelum_system->getUniversalClock()->getJulianDay();

    if (c - m_last_clock > 0.001f)
    {
        TerrainGeometryManager* gm = App::GetGameContext()->GetTerrain()->getGeometryManager();
        if (gm)
            gm->updateLightMap();
    }

    m_last_clock = c;
}

void SkyManager::SetupCaelumFog(int fogStart, int fogEnd)
{
    // Note: Our farclip is always finite, see `CameraManager::CameraManager()`
    // ------------------------------------------------------------------------

    if (fogStart != -1 && fogEnd != -1)
    {
        LOG("[RoR|SkyManager] CaelumFogStart must be smaller then CaelumFogEnd in terrn2. Ignoring boundaries.");
        return;
    }
    else if (fogStart != -1 || fogEnd != -1)
    {
        LOG("[RoR|SkyManager] You always need to define both boundaries (CaelumFogStart AND CaelumFogEnd) in terrn2. Ignoring boundaries.");
        return;
    }
    m_caelum_system->setManageSceneFog(Ogre::FOG_LINEAR);
    m_caelum_system->setManageSceneFogStart(fogStart);
    m_caelum_system->setManageSceneFogEnd(fogEnd);
}

void SkyManager::LoadCaelumScript(const std::string& script, const std::string& rg)
{
    // load the caelum config
    try
    {
        Caelum::CaelumPlugin::getSingleton().loadCaelumSystemFromScript(m_caelum_system, script, rg);

        // enforcing update, so shadows are set correctly before creating the terrain
        m_caelum_system->frameStepSubcomponents(0.01);
    }
    catch (...)
    {
        HandleGenericException(fmt::format("Could not load Caelum script '{}' from resource group '{}'", script, rg));
    }
    Ogre::Vector3 lightsrc = m_caelum_system->getSun()->getMainLight()->getDerivedDirection();
    m_caelum_system->getSun()->getMainLight()->getParentSceneNode()->setDirection(lightsrc.normalisedCopy());
}

Ogre::Light* SkyManager::GetSkyMainLight()
{
    if (m_caelum_system && m_caelum_system->getSun())
    {
        return m_caelum_system->getSun()->getMainLight();
    }
    return nullptr;
}

std::string SkyManager::GetPrettyTime()
{
    int ignore;
    int hour;
    int minute;
    Caelum::LongReal second;
    Caelum::Astronomy::getGregorianDateTimeFromJulianDay(m_caelum_system->getJulianDay()
        , ignore, ignore, ignore, hour, minute, second);

    char buf[100];
    snprintf(buf, 100, "%02d:%02d:%02d", hour, minute, static_cast<int>(second));
    return buf;
}

bool SkyManager::GetDescriptorAndTarget(const std::string& section, 
                                        const Caelum::TypeDescriptor*& outDescriptor, 
                                        void*& outTarget)
{
    if (!m_caelum_system)
        return false;

#if CAELUM_TYPE_DESCRIPTORS
    const Caelum::CaelumDefaultTypeDescriptorData* typeData = 
        Caelum::CaelumPlugin::getSingleton().getTypeDescriptorData();

    if (section == "caelum_sky_system")
    {
        outDescriptor = typeData->CaelumSystemTypeDescriptor;
        outTarget = m_caelum_system;
        return true;
    }
    else if (section == "point_starfield")
    {
        if (!m_caelum_system->getPointStarfield())
            return false;
        outDescriptor = typeData->PointStarfieldTypeDescriptor;
        outTarget = m_caelum_system->getPointStarfield();
        return true;
    }
    else if (section == "sun")
    {
        if (!m_caelum_system->getSun())
            return false;
        outDescriptor = typeData->BaseSkyLightTypeDescriptor;
        outTarget = m_caelum_system->getSun();
        return true;
    }
    else if (section == "moon")
    {
        if (!m_caelum_system->getMoon())
            return false;
        outDescriptor = typeData->BaseSkyLightTypeDescriptor;
        outTarget = m_caelum_system->getMoon();
        return true;
    }
    else if (section == "ground_fog")
    {
        if (!m_caelum_system->getGroundFog())
            return false;
        outDescriptor = typeData->GroundFogTypeDescriptor;
        outTarget = m_caelum_system->getGroundFog();
        return true;
    }
    else if (section == "precipitation")
    {
        if (!m_caelum_system->getPrecipitationController())
            return false;
        outDescriptor = typeData->PrecipitationTypeDescriptor;
        outTarget = m_caelum_system->getPrecipitationController();
        return true;
    }
    else if (section == "depth_composer")
    {
        if (!m_caelum_system->getDepthComposer())
            return false;
        outDescriptor = typeData->DepthComposerTypeDescriptor;
        outTarget = m_caelum_system->getDepthComposer();
        return true;
    }
    else if (section == "sky_dome")
    {
        if (!m_caelum_system->getSkyDome())
            return false;
        outDescriptor = typeData->SkyDomeTypeDescriptor;
        outTarget = m_caelum_system->getSkyDome();
        return true;
    }
    else if (section == "vclouds")
    {
        if (!m_caelum_system->getVCloudsManager())
            return false;
        outDescriptor = typeData->VCloudsTypeDescriptor;
        outTarget = m_caelum_system->getVCloudsManager();
        return true;
    }
#endif // CAELUM_TYPE_DESCRIPTORS

    return false;
}

bool SkyManager::SetCaelumParameter(const std::string& section, const std::string& name,
                                    float arg1, float arg2, float arg3, float arg4)
{
#if CAELUM_TYPE_DESCRIPTORS
    const Caelum::TypeDescriptor* descriptor = nullptr;
    void* target = nullptr;

    if (!GetDescriptorAndTarget(section, descriptor, target))
    {
        RoR::LogFormat("[RoR|SkyManager] SetCaelumParameter: Invalid section '%s' or component not available", section.c_str());
        return false;
    }

    const Caelum::ValuePropertyDescriptor* prop = descriptor->getPropertyDescriptor(name);
    if (!prop)
    {
        RoR::LogFormat("[RoR|SkyManager] SetCaelumParameter: Property '%s' not found in section '%s'", name.c_str(), section.c_str());
        return false;
    }

    if (!prop->canSetValue())
    {
        RoR::LogFormat("[RoR|SkyManager] SetCaelumParameter: Property '%s.%s' is read-only", section.c_str(), name.c_str());
        return false;
    }

    try
    {
        const std::type_info& typeInfo = prop->getValueTypeId();

        // Handle different value types
        if (typeInfo == typeid(Ogre::Real))
        {
            prop->setValue(target, Ogre::Any(static_cast<Ogre::Real>(arg1)));
        }
        else if (typeInfo == typeid(float))
        {
            prop->setValue(target, Ogre::Any(arg1));
        }
        else if (typeInfo == typeid(double))
        {
            prop->setValue(target, Ogre::Any(static_cast<double>(arg1)));
        }
        else if (typeInfo == typeid(Ogre::Degree))
        {
            prop->setValue(target, Ogre::Any(Ogre::Degree(arg1)));
        }
        else if (typeInfo == typeid(Ogre::Radian))
        {
            prop->setValue(target, Ogre::Any(Ogre::Radian(arg1)));
        }
        else if (typeInfo == typeid(Ogre::Vector2))
        {
            prop->setValue(target, Ogre::Any(Ogre::Vector2(arg1, arg2)));
        }
        else if (typeInfo == typeid(Ogre::Vector3))
        {
            prop->setValue(target, Ogre::Any(Ogre::Vector3(arg1, arg2, arg3)));
        }
        else if (typeInfo == typeid(Ogre::Vector4))
        {
            prop->setValue(target, Ogre::Any(Ogre::Vector4(arg1, arg2, arg3, arg4)));
        }
        else if (typeInfo == typeid(Ogre::ColourValue))
        {
            prop->setValue(target, Ogre::Any(Ogre::ColourValue(arg1, arg2, arg3, arg4)));
        }
        else if (typeInfo == typeid(bool))
        {
            prop->setValue(target, Ogre::Any(arg1 != 0.f));
        }
        else if (typeInfo == typeid(int))
        {
            prop->setValue(target, Ogre::Any(static_cast<int>(arg1)));
        }
        else if (typeInfo == typeid(Ogre::FogMode))
        {
            prop->setValue(target, Ogre::Any(static_cast<Ogre::FogMode>(static_cast<int>(arg1))));
        }
        else
        {
            RoR::LogFormat("[RoR|SkyManager] SetCaelumParameter: Unsupported type for property '%s.%s'", 
                          section.c_str(), name.c_str());
            return false;
        }

        return true;
    }
    catch (Ogre::Exception& e)
    {
        RoR::LogFormat("[RoR|SkyManager] SetCaelumParameter: Exception setting '%s.%s': %s", 
                      section.c_str(), name.c_str(), e.getFullDescription().c_str());
        return false;
    }
#else
    RoR::Log("[RoR|SkyManager] SetCaelumParameter: CAELUM_TYPE_DESCRIPTORS not enabled");
    return false;
#endif // CAELUM_TYPE_DESCRIPTORS
}

bool SkyManager::GetCaelumParameter(const std::string& section, const std::string& name,
                                    float& arg1, float& arg2, float& arg3, float& arg4)
{
#if CAELUM_TYPE_DESCRIPTORS
    const Caelum::TypeDescriptor* descriptor = nullptr;
    void* target = nullptr;

    // Initialize output parameters
    arg1 = arg2 = arg3 = arg4 = 0.f;

    if (!GetDescriptorAndTarget(section, descriptor, target))
    {
        RoR::LogFormat("[RoR|SkyManager] GetCaelumParameter: Invalid section '%s' or component not available", section.c_str());
        return false;
    }

    const Caelum::ValuePropertyDescriptor* prop = descriptor->getPropertyDescriptor(name);
    if (!prop)
    {
        RoR::LogFormat("[RoR|SkyManager] GetCaelumParameter: Property '%s' not found in section '%s'", name.c_str(), section.c_str());
        return false;
    }

    if (!prop->canGetValue())
    {
        RoR::LogFormat("[RoR|SkyManager] GetCaelumParameter: Property '%s.%s' is write-only", section.c_str(), name.c_str());
        return false;
    }

    try
    {
        Ogre::Any value = prop->getValue(target);
        const std::type_info& typeInfo = prop->getValueTypeId();

        // Handle different value types
        if (typeInfo == typeid(Ogre::Real))
        {
            arg1 = Ogre::any_cast<Ogre::Real>(value);
        }
        else if (typeInfo == typeid(float))
        {
            arg1 = Ogre::any_cast<float>(value);
        }
        else if (typeInfo == typeid(double))
        {
            arg1 = static_cast<float>(Ogre::any_cast<double>(value));
        }
        else if (typeInfo == typeid(Ogre::Degree))
        {
            arg1 = Ogre::any_cast<Ogre::Degree>(value).valueDegrees();
        }
        else if (typeInfo == typeid(Ogre::Radian))
        {
            arg1 = Ogre::any_cast<Ogre::Radian>(value).valueRadians();
        }
        else if (typeInfo == typeid(Ogre::Vector2))
        {
            Ogre::Vector2 v = Ogre::any_cast<Ogre::Vector2>(value);
            arg1 = v.x;
            arg2 = v.y;
        }
        else if (typeInfo == typeid(Ogre::Vector3))
        {
            Ogre::Vector3 v = Ogre::any_cast<Ogre::Vector3>(value);
            arg1 = v.x;
            arg2 = v.y;
            arg3 = v.z;
        }
        else if (typeInfo == typeid(Ogre::Vector4))
        {
            Ogre::Vector4 v = Ogre::any_cast<Ogre::Vector4>(value);
            arg1 = v.x;
            arg2 = v.y;
            arg3 = v.z;
            arg4 = v.w;
        }
        else if (typeInfo == typeid(Ogre::ColourValue))
        {
            Ogre::ColourValue c = Ogre::any_cast<Ogre::ColourValue>(value);
            arg1 = c.r;
            arg2 = c.g;
            arg3 = c.b;
            arg4 = c.a;
        }
        else if (typeInfo == typeid(bool))
        {
            arg1 = Ogre::any_cast<bool>(value) ? 1.f : 0.f;
        }
        else if (typeInfo == typeid(int))
        {
            arg1 = static_cast<float>(Ogre::any_cast<int>(value));
        }
        else if (typeInfo == typeid(Ogre::FogMode))
        {
            arg1 = static_cast<float>(Ogre::any_cast<Ogre::FogMode>(value));
        }
        else
        {
            RoR::LogFormat("[RoR|SkyManager] GetCaelumParameter: Unsupported type for property '%s.%s'", 
                          section.c_str(), name.c_str());
            return false;
        }

        return true;
    }
    catch (Ogre::Exception& e)
    {
        RoR::LogFormat("[RoR|SkyManager] GetCaelumParameter: Exception getting '%s.%s': %s", 
                      section.c_str(), name.c_str(), e.getFullDescription().c_str());
        return false;
    }
#else
    RoR::Log("[RoR|SkyManager] GetCaelumParameter: CAELUM_TYPE_DESCRIPTORS not enabled");
    return false;
#endif // CAELUM_TYPE_DESCRIPTORS
}

