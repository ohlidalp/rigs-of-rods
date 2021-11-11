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

/** 
    @file   RigDef_File.h
    @author Petr Ohlidal
    @brief Structures which represent a rig-definition file (1:1 match)
           See https://docs.rigsofrods.org/vehicle-creation/fileformat-truck/ for reference.
           Values prefixed by `_` are helper data, for example argument count (where it matters).
*/

#pragma once

#include "Application.h"
#include "BitFlags.h"
#include "SimConstants.h"
#include "SimData.h"

#include <list>
#include <memory>
#include <vector>
#include <string>
#include <OgreString.h>
#include <OgreVector3.h>
#include <OgreStringConverter.h>

namespace RigDef {

/// Any line referencing a node must assume the node may be named.
typedef std::string NodeRef_t;
extern const NodeRef_t NODEREF_INVALID;

// IMPORTANT! If you add a value here, you must also modify Regexes::IDENTIFY_KEYWORD, it relies on numeric values of this enum.
enum Keyword
{
    KEYWORD_ADD_ANIMATION = 1,
    KEYWORD_AIRBRAKES,
    KEYWORD_ANIMATORS,
    KEYWORD_ANTILOCKBRAKES,
    KEYWORD_AUTHOR,
    KEYWORD_AXLES,
    KEYWORD_BACKMESH,
    KEYWORD_BEAMS,
    KEYWORD_BRAKES,
    KEYWORD_CAB,
    KEYWORD_CAMERARAIL,
    KEYWORD_CAMERAS,
    KEYWORD_CINECAM,
    KEYWORD_COLLISIONBOXES,
    KEYWORD_COMMANDS,
    KEYWORD_COMMANDS2,
    KEYWORD_COMMENT,
    KEYWORD_CONTACTERS,
    KEYWORD_CRUISECONTROL,
    KEYWORD_DESCRIPTION,
    KEYWORD_DETACHER_GROUP,
    KEYWORD_DISABLEDEFAULTSOUNDS,
    KEYWORD_ENABLE_ADVANCED_DEFORMATION,
    KEYWORD_END,
    KEYWORD_END_COMMENT,
    KEYWORD_END_DESCRIPTION,
    KEYWORD_END_SECTION,
    KEYWORD_ENGINE,
    KEYWORD_ENGOPTION,
    KEYWORD_ENGTURBO,
    KEYWORD_ENVMAP,
    KEYWORD_EXHAUSTS,
    KEYWORD_EXTCAMERA,
    KEYWORD_FILEFORMATVERSION,
    KEYWORD_FILEINFO,
    KEYWORD_FIXES,
    KEYWORD_FLARES,
    KEYWORD_FLARES2,
    KEYWORD_FLEXBODIES,
    KEYWORD_FLEXBODY_CAMERA_MODE,
    KEYWORD_FLEXBODYWHEELS,
    KEYWORD_FORSET,
    KEYWORD_FORWARDCOMMANDS,
    KEYWORD_FUSEDRAG,
    KEYWORD_GLOBALS,
    KEYWORD_GUID,
    KEYWORD_GUISETTINGS,
    KEYWORD_HELP,
    KEYWORD_HIDEINCHOOSER,
    KEYWORD_HOOKGROUP, // obsolete, ignored
    KEYWORD_HOOKS,
    KEYWORD_HYDROS,
    KEYWORD_IMPORTCOMMANDS,
    KEYWORD_INTERAXLES,
    KEYWORD_LOCKGROUPS,
    KEYWORD_LOCKGROUP_DEFAULT_NOLOCK,
    KEYWORD_MANAGEDMATERIALS,
    KEYWORD_MATERIALFLAREBINDINGS,
    KEYWORD_MESHWHEELS,
    KEYWORD_MESHWHEELS2,
    KEYWORD_MINIMASS,
    KEYWORD_NODECOLLISION, // obsolete
    KEYWORD_NODES,
    KEYWORD_NODES2,
    KEYWORD_PARTICLES,
    KEYWORD_PISTONPROPS,
    KEYWORD_PROP_CAMERA_MODE,
    KEYWORD_PROPS,
    KEYWORD_RAILGROUPS,
    KEYWORD_RESCUER,
    KEYWORD_RIGIDIFIERS, // obsolete
    KEYWORD_ROLLON,
    KEYWORD_ROPABLES,
    KEYWORD_ROPES,
    KEYWORD_ROTATORS,
    KEYWORD_ROTATORS2,
    KEYWORD_SCREWPROPS,
    KEYWORD_SECTION,
    KEYWORD_SECTIONCONFIG,
    KEYWORD_SET_BEAM_DEFAULTS,
    KEYWORD_SET_BEAM_DEFAULTS_SCALE,
    KEYWORD_SET_COLLISION_RANGE,
    KEYWORD_SET_DEFAULT_MINIMASS,
    KEYWORD_SET_INERTIA_DEFAULTS,
    KEYWORD_SET_MANAGEDMATERIALS_OPTIONS,
    KEYWORD_SET_NODE_DEFAULTS,
    KEYWORD_SET_SHADOWS,
    KEYWORD_SET_SKELETON_SETTINGS,
    KEYWORD_SHOCKS,
    KEYWORD_SHOCKS2,
    KEYWORD_SHOCKS3,
    KEYWORD_SLIDENODE_CONNECT_INSTANTLY,
    KEYWORD_SLIDENODES,
    KEYWORD_SLOPE_BRAKE,
    KEYWORD_SOUNDSOURCES,
    KEYWORD_SOUNDSOURCES2,
    KEYWORD_SPEEDLIMITER,
    KEYWORD_SUBMESH,
    KEYWORD_SUBMESH_GROUNDMODEL,
    KEYWORD_TEXCOORDS,
    KEYWORD_TIES,
    KEYWORD_TORQUECURVE,
    KEYWORD_TRACTIONCONTROL,
    KEYWORD_TRANSFERCASE,
    KEYWORD_TRIGGERS,
    KEYWORD_TURBOJETS,
    KEYWORD_TURBOPROPS,
    KEYWORD_TURBOPROPS2,
    KEYWORD_VIDEOCAMERA,
    KEYWORD_WHEELDETACHERS,
    KEYWORD_WHEELS,
    KEYWORD_WHEELS2,
    KEYWORD_WINGS,

    KEYWORD_INVALID = -1
};

enum class DifferentialType: char
{
    DIFF_o_OPEN    = 'o',
    DIFF_l_LOCKED  = 'l',
    DIFF_s_SPLIT   = 's',
    DIFF_v_VISCOUS = 'v'
};

enum WheelBraking
{
    BRAKING_NO                = 0,
    BRAKING_YES               = 1,
    BRAKING_DIRECTIONAL_LEFT  = 2,
    BRAKING_DIRECTIONAL_RIGHT = 3,
    BRAKING_ONLY_FOOT         = 4,

    BRAKING_INVALID           = -1
};

enum WheelPropulsion
{
    PROPULSION_NONE     = 0,
    PROPULSION_FORWARD  = 1,
    PROPULSION_BACKWARD = 2,

