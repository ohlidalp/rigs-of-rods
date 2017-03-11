/*
    This source file is part of Rigs of Rods
    Copyright 2013-2017 Petr Ohlidal

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


/// @file   Application.cpp
/// @author Petr Ohlidal
/// @date   05/2014


#include "Application.h"

#include <OgreException.h>

#include "CacheSystem.h"
#include "ContentManager.h"
#include "GUIManager.h"
#include "InputEngine.h"
#include "OgreSubsystem.h"
#include "OverlayWrapper.h"
#include "SceneMouse.h"

/* -------------------- Research of debug options (only_a_ptr, 06/2017) ----------------------

    ROR.CONF NAME            | GVar name ('+': added) --- Description

["Debug Truck Mass"]         | GVar: diag_truck_mass  --- extra logging on runtime - mass recalculation.
["Debug Collisions"]         | GVar: diag_collisions  --- visual debug of static map collisions. Only effective on map load.
["EnvMapDebug"]              | GVar: diag_envmap      --- effective on terrain load (envmap init).
["VideoCameraDebug"]         | GVar: diag_videocameras--- creates debug mesh showing videocamera direction. Effective on vehicle spawn.

["Enable Ingame Console"]    |       +                --- Equivalent to "\log" console command, echoes all RoR.log output to console. Reported to cause massive slowdown on startup.
["Beam Break Debug"]         |       +                --- Use before spawn, lasts entire vehicle lifetime.
["Beam Deform Debug"]        |       +                --- Use before spawn, lasts entire vehicle lifetime.
["Trigger Debug"]            |       +                --- Use before spawn, lasts entire vehicle lifetime.
["DOFDebug"]                 |       +                --- Effective on CameraManager init (map loading)

["Advanced Logging"]         | ~ no gvar ~            --- DEAD, used in removed 'ScopeLog' feature of old spawner.
["DebugBeams"]                                        --- Pre configured debug overlay mode --- DEAD since debug overlay has been remade with different modes 

*/

