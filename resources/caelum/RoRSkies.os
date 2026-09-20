caelum_sky_system ror_default_sky
{
    julian_day 0
    time_scale 1

    point_starfield {
        magnitude_scale 2.51189
        mag0_pixel_size 16
        min_pixel_size 4
        max_pixel_size 6
    }

    manage_ambient_light true
    minimum_ambient_light 0.1 0.1 0.3

    manage_scene_fog yes
    ground_fog_density_multiplier 0.0015
	scene_fog_density_multiplier 0.0015

    sun {
        ambient_multiplier 0.5 0.5 0.5
        diffuse_multiplier 3 3 2.7
        specular_multiplier 5 5 5

        auto_disable_threshold 0.05
        auto_disable true
    }

    moon {
        ambient_multiplier 0.2 0.2 0.2
        diffuse_multiplier 1 1 .9
        specular_multiplier 1 1 1

        auto_disable_threshold 0.05
        auto_disable true
    }

    // Off by default
    /*
    depth_composer {
        debug_depth_render on
        haze_enabled no
        ground_fog_enabled no
        ground_fog_vertical_decay 0.06
        ground_fog_base_level 0
    }
	*/
    

    sky_dome {
        haze_enabled yes
        sky_gradients_image EarthClearSky2.png
        atmosphere_depth_image AtmosphereDepth.png
    }

    vclouds
    {
        //Volumetric clouds
        wind_speed 80.0
        wind_direction 0
        auto_update no
        vertical_bounds 825 500
        light_response 0.25 0.2 1.0 0.1
        ambient_factors 0.45 0.3 0.6 1.0
        cloudiness 0.1 0.6
        radius 10000

        //Lightnings
        enable_lightnings no
        average_lightning_appartition_time 1.5
        lightning_color 1 0.925 0.85
        lightning_time_multiplier 2.0
    }
	
}