    PROPULSION_INVALID  = -1
};

enum WheelSide
{
    SIDE_INVALID   = 0,
    SIDE_RIGHT     = 'r',
    SIDE_LEFT      = 'l'
};

enum class NodeOption: char
{
    n_MOUSE_GRAB        = 'n',
    m_NO_MOUSE_GRAB     = 'm',
    f_NO_SPARKS         = 'f',
    x_EXHAUST_POINT     = 'x',
    y_EXHAUST_DIRECTION = 'y',
    c_NO_GROUND_CONTACT = 'c',
    h_HOOK_POINT        = 'h',
    e_TERRAIN_EDIT_POINT= 'e',
    b_EXTRA_BUOYANCY    = 'b',
    p_NO_PARTICLES      = 'p',
    L_LOG               = 'L',
    l_LOAD_WEIGHT       = 'l',
};

enum class HydroOption: char
{
    n_NORMAL                    = 'n',
    i_INVISIBLE                 = 'i',
    // Useful for trucks
    s_DISABLE_ON_HIGH_SPEED     = 's',
    // Useful for planes: These can be used to control flight surfaces, or to create a thrust vectoring system.
    a_INPUT_AILERON             = 'a',
    r_INPUT_RUDDER              = 'r',
    e_INPUT_ELEVATOR            = 'e',
    u_INPUT_AILERON_ELEVATOR    = 'u',
    v_INPUT_InvAILERON_ELEVATOR = 'v',
    x_INPUT_AILERON_RUDDER      = 'x',
    y_INPUT_InvAILERON_RUDDER   = 'y',
    g_INPUT_ELEVATOR_RUDDER     = 'g',
    h_INPUT_InvELEVATOR_RUDDER  = 'h',
};

enum class SpecialProp
{
    MIRROR_LEFT = 1,
    MIRROR_RIGHT,
    DASHBOARD_LEFT,
    DASHBOARD_RIGHT,
    AERO_PROP_SPIN,
    AERO_PROP_BLADE,
    DRIVER_SEAT,
    DRIVER_SEAT_2,
    BEACON,
    REDBEACON,
    LIGHTBAR,

    INVALID = -1
};

enum class ManagedMaterialType
{
    FLEXMESH_STANDARD = 1,
    FLEXMESH_TRANSPARENT,
    MESH_STANDARD,
    MESH_TRANSPARENT,

    INVALID = -1
};

enum class WingControlSurface: char
{
    n_NONE                  = 'n',
    a_RIGHT_AILERON         = 'a',
    b_LEFT_AILERON          = 'b',
    f_FLAP                  = 'f',
    e_ELEVATOR              = 'e',
    r_RUDDER                = 'r',
    S_RIGHT_HAND_STABILATOR = 'S',
    T_LEFT_HAND_STABILATOR  = 'T',
    c_RIGHT_ELEVON          = 'c',
    d_LEFT_ELEVON           = 'd',
    g_RIGHT_FLAPERON        = 'g',
    h_LEFT_FLAPERON         = 'h',
    U_RIGHT_HAND_TAILERON   = 'U',
    V_LEFT_HAND_TAILERON    = 'V',
    i_RIGHT_RUDDERVATOR     = 'i',
    j_LEFT_RUDDERVATOR      = 'j',

    INVALID                 = 0
};

    // --------------------------------
    // Common line data


struct CameraModeCommon
{
    enum Mode
    {
        MODE_ALWAYS   = -2,
        MODE_EXTERNAL = -1,
        MODE_CINECAM  = 1,

        MODE_INVALID  = 0xFFFFFFFF
    };

    Mode mode = MODE_ALWAYS;
    unsigned int cinecam_index = 0;
};

struct InertiaCommon
{
    int _num_args = -1;

    float start_delay_factor = 0.f;
    float stop_delay_factor = 0.f;
    std::string start_function;
    std::string stop_function;
};

struct CommandsCommon
{
    NodeRef_t nodes[2];
    float max_contraction = 0.f;
    float max_extension = 0.f;
    unsigned int contract_key = 0u;
    unsigned int extend_key = 0u;
    std::string description;
    InertiaCommon inertia;
    float affect_engine = 1.f;
    bool needs_engine = true;
    bool plays_sound = true;

    bool option_i_invisible = false;
    bool option_r_rope = false;
    bool option_c_auto_center = false;
    bool option_f_not_faster = false;
    bool option_p_1press = false;
    bool option_o_1press_center = false;
};

struct FlaresCommon
{
    NodeRef_t reference_node;
    NodeRef_t node_axis_x;
    NodeRef_t node_axis_y;
    Ogre::Vector3 offset = Ogre::Vector3(0, 0, 1);
    RoR::FlareType type = RoR::FlareType::HEADLIGHT;
    int control_number; //!< Only 'u' type flares.
    std::string dashboard_link; //!< Only 'd' type flares.
    int blink_delay_milis;
    float size;
    std::string material_name;
};

struct NodeRangeCommon
{
    NodeRangeCommon(NodeRef_t _first, NodeRef_t _last): first(_first), last(_last){}

    NodeRef_t first;
    NodeRef_t last;
};

struct NodesCommon
{
    int _num_args = -1;
    Ogre::Vector3 position;
    float loadweight_override = -1.f;
    std::string options;
};

struct RotatorsCommon
{
    NodeRef_t axis_nodes[2];
    NodeRef_t base_plate_nodes[4];
    NodeRef_t rotating_plate_nodes[4];

    float rate = 0.f;
    unsigned int spin_left_key = 0u;
    unsigned int spin_right_key = 0u;
    InertiaCommon inertia;
    float engine_coupling = 1.f;
    bool needs_engine = false;
};

struct SoundsourcesCommon
{
    NodeRef_t node;
    std::string sound_script_name;
};

struct TurbopropsCommon
{
    NodeRef_t reference_node;
    NodeRef_t axis_node;
    NodeRef_t blade_tip_nodes[4];
    float turbine_power_kW = 0.f;
    std::string airfoil;
};

struct WheelsCommon
{
    float width;
    unsigned int num_rays;
    NodeRef_t nodes[2];
    NodeRef_t rigidity_node;
    WheelBraking braking = BRAKING_NO;
    WheelPropulsion propulsion;
    NodeRef_t reference_arm_node;
    float mass;
};

struct Wheels2Common: WheelsCommon
{
    float rim_radius;
    float tyre_radius;
    float tyre_springiness;
    float tyre_damping;
};

struct MeshwheelsShared: public WheelsCommon
{
    WheelSide side = SIDE_INVALID;
    std::string mesh_name;
    std::string material_name;
    float rim_radius;
    float tyre_radius;
    float spring;
    float damping;
};


    // --------------------------------
    // Individual lines, alphabetically


struct AddAnimationLine
{
    struct MotorSource
    {
        static const unsigned int SOURCE_AERO_THROTTLE = BITMASK(1);
        static const unsigned int SOURCE_AERO_RPM      = BITMASK(2);
        static const unsigned int SOURCE_AERO_TORQUE   = BITMASK(3);
        static const unsigned int SOURCE_AERO_PITCH    = BITMASK(4);
        static const unsigned int SOURCE_AERO_STATUS   = BITMASK(5);

        unsigned int source = 0;
        unsigned int motor = 0;
    };