namespace RoR {
namespace App {

// ================================================================================
// Global variables
// ================================================================================


// Object instances
static OgreSubsystem*   g_ogre_subsystem;
static ContentManager*  g_content_manager;
static OverlayWrapper*  g_overlay_wrapper;
static SceneMouse*      g_scene_mouse;
static GUIManager*      g_gui_manager;
static Console*         g_console;
static InputEngine*     g_input_engine;
static CacheSystem*     g_cache_system;
static MainMenu*        g_main_menu;
static RoRFrameListener* g_sim_controller;

// App
 GVarEnum<AppState>       app_state               ("app_state",               nullptr,                     AppState::BOOTSTRAP,     AppState::MAIN_MENU);
 GVarStr<100>             app_language            ("app_language",            "Language",                  "English",               "English");
 GVarStr<50>              app_locale              ("app_locale",              "Language Short",            "en",                    "en");
 GVarPod<bool>            app_multithread         ("app_multithread",         "Multi-threading",           true,                    true);
 GVarStr<50>              app_screenshot_format   ("app_screenshot_format",   "Screenshot Format",         "jpg",                   "jpg");

// Simulation
 GVarEnum<SimState>       sim_state               ("sim_state",               nullptr,                     SimState::OFF,           SimState::OFF);
 GVarStr<200>             sim_terrain_name        ("sim_terrain_name",        nullptr,                     "",                      "");
 GVarPod<bool>            sim_replay_enabled      ("sim_replay_enabled",      "Replay mode",               false,                   false);
 GVarPod<int>             sim_replay_length       ("sim_replay_length",       "Replay length",             200,                     200);
 GVarPod<int>             sim_replay_stepping     ("sim_replay_stepping",     "Replay Steps per second",   1000,                    1000);
 GVarPod<bool>            sim_position_storage    ("sim_position_storage",    "Position Storage",          false,                   false);
 GVarEnum<SimGearboxMode> sim_gearbox_mode        ("sim_gearbox_mode",        "GearboxMode",               SimGearboxMode::AUTO,    SimGearboxMode::AUTO);

// Multiplayer
 GVarEnum<MpState>        mp_state                ("mp_state",                nullptr,                     MpState::DISABLED,       MpState::DISABLED);
 GVarStr<200>             mp_server_host          ("mp_server_host",          "Server name",               "",                      "");
 GVarPod<int>             mp_server_port          ("mp_server_port",          "Server port",               0,                       0);
 GVarStr<100>             mp_server_password      ("mp_server_password",      "Server password",           "",                      "");
 GVarStr<100>             mp_player_name          ("mp_player_name",          "Nickname",                  "Player",                "Player");
 GVarStr<250>             mp_player_token_hash    ("mp_player_token_hash",    "User Token Hash",           "",                      "");
 GVarStr<400>             mp_portal_url           ("mp_portal_url",           "Multiplayer portal URL",    "http://multiplayer.rigsofrods.org", "http://multiplayer.rigsofrods.org");

// Diagnostic
 GVarPod<bool>            diag_trace_globals      ("diag_trace_globals",      nullptr,                     false,                   false); // Don't init to 'true', logger is not ready at startup
 GVarPod<bool>            diag_rig_log_node_import("diag_rig_log_node_import","RigImporter_Debug_TraverseAndLogAllNodes",  false,   false);
 GVarPod<bool>            diag_rig_log_node_stats ("diag_rig_log_node_stats", "RigImporter_PrintNodeStatsToLog",           false,   false);
 GVarPod<bool>            diag_rig_log_messages   ("diag_rig_log_messages",   "RigImporter_PrintMessagesToLog",            false,   false);
 GVarPod<bool>            diag_collisions         ("diag_collisions",         "Debug Collisions",          false,                   false);
 GVarPod<bool>            diag_truck_mass         ("diag_truck_mass",         "Debug Truck Mass",          false,                   false);
 GVarPod<bool>            diag_envmap             ("diag_envmap",             "EnvMapDebug",               false,                   false);
 GVarPod<bool>            diag_videocameras       ("diag_videocameras",       "VideoCameraDebug",          false,                   false);
 GVarStr<100>             diag_preset_terrain     ("diag_preset_terrain",     "Preselected Map",           "",                      "");
 GVarStr<100>             diag_preset_vehicle     ("diag_preset_vehicle",     "Preselected Truck",         "",                      "");
 GVarStr<100>             diag_preset_veh_config  ("diag_preset_veh_config",  "Preselected TruckConfig",   "",                      "");
 GVarPod<bool>            diag_preset_veh_enter   ("diag_preset_veh_enter",   "Enter Preselected Truck",   false,                   false);
 GVarPod<bool>            diag_log_console_echo   ("diag_log_console_echo",   "Enable Ingame Console",     false,                   false);
 GVarPod<bool>            diag_log_beam_break     ("diag_log_beam_break",     "Beam Break Debug",          false,                   false);
 GVarPod<bool>            diag_log_beam_deform    ("diag_log_beam_deform",    "Beam Deform Debug",         false,                   false);
 GVarPod<bool>            diag_log_beam_trigger   ("diag_log_beam_trigger",   "Trigger Debug",             false,                   false);
 GVarPod<bool>            diag_dof_effect         ("diag_dof_effect",         "DOFDebug",                  false,                   false);
 GVarStr<300>             diag_extra_resource_dir ("diag_extra_resource_dir", "resourceIncludePath",       "",                      "");

// System                                         (all paths are without ending slash!)
 GVarStr<300>             sys_process_dir         ("sys_process_dir",         nullptr,                     "",                      "");
 GVarStr<300>             sys_user_dir            ("sys_user_dir",            nullptr,                     "",                      "");
 GVarStr<300>             sys_config_dir          ("sys_config_dir",          "Config Root",               "",                      "");
 GVarStr<300>             sys_cache_dir           ("sys_cache_dir",           "Cache Path",                "",                      "");
 GVarStr<300>             sys_logs_dir            ("sys_logs_dir",            "Log Path",                  "",                      "");
 GVarStr<300>             sys_resources_dir       ("sys_resources_dir",       "Resources Path",            "",                      "");
 GVarStr<300>             sys_profiler_dir        ("sys_profiler_dir",        "Profiler output dir",       "",                      "");
 GVarStr<300>             sys_screenshot_dir      ("sys_screenshot_dir",      nullptr,                     "",                      "");

// Input - Output
 GVarPod<bool>            io_ffb_enabled          ("io_ffb_enabled",          "Force Feedback",            false,                   false);
 GVarPod<float>           io_ffb_camera_gain      ("io_ffb_camera_gain",      "Force Feedback Camera",     0.f,                     0.f);
 GVarPod<float>           io_ffb_center_gain      ("io_ffb_center_gain",      "Force Feedback Centering",  0.f,                     0.f);
 GVarPod<float>           io_ffb_master_gain      ("io_ffb_master_gain",      "Force Feedback Gain",       0.f,                     0.f);
 GVarPod<float>           io_ffb_stress_gain      ("io_ffb_stress_gain",      "Force Feedback Stress",     0.f,                     0.f);
 GVarEnum<IoInputGrabMode>io_input_grab_mode      ("io_input_grab_mode",      "Input Grab",                IoInputGrabMode::NONE,   IoInputGrabMode::NONE);
 GVarPod<bool>            io_arcade_controls      ("io_arcade_controls",      "ArcadeControls",            false,                   false);
 GVarPod<int>             io_outgauge_mode        ("io_outgauge_mode",        "OutGauge Mode",             0,                       0); // 0 = disabled, 1 = enabled
 GVarStr<50>              io_outgauge_ip          ("io_outgauge_ip",          "OutGauge IP",               "192.168.1.100",         "192.168.1.100");
 GVarPod<int>             io_outgauge_port        ("io_outgauge_port",        "OutGauge Port",             1337,                    1337);
 GVarPod<float>           io_outgauge_delay       ("io_outgauge_delay",       "OutGauge Delay",            10.f,                    10.f);
 GVarPod<int>             io_outgauge_id          ("io_outgauge_id",          "OutGauge ID",               0,                       0);

// Audio
 GVarPod<float>           audio_master_volume     ("audio_master_volume",     "Sound Volume",              0,                       0);
 GVarPod<bool>            audio_enable_creak      ("audio_enable_creak",      "Creak Sound",               false,                   false);
 GVarStr<100>             audio_device_name       ("audio_device_name",       "AudioDevice",               "",                      "");
 GVarPod<bool>            audio_menu_music        ("audio_menu_music",        "MainMenuMusic",             false,                   false);

// Graphics
 GVarEnum<GfxFlaresMode>  gfx_flares_mode         ("gfx_flares_mode",         "Lights",                    GfxFlaresMode::ALL_VEHICLES_HEAD_ONLY, GfxFlaresMode::ALL_VEHICLES_HEAD_ONLY);
 GVarEnum<GfxShadowType>  gfx_shadow_type         ("gfx_shadow_type",         "Shadow technique",          GfxShadowType::NONE,     GfxShadowType::NONE);
 GVarEnum<GfxExtCamMode>  gfx_extcam_mode         ("gfx_extcam_mode",         "External Camera Mode",      GfxExtCamMode::PITCHING, GfxExtCamMode::PITCHING);
 GVarEnum<GfxSkyMode>     gfx_sky_mode            ("gfx_sky_mode",            "Sky effects",               GfxSkyMode::SANDSTORM,   GfxSkyMode::SANDSTORM);
 GVarEnum<GfxTexFilter>   gfx_texture_filter      ("gfx_texture_filter",      "Texture Filtering",         GfxTexFilter::TRILINEAR, GfxTexFilter::TRILINEAR);
 GVarEnum<GfxVegetation>  gfx_vegetation_mode     ("gfx_vegetation_mode",     "Vegetation",                GfxVegetation::NONE,     GfxVegetation::NONE);
 GVarEnum<GfxWaterMode>   gfx_water_mode          ("gfx_water_mode",          "Water effects",             GfxWaterMode::BASIC,     GfxWaterMode::BASIC);
 GVarPod<bool>            gfx_enable_sunburn      ("gfx_enable_sunburn",      "Sunburn",                   false,                   false);
 GVarPod<bool>            gfx_water_waves         ("gfx_water_waves",         "Waves",                     false,                   false);
 GVarPod<bool>            gfx_minimap_disabled    ("gfx_minimap_disabled",    "disableOverViewMap",        false,                   false);
 GVarPod<int>             gfx_particles_mode      ("gfx_particles_mode",      "Particles",                 0,                       0);
 GVarPod<bool>            gfx_enable_glow         ("gfx_enable_glow",         "Glow",                      false,                   false);
 GVarPod<bool>            gfx_enable_hdr          ("gfx_enable_hdr",          "HDR",                       false,                   false);
 GVarPod<bool>            gfx_enable_heathaze     ("gfx_enable_heathaze",     "HeatHaze",                  false,                   false);
 GVarPod<bool>            gfx_enable_videocams    ("gfx_enable_videocams",    "gfx_enable_videocams",      false,                   false);
 GVarPod<bool>            gfx_envmap_enabled      ("gfx_envmap_enabled",      "Envmap",                    false,                   false);
 GVarPod<int>             gfx_envmap_rate         ("gfx_envmap_rate",         "EnvmapUpdateRate",          0,                       0);
 GVarPod<int>             gfx_skidmarks_mode      ("gfx_skidmarks_mode",      "Skidmarks",                 0,                       0);
 GVarPod<float>           gfx_sight_range         ("gfx_sight_range",         "SightRange",                3000.f,                  3000.f); // Previously either 2000 or 4500 (inconsistent)
 GVarPod<float>           gfx_fov_external        ("gfx_fov_external",        "FOV External",              60.f,                    60.f);
 GVarPod<float>           gfx_fov_internal        ("gfx_fov_internal",        "FOV Internal",              75.f,                    75.f);
 GVarPod<int>             gfx_fps_limit           ("gfx_fps_limit",           "FPS-Limiter",               0,                       0); // 0 = unlimited
 GVarPod<bool>            gfx_speedo_digital      ("gfx_speedo_digital",      "DigitalSpeedo",             false,                   false);
 GVarPod<bool>            gfx_speedo_imperial     ("gfx_speedo_imperial",     "gfx_speedo_imperial",       false,                   false);
 GVarPod<bool>            gfx_motion_blur         ("gfx_motion_blur",         "Motion blur",               false,                   false);

// Instance management
void SetMainMenu       (MainMenu* obj)                { g_main_menu = obj; }
void SetSimController  (RoRFrameListener* obj)        { g_sim_controller = obj;}

// Instance access
OgreSubsystem*         GetOgreSubsystem      () { return g_ogre_subsystem; };
ContentManager*        GetContentManager     () { return g_content_manager;}
OverlayWrapper*        GetOverlayWrapper     () { return g_overlay_wrapper;}
SceneMouse*            GetSceneMouse         () { return g_scene_mouse;}
GUIManager*            GetGuiManager         () { return g_gui_manager;}
Console*               GetConsole            () { return g_gui_manager->GetConsole();}
InputEngine*           GetInputEngine        () { return g_input_engine;}
CacheSystem*           GetCacheSystem        () { return g_cache_system;}
MainMenu*              GetMainMenu           () { return g_main_menu;}
RoRFrameListener*      GetSimController      () { return g_sim_controller; }

void StartOgreSubsystem()
{
    g_ogre_subsystem = new OgreSubsystem();
    if (g_ogre_subsystem == nullptr)
    {
        throw std::runtime_error("[RoR] Failed to create OgreSubsystem");
    }

    if (! g_ogre_subsystem->StartOgre("", ""))
    {
        throw std::runtime_error("[RoR] Failed to start up OGRE 3D engine");
    }
}

void ShutdownOgreSubsystem()
{
    assert(g_ogre_subsystem != nullptr && "ShutdownOgreSubsystem(): Ogre subsystem was not started");
    delete g_ogre_subsystem;
    g_ogre_subsystem = nullptr;
}

void CreateContentManager()
{
    g_content_manager = new ContentManager();
}

void DestroyContentManager()
{
    assert(g_content_manager != nullptr && "DestroyContentManager(): ContentManager never created");
    delete g_content_manager;
    g_content_manager = nullptr;
}

void CreateOverlayWrapper()
{
    g_overlay_wrapper = new OverlayWrapper();
    if (g_overlay_wrapper == nullptr)
    {
        throw std::runtime_error("[RoR] Failed to create OverlayWrapper");
    }
}

void DestroyOverlayWrapper()
{
    assert(g_overlay_wrapper != nullptr && "DestroyOverlayWrapper(): OverlayWrapper never created");
    delete g_overlay_wrapper;
    g_overlay_wrapper = nullptr;
}

void CreateSceneMouse()
{
    assert (g_scene_mouse == nullptr);
    g_scene_mouse = new SceneMouse();
}

void DeleteSceneMouse()
{
    assert (g_scene_mouse != nullptr);
    delete g_scene_mouse;
    g_scene_mouse = nullptr;
}

void CreateGuiManagerIfNotExists()
{
    if (g_gui_manager == nullptr)
    {
        g_gui_manager = new GUIManager();
    }
}

void DeleteGuiManagerIfExists()
{
    if (g_gui_manager != nullptr)
    {
        delete g_gui_manager;
        g_gui_manager = nullptr;
    }
}

void CreateInputEngine()
{
    assert(g_input_engine == nullptr);
    g_input_engine = new InputEngine();
    g_input_engine->SetupInputDevices();
    g_input_engine->LoadInputMappings();
}

void CreateCacheSystem()
{
    assert(g_cache_system == nullptr);
    g_cache_system = new CacheSystem();
}

} // namespace App

const char* EnumToStr(AppState v)
{
    switch (v)
    {
    case AppState::BOOTSTRAP:           return "BOOTSTRAP";
    case AppState::MAIN_MENU:           return "MAIN_MENU";
    case AppState::PRINT_HELP_EXIT:     return "PRINT_HELP_EXIT";
    case AppState::PRINT_VERSION_EXIT:  return "PRINT_VERSION_EXIT";
    case AppState::SHUTDOWN:            return "SHUTDOWN";
    case AppState::SIMULATION:          return "SIMULATION";
    default:                            return "~invalid~";
    }
}

const char* EnumToStr(MpState v)
{
    switch (v)
    {
    case MpState::DISABLED:  return "DISABLED";
    case MpState::CONNECTED: return "CONNECTED";
    default:                 return "~invalid~";
    }
}

const char* EnumToStr(SimState v)
{
    switch (v)
    {
    case SimState::OFF        : return "OFF";
    case SimState::RUNNING    : return "RUNNING";
    case SimState::PAUSED     : return "PAUSED";
    case SimState::SELECTING  : return "SELECTING";
    case SimState::EDITOR_MODE: return "EDITOR_MODE";
    default                   : return "~invalid~";
    }
}

const char* EnumToStr(SimGearboxMode v)
{
    switch (v)
    {
    case SimGearboxMode::AUTO         : return "AUTO";
    case SimGearboxMode::SEMI_AUTO    : return "SEMI_AUTO";
    case SimGearboxMode::MANUAL       : return "MANUAL";
    case SimGearboxMode::MANUAL_STICK : return "MANUAL_STICK";
    case SimGearboxMode::MANUAL_RANGES: return "MANUAL_RANGES";
    default                           : return "~invalid~";
    }
}

const char* EnumToStr(GfxFlaresMode v)
{
    switch (v)
    {
    case GfxFlaresMode::NONE                   : return "NONE";
    case GfxFlaresMode::NO_LIGHTSOURCES        : return "NO_LIGHTSOURCES";
    case GfxFlaresMode::CURR_VEHICLE_HEAD_ONLY : return "CURR_VEHICLE_HEAD_ONLY";
    case GfxFlaresMode::ALL_VEHICLES_HEAD_ONLY : return "ALL_VEHICLES_HEAD_ONLY";
    case GfxFlaresMode::ALL_VEHICLES_ALL_LIGHTS: return "ALL_VEHICLES_ALL_LIGHTS";
    default                                    : return "~invalid~";
    }
}

const char* EnumToStr(GfxVegetation v)
{
    switch(v)
    {
    case GfxVegetation::NONE    : return "NONE";
    case GfxVegetation::x20PERC : return "20%";
    case GfxVegetation::x50PERC : return "50%";
    case GfxVegetation::FULL    : return "FULL";
    default                     : return "~invalid~";
    }
}

const char* EnumToStr(GfxWaterMode v)
{
    switch(v)
    {
    case GfxWaterMode::NONE      : return "NONE";
    case GfxWaterMode::BASIC     : return "BASIC";
    case GfxWaterMode::REFLECT   : return "REFLECT";
    case GfxWaterMode::FULL_FAST : return "FULL_FAST";
    case GfxWaterMode::FULL_HQ   : return "FULL_HQ";
    case GfxWaterMode::HYDRAX    : return "HYDRAX";
    default                      : return "~invalid~";
    }
}

const char* EnumToStr(GfxSkyMode v)
{
    switch(v)
    {
    case GfxSkyMode::SANDSTORM: return "SANDSTORM";
    case GfxSkyMode::CAELUM   : return "CAELUM";
    case GfxSkyMode::SKYX     : return "SKYX";
    default                   : return "~invalid~";
    }
}

const char* EnumToStr(IoInputGrabMode v)
{
    switch (v)
    {
    case IoInputGrabMode::NONE   : return "NONE";
    case IoInputGrabMode::ALL    : return "ALL";
    case IoInputGrabMode::DYNAMIC: return "DYNAMIC";
    default                      : return "~invalid~";
    }
}

const char* EnumToStr(GfxShadowType v)
{
    switch(v)
    {
    case GfxShadowType::NONE   : return "NONE";
    case GfxShadowType::TEXTURE: return "TEXTURE";
    case GfxShadowType::PSSM   : return "PSSM";
    default                    : return "~invalid~";
    }
}

const char* EnumToStr(GfxTexFilter v)
{
    switch (v)
    {
    case GfxTexFilter::NONE       : return "NONE";
    case GfxTexFilter::BILINEAR   : return "BILINEAR";
    case GfxTexFilter::TRILINEAR  : return "TRILINEAR";
    case GfxTexFilter::ANISOTROPIC: return "ANISOTROPIC";
    default                       : return "~invalid~";
    }
}

const char* EnumToStr(GfxExtCamMode v)
{
    switch (v)
    {
    case GfxExtCamMode::NONE:     return "NONE";
    case GfxExtCamMode::STATIC:   return "STATIC";
    case GfxExtCamMode::PITCHING: return "PITCHING";
    default:                      return "~invalid~";
    }
}

void Log(const char* msg)
{
    Ogre::LogManager::getSingleton().logMessage(msg);
}

void LogFormat(const char* format, ...)
{
    char buffer[2000] = {};

    va_list args;
    va_start(args, format);
        vsprintf(buffer, format, args);
    va_end(args);

    RoR::Log(buffer);
}

} // namespace RoR
