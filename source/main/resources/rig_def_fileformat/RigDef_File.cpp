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

#include "RigDef_File.h"
#include "SimConstants.h"

namespace RigDef
{
// Extern
static const DataPos_t DATAPOS_INVALID = -1;
static const NodeRef_t NODEREF_INVALID = "";

// Static
const std::string WingsLine::CONTROL_LEGAL_FLAGS("nabferSTcdghUVij");

const char * File::KeywordToString(Keyword keyword)
{
    /* NOTE: Maintain alphabetical order! */

    switch (keyword)
    {
        case KEYWORD_ADD_ANIMATION:        return "add_animation";
        case KEYWORD_AIRBRAKES:            return "airbrakes";
        case KEYWORD_ANIMATORS:            return "animators";
        case KEYWORD_ANTILOCKBRAKES:       return "antilockbrakes";
        case KEYWORD_AUTHOR:               return "author";
        case KEYWORD_AXLES:                return "axles";
        case KEYWORD_BEAMS:                return "beams";
        case KEYWORD_BRAKES:               return "brakes";
        case KEYWORD_CAB:                  return "cab";
        case KEYWORD_CAMERAS:              return "cameras";
        case KEYWORD_CAMERARAIL:           return "camerarail";
        case KEYWORD_CINECAM:              return "cinecam";
        case KEYWORD_COLLISIONBOXES:       return "collisionboxes";
        case KEYWORD_COMMANDS:             return "commands";
        case KEYWORD_COMMANDS2:            return "commands2";
        case KEYWORD_COMMENT:              return "comment";
        case KEYWORD_CONTACTERS:           return "contacters";
        case KEYWORD_CRUISECONTROL:        return "cruisecontrol";
        case KEYWORD_DESCRIPTION:          return "description";
        case KEYWORD_DETACHER_GROUP:       return "detacher_group";
        case KEYWORD_DISABLEDEFAULTSOUNDS: return "disabledefaultsounds";
        case KEYWORD_ENABLE_ADVANCED_DEFORMATION: return "enable_advanced_deformation";
        case KEYWORD_END:                  return "end";
        case KEYWORD_END_COMMENT:          return "end_comment";
        case KEYWORD_END_DESCRIPTION:      return "end_description";
        case KEYWORD_END_SECTION:          return "end_section";
        case KEYWORD_ENGINE:               return "engine";
        case KEYWORD_ENGOPTION:            return "engoption";
        case KEYWORD_ENGTURBO:             return "engturbo";
        case KEYWORD_EXHAUSTS:             return "exhausts";
        case KEYWORD_EXTCAMERA:            return "extcamera";
        case KEYWORD_FILEINFO:             return "fileinfo";
        case KEYWORD_FILEFORMATVERSION:    return "fileformatversion";
        case KEYWORD_FIXES:                return "fixes";
        case KEYWORD_FLARES:               return "flares";
        case KEYWORD_FLARES2:              return "flares2";
        case KEYWORD_FLEXBODIES:           return "flexbodies";
        case KEYWORD_FLEXBODY_CAMERA_MODE: return "flexbody_camera_mode";
        case KEYWORD_FLEXBODYWHEELS:       return "flexbodywheels";
        case KEYWORD_FORSET:               return "forset";
        case KEYWORD_FORWARDCOMMANDS:      return "forwardcommands";
        case KEYWORD_FUSEDRAG:             return "fusedrag";
        case KEYWORD_GLOBALS:              return "globals";
        case KEYWORD_GUID:                 return "guid";
        case KEYWORD_GUISETTINGS:          return "guisettings";
        case KEYWORD_HELP:                 return "help";
        case KEYWORD_HIDEINCHOOSER:        return "hideinchooser";
        case KEYWORD_HOOKGROUP:            return "hookgroup";
        case KEYWORD_HOOKS:                return "hooks";
        case KEYWORD_HYDROS:               return "hydros";
        case KEYWORD_IMPORTCOMMANDS:       return "importcommands";
        case KEYWORD_INTERAXLES:           return "interaxles";
        case KEYWORD_LOCKGROUPS:           return "lockgroups";
        case KEYWORD_LOCKGROUP_DEFAULT_NOLOCK: return "lockgroup_default_nolock";
        case KEYWORD_MANAGEDMATERIALS:     return "managedmaterials";
        case KEYWORD_MATERIALFLAREBINDINGS: return "materialflarebindings";
        case KEYWORD_MESHWHEELS:           return "meshwheels";
        case KEYWORD_MESHWHEELS2:          return "meshwheels2";
        case KEYWORD_MINIMASS:             return "minimass";
        case KEYWORD_NODES:                return "nodes";
        case KEYWORD_NODES2:               return "nodes2";
        case KEYWORD_PARTICLES:            return "particles";
        case KEYWORD_PISTONPROPS:          return "pistonprops";
        case KEYWORD_PROP_CAMERA_MODE:     return "prop_camera_mode";
        case KEYWORD_PROPS:                return "props";
        case KEYWORD_RAILGROUPS:           return "railgroups";
        case KEYWORD_RESCUER:              return "rescuer";
        case KEYWORD_RIGIDIFIERS:          return "rigidifiers";
        case KEYWORD_ROLLON:               return "rollon";
        case KEYWORD_ROPABLES:             return "ropables";
        case KEYWORD_ROPES:                return "ropes";
        case KEYWORD_ROTATORS:             return "rotators";
        case KEYWORD_ROTATORS2:            return "rotators_2";
        case KEYWORD_SCREWPROPS:           return "screwprops";
        case KEYWORD_SECTION:              return "section";
        case KEYWORD_SECTIONCONFIG:        return "sectionconfig";
        case KEYWORD_SET_BEAM_DEFAULTS:    return "set_beam_defaults";
        case KEYWORD_SET_BEAM_DEFAULTS_SCALE: return "set_beam_defaults_scale";
        case KEYWORD_SET_COLLISION_RANGE:  return "set_collision_range";
        case KEYWORD_SET_DEFAULT_MINIMASS: return "set_default_minimass";
        case KEYWORD_SET_INERTIA_DEFAULTS: return "set_inertia_defaults";
        case KEYWORD_SET_MANAGEDMATERIALS_OPTIONS: return "set_managedmaterials_options";
        case KEYWORD_SET_NODE_DEFAULTS:    return "set_node_defaults";
        case KEYWORD_SET_SHADOWS:          return "set_shadows";
        case KEYWORD_SET_SKELETON_SETTINGS: return "set_skeleton_settings";
        case KEYWORD_SHOCKS:               return "shocks";
        case KEYWORD_SHOCKS2:              return "shocks2";
        case KEYWORD_SHOCKS3:              return "shocks3";
        case KEYWORD_SLIDENODE_CONNECT_INSTANTLY: return "slidenode_connect_instantly";
        case KEYWORD_SLIDENODES:           return "slidenodes";
        case KEYWORD_SLOPE_BRAKE:          return "SlopeBrake";
        case KEYWORD_SOUNDSOURCES:         return "soundsources";
        case KEYWORD_SOUNDSOURCES2:        return "soundsources2";
        case KEYWORD_SPEEDLIMITER:         return "speedlimiter";
        case KEYWORD_SUBMESH:              return "submesh";
        case KEYWORD_SUBMESH_GROUNDMODEL:  return "submesh_groundmodel";
        case KEYWORD_TEXCOORDS:            return "texcoords";
        case KEYWORD_TIES:                 return "ties";
        case KEYWORD_TORQUECURVE:          return "torquecurve";
        case KEYWORD_TRACTIONCONTROL:      return "tractioncontrol";
        case KEYWORD_TRANSFERCASE:         return "transfercase";
        case KEYWORD_TRIGGERS:             return "triggers";
        case KEYWORD_TURBOJETS:            return "turbojets";
        case KEYWORD_TURBOPROPS:           return "turboprops";
        case KEYWORD_TURBOPROPS2:          return "turboprops2";
        case KEYWORD_VIDEOCAMERA:          return "videocamera";
        case KEYWORD_WHEELDETACHERS:       return "wheeldetachers";
        case KEYWORD_WHEELS:               return "wheels";
        case KEYWORD_WHEELS2:              return "wheels2";
        case KEYWORD_WINGS:                return "wings";

        default:                             return "~Unknown~";
    }
}

bool File::HasKeyword(Keyword _keyword)
{
    return std::find_if(lines.begin(), lines.end(),
        [_keyword](RigDef::Line const& l){ return l.keyword == _keyword; }) != lines.end();
}

} /* namespace RigDef */