    BITMASK_PROPERTY( source,  1, SOURCE_AIRSPEED          , HasSource_AirSpeed            , SetHasSource_AirSpeed )
    BITMASK_PROPERTY( source,  2, SOURCE_VERTICAL_VELOCITY , HasSource_VerticalVelocity    , SetHasSource_VerticalVelocity )
    BITMASK_PROPERTY( source,  3, SOURCE_ALTIMETER_100K    , HasSource_AltiMeter100k       , SetHasSource_AltiMeter100k )
    BITMASK_PROPERTY( source,  4, SOURCE_ALTIMETER_10K     , HasSource_AltiMeter10k        , SetHasSource_AltiMeter10k )
    BITMASK_PROPERTY( source,  5, SOURCE_ALTIMETER_1K      , HasSource_AltiMeter1k         , SetHasSource_AltiMeter1k )
    BITMASK_PROPERTY( source,  6, SOURCE_ANGLE_OF_ATTACK   , HasSource_AOA                 , SetHasSource_AOA )
    BITMASK_PROPERTY( source,  7, SOURCE_FLAP              , HasSource_Flap                , SetHasSource_Flap )
    BITMASK_PROPERTY( source,  8, SOURCE_AIR_BRAKE         , HasSource_AirBrake            , SetHasSource_AirBrake )
    BITMASK_PROPERTY( source,  9, SOURCE_ROLL              , HasSource_Roll                , SetHasSource_Roll )
    BITMASK_PROPERTY( source, 10, SOURCE_PITCH             , HasSource_Pitch               , SetHasSource_Pitch )
    BITMASK_PROPERTY( source, 11, SOURCE_BRAKES            , HasSource_Brakes              , SetHasSource_Brakes )
    BITMASK_PROPERTY( source, 12, SOURCE_ACCEL             , HasSource_Accel               , SetHasSource_Accel )
    BITMASK_PROPERTY( source, 13, SOURCE_CLUTCH            , HasSource_Clutch              , SetHasSource_Clutch )
    BITMASK_PROPERTY( source, 14, SOURCE_SPEEDO            , HasSource_Speedo              , SetHasSource_Speedo )
    BITMASK_PROPERTY( source, 15, SOURCE_TACHO             , HasSource_Tacho               , SetHasSource_Tacho )
    BITMASK_PROPERTY( source, 16, SOURCE_TURBO             , HasSource_Turbo               , SetHasSource_Turbo )
    BITMASK_PROPERTY( source, 17, SOURCE_PARKING           , HasSource_ParkingBrake        , SetHasSource_ParkingBrake )
    BITMASK_PROPERTY( source, 18, SOURCE_SHIFT_LEFT_RIGHT  , HasSource_ManuShiftLeftRight  , SetHasSource_ManuShiftLeftRight )
    BITMASK_PROPERTY( source, 19, SOURCE_SHIFT_BACK_FORTH  , HasSource_ManuShiftBackForth  , SetHasSource_ManuShiftBackForth )
    BITMASK_PROPERTY( source, 20, SOURCE_SEQUENTIAL_SHIFT  , HasSource_SeqentialShift      , SetHasSource_SeqentialShift )
    BITMASK_PROPERTY( source, 21, SOURCE_SHIFTERLIN        , HasSource_ShifterLin          , SetHasSource_ShifterLin )
    BITMASK_PROPERTY( source, 22, SOURCE_TORQUE            , HasSource_Torque              , SetHasSource_Torque )
    BITMASK_PROPERTY( source, 23, SOURCE_HEADING           , HasSource_Heading             , SetHasSource_Heading )
    BITMASK_PROPERTY( source, 24, SOURCE_DIFFLOCK          , HasSource_DiffLock            , SetHasSource_DiffLock )
    BITMASK_PROPERTY( source, 25, SOURCE_BOAT_RUDDER       , HasSource_BoatRudder          , SetHasSource_BoatRudder )
    BITMASK_PROPERTY( source, 26, SOURCE_BOAT_THROTTLE     , HasSource_BoatThrottle        , SetHasSource_BoatThrottle )
    BITMASK_PROPERTY( source, 27, SOURCE_STEERING_WHEEL    , HasSource_SteeringWheel       , SetHasSource_SteeringWheel )
    BITMASK_PROPERTY( source, 28, SOURCE_AILERON           , HasSource_Aileron             , SetHasSource_Aileron )
    BITMASK_PROPERTY( source, 29, SOURCE_ELEVATOR          , HasSource_Elevator            , SetHasSource_Elevator )
    BITMASK_PROPERTY( source, 30, SOURCE_AIR_RUDDER        , HasSource_AerialRudder        , SetHasSource_AerialRudder )
    BITMASK_PROPERTY( source, 31, SOURCE_PERMANENT         , HasSource_Permanent           , SetHasSource_Permanent )
    BITMASK_PROPERTY( source, 32, SOURCE_EVENT             , HasSource_Event               , SetHasSource_Event ) // Full house32

    static const unsigned int MODE_ROTATION_X          = BITMASK(1);
    static const unsigned int MODE_ROTATION_Y          = BITMASK(2);
    static const unsigned int MODE_ROTATION_Z          = BITMASK(3);
    static const unsigned int MODE_OFFSET_X            = BITMASK(4);
    static const unsigned int MODE_OFFSET_Y            = BITMASK(5);
    static const unsigned int MODE_OFFSET_Z            = BITMASK(6);
    static const unsigned int MODE_AUTO_ANIMATE        = BITMASK(7);
    static const unsigned int MODE_NO_FLIP             = BITMASK(8);
    static const unsigned int MODE_BOUNCE              = BITMASK(9);
    static const unsigned int MODE_EVENT_LOCK          = BITMASK(10);

    float ratio;
    float lower_limit;
    float upper_limit;
    unsigned int source;
    std::list<MotorSource> motor_sources;
    unsigned int mode;

    // NOTE: MSVC highlights 'event' as keyword: http://msdn.microsoft.com/en-us/library/4b612y2s%28v=vs.100%29.aspx
    // But it's ok to use as identifier in this context: http://msdn.microsoft.com/en-us/library/8d7y7wz6%28v=vs.100%29.aspx
    std::string event;

    void AddMotorSource(unsigned int source, unsigned int motor)
    {
        MotorSource motor_source;
        motor_source.source = source;
        motor_source.motor = motor;
        this->motor_sources.push_back(motor_source);
    }
};

struct AirbrakesLine
{
    NodeRef_t reference_node;
    NodeRef_t x_axis_node;
    NodeRef_t y_axis_node;
    NodeRef_t aditional_node;
    Ogre::Vector3 offset = Ogre::Vector3::ZERO;
    float width = -1.f;
    float height = -1.f;
    float max_inclination_angle = -1.f;
    float texcoord_x1 = -1.f;
    float texcoord_x2 = -1.f;
    float texcoord_y1 = -1.f;
    float texcoord_y2 = -1.f;
    float lift_coefficient = -1.f;
};

struct AnimatorsLine
{
    struct AeroAnimator
    {
        static const unsigned int OPTION_THROTTLE = BITMASK(1);
        static const unsigned int OPTION_RPM      = BITMASK(2);
        static const unsigned int OPTION_TORQUE   = BITMASK(3);
        static const unsigned int OPTION_PITCH    = BITMASK(4);
        static const unsigned int OPTION_STATUS   = BITMASK(5);

        unsigned int flags = 0u;
        unsigned int motor = 0u;
    };

