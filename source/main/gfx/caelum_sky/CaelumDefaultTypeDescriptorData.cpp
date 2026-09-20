// This file is part of the Caelum project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution.


#include "CaelumPrerequisites.h"

#if CAELUM_TYPE_DESCRIPTORS

#include "TypeDescriptor.h"
#include "CaelumSystem.h"
#include "GroundFog.h"
#include "DepthComposer.h"
#include "PointStarfield.h"

using namespace Ogre;

namespace Caelum
{
    CaelumDefaultTypeDescriptorData::CaelumDefaultTypeDescriptorData ():
            CaelumSystemTypeDescriptor(0),
            PointStarfieldTypeDescriptor(0),
            BaseSkyLightTypeDescriptor(0),
            GroundFogTypeDescriptor(0),
            PrecipitationTypeDescriptor(0),
            VCloudsTypeDescriptor(0),
            DepthComposerTypeDescriptor(0),
            SkyDomeTypeDescriptor(0)
    {
        try {
            load ();
        } catch (...) {
            unload ();
            throw;
        }
    }

    CaelumDefaultTypeDescriptorData::~CaelumDefaultTypeDescriptorData ()
    {
        unload ();
    }

    void CaelumDefaultTypeDescriptorData::unload ()
    {
        delete (CaelumSystemTypeDescriptor);
        delete (PointStarfieldTypeDescriptor);
        delete (BaseSkyLightTypeDescriptor);
        delete (GroundFogTypeDescriptor);
        delete (PrecipitationTypeDescriptor);
        delete (VCloudsTypeDescriptor);
        delete (DepthComposerTypeDescriptor);
        delete (SkyDomeTypeDescriptor);
    }

