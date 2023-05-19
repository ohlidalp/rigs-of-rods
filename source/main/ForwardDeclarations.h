/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2020 Petr Ohlidal

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


/// @file   ForwardDeclarations.h
/// @brief  Global forward declarations.
/// @author Petr Ohlidal
/// @date   12/2013

#include "RefCountingObjectPtr.h"

#pragma once

namespace RoR
{
    typedef int ActorInstanceID_t; //!< Unique sequentially generated ID of an actor in session. Use `ActorManager::GetActorById()`
    static const ActorInstanceID_t ACTORINSTANCEID_INVALID = -1;

    typedef uint16_t NodeNum_t; //!< Node index in `Actor::ar_nodes`; use RoR::NODENUM_INVALID as empty value.
    static const NodeNum_t NODENUM_INVALID = std::numeric_limits<NodeNum_t>::max();

    typedef int BeamID_t; //!< Beam position within `Actor::ar_beams`, use RoR::BEAMID_INVALID as empty value.
    static const BeamID_t BEAMID_INVALID = -1;

    typedef int CameraID_t; //!< Index to `Actor::ar_cameras`, use RoR::CAMERAID_INVALID as empty value.
    static const CameraID_t CAMERAID_INVALID = -1;

    typedef int RopableID_t; //!< Index to `Actor::ar_ropables`, use RoR::ROPABLEID_INVALID as empty value.
    static const RopableID_t ROPABLEID_INVALID = -1;

    typedef int16_t BboxID_t; //!< Index to `Actor::ar_collision_bounding_boxes`, use `RoR::BBOXID_INVALID` as empty value.
    static const BboxID_t BBOXID_INVALID = -1;

    class  Actor;
    class  ActorManager;
    class  ActorSpawner;
    class  AeroEngine;
    class  Airbrake;
    class  Airfoil;
    class  AppContext;
    class  Autopilot;
    class  Buoyance;
    class  CacheEntry;
    class  CacheSystem;
    class  CameraManager;
    class  Character;
    class  Collisions;
    class  ConfigFile;
    class  Console;
    class  ContentManager;
    class  CVar;
    class  DashBoard;
    class  DashBoardManager;
    class  DustPool;
    class  DiscordRpc;
    class  EngineSim;
    class  Flexable;
    class  FlexAirfoil;
    class  FlexBody;
    class  FlexBodyFileIO;
    struct FlexBodyCacheData;
    class  FlexFactory;
    class  FlexMeshWheel;
    class  FlexObj;
    class  ForceFeedback;
    class  GameContext;
    class  GameScript;
    class  GfxActor;
    struct GfxCharacter;
    class  GfxEnvmap;
    class  GfxScene;
    class  GUIManager;
    struct GuiManagerImpl;
    class  HydraxWater;
    class  InputEngine;
    class  IWater;
    class  Landusemap;
    class  LanguageEngine;
    class  LocalStorage;
    class  MovableText;
    class  MumbleIntegration;
    class  OutGauge;
    class  OverlayWrapper;
    class  Network;
    class  OgreSubsystem;
    struct PlatformUtils;
    class  PointColDetector;
    class  ProceduralManager;
    struct ProceduralObject;
    struct ProceduralPoint;
    class  ProceduralRoad;
    struct Prop;
    struct PropAnim;
    class  RailGroup;
    class  Renderdash;
    class  Replay;
    class  RigLoadingProfiler;
    class  Screwprop;
    class  ScriptEngine;
    class  ShadowManager;
    class  Skidmark;
    class  SkidmarkConfig;
    struct SkinDef;
    class  SkinManager;
    class  SkyManager;
    class  SkyXManager;
    class  SlideNode;
    class  Sound;
    class  SoundManager;
    class  SoundScriptInstance;
    class  SoundScriptManager;
    class  SoundScriptTemplate;
    class  Task;
    class  TerrainEditor;
    class  TerrainGeometryManager;
    class  Terrain;
    class  TerrainObjectManager;
    struct Terrn2Author;
    struct Terrn2Def;
    class  Terrn2Parser;
    struct Terrn2Telepoint;
    class  TorqueCurve;
    class  ThreadPool;
    class  VehicleAI;
    class  VideoCamera;

    // SimData.h
    struct node_t;
    struct beam_t;
    struct shock_t;
    struct eventsource_t;
    struct soundsource_t;
    struct rigidifier_t;
    struct wheel_t;
    struct vwheel_t;
    struct ropable_t;
    struct wing_t;
    struct command_t;
    struct rotator_t;
    struct flare_t;
    struct rope_t;
    struct exhaust_t;
    struct cparticle_t;
    struct collision_box_t;
    struct tie_t;
    struct hook_t;
    struct ground_model_t;
    struct client_t;
    struct authorinfo_t;

    typedef RefCountingObjectPtr<Actor> ActorPtr;
    typedef RefCountingObjectPtr<LocalStorage> LocalStoragePtr;
    typedef RefCountingObjectPtr<ProceduralPoint> ProceduralPointPtr;
    typedef RefCountingObjectPtr<ProceduralObject> ProceduralObjectPtr;
    typedef RefCountingObjectPtr<ProceduralRoad> ProceduralRoadPtr;
    typedef RefCountingObjectPtr<ProceduralManager> ProceduralManagerPtr;
    typedef RefCountingObjectPtr<Sound> SoundPtr;
    typedef RefCountingObjectPtr<SoundScriptInstance> SoundScriptInstancePtr;
    typedef RefCountingObjectPtr<SoundScriptTemplate> SoundScriptTemplatePtr;
    typedef RefCountingObjectPtr<Terrain> TerrainPtr;
    typedef RefCountingObjectPtr<VehicleAI> VehicleAIPtr;

    namespace GUI
    {
        class  ConsoleView;
        class  Dialog;
        class  FrictionSettings;
        class  GameControls;
        class  GameMainMenu;
        class  LoadingWindow;
        class  MainSelector;
        class  MpClientList;
        class  MultiplayerSelector;
        class  RepositorySelector;
        class  DirectionArrow;
        class  SceneMouse;
        class  SimActorStats;
        class  SurveyMap;
        class  TopMenubar;
        class  VehicleButtons;
    }
} // namespace RoR

namespace RoRnet
{
    struct Header;
    struct UserInfo;
    struct StreamRegister;
    struct ActorStreamRegister;
    struct ServerInfo;
    struct VehicleState;
}

#ifdef USE_SOCKETW
class SWBaseSocket;
#endif // USE_SOCKETW