    static const unsigned int OPTION_VISIBLE           = BITMASK(1);
    static const unsigned int OPTION_INVISIBLE         = BITMASK(2);
    static const unsigned int OPTION_AIRSPEED          = BITMASK(3);
    static const unsigned int OPTION_VERTICAL_VELOCITY = BITMASK(4);
    static const unsigned int OPTION_ALTIMETER_100K    = BITMASK(5);
    static const unsigned int OPTION_ALTIMETER_10K     = BITMASK(6);
    static const unsigned int OPTION_ALTIMETER_1K      = BITMASK(7);
    static const unsigned int OPTION_ANGLE_OF_ATTACK   = BITMASK(8);
    static const unsigned int OPTION_FLAP              = BITMASK(9);
    static const unsigned int OPTION_AIR_BRAKE         = BITMASK(10);
    static const unsigned int OPTION_ROLL              = BITMASK(11);
    static const unsigned int OPTION_PITCH             = BITMASK(12);
    static const unsigned int OPTION_BRAKES            = BITMASK(13);
    static const unsigned int OPTION_ACCEL             = BITMASK(14);
    static const unsigned int OPTION_CLUTCH            = BITMASK(15);
    static const unsigned int OPTION_SPEEDO            = BITMASK(16);
    static const unsigned int OPTION_TACHO             = BITMASK(17);
    static const unsigned int OPTION_TURBO             = BITMASK(18);
    static const unsigned int OPTION_PARKING           = BITMASK(19);
    static const unsigned int OPTION_SHIFT_LEFT_RIGHT  = BITMASK(20);
    static const unsigned int OPTION_SHIFT_BACK_FORTH  = BITMASK(21);
    static const unsigned int OPTION_SEQUENTIAL_SHIFT  = BITMASK(22);
    static const unsigned int OPTION_GEAR_SELECT       = BITMASK(23);
    static const unsigned int OPTION_TORQUE            = BITMASK(24);
    static const unsigned int OPTION_DIFFLOCK          = BITMASK(25);
    static const unsigned int OPTION_BOAT_RUDDER       = BITMASK(26);
    static const unsigned int OPTION_BOAT_THROTTLE     = BITMASK(27);
    static const unsigned int OPTION_SHORT_LIMIT       = BITMASK(28);
    static const unsigned int OPTION_LONG_LIMIT        = BITMASK(29);

    NodeRef_t nodes[2];
    float lenghtening_factor = 0.f;
    unsigned int flags = 0u;
    float short_limit = 0.f;
    float long_limit = 0.f;
    AeroAnimator aero_animator;
};

struct AntiLockBrakesLine
{
    float regulation_force = 0.f;
    unsigned int min_speed = 0;
    float pulse_per_sec = 0.f;
    bool attr_is_on = true;
    bool attr_no_dashboard = false;
    bool attr_no_toggle = false;
};

struct AuthorLine
{
    std::string type;
    unsigned int forum_account_id;
    std::string name;
    std::string email;
    bool _has_forum_account;
};

struct AxlesLine
{
    NodeRef_t wheels[2][2];
    std::vector<DifferentialType> options; //!< Order matters!
};

struct BeamsLine
{
    BeamsLine():
        options(0),
        extension_break_limit(0), /* This is default */
        _has_extension_break_limit(false)
    {}

    BITMASK_PROPERTY(options, 1, OPTION_i_INVISIBLE, HasFlag_i_Invisible, SetFlag_i_Invisible);
    BITMASK_PROPERTY(options, 2, OPTION_r_ROPE     , HasFlag_r_Rope     , SetFlag_r_Rope     );
    BITMASK_PROPERTY(options, 3, OPTION_s_SUPPORT  , HasFlag_s_Support  , SetFlag_s_Support  );

    NodeRef_t nodes[2];
    unsigned int options; //!< Bit flags
    float extension_break_limit;
    bool _has_extension_break_limit;
};

struct BrakesLine
{
    float default_braking_force = -1.f;
    float parking_brake_force = -1.f;
};

struct CabLine
{
    bool GetOption_D_ContactBuoyant()
    {
        return BITMASK_IS_1(options, OPTION_b_BUOYANT) && BITMASK_IS_1(options, OPTION_c_CONTACT);
    }

    bool GetOption_F_10xTougherBuoyant()
    {
        return BITMASK_IS_1(options, OPTION_b_BUOYANT) && BITMASK_IS_1(options, OPTION_p_10xTOUGHER);
    }

    bool GetOption_S_UnpenetrableBuoyant()
    {
        return BITMASK_IS_1(options, OPTION_b_BUOYANT) && BITMASK_IS_1(options, OPTION_u_INVULNERABLE);
    }

    static const unsigned int OPTION_c_CONTACT           = BITMASK(1);
    static const unsigned int OPTION_b_BUOYANT           = BITMASK(2);
    static const unsigned int OPTION_p_10xTOUGHER        = BITMASK(3);
    static const unsigned int OPTION_u_INVULNERABLE      = BITMASK(4);
    static const unsigned int OPTION_s_BUOYANT_NO_DRAG   = BITMASK(5);
    static const unsigned int OPTION_r_BUOYANT_ONLY_DRAG = BITMASK(6);

    NodeRef_t nodes[3];
    unsigned int options = 0u;
};

struct CamerasLine
{
    NodeRef_t center_node;
    NodeRef_t back_node;
    NodeRef_t left_node;
};

struct CinecamLine
{
    Ogre::Vector3 position = Ogre::Vector3::ZERO;
    NodeRef_t nodes[8];
    float spring = -1.f;
    float damping = -1.f;
    float node_mass = -1.f;
};

struct CollisionboxesLine
{
    std::vector<NodeRef_t> nodes;
};

struct CommandsLine: public CommandsCommon
{
    float rate = 0.f;
};

struct Commands2Line: public CommandsCommon
{
    float shorten_rate = 0.f;
    float lengthen_rate = 0.f;
};

struct CruisecontrolLine
{
    float min_speed;
    int autobrake;
};

struct EngineLine
{
    float shift_down_rpm = -1.f;
    float shift_up_rpm = -1.f;
    float torque = -1.f;
    float global_gear_ratio = -1.f;
    float reverse_gear_ratio = -1.f;
    float neutral_gear_ratio = -1.f;
    std::vector<float> gear_ratios;
};

struct EngoptionLine
{
    enum EngineType
    {
        ENGINE_TYPE_c_CAR   = 'c',
        ENGINE_TYPE_e_ECAR  = 'e',
        ENGINE_TYPE_t_TRUCK = 't',

        ENGINE_TYPE_INVALID = 0xFFFFFFFF
    };

    float inertia = 10.f;
    EngineType type = ENGINE_TYPE_t_TRUCK;
    float clutch_force = -1.f;
    float shift_time_sec = -1.f;
    float clutch_time_sec = -1.f;
    float post_shift_time_sec = -1.f;
    float idle_rpm = -1.f;
    float stall_rpm = -1.f;
    float max_idle_mixture = -1.f;
    float min_idle_mixture = -1.f;
    float braking_torque = -1.f;
};

struct EngturboLine
{
    int version = -1;
    float tinertiaFactor = 1.f;
    int nturbos = 1;
    float param1  = 9999;
    float param2  = 9999;
    float param3  = 9999;
    float param4  = 9999;
    float param5  = 9999;
    float param6  = 9999;
    float param7  = 9999;
    float param8  = 9999;
    float param9  = 9999;
    float param10 = 9999;
    float param11 = 9999;
};

struct ExhaustsLine
{
    NodeRef_t reference_node;
    NodeRef_t direction_node;
    std::string particle_name;
};

struct ExtcameraLine
{
    enum Mode
    {
        MODE_CLASSIC = 0, // Hardcoded in simulation code, do not change
        MODE_CINECAM = 1, // Hardcoded in simulation code, do not change
        MODE_NODE    = 2, // Hardcoded in simulation code, do not change

        MODE_INVALID = 0xFFFFFFFF
    };