    void CaelumDefaultTypeDescriptorData::load ()
    {
        if (!CaelumSystemTypeDescriptor)
        {
            std::unique_ptr<DefaultTypeDescriptor> td (new DefaultTypeDescriptor ());

            // Timing settings.
            td->add("julian_day",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, LongReal, LongReal, LongReal>(
                            &Caelum::CaelumSystem::getJulianDay,
                            &Caelum::CaelumSystem::setJulianDay));

            // Latitude/longitude
            td->add("latitude",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, Degree, Degree, const Degree>(
                            &Caelum::CaelumSystem::getObserverLatitude,
                            &Caelum::CaelumSystem::setObserverLatitude));
            td->add("longitude",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, Degree, Degree, const Degree>(
                            &Caelum::CaelumSystem::getObserverLongitude,
                            &Caelum::CaelumSystem::setObserverLongitude));

            // Fog settings.
            td->add("global_fog_density_multiplier",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, Real, Real, Real>(
                            &Caelum::CaelumSystem::getGlobalFogDensityMultiplier,
                            &Caelum::CaelumSystem::setGlobalFogDensityMultiplier));
            td->add("global_fog_colour_multiplier",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, ColourValue>(
                            &Caelum::CaelumSystem::getGlobalFogColourMultiplier,
                            &Caelum::CaelumSystem::setGlobalFogColourMultiplier));
            td->add("manage_scene_fog_mode",
					new AccesorPropertyDescriptor<Caelum::CaelumSystem, Ogre::FogMode, Ogre::FogMode, Ogre::FogMode>(
                            &Caelum::CaelumSystem::getManageSceneFog,
                            &Caelum::CaelumSystem::setManageSceneFog));
            td->add("manage_scene_start",
					new AccesorPropertyDescriptor<Caelum::CaelumSystem, Ogre::Real, Ogre::Real, Ogre::Real>(
							&Caelum::CaelumSystem::getManageSceneFogStart,
							&Caelum::CaelumSystem::setManageSceneFogStart));
            td->add("manage_scene_end",
					new AccesorPropertyDescriptor<Caelum::CaelumSystem, Ogre::Real, Ogre::Real, Ogre::Real>(
						&Caelum::CaelumSystem::getManageSceneFogEnd,
						&Caelum::CaelumSystem::setManageSceneFogEnd));
            td->add("scene_fog_density_multiplier",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, Real, Real, Real>(
                            &Caelum::CaelumSystem::getSceneFogDensityMultiplier,
                            &Caelum::CaelumSystem::setSceneFogDensityMultiplier));
            td->add("scene_fog_colour_multiplier",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, ColourValue>(
                            &Caelum::CaelumSystem::getSceneFogColourMultiplier,
                            &Caelum::CaelumSystem::setSceneFogColourMultiplier));
            td->add("ground_fog_density_multiplier",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, Real, Real, Real>(
                            &Caelum::CaelumSystem::getGroundFogDensityMultiplier,
                            &Caelum::CaelumSystem::setGroundFogDensityMultiplier));
            td->add("ground_fog_colour_multiplier",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, ColourValue>(
                            &Caelum::CaelumSystem::getGroundFogColourMultiplier,
                            &Caelum::CaelumSystem::setGroundFogColourMultiplier));

            // Lighting settings.
            td->add("manage_ambient_light",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, bool, bool, bool>(
                            &Caelum::CaelumSystem::getManageAmbientLight,
                            &Caelum::CaelumSystem::setManageAmbientLight));
            td->add("minimum_ambient_light",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, ColourValue>(
                            &Caelum::CaelumSystem::getMinimumAmbientLight,
                            &Caelum::CaelumSystem::setMinimumAmbientLight));
            td->add("ensure_single_light_source",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, bool, bool, bool>(
                            &Caelum::CaelumSystem::getEnsureSingleLightSource,
                            &Caelum::CaelumSystem::setEnsureSingleLightSource));
            td->add("ensure_single_shadow_source",
                    new AccesorPropertyDescriptor<Caelum::CaelumSystem, bool, bool, bool>(
                            &Caelum::CaelumSystem::getEnsureSingleShadowSource,
                            &Caelum::CaelumSystem::setEnsureSingleShadowSource));

            CaelumSystemTypeDescriptor = td.release ();
        }

        if (!PointStarfieldTypeDescriptor)
        {
            std::unique_ptr<DefaultTypeDescriptor> td (new DefaultTypeDescriptor ());
            td->add("magnitude_scale",
                    new AccesorPropertyDescriptor<Caelum::PointStarfield, Real, Real, Real>(
                            &Caelum::PointStarfield::getMagnitudeScale,
                            &Caelum::PointStarfield::setMagnitudeScale));
            td->add("mag0_pixel_size",
                    new AccesorPropertyDescriptor<Caelum::PointStarfield, Real, Real, Real>(
                            &Caelum::PointStarfield::getMag0PixelSize,
                            &Caelum::PointStarfield::setMag0PixelSize));
            td->add("min_pixel_size",
                    new AccesorPropertyDescriptor<Caelum::PointStarfield, Real, Real, Real>(
                            &Caelum::PointStarfield::getMinPixelSize,
                            &Caelum::PointStarfield::setMinPixelSize));
            td->add("max_pixel_size",
                    new AccesorPropertyDescriptor<Caelum::PointStarfield, Real, Real, Real>(
                            &Caelum::PointStarfield::getMaxPixelSize,
                            &Caelum::PointStarfield::setMaxPixelSize));
            td->add("latitude",
                    new AccesorPropertyDescriptor<Caelum::PointStarfield, Degree, Degree, Degree>(
                            &Caelum::PointStarfield::getObserverLatitude,
                            &Caelum::PointStarfield::setObserverLatitude));
            td->add("longitude",
                    new AccesorPropertyDescriptor<Caelum::PointStarfield, Degree, Degree, Degree>(
                            &Caelum::PointStarfield::getObserverLongitude,
                            &Caelum::PointStarfield::setObserverLongitude));
            PointStarfieldTypeDescriptor = td.release ();
        }

        if (!BaseSkyLightTypeDescriptor)
        {
            std::unique_ptr<DefaultTypeDescriptor> td (new DefaultTypeDescriptor ());
            td->add("ambient_multiplier",
                    new AccesorPropertyDescriptor<Caelum::BaseSkyLight, ColourValue>(
                            &Caelum::BaseSkyLight::getAmbientMultiplier,
                            &Caelum::BaseSkyLight::setAmbientMultiplier));
            td->add("specular_multiplier",
                    new AccesorPropertyDescriptor<Caelum::BaseSkyLight, ColourValue>(
                            &Caelum::BaseSkyLight::getSpecularMultiplier,
                            &Caelum::BaseSkyLight::setSpecularMultiplier));
            td->add("diffuse_multiplier",
                    new AccesorPropertyDescriptor<Caelum::BaseSkyLight, ColourValue>(
                            &Caelum::BaseSkyLight::getDiffuseMultiplier,
                            &Caelum::BaseSkyLight::setDiffuseMultiplier));
            td->add("light_colour",
                    new AccesorPropertyDescriptor<Caelum::BaseSkyLight, ColourValue>(
                            &Caelum::BaseSkyLight::getLightColour,
                            &Caelum::BaseSkyLight::setLightColour));
            td->add("body_colour",
                    new AccesorPropertyDescriptor<Caelum::BaseSkyLight, ColourValue>(
                            &Caelum::BaseSkyLight::getBodyColour,
                            &Caelum::BaseSkyLight::setBodyColour));
            td->add("auto_disable_threshold",
                    new AccesorPropertyDescriptor<Caelum::BaseSkyLight, Real, Real, Real>(
                            &Caelum::BaseSkyLight::getAutoDisableThreshold,
                            &Caelum::BaseSkyLight::setAutoDisableThreshold));
            td->add("auto_disable",
                    new AccesorPropertyDescriptor<Caelum::BaseSkyLight, bool, bool, bool>(
                            &Caelum::BaseSkyLight::getAutoDisable,
                            &Caelum::BaseSkyLight::setAutoDisable));
            BaseSkyLightTypeDescriptor = td.release ();
        }

        if (!GroundFogTypeDescriptor)
        {
            std::unique_ptr<DefaultTypeDescriptor> td (new DefaultTypeDescriptor ());
            td->add("density",
                    new AccesorPropertyDescriptor<Caelum::GroundFog, Real, Real, Real>(
                            &Caelum::GroundFog::getDensity,
                            &Caelum::GroundFog::setDensity));
            td->add("vertical_decay",
                    new AccesorPropertyDescriptor<Caelum::GroundFog, Real, Real, Real>(
                            &Caelum::GroundFog::getVerticalDecay,
                            &Caelum::GroundFog::setVerticalDecay));
            td->add("ground_level",
                    new AccesorPropertyDescriptor<Caelum::GroundFog, Real, Real, Real>(
                            &Caelum::GroundFog::getGroundLevel,
                            &Caelum::GroundFog::setGroundLevel));
            td->add("colour",
                    new AccesorPropertyDescriptor<Caelum::GroundFog, ColourValue>(
                            &Caelum::GroundFog::getColour,
                            &Caelum::GroundFog::setColour));
            GroundFogTypeDescriptor = td.release ();
        }

        if (!DepthComposerTypeDescriptor)
        {
            std::unique_ptr<DefaultTypeDescriptor> td (new DefaultTypeDescriptor ());
            td->add("debug_depth_render",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, bool, bool, bool>(
                            &Caelum::DepthComposer::getDebugDepthRender,
                            &Caelum::DepthComposer::setDebugDepthRender));

            // Legacy haze
            td->add("haze_enabled",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, bool, bool, bool>(
                            &Caelum::DepthComposer::getSkyDomeHazeEnabled,
                            &Caelum::DepthComposer::setSkyDomeHazeEnabled));
            td->add("haze_colour",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, ColourValue>(
                            &Caelum::DepthComposer::getHazeColour,
                            &Caelum::DepthComposer::setHazeColour));
            td->add("haze_sun_direction",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, Vector3>(
                            &Caelum::DepthComposer::getSunDirection,
                            &Caelum::DepthComposer::setSunDirection));

            // Ground fog
            td->add("ground_fog_enabled",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, bool, bool, bool>(
                            &Caelum::DepthComposer::getGroundFogEnabled,
                            &Caelum::DepthComposer::setGroundFogEnabled));
            td->add("ground_fog_density",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, Real, Real, Real>(
                            &Caelum::DepthComposer::getGroundFogDensity,
                            &Caelum::DepthComposer::setGroundFogDensity));
            td->add("ground_fog_vertical_decay",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, Real, Real, Real>(
                            &Caelum::DepthComposer::getGroundFogVerticalDecay,
                            &Caelum::DepthComposer::setGroundFogVerticalDecay));
            td->add("ground_fog_base_level",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, Real, Real, Real>(
                            &Caelum::DepthComposer::getGroundFogBaseLevel,
                            &Caelum::DepthComposer::setGroundFogBaseLevel));
            td->add("ground_fog_colour",
                    new AccesorPropertyDescriptor<Caelum::DepthComposer, ColourValue>(
                            &Caelum::DepthComposer::getGroundFogColour,
                            &Caelum::DepthComposer::setGroundFogColour));

            DepthComposerTypeDescriptor = td.release ();
        }

        if (!PrecipitationTypeDescriptor)
        {
            std::unique_ptr<DefaultTypeDescriptor> td (new DefaultTypeDescriptor ());

            td->add("texture",
                    new AccesorPropertyDescriptor<Caelum::PrecipitationController, String>(
                            &Caelum::PrecipitationController::getTextureName,
                            &Caelum::PrecipitationController::setTextureName));
            td->add("precipitation_colour",
                    new AccesorPropertyDescriptor<Caelum::PrecipitationController, ColourValue>(
                            &Caelum::PrecipitationController::getColour,
                            &Caelum::PrecipitationController::setColour));
            td->add("falling_speed",
                    new AccesorPropertyDescriptor<Caelum::PrecipitationController, Real, Real, Real>(
                            &Caelum::PrecipitationController::getSpeed,
                            &Caelum::PrecipitationController::setSpeed));
            td->add("wind_speed",
                    new AccesorPropertyDescriptor<Caelum::PrecipitationController, Vector3>(
                            &Caelum::PrecipitationController::getWindSpeed,
                            &Caelum::PrecipitationController::setWindSpeed));
            td->add("camera_speed_scale",
                    new AccesorPropertyDescriptor<Caelum::PrecipitationController, Vector3>(
                            &Caelum::PrecipitationController::getCameraSpeedScale,
                            &Caelum::PrecipitationController::setCameraSpeedScale));
            td->add("intensity",
                    new AccesorPropertyDescriptor<Caelum::PrecipitationController, Real, Real, Real>(
                            &Caelum::PrecipitationController::getIntensity,
                            &Caelum::PrecipitationController::setIntensity));
            td->add("auto_disable_intensity",
                    new AccesorPropertyDescriptor<Caelum::PrecipitationController, Real, Real, Real>(
                            &Caelum::PrecipitationController::getAutoDisableThreshold,
                            &Caelum::PrecipitationController::setAutoDisableThreshold));
            td->add("falling_direction",
                    new AccesorPropertyDescriptor<Caelum::PrecipitationController, Vector3>(
                            &Caelum::PrecipitationController::getFallingDirection,
                            &Caelum::PrecipitationController::setFallingDirection));

            PrecipitationTypeDescriptor = td.release ();
        }


        if (!SkyDomeTypeDescriptor)
        {
            std::unique_ptr<DefaultTypeDescriptor> td (new DefaultTypeDescriptor ());

            // SkyDome is slightly special because most properties are write-only.

            // Reset by CaelumSystem every frame anyway
            td->add("sun_direction",
                    new AccesorPropertyDescriptor<Caelum::SkyDome, Ogre::Vector3>(
                            0, &Caelum::SkyDome::setSunDirection));
            td->add("haze_colour",
                    new AccesorPropertyDescriptor<Caelum::SkyDome, Ogre::ColourValue>(
                            0, &Caelum::SkyDome::setHazeColour));

            // Different files not supported anyway
            td->add("sky_gradients_image",
                    new AccesorPropertyDescriptor<Caelum::SkyDome, Ogre::String>(
                            0, &Caelum::SkyDome::setSkyGradientsImage));
            td->add("atmosphere_depth_image",
                    new AccesorPropertyDescriptor<Caelum::SkyDome, Ogre::String>(
                            0, &Caelum::SkyDome::setAtmosphereDepthImage));

            // This does actually make sense.
            td->add("haze_enabled",
                    new AccesorPropertyDescriptor<Caelum::SkyDome, bool, bool, bool>(
                            &Caelum::SkyDome::getHazeEnabled,
                            &Caelum::SkyDome::setHazeEnabled));

            SkyDomeTypeDescriptor = td.release ();
        }

        if (!VCloudsTypeDescriptor)
        {
            std::unique_ptr<DefaultTypeDescriptor> td (new DefaultTypeDescriptor ());
            // Properties ported from SkyX's config system

            td->add("wind_speed",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Real, Real, Real>(
                            &Caelum::VCloudsManager::getWindSpeed,
                            &Caelum::VCloudsManager::setWindSpeed));

            td->add("wind_direction",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Radian, Radian, Radian>(
                            &Caelum::VCloudsManager::getWindDirection,
                            &Caelum::VCloudsManager::setWindDirection));

            td->add("vertical_bounds",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Vector2>(
                            &Caelum::VCloudsManager::getHeight,
                            &Caelum::VCloudsManager::setHeight));

            td->add("ambient_color",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Vector3>(
                            &Caelum::VCloudsManager::getAmbientColor,
                            &Caelum::VCloudsManager::setAmbientColor));

            td->add("light_response",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Vector4>(
                            &Caelum::VCloudsManager::getLightResponse,
                            &Caelum::VCloudsManager::setLightResponse));

            td->add("ambient_factors",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Vector4>(
                            &Caelum::VCloudsManager::getAmbientFactors,
                            &Caelum::VCloudsManager::setAmbientFactors));

            td->add("cloudiness",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Vector2>(
                            &Caelum::VCloudsManager::getCloudiness,
                            &Caelum::VCloudsManager::setCloudiness));

            td->add("radius",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Real, Real, Real>(
                            &Caelum::VCloudsManager::getRadius,
                            &Caelum::VCloudsManager::setRadius));

            td->add("enable_lightnings",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, bool, bool, bool>(
                            &Caelum::VCloudsManager::getEnableLightnings,
                            &Caelum::VCloudsManager::setEnableLightnings));

            td->add("average_lightning_appartition_time",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Real, Real, Real>(
                            &Caelum::VCloudsManager::getAverageLightningAppartitionTime,
                            &Caelum::VCloudsManager::setAverageLightningAppartitionTime));

            td->add("lightning_color",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Vector3>(
                            &Caelum::VCloudsManager::getLightningColor,
                            &Caelum::VCloudsManager::setLightningColor));

            td->add("lightning_time_multiplier",
                    new AccesorPropertyDescriptor<Caelum::VCloudsManager, Real, Real, Real>(
                            &Caelum::VCloudsManager::getLightningTimeMultiplier,
                            &Caelum::VCloudsManager::setLightningTimeMultiplier));

            VCloudsTypeDescriptor = td.release();
        }
    }
}

#endif // CAELUM_TYPE_DESCRIPTORS
