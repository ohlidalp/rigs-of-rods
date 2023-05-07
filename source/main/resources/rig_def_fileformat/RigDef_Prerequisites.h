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

/// @file
/// @author Petr Ohlidal
/// @date   12/2013

#pragma once

#include <memory> //shared_ptr

namespace Ogre
{
    class DataStream;
}

namespace RigDef {

// IMPORTANT! If you add a value here, you must also modify Regexes::IDENTIFY_KEYWORD, it relies on numeric values of this enum.
enum class Keyword
{
    INVALID = 0,

    ADD_ANIMATION = 1,
    AIRBRAKES,
    ANIMATORS,
    ANTILOCKBRAKES,
    AUTHOR,
    AXLES,
    BACKMESH,
    BEAMS,
    BRAKES,
    CAB,
    CAMERARAIL,
    CAMERAS,
    CINECAM,
    COLLISIONBOXES,
    COMMANDS,
    COMMANDS2,
    COMMENT,
    CONTACTERS,
    CRUISECONTROL,
    DEFAULT_SKIN,
    DESCRIPTION,
    DETACHER_GROUP,
    DISABLEDEFAULTSOUNDS,
    ENABLE_ADVANCED_DEFORMATION,
    END,
    END_COMMENT,
    END_DESCRIPTION,
    END_SECTION,
    ENGINE,
    ENGOPTION,
    ENGTURBO,
    ENVMAP,
    EXHAUSTS,
    EXTCAMERA,
    FILEFORMATVERSION,
    FILEINFO,
    FIXES,
    FLARES,
    FLARES2,
    FLARES3,
    FLEXBODIES,
    FLEXBODY_CAMERA_MODE,
    FLEXBODYWHEELS,
    FORSET,
    FORWARDCOMMANDS,
    FUSEDRAG,
    GLOBALS,
    GUID,
    GUISETTINGS,
    HELP,
    HIDEINCHOOSER,
    HOOKGROUP, // obsolete, ignored
    HOOKS,
    HYDROS,
    IMPORTCOMMANDS,
    INTERAXLES,
    LOCKGROUPS,
    LOCKGROUP_DEFAULT_NOLOCK,
    MANAGEDMATERIALS,
    MATERIALFLAREBINDINGS,
    MESHWHEELS,
    MESHWHEELS2,
    MINIMASS,
    NODECOLLISION, // obsolete
    NODES,
    NODES2,
    PARTICLES,
    PISTONPROPS,
    PROP_CAMERA_MODE,
    PROPS,
    RAILGROUPS,
    RESCUER,
    RIGIDIFIERS, // obsolete
    ROLLON,
    ROPABLES,
    ROPES,
    ROTATORS,
    ROTATORS2,
    SCREWPROPS,
    SCRIPTS,
    SECTION,
    SECTIONCONFIG,
    SET_BEAM_DEFAULTS,
    SET_BEAM_DEFAULTS_SCALE,
    SET_COLLISION_RANGE,
    SET_DEFAULT_MINIMASS,
    SET_INERTIA_DEFAULTS,
    SET_MANAGEDMATERIALS_OPTIONS,
    SET_NODE_DEFAULTS,
    SET_SHADOWS,
    SET_SKELETON_SETTINGS,
    SHOCKS,
    SHOCKS2,
    SHOCKS3,
    SLIDENODE_CONNECT_INSTANTLY,
    SLIDENODES,
    SLOPE_BRAKE,
    SOUNDSOURCES,
    SOUNDSOURCES2,
    SPEEDLIMITER,
    SUBMESH,
    SUBMESH_GROUNDMODEL,
    TEXCOORDS,
    TIES,
    TORQUECURVE,
    TRACTIONCONTROL,
    TRANSFERCASE,
    TRIGGERS,
    TURBOJETS,
    TURBOPROPS,
    TURBOPROPS2,
    VIDEOCAMERA,
    WHEELDETACHERS,
    WHEELS,
    WHEELS2,
    WINGS
};

// File structures declarations
// TODO: Complete list

struct Document;
typedef std::shared_ptr<Document> DocumentPtr;

struct AeroAnimator;
struct Airbrake;
struct Animation;
struct AntiLockBrakes;
struct Axle;
struct Beam;
struct BeamDefaults;
struct BeamDefaultsScale;
struct Brakes;
struct Cab;
struct CameraRail;
struct CameraSettings;
struct Cinecam;
struct CollisionBox;
struct Command2;
struct CruiseControl;
struct DefaultMinimass;
struct Engine;
struct Engoption;
struct Engturbo;
struct ExtCamera;
struct Flare;
struct Flare2;
struct Flexbody;
struct FlexBodyWheel;
struct Fusedrag;
struct Globals;
struct GuiSettings;
struct Hook;
struct Hydro;
struct Inertia;
struct Lockgroup;
struct ManagedMaterialsOptions;
struct MeshWheel;
struct Node;
struct NodeDefaults;
struct Particle;
struct Pistonprop;
struct Prop;
struct RailGroup;
struct Ropable;
struct ShadowOptions;
struct VideoCamera;

// Parser classes

class Parser;
class Validator;
class SequentialImporter;

} // namespace RigDef