    Mode mode = MODE_CLASSIC;
    NodeRef_t node;
};

struct FileinfoLine
{
    std::string unique_id;
    int category_id;
    int file_version;
};

struct FlaresLine: public FlaresCommon
{};

struct Flares2Line: public FlaresCommon
{};

struct FlexbodyCameraModeLine: public CameraModeCommon
{};

struct FlexbodiesLine
{
    NodeRef_t reference_node;
    NodeRef_t x_axis_node;
    NodeRef_t y_axis_node;
    Ogre::Vector3 offset = Ogre::Vector3::ZERO;
    Ogre::Vector3 rotation = Ogre::Vector3::ZERO;
    std::string mesh_name;
    std::list<AddAnimationLine> animations;
};

struct FlexbodywheelsLine: Wheels2Common
{
    WheelSide side = SIDE_INVALID;
    float rim_springiness;
    float rim_damping;
    std::string rim_mesh_name;
    std::string tyre_mesh_name;
};

struct ForsetLine
{
    std::vector<NodeRangeCommon> node_ranges;
};

struct FusedragLine
{
    bool autocalc = false;
    NodeRef_t front_node;
    NodeRef_t rear_node;
    float approximate_width;
    std::string airfoil_name = "NACA0009.afl";
    float area_coefficient = 1.f;
};

struct GlobalsLine
{
    float dry_mass = 0.f;
    float cargo_mass = 0.f;
    std::string material_name;
};

struct GuiSettingsLine
{
    std::string key;
    std::string value;
};

struct HooksLine
{
    NodeRef_t node;
    float option_hook_range = HOOK_RANGE_DEFAULT;
    float option_speed_coef = 1.f;
    float option_max_force = HOOK_FORCE_DEFAULT;
    int option_hookgroup = -1;
    int option_lockgroup = NODE_LOCKGROUP_DEFAULT;
    float option_timer = HOOK_LOCK_TIMER_DEFAULT;
    float option_min_range_meters = 0.f;
    bool flag_self_lock = false;
    bool flag_auto_lock = false;
    bool flag_no_disable = false;
    bool flag_no_rope = false;
    bool flag_visible = false;
};

struct HydrosLine
{
    NodeRef_t nodes[2];
    float lenghtening_factor = 0;
    std::string options;
    InertiaCommon inertia;
};

struct InteraxlesLine
{
    int a1 = 0;
    int a2 = 0;
    std::vector<DifferentialType> options; //!< Order matters!
};

struct LockgroupsLine
{
    int number = 0;
    std::vector<NodeRef_t> nodes;
};

struct ManagedmaterialsLine
{
    std::string name;
    ManagedMaterialType type;
    // Textures:
    std::string diffuse_map;
    std::string damaged_diffuse_map;
    std::string specular_map;
};

struct MaterialflarebindingsLine
{
    unsigned int flare_number = 0;
    std::string material_name;
};

struct MeshwheelsLine: public MeshwheelsShared
{};

struct Meshwheels2Line: public MeshwheelsShared
{};

struct MinimassLine
{
    enum Option
    {
        OPTION_n_FILLER  = 'n',     //!< Updates the global minimass
        OPTION_l_SKIP_LOADED = 'l'  //!< Only apply minimum mass to nodes without "L" option.
    };

    float min_mass = DEFAULT_MINIMASS; //!< minimum node mass in Kg
    bool option_l_skip_loaded = false;
};

struct NodecollisionLine
{
    NodeRef_t node;
    float radius = 0.f;
};

struct NodesLine: public NodesCommon
{
    RoR::NodeNum_t num;
};

struct Nodes2Line: public NodesCommon
{
    std::string name;
};

struct ParticlesLine
{
    NodeRef_t emitter_node;
    NodeRef_t reference_node;
    std::string particle_system_name;
};

struct PistonpropsLine
{
    NodeRef_t reference_node;
    NodeRef_t axis_node;
    NodeRef_t blade_tip_nodes[4];
    NodeRef_t couple_node;
    float turbine_power_kW = 0.f;
    float pitch = 0.f;
    std::string airfoil;
};

struct PropCameraModeLine: public CameraModeCommon
{};

struct PropsLine
{
    struct DashboardSpecial
    {
        DashboardSpecial():
            offset(Ogre::Vector3::ZERO), // Default depends on right/left-hand dashboard placement
            rotation_angle(160.f),
            _offset_is_set(false),
            mesh_name("dirwheel.mesh")
        {}

        Ogre::Vector3 offset;
        bool _offset_is_set;
        float rotation_angle;
        std::string mesh_name;
    };

    struct BeaconSpecial
    {
        BeaconSpecial():
            color(1.0, 0.5, 0.0),
            flare_material_name("tracks/beaconflare")
        {}

        std::string flare_material_name;
        Ogre::ColourValue color;
    };

    NodeRef_t reference_node;
    NodeRef_t x_axis_node;
    NodeRef_t y_axis_node;
    Ogre::Vector3 offset = Ogre::Vector3::ZERO;
    Ogre::Vector3 rotation = Ogre::Vector3::ZERO;
    std::string mesh_name;
    SpecialProp special = SpecialProp::INVALID;
    BeaconSpecial special_prop_beacon;
    DashboardSpecial special_prop_dashboard;
};

struct RailgroupsLine
{
    unsigned int id = 0;
    std::vector<NodeRangeCommon> node_list;
};

struct RopablesLine
{
    NodeRef_t node;
    int group = -1;
    bool has_multilock = false;
};

struct RopesLine
{
    NodeRef_t root_node;
    NodeRef_t end_node;
    bool invisible = false;
};

struct RotatorsLine: public RotatorsCommon
{};

struct Rotators2Line: public RotatorsCommon
{
    float rotating_force = 10000000;
    float tolerance = 0.f;
    std::string description;
};

struct ScrewpropsLine
{
    NodeRef_t prop_node;
    NodeRef_t back_node;
    NodeRef_t top_node;
    float power = 0.f;
};

struct SectionLine
{
    std::vector<std::string> configs;
};

struct SectionconfigLine
{
    int number = -1; // unused
    std::string name;
};

struct SetBeamDefaultsLine
{
    int _num_args = -1;

    float springiness;
    float damping_constant;
    float deformation_threshold;
    float breaking_threshold;
    float visual_beam_diameter;
    std::string beam_material_name;
    float plastic_deform_coef;
};

struct SetBeamDefaultsScaleLine
{
    int _num_args = -1;

    float springiness = -1.f;
    float damping_constant = -1.f;
    float deformation_threshold_constant = -1.f;
    float breaking_threshold_constant = -1.f;
};

struct SetInertiaDefaultsLine: public InertiaCommon
{};

struct SetDefaultMinimassLine
{
    float min_mass = -1.f;
};

struct SetManagedmaterialsOptionsLine
{
    bool double_sided = false;
};

struct SetNodeDefaultsLine
{
    int _num_args = -1;

    float loadweight = -1.f;
    float friction = -1.f;
    float volume = -1.f;
    float surface = -1.f;
    std::string options;
};

struct SetShadowsLine
{
    int shadow_mode = 0;
};

struct SetSkeletonSettingsLine
{
    float visibility_range_meters = 150.f;
    float beam_thickness_meters = BEAM_SKELETON_DIAMETER;
};

struct ShocksLine
{
    BITMASK_PROPERTY(options, 1, OPTION_i_INVISIBLE    , HasOption_i_Invisible,   SetOption_i_Invisible) 
    // Stability active suspension can be made with "L" for suspension on the truck's left and "R" for suspension on the truck's right. 
    BITMASK_PROPERTY(options, 2, OPTION_L_ACTIVE_LEFT  , HasOption_L_ActiveLeft,  SetOption_L_ActiveLeft) 
    // Stability active suspension can be made with "L" for suspension on the truck's left and "R" for suspension on the truck's right. 
    BITMASK_PROPERTY(options, 3, OPTION_R_ACTIVE_RIGHT , HasOption_R_ActiveRight, SetOption_R_ActiveRight)
    BITMASK_PROPERTY(options, 4, OPTION_m_METRIC       , HasOption_m_Metric,      SetOption_m_Metric) 

    NodeRef_t nodes[2];
    float spring_rate = -1.f;         //!< The 'stiffness' of the shock. The higher the value, the less the shock will move for a given bump. 
    float damping = -1.f;             //!< The 'resistance to motion' of the shock. The best value is given by this equation:  2 * sqrt(suspended mass * springness)
    float short_bound = 0.f;         //!< Maximum contraction. The shortest length the shock can be, as a proportion of its original length. "0" means the shock will not be able to contract at all, "1" will let it contract all the way to zero length. If the shock tries to shorten more than this value allows, it will become as rigid as a normal beam. 
    float long_bound = 0.f;          //!< Maximum extension. The longest length a shock can be, as a proportion of its original length. "0" means the shock will not be able to extend at all. "1" means the shock will be able to double its length. Higher values allow for longer extension.
    float precompression = 1.f;      //!< Changes compression or extension of the suspension when the truck spawns. This can be used to "level" the suspension of a truck if it sags in game. The default value is 1.0. 
    unsigned int options = 0u;      //!< Bit flags.
};

struct Shocks2Line
{
    BITMASK_PROPERTY(options, 1, OPTION_i_INVISIBLE       , HasOption_i_Invisible,      SetOption_i_Invisible) 
    // soft bump boundaries, use when shocks reach limiters too often and "jumprebound" (default is hard bump boundaries)
    BITMASK_PROPERTY(options, 2, OPTION_s_SOFT_BUMP_BOUNDS, HasOption_s_SoftBumpBounds, SetOption_s_SoftBumpBounds)
    // metric values for shortbound/longbound applying to the length of the beam.
    BITMASK_PROPERTY(options, 3, OPTION_m_METRIC          , HasOption_m_Metric,         SetOption_m_Metric)
    // Absolute metric values for shortbound/longbound, settings apply without regarding to the original length of the beam.(Use with caution, check ror.log for errors)
    BITMASK_PROPERTY(options, 4, OPTION_M_ABSOLUTE_METRIC , HasOption_M_AbsoluteMetric, SetOption_M_AbsoluteMetric)  

    NodeRef_t nodes[2];
    float spring_in = 0.f;                  //!< Spring value applied when the shock is compressing.
    float damp_in = 0.f;                    //!< Damping value applied when the shock is compressing. 
    float progress_factor_spring_in = 0.f;  //!< Progression factor for springin. A value of 0 disables this option. 1...x as multipliers, example:maximum springrate == springrate + (factor*springrate)
    float progress_factor_damp_in = 0.f;    //!< Progression factor for dampin. 0 = disabled, 1...x as multipliers, example:maximum dampingrate == springrate + (factor*dampingrate)
    float spring_out = 0.f;                 //!< spring value applied when shock extending
    float damp_out = 0.f;                   //!< damping value applied when shock extending
    float progress_factor_spring_out = 0.f; //!< Progression factor springout, 0 = disabled, 1...x as multipliers, example:maximum springrate == springrate + (factor*springrate)
    float progress_factor_damp_out = 0.f;   //!< Progression factor dampout, 0 = disabled, 1...x as multipliers, example:maximum dampingrate == springrate + (factor*dampingrate)
    float short_bound = 0.f;                //!< Maximum contraction limit, in percentage ( 1.00 = 100% )
    float long_bound = 0.f;                 //!< Maximum extension limit, in percentage ( 1.00 = 100% )
    float precompression = 0.f;             //!< Changes compression or extension of the suspension when the truck spawns. This can be used to "level" the suspension of a truck if it sags in game. The default value is 1.0.  
    unsigned int options = 0u;              //!< Bit flags.
};

struct Shocks3Line
{
    BITMASK_PROPERTY(options, 1, OPTION_i_INVISIBLE       , HasOption_i_Invisible,      SetOption_i_Invisible) 
    // metric values for shortbound/longbound applying to the length of the beam.
    BITMASK_PROPERTY(options, 2, OPTION_m_METRIC          , HasOption_m_Metric,         SetOption_m_Metric)
    // Absolute metric values for shortbound/longbound, settings apply without regarding to the original length of the beam.(Use with caution, check ror.log for errors)
    BITMASK_PROPERTY(options, 3, OPTION_M_ABSOLUTE_METRIC , HasOption_M_AbsoluteMetric, SetOption_M_AbsoluteMetric)  

    NodeRef_t nodes[2];
    float spring_in = 0.f;                  //!< Spring value applied when the shock is compressing.
    float damp_in = 0.f;                    //!< Damping value applied when the shock is compressing. 
    float spring_out = 0.f;                 //!< Spring value applied when shock extending
    float damp_out = 0.f;                   //!< Damping value applied when shock extending
    float damp_in_slow = 0.f;               //!< Damping value applied when shock is commpressing slower than split in velocity
    float split_vel_in = 0.f;               //!< Split velocity in (m/s) - threshold for slow / fast damping during compression
    float damp_in_fast = 0.f;               //!< Damping value applied when shock is commpressing faster than split in velocity
    float damp_out_slow = 0.f;              //!< Damping value applied when shock is commpressing slower than split out velocity
    float split_vel_out = 0.f;              //!< Split velocity in (m/s) - threshold for slow / fast damping during extension
    float damp_out_fast = 0.f;              //!< Damping value applied when shock is commpressing faster than split out velocity
    float short_bound = 0.f;                //!< Maximum contraction limit, in percentage ( 1.00 = 100% )
    float long_bound = 0.f;                 //!< Maximum extension limit, in percentage ( 1.00 = 100% )
    float precompression = 0.f;             //!< Changes compression or extension of the suspension when the truck spawns. This can be used to "level" the suspension of a truck if it sags in game. The default value is 1.0.  
    unsigned int options = 0u;              //!< Bit flags.
};

struct SlidenodesLine
{
    BITMASK_PROPERTY( constraint_flags, 1, CONSTRAINT_ATTACH_ALL     , HasConstraint_a_AttachAll     , SetConstraint_a_AttachAll     )   
    BITMASK_PROPERTY( constraint_flags, 2, CONSTRAINT_ATTACH_FOREIGN , HasConstraint_f_AttachForeign , SetConstraint_f_AttachForeign )
    BITMASK_PROPERTY( constraint_flags, 3, CONSTRAINT_ATTACH_SELF    , HasConstraint_s_AttachSelf 	 , SetConstraint_s_AttachSelf	 )
    BITMASK_PROPERTY( constraint_flags, 4, CONSTRAINT_ATTACH_NONE    , HasConstraint_n_AttachNone 	 , SetConstraint_n_AttachNone	 )

    NodeRef_t slide_node;
    std::vector<NodeRangeCommon> rail_node_ranges;
    float spring_rate = 9000000;
    float break_force = 0.f;
    float tolerance = 0.f;
    int railgroup_id = -1;
    bool _railgroup_id_set = false;
    float attachment_rate = 1.f;
    float max_attachment_distance = 0.1f;
    bool _break_force_set = false;
    unsigned int constraint_flags = 0u;
};

struct SoundsourcesLine: public SoundsourcesCommon
{};

struct Soundsources2Line: SoundsourcesCommon
{
    enum Mode
    {
        MODE_ALWAYS  = -2,
        MODE_OUTSIDE = -1,
        MODE_CINECAM = 1,

        MODE_INVALID = 0xFFFFFFFF
    };

    Mode mode = MODE_INVALID;
    unsigned int cinecam_index = 0;
};

struct SpeedlimiterLine
{
    float max_speed = 0.f;
};

struct TexcoordsLine
{
    NodeRef_t node;
    float u = 0;
    float v = 0;
};

struct TiesLine
{
    static const char OPTION_n_FILLER = 'n';
    static const char OPTION_v_FILLER = 'v';
    static const char OPTION_i_INVISIBLE = 'i';
    static const char OPTION_s_NO_SELF_LOCK = 's';

    NodeRef_t root_node;
    float max_reach_length = 0.f;
    float auto_shorten_rate = 0.f;
    float min_length = 0.f;
    float max_length = 0.f;
    bool is_invisible = false;
    bool disable_self_lock = false;
    float max_stress = 100000.0f;
    int group = -1;
};

struct TorquecurveLine
{
    struct Sample
    {
        float power = 0;
        float torque_percent = 0;
    };

    // If func. name is empty string, sample applies.
    Sample sample;
    std::string predefined_func_name;
};

struct TractionControlLine
{
    float regulation_force = -1.f;
    float wheel_slip = -1.f;
    float fade_speed = -1.f;
    float pulse_per_sec = 0.f;
    bool attr_is_on = false;
    bool attr_no_dashboard = false;
    bool attr_no_toggle = false;
};

struct TransfercaseLine
{
    int a1 = 0;
    int a2 = -1;
    bool has_2wd = true;
    bool has_2wd_lo = false;
    std::vector<float> gear_ratios;
};

struct TriggersLine
{
    struct EngineTrigger
    {
        enum Function
        {
            ENGINE_TRIGGER_FUNCTION_CLUTCH      = 0,
            ENGINE_TRIGGER_FUNCTION_BRAKE       = 1,
            ENGINE_TRIGGER_FUNCTION_ACCELERATOR = 2,
            ENGINE_TRIGGER_FUNCTION_RPM_CONTROL = 3,
            ENGINE_TRIGGER_FUNCTION_SHIFT_UP    = 4, //!< Do not mix with OPTION_t_CONTINUOUS
            ENGINE_TRIGGER_FUNCTION_SHIFT_DOWN  = 5, //!< Do not mix with OPTION_t_CONTINUOUS

            ENGINE_TRIGGER_FUNCTION_INVALID     = 0xFFFFFFFF
        };

        Function function;
        unsigned int motor_index;
    };

    struct CommandKeyTrigger
    {
        unsigned int contraction_trigger_key;
        unsigned int extension_trigger_key;
    };

    struct HookToggleTrigger
    {
        int contraction_trigger_hookgroup_id;
        int extension_trigger_hookgroup_id;
    };

    BITMASK_PROPERTY(options,  1, OPTION_i_INVISIBLE             , HasFlag_i_Invisible,         SetFlag_i_Invisible         )
    BITMASK_PROPERTY(options,  2, OPTION_c_COMMAND_STYLE         , HasFlag_c_CommandStyle,      SetFlag_c_CommandStyle      )
    BITMASK_PROPERTY(options,  3, OPTION_x_START_OFF             , HasFlag_x_StartDisabled,     SetFlag_x_StartDisabled     )
    BITMASK_PROPERTY(options,  4, OPTION_b_BLOCK_KEYS            , HasFlag_b_KeyBlocker,        SetFlag_b_KeyBlocker        )
    BITMASK_PROPERTY(options,  5, OPTION_B_BLOCK_TRIGGERS        , HasFlag_B_TriggerBlocker,    SetFlag_B_TriggerBlocker    )
    BITMASK_PROPERTY(options,  6, OPTION_A_INV_BLOCK_TRIGGERS    , HasFlag_A_InvTriggerBlocker, SetFlag_A_InvTriggerBlocker )
    BITMASK_PROPERTY(options,  7, OPTION_s_SWITCH_CMD_NUM        , HasFlag_s_CmdNumSwitch,      SetFlag_s_CmdNumSwitch      )
    BITMASK_PROPERTY(options,  8, OPTION_h_UNLOCK_HOOKGROUPS_KEY , HasFlag_h_UnlocksHookGroup,  SetFlag_h_UnlocksHookGroup  )
    BITMASK_PROPERTY(options,  9, OPTION_H_LOCK_HOOKGROUPS_KEY   , HasFlag_H_LocksHookGroup,    SetFlag_H_LocksHookGroup    )
    BITMASK_PROPERTY(options, 10, OPTION_t_CONTINUOUS            , HasFlag_t_Continuous,        SetFlag_t_Continuous        )
    BITMASK_PROPERTY(options, 11, OPTION_E_ENGINE_TRIGGER        , HasFlag_E_EngineTrigger,     SetFlag_E_EngineTrigger     )

    inline bool IsHookToggleTrigger() { return HasFlag_H_LocksHookGroup() || HasFlag_h_UnlocksHookGroup(); }

    inline bool IsTriggerBlockerAnyType() { return HasFlag_B_TriggerBlocker() || HasFlag_A_InvTriggerBlocker(); }

    inline void SetEngineTrigger(TriggersLine::EngineTrigger const & trig)
    {
        shortbound_trigger_action = (int) trig.function;
        longbound_trigger_action = (int) trig.motor_index;
    }

    inline TriggersLine::EngineTrigger GetEngineTrigger() const
    {
        ROR_ASSERT(HasFlag_E_EngineTrigger());
        EngineTrigger trig;
        trig.function = static_cast<EngineTrigger::Function>(shortbound_trigger_action);
        trig.motor_index = static_cast<unsigned int>(longbound_trigger_action);
        return trig;
    }

    inline void SetCommandKeyTrigger(CommandKeyTrigger const & trig)
    {
        shortbound_trigger_action = (int) trig.contraction_trigger_key;
        longbound_trigger_action = (int) trig.extension_trigger_key;
    }

    inline CommandKeyTrigger GetCommandKeyTrigger() const
    {
        ROR_ASSERT(BITMASK_IS_0(options, OPTION_B_BLOCK_TRIGGERS | OPTION_A_INV_BLOCK_TRIGGERS 
            | OPTION_h_UNLOCK_HOOKGROUPS_KEY | OPTION_H_LOCK_HOOKGROUPS_KEY | OPTION_E_ENGINE_TRIGGER));
        CommandKeyTrigger out;
        out.contraction_trigger_key = static_cast<unsigned int>(shortbound_trigger_action);
        out.extension_trigger_key = static_cast<unsigned int>(longbound_trigger_action);
        return out;
    }

    inline void SetHookToggleTrigger(HookToggleTrigger const & trig)
    {
        shortbound_trigger_action = trig.contraction_trigger_hookgroup_id;
        longbound_trigger_action = trig.extension_trigger_hookgroup_id;
    }

    inline HookToggleTrigger GetHookToggleTrigger() const
    {
        ROR_ASSERT(HasFlag_h_UnlocksHookGroup() || HasFlag_H_LocksHookGroup());
        HookToggleTrigger trig;
        trig.contraction_trigger_hookgroup_id = shortbound_trigger_action;
        trig.extension_trigger_hookgroup_id = longbound_trigger_action;
        return trig;
    }

    NodeRef_t nodes[2];
    float contraction_trigger_limit = 0;
    float expansion_trigger_limit = 0;
    unsigned int options = 0u;
    float boundary_timer = 1.f;
    int shortbound_trigger_action = 0;
    int longbound_trigger_action = 0;
};

struct TurbojetsLine
{
    NodeRef_t front_node;
    NodeRef_t back_node;
    NodeRef_t side_node;
    int is_reversable = 0;
    float dry_thrust = 0.f;
    float wet_thrust = 0.f;
    float front_diameter = 0.f;
    float back_diameter = 0.f;
    float nozzle_length = 0.f;
};

struct TurbopropsLine: public TurbopropsCommon
{};

struct Turboprops2Line: public TurbopropsCommon
{
    NodeRef_t couple_node;
};

struct VideocamerasLine
{
    NodeRef_t reference_node;
    NodeRef_t left_node;
    NodeRef_t bottom_node;
    NodeRef_t alt_reference_node;
    NodeRef_t alt_orientation_node;
    Ogre::Vector3 offset = Ogre::Vector3::ZERO;
    Ogre::Vector3 rotation = Ogre::Vector3::ZERO;
    float field_of_view = 0.f;
    unsigned int texture_width = 0;
    unsigned int texture_height = 0;
    float min_clip_distance = 0.f;
    float max_clip_distance = 0.f;
    int camera_role = 0;
    int camera_mode = 0;
    std::string material_name;
    std::string camera_name;
};

struct WheeldetachersLine
{
    int wheel_id = -1;
    int detacher_group = -1;
};

struct WheelsLine: public WheelsCommon
{
    float radius;
    float springiness;
    float damping;
    std::string face_material_name = "tracks/wheelface";
    std::string band_material_name = "tracks/wheelband1";
};

struct Wheels2Line: public Wheels2Common
{
    std::string face_material_name = "tracks/wheelface";
    std::string band_material_name = "tracks/wheelband1";
    float rim_springiness;
    float rim_damping;
};

struct WingsLine
{
    static const std::string CONTROL_LEGAL_FLAGS;

    NodeRef_t nodes[8];
    float tex_coords[8] = {};
    WingControlSurface control_surface = WingControlSurface::n_NONE;
    float chord_point = -1.f;
    float min_deflection = -1.f;
    float max_deflection = -1.f;
    std::string airfoil;
    float efficacy_coef = 1.f; //!< So-called 'liftcoef'.
};


    // --------------------------------
    // The document


typedef int DataPos_t; // ALWAYS use this when referencing data in truck document, for readability.
extern const DataPos_t DATAPOS_INVALID;

/// Represents a single line in the document. Used to maintain sequence.
struct Line
{
    Line(Keyword k, int p): keyword(k), data_pos(p) {}

    Keyword keyword = KEYWORD_INVALID;    //!< What kind of data this line defines.
    DataPos_t data_pos = DATAPOS_INVALID; //!< Offset to the associated data array.
    bool begins_block = false;            //!< Line with only the keyword.
};

struct File
{
    static const char* KeywordToString(Keyword keyword);

    bool HasKeyword(Keyword keyword);

    // Metadata:
    std::string name;
    std::string hash;
    std::string guid;
    int file_format_version = 0;
    std::vector<Line> lines; //!<The format is strictly sequential; this array keeps the parsed data in correct order.

    // Parsed data:
    // ALWAYS use DataPos_t when referencing these entries, for readability.
    std::vector<std::string>           help;
    std::vector<AddAnimationLine>      add_animation;
    std::vector<AirbrakesLine>         airbrakes;
    std::vector<AnimatorsLine>         animators;
    std::vector<AntiLockBrakesLine>    antilockbrakes;
    std::vector<AuthorLine>            authors;
    std::vector<AxlesLine>             axles;
    std::vector<BeamsLine>             beams;
    std::vector<BrakesLine>            brakes;
    std::vector<CabLine>               cab;
    std::vector<CamerasLine>           cameras;
    std::vector<NodeRef_t>             camerarails;
    std::vector<CollisionboxesLine>    collisionboxes;
    std::vector<CinecamLine>           cinecam;
    std::vector<CommandsLine>          commands;
    std::vector<Commands2Line>         commands2;
    std::vector<CruisecontrolLine>     cruisecontrol;
    std::vector<NodeRef_t>             contacters;
    std::vector<std::string>           description;
    std::vector<int>                   detacher_group;
    std::vector<EngineLine>            engine;
    std::vector<EngoptionLine>         engoption;
    std::vector<EngturboLine>          engturbo;
    std::vector<ExhaustsLine>          exhausts;
    std::vector<ExtcameraLine>         extcamera;
    std::vector<FileinfoLine>          file_info;
    std::vector<NodeRef_t>             fixes;
    std::vector<FlaresLine>            flares;
    std::vector<Flares2Line>           flares2;
    std::vector<FlexbodyCameraModeLine> flexbody_camera_mode;
    std::vector<FlexbodiesLine>        flexbodies;
    std::vector<ForsetLine>            forset;
    std::vector<FlexbodywheelsLine>    flexbodywheels;
    std::vector<FusedragLine>          fusedrag;
    std::vector<GlobalsLine>           globals;
    std::vector<GuiSettingsLine>       guisettings;
    std::vector<HooksLine>             hooks;
    std::vector<HydrosLine>            hydros;
    std::vector<InteraxlesLine>        interaxles;
    std::vector<LockgroupsLine>        lockgroups;
    std::vector<ManagedmaterialsLine>  managed_materials;
    std::vector<MaterialflarebindingsLine> materialflarebindings;
    std::vector<MeshwheelsLine>        meshwheels;
    std::vector<Meshwheels2Line>       meshwheels2;
    std::vector<MinimassLine>          minimass;
    std::vector<NodesLine>             nodes;
    std::vector<Nodes2Line>            nodes2;
    std::vector<NodecollisionLine>     nodecollision;
    std::vector<ParticlesLine>         particles;
    std::vector<PistonpropsLine>       pistonprops;
    std::vector<PropCameraModeLine>    prop_camera_mode;
    std::vector<PropsLine>             props;
    std::vector<RailgroupsLine>        railgroups;
    std::vector<RopablesLine>          ropables;
    std::vector<RopesLine>             ropes;
    std::vector<RotatorsLine>          rotators;
    std::vector<Rotators2Line>         rotators2;
    std::vector<ScrewpropsLine>        screwprops;
    std::vector<SectionLine>           section;
    std::vector<SectionconfigLine>     sectionconfig;
    std::vector<float>                    set_collision_range;
    std::vector<SetDefaultMinimassLine>   set_default_minimass;
    std::vector<SetBeamDefaultsLine>      set_beam_defaults;
    std::vector<SetBeamDefaultsScaleLine> set_beam_defaults_scale;
    std::vector<SetInertiaDefaultsLine>   set_inertia_defaults;
    std::vector<SetManagedmaterialsOptionsLine> set_managedmaterials_options;
    std::vector<SetNodeDefaultsLine>      set_node_defaults;
    std::vector<SetSkeletonSettingsLine>  set_skeleton_settings;
    std::vector<ShocksLine>            shocks;
    std::vector<Shocks2Line>           shocks2;
    std::vector<Shocks3Line>           shocks3;
    std::vector<SlidenodesLine>        slidenodes;
    std::vector<SoundsourcesLine>      soundsources;
    std::vector<Soundsources2Line>     soundsources2;
    std::vector<SpeedlimiterLine>      speedlimiter;
    std::vector<std::string>           submesh_groundmodel;
    std::vector<TexcoordsLine>         texcoords;
    std::vector<TiesLine>              ties;
    std::vector<TorquecurveLine>       torquecurve;
    std::vector<TractionControlLine>   tractioncontrol;
    std::vector<TransfercaseLine>      transfercase;
    std::vector<TriggersLine>          triggers;
    std::vector<TurbojetsLine>         turbojets;
    std::vector<TurbopropsLine>        turboprops;
    std::vector<Turboprops2Line>       turboprops2;
    std::vector<VideocamerasLine>      videocameras;
    std::vector<WheeldetachersLine>    wheeldetachers;
    std::vector<WheelsLine>            wheels;
    std::vector<Wheels2Line>           wheels2;
    std::vector<WingsLine>             wings;
};

} // namespace RigDef
