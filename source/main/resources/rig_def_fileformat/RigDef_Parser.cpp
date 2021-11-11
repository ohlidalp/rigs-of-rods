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

#include "RigDef_Parser.h"

#include "Application.h"
#include "SimConstants.h"
#include "CacheSystem.h"
#include "Console.h"
#include "RigDef_File.h"
#include "RigDef_Regexes.h"
#include "Utils.h"

#include <OgreException.h>
#include <OgreString.h>
#include <OgreStringVector.h>
#include <OgreStringConverter.h>

using namespace RoR;

namespace RigDef
{

inline bool IsWhitespace(char c)
{
    return (c == ' ') || (c == '\t');
}

inline bool IsSeparator(char c)
{
    return IsWhitespace(c) || (c == ':') || (c == '|') || (c == ',');
}

inline bool StrEqualsNocase(std::string const & s1, std::string const & s2)
{
    if (s1.size() != s2.size()) { return false; }
    for (size_t i = 0; i < s1.size(); ++i)
    {
        if (tolower(s1[i]) != tolower(s2[i])) { return false; }
    }
    return true;
}

void Parser::ProcessCurrentLine()
{
    // Ignore comment lines
    if ((m_current_line[0] == ';') || (m_current_line[0] == '/'))
    {
        return;
    }

    // First line in file (except blanks or comments) is the actor name
    if (m_document->name == "" && m_current_line != "")
    {
        m_document->name = m_current_line; // Already trimmed
        return;
    }

    // Split line to tokens
    if (m_current_block != KEYWORD_COMMENT &&
        m_current_block != KEYWORD_DESCRIPTION)
    {
        this->TokenizeCurrentLine();
    }

    // Detect keyword on current line 
    Keyword keyword = IdentifyKeywordInCurrentLine();
    switch (keyword)
    {
        // No keyword - Continue below to process current block.
        case KEYWORD_INVALID:
            break; // << NOT RETURN.

        // Directives without arguments: just record, do not change current block.
        case KEYWORD_DISABLEDEFAULTSOUNDS:
        case KEYWORD_ENABLE_ADVANCED_DEFORMATION:
        case KEYWORD_END:
        case KEYWORD_END_SECTION:
        case KEYWORD_FORWARDCOMMANDS:
        case KEYWORD_HIDEINCHOOSER:
        case KEYWORD_IMPORTCOMMANDS:
        case KEYWORD_LOCKGROUP_DEFAULT_NOLOCK:
        case KEYWORD_RESCUER:
        case KEYWORD_ROLLON:
        case KEYWORD_SLIDENODE_CONNECT_INSTANTLY:
            m_document->lines.emplace_back(Line(keyword, DATAPOS_INVALID));
            return;

        // Directives with arguments: process immediately, do not change current block.
        case KEYWORD_ADD_ANIMATION:
            this->ParseDirectiveAddAnimation();
            return;
        case KEYWORD_ANTILOCKBRAKES:
            this->ParseAntiLockBrakes();
            return;
        case KEYWORD_AUTHOR:
            this->ParseAuthor();
            return;
        case KEYWORD_BACKMESH:
            this->ParseDirectiveBackmesh();
            return;
        case KEYWORD_CRUISECONTROL:
            this->ParseCruiseControl();
            return;
        case KEYWORD_DETACHER_GROUP:
            this->ParseDirectiveDetacherGroup();
            return;
        case KEYWORD_EXTCAMERA:
            this->ParseExtCamera();
            return;
        case KEYWORD_FILEFORMATVERSION:
            this->ParseFileFormatVersion();
            return;
        case KEYWORD_FILEINFO:
            this->ParseFileinfo();
            return;
        case KEYWORD_FLEXBODY_CAMERA_MODE:
            this->ParseDirectiveFlexbodyCameraMode();
            return;
        case KEYWORD_FORSET:
            this->ParseForset();
            return;
        case KEYWORD_GUID:
            this->ParseGuid();
            return;
        case KEYWORD_PROP_CAMERA_MODE:
            this->ParseDirectivePropCameraMode();
            return;
        case KEYWORD_SECTION:
            this->ParseDirectiveSection();
            return;
        case KEYWORD_SET_BEAM_DEFAULTS:
            this->ParseDirectiveSetBeamDefaults();
            return;
        case KEYWORD_SET_BEAM_DEFAULTS_SCALE:
            this->ParseDirectiveSetBeamDefaultsScale();
            return;
        case KEYWORD_SET_COLLISION_RANGE:
            this->ParseSetCollisionRange();
            return;
        case KEYWORD_SET_DEFAULT_MINIMASS:
            this->ParseDirectiveSetDefaultMinimass();
            return;
        case KEYWORD_SET_INERTIA_DEFAULTS:
            this->ParseDirectiveSetInertiaDefaults();
            return;
        case KEYWORD_SET_MANAGEDMATERIALS_OPTIONS:
            this->ParseDirectiveSetManagedMaterialsOptions();
            return;
        case KEYWORD_SET_NODE_DEFAULTS:
            this->ParseDirectiveSetNodeDefaults();
            return;
        case KEYWORD_SET_SKELETON_SETTINGS:
            this->ParseSetSkeletonSettings();
            return;
        case KEYWORD_SPEEDLIMITER:
            this->ParseSpeedLimiter();
            return;
        case KEYWORD_SUBMESH_GROUNDMODEL:
            this->ParseSubmeshGroundModel();
            return;
        case KEYWORD_TRACTIONCONTROL:
            this->ParseTractionControl();
            return;

        // Keywords which end current block:
        case KEYWORD_END_COMMENT:
        case KEYWORD_END_DESCRIPTION:
            this->EndBlock(keyword);
            return;

        // Ignored keywords (obsolete):
        case KEYWORD_ENVMAP:
        case KEYWORD_HOOKGROUP:
        case KEYWORD_RIGIDIFIERS:
            return;

        // Keywords which start new block:
        default:
            this->BeginBlock(keyword);
            return;
    }

    // Parse current block, if any
    switch (m_current_block)
    {
        case KEYWORD_AIRBRAKES:            this->ParseAirbrakes();               return;
        case KEYWORD_ANIMATORS:            this->ParseAnimators();               return;
        case KEYWORD_AXLES:                this->ParseAxles();                   return;
        case KEYWORD_BEAMS:                this->ParseBeams();                   return;
        case KEYWORD_BRAKES:               this->ParseBrakes();                  return;
        case KEYWORD_CAMERAS:              this->ParseCameras();                 return;
        case KEYWORD_CAMERARAIL:           this->ParseCamerarails();             return;
        case KEYWORD_CINECAM:              this->ParseCinecam();                 return;
        case KEYWORD_COMMANDS:             this->ParseCommands();                return;
        case KEYWORD_COMMANDS2:            this->ParseCommands2();               return;
        case KEYWORD_COLLISIONBOXES:       this->ParseCollisionboxes();          return;
        case KEYWORD_CONTACTERS:           this->ParseContacters();              return;
        case KEYWORD_ENGINE:               this->ParseEngine();                  return;
        case KEYWORD_ENGOPTION:            this->ParseEngoption();               return;
        case KEYWORD_ENGTURBO:             this->ParseEngturbo();                return;
        case KEYWORD_EXHAUSTS:             this->ParseExhausts();                return;
        case KEYWORD_FIXES:                this->ParseFixes();                   return;
        case KEYWORD_FLARES:               this->ParseFlares();                  return;
        case KEYWORD_FLARES2:              this->ParseFlares2();                 return;
        case KEYWORD_FLEXBODIES:           this->ParseFlexbodies();              return;
        case KEYWORD_FLEXBODYWHEELS:       this->ParseFlexbodywheels();          return;
        case KEYWORD_FUSEDRAG:             this->ParseFusedrag();                return;
        case KEYWORD_GLOBALS:              this->ParseGlobals();                 return;
        case KEYWORD_GUISETTINGS:          this->ParseGuiSettings();             return;
        case KEYWORD_HELP:                 this->ParseHelp();                    return;
        case KEYWORD_HOOKS:                this->ParseHook();                    return;
        case KEYWORD_HYDROS:               this->ParseHydros();                  return;
        case KEYWORD_INTERAXLES:           this->ParseInterAxles();              return;
        case KEYWORD_LOCKGROUPS:           this->ParseLockgroups();              return;
        case KEYWORD_MANAGEDMATERIALS:     this->ParseManagedMaterials();        return;
        case KEYWORD_MATERIALFLAREBINDINGS:this->ParseMaterialFlareBindings();   return;
        case KEYWORD_MESHWHEELS:           this->ParseMeshwheels();              return;
        case KEYWORD_MESHWHEELS2:          this->ParseMeshwheels2();             return;
        case KEYWORD_MINIMASS:             this->ParseMinimass();                return;
        case KEYWORD_NODECOLLISION:        this->ParseNodeCollision();           return;
        case KEYWORD_NODES:                this->ParseNodes();                   return;
        case KEYWORD_NODES2:               this->ParseNodes2();                  return;
        case KEYWORD_PARTICLES:            this->ParseParticles();               return;
        case KEYWORD_PISTONPROPS:          this->ParsePistonprops();             return;
        case KEYWORD_PROPS:                this->ParseProps();                   return;
        case KEYWORD_RAILGROUPS:           this->ParseRailGroups();              return;
        case KEYWORD_ROPABLES:             this->ParseRopables();                return;
        case KEYWORD_ROPES:                this->ParseRopes();                   return;
        case KEYWORD_ROTATORS:             this->ParseRotators();                return;
        case KEYWORD_ROTATORS2:            this->ParseRotators2();               return;
        case KEYWORD_SCREWPROPS:           this->ParseScrewprops();              return;
        case KEYWORD_SHOCKS:               this->ParseShock();                   return;
        case KEYWORD_SHOCKS2:              this->ParseShock2();                  return;
        case KEYWORD_SHOCKS3:              this->ParseShock3();                  return;
        case KEYWORD_SLIDENODES:           this->ParseSlidenodes();              return;
        case KEYWORD_SOUNDSOURCES:         this->ParseSoundsources();            return;
        case KEYWORD_SOUNDSOURCES2:        this->ParseSoundsources2();           return;
        case KEYWORD_TEXCOORDS:            this->ParseTexcoords();               return;
        case KEYWORD_TIES:                 this->ParseTies();                    return;
        case KEYWORD_TORQUECURVE:          this->ParseTorqueCurve();             return;
        case KEYWORD_TRANSFERCASE:         this->ParseTransferCase();            return;
        case KEYWORD_TRIGGERS:             this->ParseTriggers();                return;
        case KEYWORD_TURBOJETS:            this->ParseTurbojets();               return;
        case KEYWORD_TURBOPROPS:           this->ParseTurboprops();              return;
        case KEYWORD_TURBOPROPS2:          this->ParseTurboprops2();             return;
        case KEYWORD_VIDEOCAMERA:          this->ParseVideoCamera();             return;
        case KEYWORD_WHEELDETACHERS:       this->ParseWheelDetachers();          return;
        case KEYWORD_WHEELS:               this->ParseWheel();                   return;
        case KEYWORD_WHEELS2:              this->ParseWheel2();                  return;
        case KEYWORD_WINGS:                this->ParseWing();                    return;
        default:;
    };
}

bool Parser::CheckNumArguments(int num_required_args)
{
    if (num_required_args > m_num_args)
    {
        char msg[200];
        snprintf(msg, 200, "Not enough arguments, %d required, got %d. Skipping line.", num_required_args, m_num_args);
        this->AddMessage(Message::TYPE_WARNING, msg);
        return false;
    }
    return true;
}

// -------------------------------------------------------------------------- 
// Parsing individual keywords                                                
// -------------------------------------------------------------------------- 


void Parser::ParseWing()
{
    if (!this->CheckNumArguments(16)) { return; }

    WingsLine wing;

    for (int i = 0; i <  8; i++) { wing.nodes[i]        = this->GetArgNodeRef     (i);  }
    for (int i = 8; i < 16; i++) { wing.tex_coords[i-8] = this->GetArgFloat       (i);  }

    if (m_num_args > 16)         { wing.control_surface = this->GetArgWingSurface (16); }
    if (m_num_args > 17)         { wing.chord_point     = this->GetArgFloat       (17); }
    if (m_num_args > 18)         { wing.min_deflection  = this->GetArgFloat       (18); }
    if (m_num_args > 19)         { wing.max_deflection  = this->GetArgFloat       (19); }
    if (m_num_args > 20)         { wing.airfoil         = this->GetArgStr         (20); }
    if (m_num_args > 21)         { wing.efficacy_coef   = this->GetArgFloat       (21); }

    m_document->wings.push_back(wing);
    m_document->lines.emplace_back(Line(KEYWORD_WINGS, (int)m_document->wings.size() - 1));
}

void Parser::ParseSetCollisionRange()
{
    if (! this->CheckNumArguments(2)) { return; } // 2 items: keyword, arg

    float value = this->GetArgFloat(1);

    m_document->set_collision_range.push_back(value);
    m_document->lines.emplace_back(Line(KEYWORD_SET_COLLISION_RANGE, (int)m_document->set_collision_range.size() - 1));
}

void Parser::ParseWheel2()
{
    if (!this->CheckNumArguments(17)) { return; }

    Wheels2Line wheel_2;

    wheel_2.rim_radius         = this->GetArgFloat        ( 0);
    wheel_2.tyre_radius        = this->GetArgFloat        ( 1);
    wheel_2.width              = this->GetArgFloat        ( 2);
    wheel_2.num_rays           = this->GetArgInt          ( 3);
    wheel_2.nodes[0]           = this->GetArgNodeRef      ( 4);
    wheel_2.nodes[1]           = this->GetArgNodeRef      ( 5);
    wheel_2.rigidity_node      = this->GetArgRigidityNode ( 6);
    wheel_2.braking            = this->GetArgBraking      ( 7);
    wheel_2.propulsion         = this->GetArgPropulsion   ( 8);
    wheel_2.reference_arm_node = this->GetArgNodeRef      ( 9);
    wheel_2.mass               = this->GetArgFloat        (10);
    wheel_2.rim_springiness    = this->GetArgFloat        (11);
    wheel_2.rim_damping        = this->GetArgFloat        (12);
    wheel_2.tyre_springiness   = this->GetArgFloat        (13);
    wheel_2.tyre_damping       = this->GetArgFloat        (14);
    wheel_2.face_material_name = this->GetArgStr          (15);
    wheel_2.band_material_name = this->GetArgStr          (16);

    m_document->wheels2.push_back(wheel_2);
    m_document->lines.emplace_back(Line(KEYWORD_WHEELS2, (int)m_document->wheels2.size() - 1));
}

void Parser::ParseWheel()
{
    if (! this->CheckNumArguments(14)) { return; }

    WheelsLine wheel;

    wheel.radius             = this->GetArgFloat        ( 0);
    wheel.width              = this->GetArgFloat        ( 1);
    wheel.num_rays           = this->GetArgInt          ( 2);
    wheel.nodes[0]           = this->GetArgNodeRef      ( 3);
    wheel.nodes[1]           = this->GetArgNodeRef      ( 4);
    wheel.rigidity_node      = this->GetArgRigidityNode ( 5);
    wheel.braking            = this->GetArgBraking      ( 6);
    wheel.propulsion         = this->GetArgPropulsion   ( 7);
    wheel.reference_arm_node = this->GetArgNodeRef      ( 8);
    wheel.mass               = this->GetArgFloat        ( 9);
    wheel.springiness        = this->GetArgFloat        (10);
    wheel.damping            = this->GetArgFloat        (11);
    wheel.face_material_name = this->GetArgStr          (12);
    wheel.band_material_name = this->GetArgStr          (13);

    m_document->wheels.push_back(wheel);
    m_document->lines.emplace_back(Line(KEYWORD_WHEELS, (int)m_document->wheels.size() - 1));
}

void Parser::ParseWheelDetachers()
{
    if (! this->CheckNumArguments(2)) { return; }

    WheeldetachersLine wheeldetacher;

    wheeldetacher.wheel_id       = this->GetArgInt(0);
    wheeldetacher.detacher_group = this->GetArgInt(1);

    m_document->wheeldetachers.push_back(wheeldetacher);
}

void Parser::ParseTractionControl()
{
    Ogre::StringVector tokens = Ogre::StringUtil::split(m_current_line + 15, ","); // "TractionControl" = 15 characters
    if (tokens.size() < 2)
    {
        this->AddMessage(Message::TYPE_ERROR, "Too few arguments");
        return;
    }

    TractionControlLine tc;
                             tc.regulation_force = this->ParseArgFloat(tokens[0].c_str());
                             tc.wheel_slip       = this->ParseArgFloat(tokens[1].c_str());
    if (tokens.size() > 2) { tc.fade_speed       = this->ParseArgFloat(tokens[2].c_str()); }
    if (tokens.size() > 3) { tc.pulse_per_sec    = this->ParseArgFloat(tokens[3].c_str()); }

    for (unsigned int i=4; i<tokens.size(); i++)
    {
        Ogre::StringVector args2 = Ogre::StringUtil::split(tokens[i], ":");
        Ogre::StringUtil::trim(args2[0]);
        Ogre::StringUtil::toLowerCase(args2[0]);

        if (args2[0] == "mode" && args2.size() == 2)
        {
            Ogre::StringVector attrs = Ogre::StringUtil::split(args2[1], "&");
            auto itor = attrs.begin();
            auto endi = attrs.end();
            for (; itor != endi; ++itor)
            {
                std::string attr = *itor;
                Ogre::StringUtil::trim(attr);
                Ogre::StringUtil::toLowerCase(attr);
                     if (strncmp(attr.c_str(), "nodash", 6)   == 0) { tc.attr_no_dashboard = true;  }
                else if (strncmp(attr.c_str(), "notoggle", 8) == 0) { tc.attr_no_toggle    = true;  }
                else if (strncmp(attr.c_str(), "on", 2)       == 0) { tc.attr_is_on        = true;  }
                else if (strncmp(attr.c_str(), "off", 3)      == 0) { tc.attr_is_on        = false; }
            }
        }
        else
        {
            this->AddMessage(Message::TYPE_ERROR, "TractionControl Mode: missing");
            tc.attr_no_dashboard = false;
            tc.attr_no_toggle = false;
            tc.attr_is_on = true;
        }
    }

    m_document->tractioncontrol.push_back(tc);
    m_document->lines.emplace_back(Line(KEYWORD_TRACTIONCONTROL, (int)m_document->tractioncontrol.size() - 1));
}

void Parser::ParseTransferCase()
{
    if (! this->CheckNumArguments(2)) { return; }

    TransfercaseLine tc;

    tc.a1 = this->GetArgInt(0) - 1;
    tc.a2 = this->GetArgInt(1) - 1;
    if (m_num_args > 2) { tc.has_2wd    = this->GetArgInt(2); }
    if (m_num_args > 3) { tc.has_2wd_lo = this->GetArgInt(3); }
    for (int i = 4; i < m_num_args; i++) { tc.gear_ratios.push_back(this->GetArgFloat(i)); }

    m_document->transfercase.push_back(tc);
    m_document->lines.emplace_back(Line(KEYWORD_TRANSFERCASE, (int)m_document->transfercase.size() - 1));
}

void Parser::ParseSubmeshGroundModel()
{
    if (!this->CheckNumArguments(2)) { return; } // Items: keyword, arg

    m_document->submesh_groundmodel.push_back(this->GetArgStr(1));
    m_document->lines.emplace_back(Line(KEYWORD_SUBMESH_GROUNDMODEL, (int)m_document->submesh_groundmodel.size() - 1));
}

void Parser::ParseSpeedLimiter()
{
    if (! this->CheckNumArguments(2)) { return; } // 2 items: keyword, arg

    SpeedlimiterLine sl;

    sl.max_speed = this->GetArgFloat(1);
    if (sl.max_speed <= 0.f)
    {
        char msg[200];
        snprintf(msg, 200, "Invalid 'max_speed' (%f), must be > 0.0. Using it anyway (compatibility)", sl.max_speed);
        this->AddMessage(Message::TYPE_WARNING, msg);
    }

    m_document->speedlimiter.push_back(sl);
    m_document->lines.emplace_back(Line(KEYWORD_SPEEDLIMITER, (int)m_document->speedlimiter.size() - 1));
}

void Parser::ParseSlopeBrake()
{
    // Obsolete, removed.
}

void Parser::ParseSetSkeletonSettings()
{
    if (! this->CheckNumArguments(2)) { return; }
    
    SetSkeletonSettingsLine skel;
    skel.visibility_range_meters = this->GetArgFloat(1);
    if (m_num_args > 2) { skel.beam_thickness_meters = this->GetArgFloat(2); }
    
    // Defaults
    if (skel.visibility_range_meters < 0.f) { skel.visibility_range_meters = 150.f; }
    if (skel.beam_thickness_meters   < 0.f) { skel.beam_thickness_meters   = BEAM_SKELETON_DIAMETER; }

    m_document->set_skeleton_settings.push_back(skel);
    m_document->lines.emplace_back(Line(KEYWORD_SET_SKELETON_SETTINGS, (int)m_document->set_skeleton_settings.size() - 1));
}

void Parser::ParseDirectiveSetNodeDefaults()
{
    if (!this->CheckNumArguments(2)) { return; }

    SetNodeDefaultsLine def;
    def._num_args = m_num_args;

                        def.loadweight = this->GetArgFloat(1);
    if (m_num_args > 2) def.friction   = this->GetArgFloat(2);
    if (m_num_args > 3) def.volume     = this->GetArgFloat(3);
    if (m_num_args > 4) def.surface    = this->GetArgFloat(4);
    if (m_num_args > 5) def.options    = this->GetArgStr(5);

    m_document->set_node_defaults.push_back(def);
    m_document->lines.emplace_back(Line(KEYWORD_SET_NODE_DEFAULTS, (int)m_document->set_node_defaults.size() - 1));
}

void Parser::ParseDirectiveSetManagedMaterialsOptions()
{
    if (! this->CheckNumArguments(2)) { return; } // 2 items: keyword, arg

    SetManagedmaterialsOptionsLine mmo;

    // This is what v0.3x's parser did.
    char c = this->GetArgChar(1);
    mmo.double_sided = (c != '0');

    if (c != '0' && c != '1')
    {
        this->AddMessage(Message::TYPE_WARNING,
            "Param 'doublesided' should be only 1 or 0, got '" + this->GetArgStr(1) + "', parsing as 0");
    }

    m_document->set_managedmaterials_options.push_back(mmo);
    m_document->lines.emplace_back(Line(KEYWORD_SET_MANAGEDMATERIALS_OPTIONS, (int)m_document->set_managedmaterials_options.size() - 1));
}

void Parser::ParseDirectiveSetBeamDefaultsScale()
{
    if (! this->CheckNumArguments(5)) { return; }

    SetBeamDefaultsScaleLine scale;
    scale._num_args = m_num_args;

    scale.springiness = this->GetArgFloat(1);
    if (m_num_args > 2) { scale.damping_constant = this->GetArgFloat(2); }
    if (m_num_args > 3) { scale.deformation_threshold_constant = this->GetArgFloat(3); }
    if (m_num_args > 4) { scale.breaking_threshold_constant = this->GetArgFloat(4); }

    m_document->set_beam_defaults_scale.push_back(scale);
    m_document->lines.emplace_back(Line(KEYWORD_SET_BEAM_DEFAULTS_SCALE, (int)m_document->set_beam_defaults_scale.size() - 1));
}

void Parser::ParseDirectiveSetBeamDefaults()
{
    if (! this->CheckNumArguments(2)) { return; } // 2 items: keyword, arg

    SetBeamDefaultsLine d;
    d._num_args = m_num_args;

    d.springiness = this->GetArgFloat(1);
    if (m_num_args > 2) d.damping_constant = this->GetArgFloat(2);
    if (m_num_args > 3) d.deformation_threshold = this->GetArgFloat(3);
    if (m_num_args > 4) d.breaking_threshold = this->GetArgFloat(4);
    if (m_num_args > 5) d.visual_beam_diameter = this->GetArgFloat(5);
    if (m_num_args > 6) d.beam_material_name = this->GetArgStr(6);
    if (m_num_args > 7) d.plastic_deform_coef = this->GetArgFloat(7);

    m_document->set_beam_defaults.push_back(d);
    m_document->lines.emplace_back(Line(KEYWORD_SET_BEAM_DEFAULTS, (int)m_document->set_beam_defaults.size() - 1));
}

void Parser::ParseDirectivePropCameraMode()
{
    if (! this->CheckNumArguments(2)) { return; } // 2 items: keyword, arg

    PropCameraModeLine line;
    this->_ParseCameraSettings(line, this->GetArgStr(1));

    m_document->prop_camera_mode.push_back(line);
    m_document->lines.emplace_back(Line(KEYWORD_PROP_CAMERA_MODE, (int)m_document->prop_camera_mode.size() - 1));
}

void Parser::ParseDirectiveSection()
{
    SectionLine def;

    // arg 0: 'section'
    // arg 1: version (unused)
    for (int i = 2; i < m_num_args; ++i)
    {
        def.configs.push_back(this->GetArgStr(i));
    }

    m_document->section.push_back(def);
    m_document->lines.emplace_back(Line(KEYWORD_SECTION, (int)m_document->section.size() - 1));
}

void Parser::ParseDirectiveSectionConfig()
{
    SectionconfigLine line;
    // arg 0 is the keyword 'sectionconfig'
    line.number = this->GetArgInt(1);
    line.name = this->GetArgStr(2);

    m_document->sectionconfig.push_back(line);
    m_document->lines.emplace_back(Line(KEYWORD_SECTIONCONFIG, (int)m_document->sectionconfig.size() - 1));
}

void Parser::ParseDirectiveBackmesh()
{
    m_document->lines.emplace_back(Line(KEYWORD_BACKMESH, -1));
}

void Parser::ParseMeshwheels()
{
    if (! this->CheckNumArguments(16)) { return; }

    MeshwheelsLine mesh_wheel;

    mesh_wheel.tyre_radius        = this->GetArgFloat        ( 0);
    mesh_wheel.rim_radius         = this->GetArgFloat        ( 1);
    mesh_wheel.width              = this->GetArgFloat        ( 2);
    mesh_wheel.num_rays           = this->GetArgInt          ( 3);
    mesh_wheel.nodes[0]           = this->GetArgNodeRef      ( 4);
    mesh_wheel.nodes[1]           = this->GetArgNodeRef      ( 5);
    mesh_wheel.rigidity_node      = this->GetArgRigidityNode ( 6);
    mesh_wheel.braking            = this->GetArgBraking      ( 7);
    mesh_wheel.propulsion         = this->GetArgPropulsion   ( 8);
    mesh_wheel.reference_arm_node = this->GetArgNodeRef      ( 9);
    mesh_wheel.mass               = this->GetArgFloat        (10);
    mesh_wheel.spring             = this->GetArgFloat        (11);
    mesh_wheel.damping            = this->GetArgFloat        (12);
    mesh_wheel.side               = this->GetArgWheelSide    (13);
    mesh_wheel.mesh_name          = this->GetArgStr          (14);
    mesh_wheel.material_name      = this->GetArgStr          (15);

    m_document->meshwheels.push_back(mesh_wheel);
    m_document->lines.emplace_back(Line(KEYWORD_MESHWHEELS, (int)m_document->meshwheels.size() - 1));
}

void Parser::ParseMeshwheels2()
{
    if (! this->CheckNumArguments(16)) { return; }

    Meshwheels2Line mesh_wheel;

    mesh_wheel.tyre_radius        = this->GetArgFloat        ( 0);
    mesh_wheel.rim_radius         = this->GetArgFloat        ( 1);
    mesh_wheel.width              = this->GetArgFloat        ( 2);
    mesh_wheel.num_rays           = this->GetArgInt          ( 3);
    mesh_wheel.nodes[0]           = this->GetArgNodeRef      ( 4);
    mesh_wheel.nodes[1]           = this->GetArgNodeRef      ( 5);
    mesh_wheel.rigidity_node      = this->GetArgRigidityNode ( 6);
    mesh_wheel.braking            = this->GetArgBraking      ( 7);
    mesh_wheel.propulsion         = this->GetArgPropulsion   ( 8);
    mesh_wheel.reference_arm_node = this->GetArgNodeRef      ( 9);
    mesh_wheel.mass               = this->GetArgFloat        (10);
    mesh_wheel.spring             = this->GetArgFloat        (11);
    mesh_wheel.damping            = this->GetArgFloat        (12);
    mesh_wheel.side               = this->GetArgWheelSide    (13);
    mesh_wheel.mesh_name          = this->GetArgStr          (14);
    mesh_wheel.material_name      = this->GetArgStr          (15);

    m_document->meshwheels2.push_back(mesh_wheel);
    m_document->lines.emplace_back(Line(KEYWORD_MESHWHEELS2, (int)m_document->meshwheels2.size() - 1));
}

void Parser::ParseHook()
{
    if (! this->CheckNumArguments(1)) { return; }

    HooksLine hook;
    hook.node = this->GetArgNodeRef(0);

    int i = 1;
    while (i < m_num_args)
    {
        std::string attr = this->GetArgStr(i);
        Ogre::StringUtil::trim(attr);
        const bool has_value = (i < (m_num_args - 1));

        // Values
             if (has_value && (attr == "hookrange")                          ) { hook.option_hook_range       = this->GetArgFloat(++i); }
        else if (has_value && (attr == "speedcoef")                          ) { hook.option_speed_coef       = this->GetArgFloat(++i); }
        else if (has_value && (attr == "maxforce")                           ) { hook.option_max_force        = this->GetArgFloat(++i); }
        else if (has_value && (attr == "timer")                              ) { hook.option_timer            = this->GetArgFloat(++i); }
        else if (has_value && (attr == "hookgroup"  || attr == "hgroup")     ) { hook.option_hookgroup        = this->GetArgInt  (++i); }
        else if (has_value && (attr == "lockgroup"  || attr == "lgroup")     ) { hook.option_lockgroup        = this->GetArgInt  (++i); }
        else if (has_value && (attr == "shortlimit" || attr == "short_limit")) { hook.option_min_range_meters = this->GetArgFloat(++i); }
        // Flags
        else if ((attr == "selflock") ||(attr == "self-lock") ||(attr == "self_lock") ) { hook.flag_self_lock  = true; }
        else if ((attr == "autolock") ||(attr == "auto-lock") ||(attr == "auto_lock") ) { hook.flag_auto_lock  = true; }
        else if ((attr == "nodisable")||(attr == "no-disable")||(attr == "no_disable")) { hook.flag_no_disable = true; }
        else if ((attr == "norope")   ||(attr == "no-rope")   ||(attr == "no_rope")   ) { hook.flag_no_rope    = true; }
        else if ((attr == "visible")  ||(attr == "vis")                               ) { hook.flag_visible    = true; }
        else
        {
            std::string msg = "Ignoring invalid option: " + attr;
            this->AddMessage(Message::TYPE_ERROR, msg.c_str());
        }
        i++;
    }

    m_document->hooks.push_back(hook);
    m_document->lines.emplace_back(Line(KEYWORD_HOOKS, (int)m_document->hooks.size() - 1));
}

void Parser::ParseHelp()
{
    m_document->help.push_back(m_current_line); // already trimmed
    m_document->lines.emplace_back(Line(KEYWORD_HELP, (int)m_document->help.size() - 1));
}

void Parser::ParseGuiSettings()
{
    if (! this->CheckNumArguments(2)) { return; }
   
    GuiSettingsLine gs;
    gs.key = this->GetArgStr(0);
    gs.value = this->GetArgStr(1);

    m_document->guisettings.push_back(gs);
    m_document->lines.emplace_back(Line(KEYWORD_GUISETTINGS, (int)m_document->guisettings.size() - 1));
}

void Parser::ParseGuid()
{
    if (! this->CheckNumArguments(2)) { return; }
    
    m_document->guid = this->GetArgStr(1);
    m_document->lines.emplace_back(Line(KEYWORD_GUID, -1));
}

void Parser::ParseGlobals()
{
    if (! this->CheckNumArguments(2)) { return; }

    GlobalsLine globals;
    globals.dry_mass   = this->GetArgFloat(0);
    globals.cargo_mass = this->GetArgFloat(1);

    if (m_num_args > 2) { globals.material_name = this->GetArgStr(2); }

    m_document->globals.push_back(globals);
    m_document->lines.emplace_back(Line(KEYWORD_GLOBALS, (int)m_document->globals.size() - 1));
}

void Parser::ParseFusedrag()
{
    if (! this->CheckNumArguments(3)) { return; }

    FusedragLine fusedrag;
    fusedrag.front_node = this->GetArgNodeRef(0);
    fusedrag.rear_node  = this->GetArgNodeRef(1);

    if (this->GetArgStr(2) == "autocalc")
    {
        fusedrag.autocalc = true;

        // Fusedrag autocalculation from truck size
        if (m_num_args > 3) { fusedrag.area_coefficient = this->GetArgFloat(3); }
        if (m_num_args > 4) { fusedrag.airfoil_name     = this->GetArgStr  (4); }
    }
    else
    {
        // Original calculation
        fusedrag.approximate_width = this->GetArgFloat(2);
        
        if (m_num_args > 3) { fusedrag.airfoil_name = this->GetArgStr(3); }
    }

    m_document->fusedrag.push_back(fusedrag);
}

void Parser::_ParseCameraSettings(CameraModeCommon & camera_settings, Ogre::String input_str)
{
    int input = PARSEINT(input_str);
    if (input >= 0)
    {
        camera_settings.mode = CameraModeCommon::MODE_CINECAM;
        camera_settings.cinecam_index = input;
    }
    else if (input >= -2)
    {
        camera_settings.mode = CameraModeCommon::Mode(input);
    }
    else
    {
        AddMessage(input_str, Message::TYPE_ERROR, "Invalid value of camera setting, ignoring...");
        return;
    }
}

void Parser::ParseDirectiveFlexbodyCameraMode()
{
    if (! this->CheckNumArguments(2)) { return; } // 2 items: keyword, arg

    FlexbodyCameraModeLine line;
    this->_ParseCameraSettings(line, this->GetArgStr(1));

    m_document->flexbody_camera_mode.push_back(line);
    m_document->lines.emplace_back(Line(KEYWORD_FLEXBODY_CAMERA_MODE, (int)m_document->flexbody_camera_mode.size() - 1));
}

void Parser::ParseCab()
{
    if (! this->CheckNumArguments(3)) { return; }

    CabLine cab;
    cab.nodes[0] = this->GetArgNodeRef(0);
    cab.nodes[1] = this->GetArgNodeRef(1);
    cab.nodes[2] = this->GetArgNodeRef(2);
    if (m_num_args > 3)
    {
        cab.options = 0;
        std::string options_str = this->GetArgStr(3);
        for (unsigned int i = 0; i < options_str.length(); i++)
        {
            switch (options_str.at(i))
            {
            case 'c': cab.options |=  CabLine::OPTION_c_CONTACT;                               break;
            case 'b': cab.options |=  CabLine::OPTION_b_BUOYANT;                               break;
            case 'D': cab.options |= (CabLine::OPTION_c_CONTACT      | CabLine::OPTION_b_BUOYANT); break;
            case 'p': cab.options |=  CabLine::OPTION_p_10xTOUGHER;                            break;
            case 'u': cab.options |=  CabLine::OPTION_u_INVULNERABLE;                          break;
            case 'F': cab.options |= (CabLine::OPTION_p_10xTOUGHER   | CabLine::OPTION_b_BUOYANT); break;
            case 'S': cab.options |= (CabLine::OPTION_u_INVULNERABLE | CabLine::OPTION_b_BUOYANT); break; 
            case 'n': break; // Placeholder, does nothing 

            default:
                char msg[200] = "";
                snprintf(msg, 200, "'submesh/cab' Ignoring invalid option '%c'...", options_str.at(i));
                this->AddMessage(Message::TYPE_WARNING, msg);
                break;
            }
        }
    }

    m_document->cab.push_back(cab);
    m_document->lines.emplace_back(Line(KEYWORD_CAB, (int)m_document->cab.size() - 1));
}

void Parser::ParseTexcoords()
{
    if (! this->CheckNumArguments(3)) { return; }

    TexcoordsLine texcoord;
    texcoord.node = this->GetArgNodeRef(0);
    texcoord.u    = this->GetArgFloat  (1);
    texcoord.v    = this->GetArgFloat  (2);

    m_document->texcoords.push_back(texcoord);
    m_document->lines.emplace_back(Line(KEYWORD_TEXCOORDS, (int)m_document->texcoords.size() - 1));
}

void Parser::ParseFlexbodies()
{
    if (! this->CheckNumArguments(10)) { return; }

    FlexbodiesLine flexbody;
    flexbody.reference_node = this->GetArgNodeRef (0);
    flexbody.x_axis_node    = this->GetArgNodeRef (1);
    flexbody.y_axis_node    = this->GetArgNodeRef (2);
    flexbody.offset.x       = this->GetArgFloat   (3);
    flexbody.offset.y       = this->GetArgFloat   (4);
    flexbody.offset.z       = this->GetArgFloat   (5);
    flexbody.rotation.x     = this->GetArgFloat   (6);
    flexbody.rotation.y     = this->GetArgFloat   (7);
    flexbody.rotation.z     = this->GetArgFloat   (8);
    flexbody.mesh_name      = this->GetArgStr     (9);

    m_document->flexbodies.push_back(flexbody);
    m_document->lines.emplace_back(Line(KEYWORD_FLEXBODIES, (int)m_document->flexbodies.size() - 1));
}

void Parser::ParseForset()
{
    ForsetLine def;

    // Syntax: "forset", followed by space/comma, followed by ","-separated items.
    // Acceptable item forms:
    // * Single node number / node name
    // * Pair of node numbers:" 123 - 456 ". Whitespace is optional.

    char setdef[LINE_BUFFER_LENGTH] = ""; // strtok() is destructive, we need own buffer.
    strncpy(setdef, m_current_line + 6, LINE_BUFFER_LENGTH - 6); // Cut away "forset"
    const char* item = std::strtok(setdef, ",");

    // TODO: Add error reporting

    const ptrdiff_t MAX_ITEM_LEN = 200;
    while (item != nullptr)
    {
        const char* hyphen = strchr(item, '-');
        if (hyphen != nullptr)
        {
            char* a_end = nullptr;
            std::string a_text;
            std::string b_text;
            if (hyphen != item)
            {
                size_t length = std::min(a_end - item, MAX_ITEM_LEN);
                a_text = std::string(item, length);
            }
            char* b_end = nullptr;
            const char* item2 = hyphen + 1;
            size_t length = std::min(b_end - item2, MAX_ITEM_LEN);
            b_text = std::string(item2, length);

            // Add interval [a-b]
            def.node_ranges.push_back(NodeRangeCommon(a_text, b_text));
        }
        else
        {
            // Add "interval" [a-a]
            def.node_ranges.push_back(NodeRangeCommon(item, item));
        }
        item = strtok(nullptr, ",");
    }

    m_document->forset.push_back(def);
    m_document->lines.emplace_back(Line(KEYWORD_FORSET, (int)m_document->forset.size() - 1));

}

void Parser::ParseFlares()
{
    if (! this->CheckNumArguments(5)) { return; }

    FlaresLine flare;
    flare.reference_node = this->GetArgNodeRef(0);
    flare.node_axis_x    = this->GetArgNodeRef(1);
    flare.node_axis_y    = this->GetArgNodeRef(2);
    flare.offset.x       = this->GetArgFloat  (3);
    flare.offset.y       = this->GetArgFloat  (4);

    if (m_num_args > 5) { flare.type = this->GetArgFlareType(5); }

    if (m_num_args > 6)
    {
        switch (flare.type)
        {
            case FlareType::USER:      flare.control_number = this->GetArgInt(6); break;
            case FlareType::DASHBOARD: flare.dashboard_link = this->GetArgStr(6); break;
            default: break;
        }
    }

    if (m_num_args > 7) { flare.blink_delay_milis = this->GetArgInt      (7); }
    if (m_num_args > 8) { flare.size              = this->GetArgFloat    (8); }
    if (m_num_args > 9) { flare.material_name     = this->GetArgStr      (9); }

    m_document->flares.push_back(flare);
    m_document->lines.emplace_back(Line(KEYWORD_FLARES, (int)m_document->flares.size() - 1));
}

void Parser::ParseFlares2()
{
    if (! this->CheckNumArguments(6)) { return; }

    Flares2Line flare2;
    flare2.reference_node = this->GetArgNodeRef(0);
    flare2.node_axis_x    = this->GetArgNodeRef(1);
    flare2.node_axis_y    = this->GetArgNodeRef(2);
    flare2.offset.x       = this->GetArgFloat  (3);
    flare2.offset.y       = this->GetArgFloat  (4);
    flare2.offset.z       = this->GetArgFloat  (5); //<< only difference from 'flares'

    if (m_num_args > 6) { flare2.type = this->GetArgFlareType(6); }

    if (m_num_args > 7)
    {
        switch (flare2.type)
        {
            case FlareType::USER:      flare2.control_number = this->GetArgInt(7); break;
            case FlareType::DASHBOARD: flare2.dashboard_link = this->GetArgStr(7); break;
            default: break;
        }
    }

    if (m_num_args > 8)  { flare2.blink_delay_milis = this->GetArgInt      (8); }
    if (m_num_args > 9)  { flare2.size              = this->GetArgFloat    (9); }
    if (m_num_args > 10) { flare2.material_name     = this->GetArgStr      (10); }

    m_document->flares2.push_back(flare2);
    m_document->lines.emplace_back(Line(KEYWORD_FLARES2, (int)m_document->flares2.size() - 1));
}

void Parser::ParseFixes()
{
    m_document->fixes.push_back(this->GetArgNodeRef(0));
    m_document->lines.emplace_back(Line(KEYWORD_FIXES, (int)m_document->fixes.size() - 1));
}

void Parser::ParseExtCamera()
{
    if (! this->CheckNumArguments(2)) { return; }
    
    ExtcameraLine extcam;
    
    auto mode_str = this->GetArgStr(1);
    if (mode_str == "classic")
    {
        extcam.mode = ExtcameraLine::MODE_CLASSIC;
    }
    else if (mode_str == "cinecam")
    {
        extcam.mode = ExtcameraLine::MODE_CINECAM;
    }
    else if ((mode_str == "node") && (m_num_args > 2))
    {
        extcam.mode = ExtcameraLine::MODE_NODE;
        extcam.node = this->GetArgNodeRef(2);
    }

    m_document->extcamera.push_back(extcam);
    m_document->lines.emplace_back(Line(KEYWORD_EXTCAMERA, (int)m_document->extcamera.size() - 1));
}

void Parser::ParseExhausts()
{
    if (! this->CheckNumArguments(2)) { return; }

    ExhaustsLine exhaust;
    exhaust.reference_node = this->GetArgNodeRef(0);
    exhaust.direction_node = this->GetArgNodeRef(1);
    
    // Param [2] is unused
    if (m_num_args > 3) { exhaust.particle_name = this->GetArgStr(3); }

    m_document->exhausts.push_back(exhaust);
    m_document->lines.emplace_back(Line(KEYWORD_EXHAUSTS, (int)m_document->exhausts.size() - 1));
}

void Parser::ParseFileFormatVersion()
{
    if (! this->CheckNumArguments(2)) { return; }

    m_document->file_format_version = this->GetArgUint(1);
    m_document->lines.emplace_back(Line(KEYWORD_FILEFORMATVERSION, -1));
}

void Parser::ParseDirectiveDetacherGroup()
{
    if (! this->CheckNumArguments(2)) { return; } // 2 items: keyword, param

    int detacher_group = -1;
    if (this->GetArgStr(1) == "end")
    {
        detacher_group = 0;
    }
    else
    {
        detacher_group = this->GetArgInt(1);
    }

    m_document->detacher_group.push_back(detacher_group);
    m_document->lines.emplace_back(Line(KEYWORD_DETACHER_GROUP, (int)m_document->detacher_group.size() - 1));
}

void Parser::ParseCruiseControl()
{
    if (! this->CheckNumArguments(3)) { return; } // keyword + 2 params

    CruisecontrolLine cruisecontrol;
    cruisecontrol.min_speed = this->GetArgFloat(1);
    cruisecontrol.autobrake = this->GetArgInt(2);

    m_document->cruisecontrol.push_back(cruisecontrol);
    m_document->lines.emplace_back(Line(KEYWORD_CRUISECONTROL, (int)m_document->cruisecontrol.size() - 1));
}

void Parser::ParseDescription()
{
    m_document->description.push_back(m_current_line); // Already trimmed
    m_document->lines.emplace_back(Line(KEYWORD_DESCRIPTION, (int)m_document->description.size() - 1));
}

void Parser::ParseDirectiveAddAnimation()
{
    Ogre::StringVector tokens = Ogre::StringUtil::split(m_current_line + 14, ","); // "add_animation " = 14 characters

    if (tokens.size() < 4)
    {
        AddMessage(Message::TYPE_ERROR, "Not enough arguments, skipping...");
        return;
    }

    AddAnimationLine animation;
    animation.ratio       = this->ParseArgFloat(tokens[0].c_str());
    animation.lower_limit = this->ParseArgFloat(tokens[1].c_str());
    animation.upper_limit = this->ParseArgFloat(tokens[2].c_str());

    for (auto itor = tokens.begin() + 3; itor != tokens.end(); ++itor)
    {
        Ogre::StringVector entry = Ogre::StringUtil::split(*itor, ":");
        Ogre::StringUtil::trim(entry[0]);
        if (entry.size() > 1) Ogre::StringUtil::trim(entry[1]); 

        const int WARN_LEN = 500;
        char warn_msg[WARN_LEN] = "";

        if (entry.size() == 1) // Single keyword
        {
            if      (entry[0] == "autoanimate") { animation.mode |= AddAnimationLine::MODE_AUTO_ANIMATE; }
            else if (entry[0] == "noflip")      { animation.mode |= AddAnimationLine::MODE_NO_FLIP; }
            else if (entry[0] == "bounce")      { animation.mode |= AddAnimationLine::MODE_BOUNCE; }
            else if (entry[0] == "eventlock")   { animation.mode |= AddAnimationLine::MODE_EVENT_LOCK; }

            else { snprintf(warn_msg, WARN_LEN, "Invalid keyword: %s", entry[0].c_str()); }
        }
        else if (entry.size() == 2 && (entry[0] == "mode" || entry[0] == "event" || entry[0] == "source"))
        {
            Ogre::StringVector values = Ogre::StringUtil::split(entry[1], "|");
            if (entry[0] == "mode")
            {
                for (auto itor = values.begin(); itor != values.end(); ++itor)
                {
                    std::string value = *itor;
                    Ogre::StringUtil::trim(value);

                         if (value == "x-rotation") { animation.mode |= AddAnimationLine::MODE_ROTATION_X; }
                    else if (value == "y-rotation") { animation.mode |= AddAnimationLine::MODE_ROTATION_Y; }
                    else if (value == "z-rotation") { animation.mode |= AddAnimationLine::MODE_ROTATION_Z; }
                    else if (value == "x-offset"  ) { animation.mode |= AddAnimationLine::MODE_OFFSET_X;   }
                    else if (value == "y-offset"  ) { animation.mode |= AddAnimationLine::MODE_OFFSET_Y;   }
                    else if (value == "z-offset"  ) { animation.mode |= AddAnimationLine::MODE_OFFSET_Z;   }

                    else { snprintf(warn_msg, WARN_LEN, "Invalid 'mode': %s, ignoring...", entry[1].c_str()); }
                }
            }
            else if (entry[0] == "event")
            {
                animation.event = entry[1];
                Ogre::StringUtil::trim(animation.event);
                Ogre::StringUtil::toUpperCase(animation.event);
            }
            else if (entry[0] == "source")
            {
                for (auto itor = values.begin(); itor != values.end(); ++itor)
                {
                    std::string value = *itor;
                    Ogre::StringUtil::trim(value);

                         if (value == "airspeed")      { animation.source |= AddAnimationLine::SOURCE_AIRSPEED;          }
                    else if (value == "vvi")           { animation.source |= AddAnimationLine::SOURCE_VERTICAL_VELOCITY; }
                    else if (value == "altimeter100k") { animation.source |= AddAnimationLine::SOURCE_ALTIMETER_100K;    }
                    else if (value == "altimeter10k")  { animation.source |= AddAnimationLine::SOURCE_ALTIMETER_10K;     }
                    else if (value == "altimeter1k")   { animation.source |= AddAnimationLine::SOURCE_ALTIMETER_1K;      }
                    else if (value == "aoa")           { animation.source |= AddAnimationLine::SOURCE_ANGLE_OF_ATTACK;   }
                    else if (value == "flap")          { animation.source |= AddAnimationLine::SOURCE_FLAP;              }
                    else if (value == "airbrake")      { animation.source |= AddAnimationLine::SOURCE_AIR_BRAKE;         }
                    else if (value == "roll")          { animation.source |= AddAnimationLine::SOURCE_ROLL;              }
                    else if (value == "pitch")         { animation.source |= AddAnimationLine::SOURCE_PITCH;             }
                    else if (value == "brakes")        { animation.source |= AddAnimationLine::SOURCE_BRAKES;            }
                    else if (value == "accel")         { animation.source |= AddAnimationLine::SOURCE_ACCEL;             }
                    else if (value == "clutch")        { animation.source |= AddAnimationLine::SOURCE_CLUTCH;            }
                    else if (value == "speedo")        { animation.source |= AddAnimationLine::SOURCE_SPEEDO;            }
                    else if (value == "tacho")         { animation.source |= AddAnimationLine::SOURCE_TACHO;             }
                    else if (value == "turbo")         { animation.source |= AddAnimationLine::SOURCE_TURBO;             }
                    else if (value == "parking")       { animation.source |= AddAnimationLine::SOURCE_PARKING;           }
                    else if (value == "shifterman1")   { animation.source |= AddAnimationLine::SOURCE_SHIFT_LEFT_RIGHT;  }
                    else if (value == "shifterman2")   { animation.source |= AddAnimationLine::SOURCE_SHIFT_BACK_FORTH;  }
                    else if (value == "sequential")    { animation.source |= AddAnimationLine::SOURCE_SEQUENTIAL_SHIFT;  }
                    else if (value == "shifterlin")    { animation.source |= AddAnimationLine::SOURCE_SHIFTERLIN;        }
                    else if (value == "torque")        { animation.source |= AddAnimationLine::SOURCE_TORQUE;            }
                    else if (value == "heading")       { animation.source |= AddAnimationLine::SOURCE_HEADING;           }
                    else if (value == "difflock")      { animation.source |= AddAnimationLine::SOURCE_DIFFLOCK;          }
                    else if (value == "rudderboat")    { animation.source |= AddAnimationLine::SOURCE_BOAT_RUDDER;       }
                    else if (value == "throttleboat")  { animation.source |= AddAnimationLine::SOURCE_BOAT_THROTTLE;     }
                    else if (value == "steeringwheel") { animation.source |= AddAnimationLine::SOURCE_STEERING_WHEEL;    }
                    else if (value == "aileron")       { animation.source |= AddAnimationLine::SOURCE_AILERON;           }
                    else if (value == "elevator")      { animation.source |= AddAnimationLine::SOURCE_ELEVATOR;          }
                    else if (value == "rudderair")     { animation.source |= AddAnimationLine::SOURCE_AIR_RUDDER;        }
                    else if (value == "permanent")     { animation.source |= AddAnimationLine::SOURCE_PERMANENT;         }
                    else if (value == "event")         { animation.source |= AddAnimationLine::SOURCE_EVENT;             }

                    else
                    {
                        AddAnimationLine::MotorSource motor_source;
                        if (entry[1].compare(0, 8, "throttle") == 0)
                        {
                            motor_source.source = AddAnimationLine::MotorSource::SOURCE_AERO_THROTTLE;
                            motor_source.motor = this->ParseArgUint(entry[1].substr(8));
                        }
                        else if (entry[1].compare(0, 3, "rpm") == 0)
                        {
                            motor_source.source = AddAnimationLine::MotorSource::SOURCE_AERO_RPM;
                            motor_source.motor = this->ParseArgUint(entry[1].substr(3));
                        }
                        else if (entry[1].compare(0, 8, "aerotorq") == 0)
                        {
                            motor_source.source = AddAnimationLine::MotorSource::SOURCE_AERO_TORQUE;
                            motor_source.motor = this->ParseArgUint(entry[1].substr(8));
                        }
                        else if (entry[1].compare(0, 7, "aeropit") == 0)
                        {
                            motor_source.source = AddAnimationLine::MotorSource::SOURCE_AERO_PITCH;
                            motor_source.motor = this->ParseArgUint(entry[1].substr(7));
                        }
                        else if (entry[1].compare(0, 10, "aerostatus") == 0)
                        {
                            motor_source.source = AddAnimationLine::MotorSource::SOURCE_AERO_STATUS;
                            motor_source.motor = this->ParseArgUint(entry[1].substr(10));
                        }
                        else
                        {
                            snprintf(warn_msg, WARN_LEN, "Invalid 'source': %s, ignoring...", entry[1].c_str());
                            continue;
                        }
                        animation.motor_sources.push_back(motor_source);
                    }
                }
            }
            else
            {
                snprintf(warn_msg, WARN_LEN, "Invalid keyword: %s, ignoring...", entry[0].c_str());
            }
        }
        else
        {
            snprintf(warn_msg, WARN_LEN, "Invalid item: %s, ignoring...", entry[0].c_str());
        }

        if (warn_msg[0] != '\0')
        {
            char msg[WARN_LEN + 100];
            snprintf(msg, WARN_LEN + 100, "Invalid token: %s (%s) ignoring....", itor->c_str(), warn_msg);
            this->AddMessage(Message::TYPE_WARNING, msg);
        }
    }

    m_document->add_animation.push_back(animation);
    m_document->lines.emplace_back(Line(KEYWORD_ADD_ANIMATION, (int)m_document->add_animation.size() - 1));
}

void Parser::ParseAntiLockBrakes()
{
    AntiLockBrakesLine alb;
    Ogre::StringVector tokens = Ogre::StringUtil::split(m_current_line + 15, ","); // "AntiLockBrakes " = 15 characters
    if (tokens.size() < 2)
    {
        this->AddMessage(Message::TYPE_ERROR, "Too few arguments for `AntiLockBrakes`");
        return;
    }

    alb.regulation_force = this->ParseArgFloat(tokens[0].c_str());
    alb.min_speed        = this->ParseArgInt  (tokens[1].c_str());

    if (tokens.size() > 3) { alb.pulse_per_sec = this->ParseArgFloat(tokens[2].c_str()); }

    for (unsigned int i=3; i<tokens.size(); i++)
    {
        Ogre::StringVector args2 = Ogre::StringUtil::split(tokens[i], ":");
        Ogre::StringUtil::trim(args2[0]);
        Ogre::StringUtil::toLowerCase(args2[0]);
        if (args2[0] == "mode" && args2.size() == 2)
        {
            Ogre::StringVector attrs = Ogre::StringUtil::split(args2[1], "&");
            auto itor = attrs.begin();
            auto endi = attrs.end();
            for (; itor != endi; ++itor)
            {
                std::string attr = *itor;
                Ogre::StringUtil::trim(attr);
                Ogre::StringUtil::toLowerCase(attr);
                     if (strncmp(attr.c_str(), "nodash", 6)   == 0) { alb.attr_no_dashboard = true;  }
                else if (strncmp(attr.c_str(), "notoggle", 8) == 0) { alb.attr_no_toggle    = true;  }
                else if (strncmp(attr.c_str(), "on", 2)       == 0) { alb.attr_is_on        = true;  }
                else if (strncmp(attr.c_str(), "off", 3)      == 0) { alb.attr_is_on        = false; }
            }
        }
        else
        {
            this->AddMessage(Message::TYPE_ERROR, "Antilockbrakes Mode: missing");
            alb.attr_no_dashboard = false;
            alb.attr_no_toggle = false;
            alb.attr_is_on = true;
        }
    }

    m_document->antilockbrakes.push_back(alb);
    m_document->lines.emplace_back(Line(KEYWORD_ANTILOCKBRAKES, (int)m_document->antilockbrakes.size() - 1));
}

void Parser::ParseEngoption()
{
    if (! this->CheckNumArguments(1)) { return; }

    EngoptionLine engoption;
    engoption.inertia = this->GetArgFloat(0);

    if (m_num_args > 1)
    {
        engoption.type = EngoptionLine::EngineType(this->GetArgChar(1));
    }

    if (m_num_args > 2) { engoption.clutch_force     = this->GetArgFloat(2); }
    if (m_num_args > 3) { engoption.shift_time_sec       = this->GetArgFloat(3); }
    if (m_num_args > 4) { engoption.clutch_time_sec      = this->GetArgFloat(4); }
    if (m_num_args > 5) { engoption.post_shift_time_sec  = this->GetArgFloat(5); }
    if (m_num_args > 6) { engoption.stall_rpm        = this->GetArgFloat(6); }
    if (m_num_args > 7) { engoption.idle_rpm         = this->GetArgFloat(7); }
    if (m_num_args > 8) { engoption.max_idle_mixture = this->GetArgFloat(8); }
    if (m_num_args > 9) { engoption.min_idle_mixture = this->GetArgFloat(9); }
    if (m_num_args > 10){ engoption.braking_torque   = this->GetArgFloat(10);}

    m_document->engoption.push_back(engoption);
    m_document->lines.emplace_back(Line(KEYWORD_ENGOPTION, (int)m_document->engoption.size() - 1));
}

void Parser::ParseEngturbo()
{
    if (! this->CheckNumArguments(4)) { return; }

    EngturboLine engturbo;
    engturbo.version        = this->GetArgInt  ( 0);
    engturbo.tinertiaFactor = this->GetArgFloat( 1);
    engturbo.nturbos        = this->GetArgInt  ( 2);
    engturbo.param1         = this->GetArgFloat( 3);

    if (m_num_args >  4) { engturbo.param2  = this->GetArgFloat( 4); }
    if (m_num_args >  5) { engturbo.param3  = this->GetArgFloat( 5); }
    if (m_num_args >  6) { engturbo.param4  = this->GetArgFloat( 6); }
    if (m_num_args >  7) { engturbo.param5  = this->GetArgFloat( 7); }
    if (m_num_args >  8) { engturbo.param6  = this->GetArgFloat( 8); }
    if (m_num_args >  9) { engturbo.param7  = this->GetArgFloat( 9); }
    if (m_num_args > 10) { engturbo.param8  = this->GetArgFloat(10); }
    if (m_num_args > 11) { engturbo.param9  = this->GetArgFloat(11); }
    if (m_num_args > 12) { engturbo.param10 = this->GetArgFloat(12); }
    if (m_num_args > 13) { engturbo.param11 = this->GetArgFloat(13); }

    if (engturbo.nturbos > 4)
    {
        this->AddMessage(Message::TYPE_WARNING, "You cannot have more than 4 turbos. Fallback: using 4 instead.");
        engturbo.nturbos = 4;
    }

    m_document->engturbo.push_back(engturbo);
    m_document->lines.emplace_back(Line(KEYWORD_ENGTURBO, (int)m_document->engturbo.size() - 1));
}

void Parser::ParseEngine()
{
    if (! this->CheckNumArguments(6)) { return; }

    EngineLine engine;
    engine.shift_down_rpm     = this->GetArgFloat(0);
    engine.shift_up_rpm       = this->GetArgFloat(1);
    engine.torque             = this->GetArgFloat(2);
    engine.global_gear_ratio  = this->GetArgFloat(3);
    engine.reverse_gear_ratio = this->GetArgFloat(4);
    engine.neutral_gear_ratio = this->GetArgFloat(5);

    // Forward gears
    for (int i = 6; i < m_num_args; i++)
    {
        float ratio = this->GetArgFloat(i);
        if (ratio < 0.f)
        {
            break; // Optional terminator argument
        }
        engine.gear_ratios.push_back(ratio);   
    }

    if (engine.gear_ratios.size() == 0)
    {
        AddMessage(Message::TYPE_ERROR, "Engine has no forward gear, ignoring...");
        return;
    }

    m_document->engine.push_back(engine);
    m_document->lines.emplace_back(Line(KEYWORD_ENGINE, (int)m_document->engine.size() - 1));
}

void Parser::ParseContacters()
{
    if (! this->CheckNumArguments(1)) { return; }

    m_document->contacters.push_back(this->GetArgNodeRef(0));
    m_document->lines.emplace_back(Line(KEYWORD_CONTACTERS, (int)m_document->contacters.size() - 1));
}

void Parser::ParseCommands()
{
    if (! this->CheckNumArguments(7)) { return; }

    CommandsLine command;

    command.nodes[0]          = this->GetArgNodeRef(0);
    command.nodes[1]          = this->GetArgNodeRef(1);
    command.rate              = this->GetArgFloat  (2);
    command.max_contraction = this->GetArgFloat(3);
    command.max_extension   = this->GetArgFloat(4);
    command.contract_key    = this->GetArgInt  (5);
    command.extend_key      = this->GetArgInt  (6);

    if (m_num_args > 7) { this->ParseCommandOptions(command, this->GetArgStr(7)); }
    if (m_num_args > 8) { command.description   = this->GetArgStr  (8);}

    if (m_num_args > 9) { this->ParseOptionalInertia(command.inertia, 9); } // 4 args

    if (m_num_args > 13) { command.affect_engine = this->GetArgFloat(13);}
    if (m_num_args > 14) { command.needs_engine  = this->GetArgBool (14);}
    if (m_num_args > 15) { command.plays_sound   = this->GetArgBool (15);}

    m_document->commands.push_back(command);
    m_document->lines.emplace_back(Line(KEYWORD_COMMANDS, (int)m_document->commands.size() - 1));
}

void Parser::ParseCommands2()
{
    if (! this->CheckNumArguments(8)) { return; }

    Commands2Line command;

    command.nodes[0]          = this->GetArgNodeRef(0);
    command.nodes[1]          = this->GetArgNodeRef(1);
    command.shorten_rate      = this->GetArgFloat  (2); // <- different from 'commands'
    command.lengthen_rate     = this->GetArgFloat  (3); // <- different from 'commands'
    command.max_contraction = this->GetArgFloat(4);
    command.max_extension   = this->GetArgFloat(5);
    command.contract_key    = this->GetArgInt  (6);
    command.extend_key      = this->GetArgInt  (7);

    if (m_num_args > 8) { this->ParseCommandOptions(command, this->GetArgStr(8)); }
    if (m_num_args > 9) { command.description   = this->GetArgStr  (9);}

    if (m_num_args > 10) { this->ParseOptionalInertia(command.inertia, 10); } // 4 args

    if (m_num_args > 14) { command.affect_engine = this->GetArgFloat(14);}
    if (m_num_args > 15) { command.needs_engine  = this->GetArgBool (15);}
    if (m_num_args > 16) { command.plays_sound   = this->GetArgBool (16);}

    m_document->commands2.push_back(command);
    m_document->lines.emplace_back(Line(KEYWORD_COMMANDS2, (int)m_document->commands2.size() - 1));
}

void Parser::ParseCommandOptions(CommandsCommon& command, std::string const& options_str)
{
    const int WARN_LEN = 200;
    char warn_msg[WARN_LEN] = "";
    char winner = 0;
    for (auto itor = options_str.begin(); itor != options_str.end(); ++itor)
    {
        const char c = *itor;
        if ((winner == 0) && (c == 'o' || c == 'p' || c == 'c')) { winner = c; }
        
             if (c == 'n') {} // Filler, does nothing
        else if (c == 'i') { command.option_i_invisible     = true; }
        else if (c == 'r') { command.option_r_rope          = true; }
        else if (c == 'f') { command.option_f_not_faster    = true; }
        else if (c == 'c') { command.option_c_auto_center   = true; }
        else if (c == 'p') { command.option_p_1press        = true; }
        else if (c == 'o') { command.option_o_1press_center = true; }
        else
        {
            snprintf(warn_msg, WARN_LEN, "Ignoring unknown flag '%c'", c);
            this->AddMessage(Message::TYPE_WARNING, warn_msg);
        }
    }

    // Resolve option conflicts
    if (command.option_c_auto_center && winner != 'c' && winner != 0)
    {
        AddMessage(Message::TYPE_WARNING, "Command cannot be one-pressed and self centering at the same time, ignoring flag 'c'");
        command.option_c_auto_center = false;
    }
    char ignored = '\0';
    if (command.option_o_1press_center && winner != 'o' && winner != 0)
    {
        command.option_o_1press_center = false;
        ignored = 'o';
    }
    else if (command.option_p_1press && winner != 'p' && winner != 0)
    {
        command.option_p_1press = false;
        ignored = 'p';
    }

    // Report conflicts
    if (ignored != 0 && winner == 'c')
    {
        snprintf(warn_msg, WARN_LEN, "Command cannot be one-pressed and self centering at the same time, ignoring flag '%c'", ignored);
        AddMessage(Message::TYPE_WARNING, warn_msg);
    }
    else if (ignored != 0 && (winner == 'o' || winner == 'p'))
    {
        snprintf(warn_msg, WARN_LEN, "Command already has a one-pressed c.mode, ignoring flag '%c'", ignored);
        AddMessage(Message::TYPE_WARNING, warn_msg);
    }
}

void Parser::ParseCollisionboxes()
{
    CollisionboxesLine collisionbox;

    Ogre::StringVector tokens = Ogre::StringUtil::split(m_current_line, ",");
    Ogre::StringVector::iterator iter = tokens.begin();
    for ( ; iter != tokens.end(); iter++)
    {
        collisionbox.nodes.push_back( *iter );
    }

    m_document->collisionboxes.push_back(collisionbox);
    m_document->lines.emplace_back(Line(KEYWORD_COLLISIONBOXES, (int)m_document->collisionboxes.size() - 1));
}

void Parser::ParseCinecam()
{
    if (! this->CheckNumArguments(11)) { return; }

    CinecamLine cinecam;

    // Required arguments
    cinecam.position.x = this->GetArgFloat  ( 0);
    cinecam.position.y = this->GetArgFloat  ( 1);
    cinecam.position.z = this->GetArgFloat  ( 2);
    cinecam.nodes[0]   = this->GetArgNodeRef( 3);
    cinecam.nodes[1]   = this->GetArgNodeRef( 4);
    cinecam.nodes[2]   = this->GetArgNodeRef( 5);
    cinecam.nodes[3]   = this->GetArgNodeRef( 6);
    cinecam.nodes[4]   = this->GetArgNodeRef( 7);
    cinecam.nodes[5]   = this->GetArgNodeRef( 8);
    cinecam.nodes[6]   = this->GetArgNodeRef( 9);
    cinecam.nodes[7]   = this->GetArgNodeRef(10);

    // Optional arguments
    if (m_num_args > 11) { cinecam.spring    = this->GetArgFloat(11); }
    if (m_num_args > 12) { cinecam.damping   = this->GetArgFloat(12); }

    if (m_num_args > 13)
    {
        float value = this->GetArgFloat(13);
        if (value > 0.f) // Invalid input (for example illegal trailing ";pseudo-comment") parses as 0
            cinecam.node_mass = value;
    }

    m_document->cinecam.push_back(cinecam);
    m_document->lines.emplace_back(Line(KEYWORD_CINECAM, (int)m_document->cinecam.size() - 1));
}

void Parser::ParseCamerarails()
{
    m_document->camerarails.push_back( this->GetArgNodeRef(0) );
    m_document->lines.emplace_back(Line(KEYWORD_CAMERARAIL, (int)m_document->camerarails.size() - 1));
}

void Parser::ParseBrakes()
{
    if (!this->CheckNumArguments(1)) { return; }

    BrakesLine brakes;
    brakes.default_braking_force = this->GetArgFloat(0);

    if (m_num_args > 1)
    {
        brakes.parking_brake_force = this->GetArgFloat(1);
    }

    m_document->brakes.push_back(brakes);
    m_document->lines.emplace_back(Line(KEYWORD_BRAKES, (int)m_document->brakes.size() - 1));
}

void Parser::ParseAxles()
{
    AxlesLine axle;

    Ogre::StringVector tokens = Ogre::StringUtil::split(m_current_line, ",");
    Ogre::StringVector::iterator iter = tokens.begin();
    for ( ; iter != tokens.end(); iter++)
    {
        std::smatch results;
        if (! std::regex_search(*iter, results, Regexes::SECTION_AXLES_PROPERTY))
        {
            this->AddMessage(Message::TYPE_ERROR, "Invalid property, ignoring whole line...");
            return;
        }
        // NOTE: Positions in 'results' array match E_CAPTURE*() positions (starting with 1) in the respective regex. 

        if (results[1].matched)
        {
            unsigned int wheel_index = PARSEINT(results[2]) - 1;
            axle.wheels[wheel_index][0] = (results[3]);
            axle.wheels[wheel_index][1] = (results[4]);
        }
        else if (results[5].matched)
        {
            std::string options_str = results[6].str();
            for (unsigned int i = 0; i < options_str.length(); i++)
            {
                switch(options_str.at(i))
                {
                    case 'o':
                        axle.options.push_back(DifferentialType::DIFF_o_OPEN);
                        break;
                    case 'l':
                        axle.options.push_back(DifferentialType::DIFF_l_LOCKED);
                        break;
                    case 's':
                        axle.options.push_back(DifferentialType::DIFF_s_SPLIT);
                        break;
                    case 'v':
                        axle.options.push_back(DifferentialType::DIFF_v_VISCOUS);
                        break;

                    default: // No check needed, regex takes care of that 
                        break;
                }
            }
        }
    }

    m_document->axles.push_back(axle);
    m_document->lines.emplace_back(Line(KEYWORD_AXLES, (int)m_document->axles.size() - 1));
}

void Parser::ParseInterAxles()
{
    auto args = Ogre::StringUtil::split(m_current_line, ",");
    if (args.size() < 2) { return; }

    InteraxlesLine interaxle;

    interaxle.a1 = this->ParseArgInt(args[0].c_str()) - 1;
    interaxle.a2 = this->ParseArgInt(args[1].c_str()) - 1;

    std::smatch results;
    if (! std::regex_search(args[2], results, Regexes::SECTION_AXLES_PROPERTY))
    {
        this->AddMessage(Message::TYPE_ERROR, "Invalid property, ignoring whole line...");
        return;
    }
    // NOTE: Positions in 'results' array match E_CAPTURE*() positions (starting with 1) in the respective regex. 

    if (results[5].matched)
    {
        std::string options_str = results[6].str();
        for (unsigned int i = 0; i < options_str.length(); i++)
        {
            switch(options_str.at(i))
            {
                case 'o':
                    interaxle.options.push_back(DifferentialType::DIFF_o_OPEN);
                    break;
                case 'l':
                    interaxle.options.push_back(DifferentialType::DIFF_l_LOCKED);
                    break;
                case 's':
                    interaxle.options.push_back(DifferentialType::DIFF_s_SPLIT);
                    break;
                case 'v':
                    interaxle.options.push_back(DifferentialType::DIFF_v_VISCOUS);
                    break;

                default: // No check needed, regex takes care of that 
                    break;
            }
        }
    }

    m_document->interaxles.push_back(interaxle);
    m_document->lines.emplace_back(Line(KEYWORD_INTERAXLES, (int)m_document->interaxles.size() - 1));
}

void Parser::ParseAirbrakes()
{
    if (! this->CheckNumArguments(14)) { return; }

    AirbrakesLine airbrake;
    airbrake.reference_node        = this->GetArgNodeRef( 0);
    airbrake.x_axis_node           = this->GetArgNodeRef( 1);
    airbrake.y_axis_node           = this->GetArgNodeRef( 2);
    airbrake.aditional_node        = this->GetArgNodeRef( 3);
    airbrake.offset.x              = this->GetArgFloat  ( 4);
    airbrake.offset.y              = this->GetArgFloat  ( 5);
    airbrake.offset.z              = this->GetArgFloat  ( 6);
    airbrake.width                 = this->GetArgFloat  ( 7);
    airbrake.height                = this->GetArgFloat  ( 8);
    airbrake.max_inclination_angle = this->GetArgFloat  ( 9);
    airbrake.texcoord_x1           = this->GetArgFloat  (10);
    airbrake.texcoord_y1           = this->GetArgFloat  (11);
    airbrake.texcoord_x2           = this->GetArgFloat  (12);
    airbrake.texcoord_y2           = this->GetArgFloat  (13);

    m_document->airbrakes.push_back(airbrake);
    m_document->lines.emplace_back(Line(KEYWORD_AIRBRAKES, (int)m_document->airbrakes.size() - 1));
}

void Parser::ParseVideoCamera()
{
    if (! this->CheckNumArguments(19)) { return; }

    VideocamerasLine videocamera;

    videocamera.reference_node       = this->GetArgNodeRef     ( 0);
    videocamera.left_node            = this->GetArgNodeRef     ( 1);
    videocamera.bottom_node          = this->GetArgNodeRef     ( 2);
    videocamera.alt_reference_node   = this->GetArgNullableNode( 3);
    videocamera.alt_orientation_node = this->GetArgNullableNode( 4);
    videocamera.offset.x             = this->GetArgFloat       ( 5);
    videocamera.offset.y             = this->GetArgFloat       ( 6);
    videocamera.offset.z             = this->GetArgFloat       ( 7);
    videocamera.rotation.x           = this->GetArgFloat       ( 8);
    videocamera.rotation.y           = this->GetArgFloat       ( 9);
    videocamera.rotation.z           = this->GetArgFloat       (10);
    videocamera.field_of_view        = this->GetArgFloat       (11);
    videocamera.texture_width        = this->GetArgInt         (12);
    videocamera.texture_height       = this->GetArgInt         (13);
    videocamera.min_clip_distance    = this->GetArgFloat       (14);
    videocamera.max_clip_distance    = this->GetArgFloat       (15);
    videocamera.camera_role          = this->GetArgInt         (16);
    videocamera.camera_mode          = this->GetArgInt         (17);
    videocamera.material_name        = this->GetArgStr         (18);

    if (m_num_args > 19) { videocamera.camera_name = this->GetArgStr(19); }

    m_document->videocameras.push_back(videocamera);
    m_document->lines.emplace_back(Line(KEYWORD_VIDEOCAMERA, (int)m_document->videocameras.size() - 1));
}

void Parser::ParseCameras()
{
    if (! this->CheckNumArguments(3)) { return; }

    CamerasLine camera;
    camera.center_node = this->GetArgNodeRef(0);
    camera.back_node   = this->GetArgNodeRef(1);
    camera.left_node   = this->GetArgNodeRef(2);

    m_document->cameras.push_back(camera);
    m_document->lines.emplace_back(Line(KEYWORD_CAMERAS, (int)m_document->cameras.size() - 1));
}

void Parser::ParseTurboprops()
{
    if (! this->CheckNumArguments(8)) { return; }

    TurbopropsLine turboprop;

    turboprop.reference_node     = this->GetArgNodeRef(0);
    turboprop.axis_node          = this->GetArgNodeRef(1);
    turboprop.blade_tip_nodes[0] = this->GetArgNodeRef(2);
    turboprop.blade_tip_nodes[1] = this->GetArgNodeRef(3);
    turboprop.blade_tip_nodes[2] = this->GetArgNullableNode(4);
    turboprop.blade_tip_nodes[3] = this->GetArgNullableNode(5);
    turboprop.turbine_power_kW   = this->GetArgFloat  (6);
    turboprop.airfoil            = this->GetArgStr    (7);
    
    m_document->turboprops.push_back(turboprop);
    m_document->lines.emplace_back(Line(KEYWORD_TURBOPROPS, (int)m_document->turboprops.size() - 1));
}

void Parser::ParseTurboprops2()
{
    if (! this->CheckNumArguments(9)) { return; }

    Turboprops2Line turboprop;
    
    turboprop.reference_node     = this->GetArgNodeRef(0);
    turboprop.axis_node          = this->GetArgNodeRef(1);
    turboprop.blade_tip_nodes[0] = this->GetArgNodeRef(2);
    turboprop.blade_tip_nodes[1] = this->GetArgNodeRef(3);
    turboprop.blade_tip_nodes[2] = this->GetArgNullableNode(4);
    turboprop.blade_tip_nodes[3] = this->GetArgNullableNode(5);
    turboprop.couple_node        = this->GetArgNullableNode(6);
    turboprop.turbine_power_kW   = this->GetArgFloat  (7);
    turboprop.airfoil            = this->GetArgStr    (8);
    
    m_document->turboprops2.push_back(turboprop);
    m_document->lines.emplace_back(Line(KEYWORD_TURBOPROPS2, (int)m_document->turboprops2.size() - 1));
}

void Parser::ParseTurbojets()
{
    if (! this->CheckNumArguments(9)) { return; }

    TurbojetsLine turbojet;
    turbojet.front_node     = this->GetArgNodeRef(0);
    turbojet.back_node      = this->GetArgNodeRef(1);
    turbojet.side_node      = this->GetArgNodeRef(2);
    turbojet.is_reversable  = this->GetArgInt    (3);
    turbojet.dry_thrust     = this->GetArgFloat  (4);
    turbojet.wet_thrust     = this->GetArgFloat  (5);
    turbojet.front_diameter = this->GetArgFloat  (6);
    turbojet.back_diameter  = this->GetArgFloat  (7);
    turbojet.nozzle_length  = this->GetArgFloat  (8);

    m_document->turbojets.push_back(turbojet);
    m_document->lines.emplace_back(Line(KEYWORD_TURBOJETS, (int)m_document->turbojets.size() - 1));
}

void Parser::ParseTriggers()
{
    if (! this->CheckNumArguments(6)) { return; }

    TriggersLine trigger;
    trigger.nodes[0]                  = this->GetArgNodeRef(0);
    trigger.nodes[1]                  = this->GetArgNodeRef(1);
    trigger.contraction_trigger_limit = this->GetArgFloat  (2);
    trigger.expansion_trigger_limit   = this->GetArgFloat  (3);
    
    int shortbound_trigger_action = this->GetArgInt(4); 
    int longbound_trigger_action  = this->GetArgInt(5); 
    if (m_num_args > 6)
    {
        std::string options_str = this->GetArgStr(6);
        for (unsigned int i = 0; i < options_str.length(); i++)
        {
            switch(options_str.at(i))
            {
                case 'i': trigger.options |= TriggersLine::OPTION_i_INVISIBLE;             break;
                case 'c': trigger.options |= TriggersLine::OPTION_c_COMMAND_STYLE;         break;
                case 'x': trigger.options |= TriggersLine::OPTION_x_START_OFF;             break;
                case 'b': trigger.options |= TriggersLine::OPTION_b_BLOCK_KEYS;            break;
                case 'B': trigger.options |= TriggersLine::OPTION_B_BLOCK_TRIGGERS;        break;
                case 'A': trigger.options |= TriggersLine::OPTION_A_INV_BLOCK_TRIGGERS;    break;
                case 's': trigger.options |= TriggersLine::OPTION_s_SWITCH_CMD_NUM;        break;
                case 'h': trigger.options |= TriggersLine::OPTION_h_UNLOCK_HOOKGROUPS_KEY; break;
                case 'H': trigger.options |= TriggersLine::OPTION_H_LOCK_HOOKGROUPS_KEY;   break;
                case 't': trigger.options |= TriggersLine::OPTION_t_CONTINUOUS;            break;
                case 'E': trigger.options |= TriggersLine::OPTION_E_ENGINE_TRIGGER;        break;

                default:
                    this->AddMessage(Message::TYPE_WARNING, Ogre::String("Invalid trigger option: " + options_str.at(i)));
            }
        }
    }

    if (m_num_args > 7)
    {
        float boundary_timer = this->GetArgFloat(7);
        if (boundary_timer > 0.0f)
            trigger.boundary_timer = boundary_timer;
    }

    // Handle actions
    if (trigger.IsHookToggleTrigger())
    {
        TriggersLine::HookToggleTrigger hook_toggle;
        hook_toggle.contraction_trigger_hookgroup_id = shortbound_trigger_action;
        hook_toggle.extension_trigger_hookgroup_id = longbound_trigger_action;
        trigger.SetHookToggleTrigger(hook_toggle);
    }
    else if (trigger.HasFlag_E_EngineTrigger())
    {
        TriggersLine::EngineTrigger engine_trigger;
        engine_trigger.function = TriggersLine::EngineTrigger::Function(shortbound_trigger_action);
        engine_trigger.motor_index = longbound_trigger_action;
        trigger.SetEngineTrigger(engine_trigger);
    }
    else
    {
        TriggersLine::CommandKeyTrigger command_keys;
        command_keys.contraction_trigger_key = shortbound_trigger_action;
        command_keys.extension_trigger_key   = longbound_trigger_action;
        trigger.SetCommandKeyTrigger(command_keys);
    }

    m_document->triggers.push_back(trigger);
    m_document->lines.emplace_back(Line(KEYWORD_TRIGGERS, (int)m_document->triggers.size() - 1));
}

void Parser::ParseTorqueCurve()
{
    TorquecurveLine torque_curve;

    Ogre::StringVector args = Ogre::StringUtil::split(m_current_line, ",");
    bool valid = true;
    if (args.size() == 1u)
    {
        torque_curve.predefined_func_name = args[0];
    }
    else if (args.size() == 2u)
    {
        torque_curve.sample.power          = this->ParseArgFloat(args[0].c_str());
        torque_curve.sample.torque_percent = this->ParseArgFloat(args[1].c_str());
    }
    else
    {
        // Consistent with 0.38's parser.
        this->AddMessage(Message::TYPE_ERROR, "Invalid line, too many arguments");
        valid = false;
    }

    if (valid)
    {
        m_document->torquecurve.push_back(torque_curve);
        m_document->lines.emplace_back(Line(KEYWORD_TORQUECURVE, (int)m_document->torquecurve.size() - 1));
    }
}

void Parser::ParseTies()
{
    if (! this->CheckNumArguments(5)) { return; }

    TiesLine tie;

    tie.root_node         = this->GetArgNodeRef(0);
    tie.max_reach_length  = this->GetArgFloat  (1);
    tie.auto_shorten_rate = this->GetArgFloat  (2);
    tie.min_length        = this->GetArgFloat  (3);
    tie.max_length        = this->GetArgFloat  (4);

    if (m_num_args > 5)
    {
        for (char c: this->GetArgStr(5))
        {
            switch (c)
            {
            case TiesLine::OPTION_n_FILLER:
            case TiesLine::OPTION_v_FILLER:
                break;

            case TiesLine::OPTION_i_INVISIBLE:
                tie.is_invisible = true;
                break;

            case TiesLine::OPTION_s_NO_SELF_LOCK:
                tie.disable_self_lock = true;
                break;

            default:
                this->AddMessage(Message::TYPE_WARNING, std::string("Invalid option: ") + c + ", ignoring...");
                break;
            }
        }
    }

    if (m_num_args > 6) { tie.max_stress   =  this->GetArgFloat (6); }
    if (m_num_args > 7) { tie.group        =  this->GetArgInt   (7); }

    m_document->ties.push_back(tie);
    m_document->lines.emplace_back(Line(KEYWORD_TIES, (int)m_document->ties.size() - 1));
}

void Parser::ParseSoundsources()
{
    if (! this->CheckNumArguments(2)) { return; }
    
    SoundsourcesLine soundsource;
    soundsource.node              = this->GetArgNodeRef(0);
    soundsource.sound_script_name = this->GetArgStr(1);

    m_document->soundsources.push_back(soundsource);
    m_document->lines.emplace_back(Line(KEYWORD_SOUNDSOURCES, (int)m_document->soundsources.size() - 1));
}

void Parser::ParseSoundsources2()
{
    if (! this->CheckNumArguments(3)) { return; }
    
    Soundsources2Line soundsource2;
    soundsource2.node              = this->GetArgNodeRef(0);
    soundsource2.sound_script_name = this->GetArgStr(2);

    int mode = this->GetArgInt(1);
    if (mode < 0)
    {
        if (mode < -2)
        {
            std::string msg = this->GetArgStr(1) + " is invalid soundsources2.mode, falling back to default -2";
            this->AddMessage(Message::TYPE_ERROR, msg.c_str());
            mode = -2;
        }
        soundsource2.mode = Soundsources2Line::Mode(mode);
    }
    else
    {
        soundsource2.mode = Soundsources2Line::MODE_CINECAM;
        soundsource2.cinecam_index = mode;
    }

    m_document->soundsources2.push_back(soundsource2);
    m_document->lines.emplace_back(Line(KEYWORD_SOUNDSOURCES2, (int)m_document->soundsources2.size() - 1));
}

void Parser::ParseSlidenodes()
{
    Ogre::StringVector args = Ogre::StringUtil::split(m_current_line, ", ");
    if (args.size() < 2u)
    {
        this->AddMessage(Message::TYPE_ERROR, "Too few arguments");
    }

    SlidenodesLine slidenode;
    slidenode.slide_node = (args[0]);
    
    bool in_rail_node_list = true;

    for (auto itor = args.begin() + 1; itor != args.end(); ++itor)
    {
        char c = toupper(itor->at(0));
        switch (c)
        {
        case 'S':
            slidenode.spring_rate = this->ParseArgFloat(itor->substr(1));
            in_rail_node_list = false;
            break;
        case 'B':
            slidenode.break_force = this->ParseArgFloat(itor->substr(1));
            slidenode._break_force_set = true;
            in_rail_node_list = false;
            break;
        case 'T':
            slidenode.tolerance = this->ParseArgFloat(itor->substr(1));
            in_rail_node_list = false;
            break;
        case 'R':
            slidenode.attachment_rate = this->ParseArgFloat(itor->substr(1));
            in_rail_node_list = false;
            break;
        case 'G':
            slidenode.railgroup_id = this->ParseArgFloat(itor->substr(1));
            slidenode._railgroup_id_set = true;
            in_rail_node_list = false;
            break;
        case 'D':
            slidenode.max_attachment_distance = this->ParseArgFloat(itor->substr(1));
            in_rail_node_list = false;
            break;
        case 'C':
            switch (itor->at(1))
            {
            case 'a':
                BITMASK_SET_1(slidenode.constraint_flags, SlidenodesLine::CONSTRAINT_ATTACH_ALL);
                break;
            case 'f':
                BITMASK_SET_1(slidenode.constraint_flags, SlidenodesLine::CONSTRAINT_ATTACH_FOREIGN);
                break;
            case 's':
                BITMASK_SET_1(slidenode.constraint_flags, SlidenodesLine::CONSTRAINT_ATTACH_SELF);
                break;
            case 'n':
                BITMASK_SET_1(slidenode.constraint_flags, SlidenodesLine::CONSTRAINT_ATTACH_NONE);
                break;
            default:
                this->AddMessage(Message::TYPE_WARNING, std::string("Ignoring invalid option: ") + itor->at(1));
                break;
            }
            in_rail_node_list = false;
            break;
        default:
            if (in_rail_node_list)
            {
                NodeRef_t ref(*itor);
                slidenode.rail_node_ranges.push_back(NodeRangeCommon(ref, ref));
            }
            break;
        }
    }
    
    m_document->slidenodes.push_back(slidenode);
    m_document->lines.emplace_back(Line(KEYWORD_SLIDENODES, (int)m_document->slidenodes.size() - 1));
}

void Parser::ParseShock3()
{
    if (! this->CheckNumArguments(15)) { return; }

    Shocks3Line shock_3;

    shock_3.nodes[0]       = this->GetArgNodeRef( 0);
    shock_3.nodes[1]       = this->GetArgNodeRef( 1);
    shock_3.spring_in      = this->GetArgFloat  ( 2);
    shock_3.damp_in        = this->GetArgFloat  ( 3);
    shock_3.damp_in_slow   = this->GetArgFloat  ( 4);
    shock_3.split_vel_in   = this->GetArgFloat  ( 5);
    shock_3.damp_in_fast   = this->GetArgFloat  ( 6);
    shock_3.spring_out     = this->GetArgFloat  ( 7);
    shock_3.damp_out       = this->GetArgFloat  ( 8);
    shock_3.damp_out_slow  = this->GetArgFloat  ( 9);
    shock_3.split_vel_out  = this->GetArgFloat  (10);
    shock_3.damp_out_fast  = this->GetArgFloat  (11);
    shock_3.short_bound    = this->GetArgFloat  (12);
    shock_3.long_bound     = this->GetArgFloat  (13);
    shock_3.precompression = this->GetArgFloat  (14);

    shock_3.options = 0u;
    if (m_num_args > 15)
    {
        std::string options_str = this->GetArgStr(15);
        auto itor = options_str.begin();
        auto endi = options_str.end();
        while (itor != endi)
        {
            char c = *itor++; // ++
            switch (c)
            {
                case 'n': 
                case 'v': 
                    break; // Placeholder, does nothing.
                case 'i': BITMASK_SET_1(shock_3.options, Shocks3Line::OPTION_i_INVISIBLE);
                    break;
                case 'm': BITMASK_SET_1(shock_3.options, Shocks3Line::OPTION_m_METRIC);
                    break;
                case 'M': BITMASK_SET_1(shock_3.options, Shocks3Line::OPTION_M_ABSOLUTE_METRIC);
                    break;
                default: {
                        char msg[100] = "";
                        snprintf(msg, 100, "Invalid option: '%c', ignoring...", c);
                        AddMessage(Message::TYPE_WARNING, msg);
                    }
                    break;
            }
        }
    }

    m_document->shocks3.push_back(shock_3);
    m_document->lines.emplace_back(Line(KEYWORD_SHOCKS3, (int)m_document->shocks3.size() - 1));
}

void Parser::ParseShock2()
{
    if (! this->CheckNumArguments(13)) { return; }

    Shocks2Line shock_2;

    shock_2.nodes[0]                   = this->GetArgNodeRef( 0);
    shock_2.nodes[1]                   = this->GetArgNodeRef( 1);
    shock_2.spring_in                  = this->GetArgFloat  ( 2);
    shock_2.damp_in                    = this->GetArgFloat  ( 3);
    shock_2.progress_factor_spring_in  = this->GetArgFloat  ( 4);
    shock_2.progress_factor_damp_in    = this->GetArgFloat  ( 5);
    shock_2.spring_out                 = this->GetArgFloat  ( 6);
    shock_2.damp_out                   = this->GetArgFloat  ( 7);
    shock_2.progress_factor_spring_out = this->GetArgFloat  ( 8);
    shock_2.progress_factor_damp_out   = this->GetArgFloat  ( 9);
    shock_2.short_bound                = this->GetArgFloat  (10);
    shock_2.long_bound                 = this->GetArgFloat  (11);
    shock_2.precompression             = this->GetArgFloat  (12);

    shock_2.options = 0u;
    if (m_num_args > 13)
    {
        std::string options_str = this->GetArgStr(13);
        auto itor = options_str.begin();
        auto endi = options_str.end();
        while (itor != endi)
        {
            char c = *itor++; // ++
            switch (c)
            {
                case 'n': 
                case 'v': 
                    break; // Placeholder, does nothing.
                case 'i': BITMASK_SET_1(shock_2.options, Shocks2Line::OPTION_i_INVISIBLE);
                    break;
                case 'm': BITMASK_SET_1(shock_2.options, Shocks2Line::OPTION_m_METRIC);
                    break;
                case 'M': BITMASK_SET_1(shock_2.options, Shocks2Line::OPTION_M_ABSOLUTE_METRIC);
                    break;
                case 's': BITMASK_SET_1(shock_2.options, Shocks2Line::OPTION_s_SOFT_BUMP_BOUNDS);
                    break;
                default: {
                        char msg[100] = "";
                        snprintf(msg, 100, "Invalid option: '%c', ignoring...", c);
                        AddMessage(Message::TYPE_WARNING, msg);
                    }
                    break;
            }
        }
    }

    m_document->shocks2.push_back(shock_2);
    m_document->lines.emplace_back(Line(KEYWORD_SHOCKS2, (int)m_document->shocks3.size() - 1));
}

void Parser::ParseShock()
{
    if (! this->CheckNumArguments(7)) { return; }

    ShocksLine shock;

    shock.nodes[0]       = this->GetArgNodeRef(0);
    shock.nodes[1]       = this->GetArgNodeRef(1);
    shock.spring_rate    = this->GetArgFloat  (2);
    shock.damping        = this->GetArgFloat  (3);
    shock.short_bound    = this->GetArgFloat  (4);
    shock.long_bound     = this->GetArgFloat  (5);
    shock.precompression = this->GetArgFloat  (6);

    shock.options = 0u;
    if (m_num_args > 7)
    {
        std::string options_str = this->GetArgStr(7);
        auto itor = options_str.begin();
        auto endi = options_str.end();
        while (itor != endi)
        {
            char c = *itor++;
            switch (c)
            {
                case 'n':
                case 'v':
                    break; // Placeholder, does nothing.
                case 'i': BITMASK_SET_1(shock.options, ShocksLine::OPTION_i_INVISIBLE);
                    break;
                case 'm': BITMASK_SET_1(shock.options, ShocksLine::OPTION_m_METRIC);
                    break;
                case 'r':
                case 'R': BITMASK_SET_1(shock.options, ShocksLine::OPTION_R_ACTIVE_RIGHT);
                    break;
                case 'l':
                case 'L': BITMASK_SET_1(shock.options, ShocksLine::OPTION_L_ACTIVE_LEFT);
                    break;
                default: {
                    char msg[100] = "";
                    snprintf(msg, 100, "Invalid option: '%c', ignoring...", c);
                    AddMessage(Message::TYPE_WARNING, msg);
                }
                break;
            }
        }
    }
    m_document->shocks.push_back(shock);
    m_document->lines.emplace_back(Line(KEYWORD_SHOCKS, (int)m_document->shocks.size() - 1));
}

void Parser::_CheckInvalidTrailingText(Ogre::String const & line, std::smatch const & results, unsigned int index)
{
    if (results[index].matched) // Invalid trailing text 
    {
        std::stringstream msg;
        msg << "Invalid text after parameters: '" << results[index] << "'. Please remove. Ignoring...";
        AddMessage(line, Message::TYPE_WARNING, msg.str());
    }
}

void Parser::ParseDirectiveSetDefaultMinimass()
{
    if (! this->CheckNumArguments(2)) { return; } // Directive name + parameter

    SetDefaultMinimassLine dm;
    dm.min_mass = this->GetArgFloat(1);

    m_document->set_default_minimass.push_back(dm);
    m_document->lines.emplace_back(Line(KEYWORD_SET_DEFAULT_MINIMASS, (int)m_document->set_default_minimass.size() - 1));
}

void Parser::ParseDirectiveSetInertiaDefaults()
{
    if (! this->CheckNumArguments(2)) { return; }

    SetInertiaDefaultsLine inertia;
    inertia._num_args = m_num_args;
    inertia.start_delay_factor = this->GetArgFloat(1);
    if (m_num_args > 2) { inertia.stop_delay_factor = this->GetArgFloat(2); }
    if (m_num_args > 3) { inertia.start_function = this->GetArgStr(3); }
    if (m_num_args > 4) { inertia.stop_function  = this->GetArgStr(4); }
    
    m_document->set_inertia_defaults.push_back(inertia);
    m_document->lines.emplace_back(Line(KEYWORD_SET_INERTIA_DEFAULTS, (int)m_document->set_inertia_defaults.size() - 1));
}

void Parser::ParseScrewprops()
{
    if (! this->CheckNumArguments(4)) { return; }
    
    ScrewpropsLine screwprop;

    screwprop.prop_node = this->GetArgNodeRef(0);
    screwprop.back_node = this->GetArgNodeRef(1);
    screwprop.top_node  = this->GetArgNodeRef(2);
    screwprop.power     = this->GetArgFloat  (3);

    m_document->screwprops.push_back(screwprop);
    m_document->lines.emplace_back(Line(KEYWORD_SCREWPROPS, (int)m_document->screwprops.size() - 1));
}

void Parser::ParseRotators()
{
    if (! this->CheckNumArguments(13)) { return; }

    RotatorsLine rotator;
    rotator.axis_nodes[0]           = this->GetArgNodeRef( 0);
    rotator.axis_nodes[1]           = this->GetArgNodeRef( 1);
    rotator.base_plate_nodes[0]     = this->GetArgNodeRef( 2);
    rotator.base_plate_nodes[1]     = this->GetArgNodeRef( 3);
    rotator.base_plate_nodes[2]     = this->GetArgNodeRef( 4);
    rotator.base_plate_nodes[3]     = this->GetArgNodeRef( 5);
    rotator.rotating_plate_nodes[0] = this->GetArgNodeRef( 6);
    rotator.rotating_plate_nodes[1] = this->GetArgNodeRef( 7);
    rotator.rotating_plate_nodes[2] = this->GetArgNodeRef( 8);
    rotator.rotating_plate_nodes[3] = this->GetArgNodeRef( 9);
    rotator.rate                    = this->GetArgFloat  (10);
    rotator.spin_left_key           = this->GetArgInt    (11);
    rotator.spin_right_key          = this->GetArgInt    (12);

    this->ParseOptionalInertia(rotator.inertia, 13);
    if (m_num_args > 17) { rotator.engine_coupling = this->GetArgFloat(17); }
    if (m_num_args > 18) { rotator.needs_engine    = this->GetArgBool (18); }

    m_document->rotators.push_back(rotator);
    m_document->lines.emplace_back(Line(KEYWORD_ROTATORS, (int)m_document->rotators.size() - 1));
}

void Parser::ParseRotators2()
{
    if (! this->CheckNumArguments(13)) { return; }

    Rotators2Line rotator;
    
    rotator.axis_nodes[0]           = this->GetArgNodeRef( 0);
    rotator.axis_nodes[1]           = this->GetArgNodeRef( 1);
    rotator.base_plate_nodes[0]     = this->GetArgNodeRef( 2);
    rotator.base_plate_nodes[1]     = this->GetArgNodeRef( 3);
    rotator.base_plate_nodes[2]     = this->GetArgNodeRef( 4);
    rotator.base_plate_nodes[3]     = this->GetArgNodeRef( 5);
    rotator.rotating_plate_nodes[0] = this->GetArgNodeRef( 6);
    rotator.rotating_plate_nodes[1] = this->GetArgNodeRef( 7);
    rotator.rotating_plate_nodes[2] = this->GetArgNodeRef( 8);
    rotator.rotating_plate_nodes[3] = this->GetArgNodeRef( 9);
    rotator.rate                    = this->GetArgFloat  (10);
    rotator.spin_left_key           = this->GetArgInt    (11);
    rotator.spin_right_key          = this->GetArgInt    (12);

    // Extra args for rotators2
    if (m_num_args > 13) { rotator.rotating_force  = this->GetArgFloat(13); }
    if (m_num_args > 14) { rotator.tolerance       = this->GetArgFloat(14); }
    if (m_num_args > 15) { rotator.description     = this->GetArgStr  (15); }

    this->ParseOptionalInertia(rotator.inertia, 16); // 4 args
    if (m_num_args > 20) { rotator.engine_coupling = this->GetArgFloat(20); }
    if (m_num_args > 21) { rotator.needs_engine    = this->GetArgBool (21); }

    m_document->rotators2.push_back(rotator);
    m_document->lines.emplace_back(Line(KEYWORD_ROTATORS2, (int)m_document->rotators2.size() - 1));

}

void Parser::ParseFileinfo()
{
    if (! this->CheckNumArguments(2)) { return; }

    FileinfoLine fileinfo;

    fileinfo.unique_id = this->GetArgStr(1);
    Ogre::StringUtil::trim(fileinfo.unique_id);

    if (m_num_args > 2) { fileinfo.category_id  = this->GetArgInt(2); }
    if (m_num_args > 3) { fileinfo.file_version = this->GetArgInt(3); }

    m_document->file_info.push_back(fileinfo);
    m_document->lines.emplace_back(Line(KEYWORD_FILEINFO, (int)m_document->file_info.size() - 1));
}

void Parser::ParseRopes()
{
    if (! this->CheckNumArguments(2)) { return; }
    
    RopesLine rope;
    rope.root_node      = this->GetArgNodeRef(0);
    rope.end_node       = this->GetArgNodeRef(1);
    
    if (m_num_args > 2) { rope.invisible  = (this->GetArgChar(2) == 'i'); }

    m_document->ropes.push_back(rope);
    m_document->lines.emplace_back(Line(KEYWORD_ROPES, (int)m_document->ropes.size() - 1));
}

void Parser::ParseRopables()
{
    if (! this->CheckNumArguments(1)) { return; }

    RopablesLine ropable;
    ropable.node = this->GetArgNodeRef(0);
    
    if (m_num_args > 1) { ropable.group         =  this->GetArgInt(1); }
    if (m_num_args > 2) { ropable.has_multilock = (this->GetArgInt(2) == 1); }

    m_document->ropables.push_back(ropable);
    m_document->lines.emplace_back(Line(KEYWORD_ROPABLES, (int)m_document->ropables.size() - 1));
}

void Parser::ParseRailGroups()
{
    Ogre::StringVector args = Ogre::StringUtil::split(m_current_line, ",");
    if (args.size() < 3u)
    {
        this->AddMessage(Message::TYPE_ERROR, "Not enough parameters");
        return;
    }

    RailgroupsLine railgroup;
    railgroup.id = this->ParseArgInt(args[0].c_str());

    for (auto itor = args.begin() + 1; itor != args.end(); itor++)
    {
        NodeRef_t ref = *itor;
        railgroup.node_list.push_back(NodeRangeCommon(ref,ref));
    }

    m_document->railgroups.push_back(railgroup);
}

void Parser::ParseProps()
{
    if (! this->CheckNumArguments(10)) { return; }

    PropsLine prop;
    prop.reference_node = this->GetArgNodeRef(0);
    prop.x_axis_node    = this->GetArgNodeRef(1);
    prop.y_axis_node    = this->GetArgNodeRef(2);
    prop.offset.x       = this->GetArgFloat  (3);
    prop.offset.y       = this->GetArgFloat  (4);
    prop.offset.z       = this->GetArgFloat  (5);
    prop.rotation.x     = this->GetArgFloat  (6);
    prop.rotation.y     = this->GetArgFloat  (7);
    prop.rotation.z     = this->GetArgFloat  (8);
    prop.mesh_name      = this->GetArgStr    (9);

    bool is_dash = false;
         if (prop.mesh_name.find("leftmirror"  ) != std::string::npos) { prop.special = SpecialProp::MIRROR_LEFT; }
    else if (prop.mesh_name.find("rightmirror" ) != std::string::npos) { prop.special = SpecialProp::MIRROR_RIGHT; }
    else if (prop.mesh_name.find("dashboard-rh") != std::string::npos) { prop.special = SpecialProp::DASHBOARD_RIGHT; is_dash = true; }
    else if (prop.mesh_name.find("dashboard"   ) != std::string::npos) { prop.special = SpecialProp::DASHBOARD_LEFT;  is_dash = true; }
    else if (Ogre::StringUtil::startsWith(prop.mesh_name, "spinprop", false) ) { prop.special = SpecialProp::AERO_PROP_SPIN; }
    else if (Ogre::StringUtil::startsWith(prop.mesh_name, "pale", false)     ) { prop.special = SpecialProp::AERO_PROP_BLADE; }
    else if (Ogre::StringUtil::startsWith(prop.mesh_name, "seat", false)     ) { prop.special = SpecialProp::DRIVER_SEAT; }
    else if (Ogre::StringUtil::startsWith(prop.mesh_name, "seat2", false)    ) { prop.special = SpecialProp::DRIVER_SEAT_2; }
    else if (Ogre::StringUtil::startsWith(prop.mesh_name, "beacon", false)   ) { prop.special = SpecialProp::BEACON; }
    else if (Ogre::StringUtil::startsWith(prop.mesh_name, "redbeacon", false)) { prop.special = SpecialProp::REDBEACON; }
    else if (Ogre::StringUtil::startsWith(prop.mesh_name, "lightb", false)   ) { prop.special = SpecialProp::LIGHTBAR; } // Previously: 'strncmp("lightbar", meshname, 6)'

    if ((prop.special == SpecialProp::BEACON) && (m_num_args >= 14))
    {
        prop.special_prop_beacon.flare_material_name = this->GetArgStr(10);
        Ogre::StringUtil::trim(prop.special_prop_beacon.flare_material_name);

        prop.special_prop_beacon.color = Ogre::ColourValue(
            this->GetArgFloat(11), this->GetArgFloat(12), this->GetArgFloat(13));
    }
    else if (is_dash)
    {
        if (m_num_args > 10) prop.special_prop_dashboard.mesh_name = this->GetArgStr(10);
        if (m_num_args > 13)
        {
            prop.special_prop_dashboard.offset = Ogre::Vector3(this->GetArgFloat(11), this->GetArgFloat(12), this->GetArgFloat(13));
            prop.special_prop_dashboard._offset_is_set = true;
        }
        if (m_num_args > 14) prop.special_prop_dashboard.rotation_angle = this->GetArgFloat(14);
    }

    m_document->props.push_back(prop);
    m_document->lines.emplace_back(Line(KEYWORD_PROPS, (int)m_document->props.size() - 1));
}

void Parser::ParsePistonprops()
{
    if (!this->CheckNumArguments(10)) { return; }

    PistonpropsLine pistonprop;
    pistonprop.reference_node     = this->GetArgNodeRef     (0);
    pistonprop.axis_node          = this->GetArgNodeRef     (1);
    pistonprop.blade_tip_nodes[0] = this->GetArgNodeRef     (2);
    pistonprop.blade_tip_nodes[1] = this->GetArgNodeRef     (3);
    pistonprop.blade_tip_nodes[2] = this->GetArgNullableNode(4);
    pistonprop.blade_tip_nodes[3] = this->GetArgNullableNode(5);
    pistonprop.couple_node        = this->GetArgNullableNode(6);
    pistonprop.turbine_power_kW   = this->GetArgFloat       (7);
    pistonprop.pitch              = this->GetArgFloat       (8);
    pistonprop.airfoil            = this->GetArgStr         (9);

    m_document->pistonprops.push_back(pistonprop);
    m_document->lines.emplace_back(Line(KEYWORD_PISTONPROPS, (int)m_document->pistonprops.size() - 1));

}

void Parser::ParseParticles()
{
    if (!this->CheckNumArguments(3)) { return; }

    ParticlesLine particle;
    particle.emitter_node         = this->GetArgNodeRef(0);
    particle.reference_node       = this->GetArgNodeRef(1);
    particle.particle_system_name = this->GetArgStr    (2);

    m_document->particles.push_back(particle);
    m_document->lines.emplace_back(Line(KEYWORD_PARTICLES, (int)m_document->particles.size() - 1));
}

// Static
void Parser::_TrimTrailingComments(std::string const & line_in, std::string & line_out)
{
    // Trim trailing comment
    // We need to handle a case of lines as [keyword 1, 2, 3 ;;///// Comment!]
    int comment_start = static_cast<int>(line_in.find_first_of(";"));
    if (comment_start != Ogre::String::npos)
    {
        line_out = line_in.substr(0, comment_start);
        return;
    }
    // The [//Comment] is harder - the '/' character may also be present in DESCRIPTION arguments!
    comment_start = static_cast<int>(line_in.find_last_of("/"));
    if (comment_start != Ogre::String::npos)
    {
        while (comment_start >= 0)
        {
            char c = line_in[comment_start - 1];
            if (c != '/' && c != ' ' && c != '\t')
            {
                break; // Start of comment found
            }
            --comment_start;
        }
        line_out = line_in.substr(0, comment_start);
        return;
    }
    // No comment found
    line_out = line_in;
}

void Parser::ParseNodes()
{
    if (! this->CheckNumArguments(4)) { return; }

    NodesLine node;
    node._num_args = m_num_args;
    node.num = NodeNum_t(this->GetArgInt(0));
    node.position.x = this->GetArgFloat(1);
    node.position.y = this->GetArgFloat(2);
    node.position.z = this->GetArgFloat(3);
    node.options = this->GetArgStr(4);
    if (m_num_args > 5)
    {
        // Only used on spawn if 'l' flag is present
        node.loadweight_override = this->GetArgFloat(5);
    }

    m_document->nodes.push_back(node);
    m_document->lines.emplace_back(Line(KEYWORD_NODES, (int)m_document->nodes.size() - 1));
}

void Parser::ParseNodes2()
{
    if (! this->CheckNumArguments(4)) { return; }

    Nodes2Line node;
    node._num_args = m_num_args;
    node.name = this->GetArgStr(0);
    node.position.x = this->GetArgFloat(1);
    node.position.y = this->GetArgFloat(2);
    node.position.z = this->GetArgFloat(3);
    node.options = this->GetArgStr(4);
    if (m_num_args > 5)
    {
        // Only used on spawn if 'l' flag is present
        node.loadweight_override = this->GetArgFloat(5);
    }

    m_document->nodes2.push_back(node);
    m_document->lines.emplace_back(Line(KEYWORD_NODES2, (int)m_document->nodes2.size() - 1));
}

void Parser::ParseNodeCollision()
{
    if (! this->CheckNumArguments(2)) { return; }
    
    NodecollisionLine node_collision;
    node_collision.node   = this->GetArgNodeRef(0);
    node_collision.radius = this->GetArgFloat  (1);
    
    m_document->nodecollision.push_back(node_collision);
    m_document->lines.emplace_back(Line(KEYWORD_NODECOLLISION, (int)m_document->nodecollision.size() - 1));
}

void Parser::ParseMinimass()
{
    MinimassLine minimass;
    minimass.min_mass = this->GetArgFloat(0);
    if (m_num_args > 1)
    {
        const std::string options_str = this->GetArgStr(1);
        for (char c: options_str)
        {
            switch (c)
            {
            case '\0': // Terminator NULL character
            case (MinimassLine::OPTION_n_FILLER):
                break;

            case (MinimassLine::OPTION_l_SKIP_LOADED):
                minimass.option_l_skip_loaded = true;
                break;

            default:
                this->AddMessage(Message::TYPE_WARNING, std::string("Unknown option: ") + c);
                break;
            }
        }
    }

    m_document->minimass.push_back(minimass);
    m_document->lines.emplace_back(Line(KEYWORD_MINIMASS, (int)m_document->minimass.size() - 1));
}

void Parser::ParseFlexbodywheels()
{
    if (! this->CheckNumArguments(16)) { return; }

    FlexbodywheelsLine flexbody_wheel;

    flexbody_wheel.tyre_radius        = this->GetArgFloat        ( 0);
    flexbody_wheel.rim_radius         = this->GetArgFloat        ( 1);
    flexbody_wheel.width              = this->GetArgFloat        ( 2);
    flexbody_wheel.num_rays           = this->GetArgInt          ( 3);
    flexbody_wheel.nodes[0]           = this->GetArgNodeRef      ( 4);
    flexbody_wheel.nodes[1]           = this->GetArgNodeRef      ( 5);
    flexbody_wheel.rigidity_node      = this->GetArgRigidityNode ( 6);
    flexbody_wheel.braking            = this->GetArgBraking      ( 7);
    flexbody_wheel.propulsion         = this->GetArgPropulsion   ( 8);
    flexbody_wheel.reference_arm_node = this->GetArgNodeRef      ( 9);
    flexbody_wheel.mass               = this->GetArgFloat        (10);
    flexbody_wheel.tyre_springiness   = this->GetArgFloat        (11);
    flexbody_wheel.tyre_damping       = this->GetArgFloat        (12);
    flexbody_wheel.rim_springiness    = this->GetArgFloat        (13);
    flexbody_wheel.rim_damping        = this->GetArgFloat        (14);
    flexbody_wheel.side               = this->GetArgWheelSide    (15);

    if (m_num_args > 16) { flexbody_wheel.rim_mesh_name  = this->GetArgStr(16); }
    if (m_num_args > 17) { flexbody_wheel.tyre_mesh_name = this->GetArgStr(17); }

    m_document->flexbodywheels.push_back(flexbody_wheel);
    m_document->lines.emplace_back(Line(KEYWORD_FLEXBODYWHEELS, (int)m_document->flexbodywheels.size() - 1));
}

void Parser::ParseMaterialFlareBindings()
{
    if (! this->CheckNumArguments(2)) { return; }

    MaterialflarebindingsLine binding;
    binding.flare_number  = this->GetArgInt(0);
    binding.material_name = this->GetArgStr(1);
    
    m_document->materialflarebindings.push_back(binding);
    m_document->lines.emplace_back(Line(KEYWORD_MATERIALFLAREBINDINGS, (int)m_document->materialflarebindings.size() - 1));
}

void Parser::ParseManagedMaterials()
{
    if (! this->CheckNumArguments(2)) { return; }

    ManagedmaterialsLine managed_mat;
    
    managed_mat.name    = this->GetArgStr(0);

    const std::string type_str = this->GetArgStr(1);
    if (type_str == "mesh_standard" || type_str == "mesh_transparent")
    {
        if (! this->CheckNumArguments(3)) { return; }

        managed_mat.type = (type_str == "mesh_standard")
            ? ManagedMaterialType::MESH_STANDARD
            : ManagedMaterialType::MESH_TRANSPARENT;
        
        managed_mat.diffuse_map = this->GetArgStr(2);
        
        if (m_num_args > 3) { managed_mat.specular_map = this->GetArgManagedTex(3); }
    }
    else if (type_str == "flexmesh_standard" || type_str == "flexmesh_transparent")
    {
        if (! this->CheckNumArguments(3)) { return; }

        managed_mat.type = (type_str == "flexmesh_standard")
            ? ManagedMaterialType::FLEXMESH_STANDARD
            : ManagedMaterialType::FLEXMESH_TRANSPARENT;
            
        managed_mat.diffuse_map = this->GetArgStr(2);
        
        if (m_num_args > 3) { managed_mat.damaged_diffuse_map = this->GetArgManagedTex(3); }
        if (m_num_args > 4) { managed_mat.specular_map        = this->GetArgManagedTex(4); }
    }
    else
    {
        this->AddMessage(Message::TYPE_WARNING, type_str + " is an unkown effect");
        return;
    }

    Ogre::ResourceGroupManager& rgm = Ogre::ResourceGroupManager::getSingleton();

    if (!rgm.resourceExists(m_resource_group, managed_mat.diffuse_map))
    {
        this->AddMessage(Message::TYPE_WARNING, "Missing texture file: " + managed_mat.diffuse_map);
        return;
    }
    if (managed_mat.damaged_diffuse_map != "" &&
        !rgm.resourceExists(m_resource_group, managed_mat.damaged_diffuse_map))
    {
        this->AddMessage(Message::TYPE_WARNING, "Missing texture file: " + managed_mat.damaged_diffuse_map);
        managed_mat.damaged_diffuse_map = "-";
    }
    if (managed_mat.specular_map != "" &&
        !rgm.resourceExists(m_resource_group, managed_mat.specular_map))
    {
        this->AddMessage(Message::TYPE_WARNING, "Missing texture file: " + managed_mat.specular_map);
        managed_mat.specular_map = "-";
    }

    m_document->managed_materials.push_back(managed_mat);
    m_document->lines.emplace_back(Line(KEYWORD_MANAGEDMATERIALS, (int)m_document->managed_materials.size() - 1));
}

void Parser::ParseLockgroups()
{
    if (! this->CheckNumArguments(2)) { return; } // Lockgroup num. + at least 1 node...

    LockgroupsLine lockgroup;
    lockgroup.number = this->GetArgInt(0);
    
    for (int i = 1; i < m_num_args; ++i)
    {
        lockgroup.nodes.push_back(this->GetArgNodeRef(i));
    }
    
    m_document->lockgroups.push_back(lockgroup);
    m_document->lines.emplace_back(Line(KEYWORD_LOCKGROUPS, (int)m_document->lockgroups.size() - 1));
}

void Parser::ParseHydros()
{
    if (! this->CheckNumArguments(3)) { return; }

    HydrosLine hydro;
    
    hydro.nodes[0]           = this->GetArgNodeRef(0);
    hydro.nodes[1]           = this->GetArgNodeRef(1);
    hydro.lenghtening_factor = this->GetArgFloat  (2);
    
    if (m_num_args > 3) { hydro.options = this->GetArgStr(3); }
    
    this->ParseOptionalInertia(hydro.inertia, 4);

    m_document->hydros.push_back(hydro);
    m_document->lines.emplace_back(Line(KEYWORD_HYDROS, (int)m_document->hydros.size() - 1));
}

void Parser::ParseOptionalInertia(InertiaCommon & inertia, int index)
{
    if (m_num_args > index) { inertia.start_delay_factor = this->GetArgFloat(index++); }
    if (m_num_args > index) { inertia.stop_delay_factor  = this->GetArgFloat(index++); }
    if (m_num_args > index) { inertia.start_function     = this->GetArgStr  (index++); }
    if (m_num_args > index) { inertia.stop_function      = this->GetArgStr  (index++); }
}

void Parser::ParseBeams()
{
    if (! this->CheckNumArguments(2)) { return; }
    
    BeamsLine beam;
    
    beam.nodes[0] = this->GetArgNodeRef(0);
    beam.nodes[1] = this->GetArgNodeRef(1);

    // Flags 
    if (m_num_args > 2)
    {
        std::string options_str = this->GetArgStr(2);
        for (auto itor = options_str.begin(); itor != options_str.end(); ++itor)
        {
                 if (*itor == 'v') { continue; } // Dummy flag
            else if (*itor == 'i') { beam.options |= BeamsLine::OPTION_i_INVISIBLE; }
            else if (*itor == 'r') { beam.options |= BeamsLine::OPTION_r_ROPE; }
            else if (*itor == 's') { beam.options |= BeamsLine::OPTION_s_SUPPORT; }
            else
            {
                char msg[200] = "";
                sprintf(msg, "Invalid flag: %c", *itor);
                this->AddMessage(Message::TYPE_WARNING, msg);
            }
        }
    }
    
    if ((m_num_args > 3) && (beam.options & BeamsLine::OPTION_s_SUPPORT))
    {
        float support_break_limit = 0.0f;
        float support_break_factor = this->GetArgInt(3);
        if (support_break_factor > 0.0f)
        {
            support_break_limit = support_break_factor;
        }
        beam.extension_break_limit = support_break_limit;
        beam._has_extension_break_limit = true;
    }

    m_document->beams.push_back(beam);
    m_document->lines.emplace_back(Line(KEYWORD_BEAMS, (int)m_document->beams.size() - 1));
}

void Parser::ParseAnimators()
{
    auto args = Ogre::StringUtil::split(m_current_line, ",");
    if (args.size() < 4) { return; }

    AnimatorsLine animator;

    animator.nodes[0]           = args[0];
    animator.nodes[1]           = args[1];
    animator.lenghtening_factor = this->ParseArgFloat(args[2]);

    // Parse options; Just use the split/trim/compare method
    Ogre::StringVector attrs = Ogre::StringUtil::split(args[3], "|");

    auto itor = attrs.begin();
    auto endi = attrs.end();
    for (; itor != endi; ++itor)
    {
        Ogre::String token = *itor;
        Ogre::StringUtil::trim(token);
        std::smatch results;
        bool is_shortlimit = false;

        // Numbered keywords 
        if (std::regex_search(token, results, Regexes::PARSE_ANIMATORS_NUMBERED_KEYWORD))
        {
                 if (results[1] == "throttle")   animator.aero_animator.flags |= AnimatorsLine::AeroAnimator::OPTION_THROTTLE;
            else if (results[1] == "rpm")        animator.aero_animator.flags |= AnimatorsLine::AeroAnimator::OPTION_RPM;
            else if (results[1] == "aerotorq")   animator.aero_animator.flags |= AnimatorsLine::AeroAnimator::OPTION_TORQUE;
            else if (results[1] == "aeropit")    animator.aero_animator.flags |= AnimatorsLine::AeroAnimator::OPTION_PITCH;
            else if (results[1] == "aerostatus") animator.aero_animator.flags |= AnimatorsLine::AeroAnimator::OPTION_STATUS;

            animator.aero_animator.motor = this->ParseArgInt(results[2].str().c_str());
        }
        else if ((is_shortlimit = (token.compare(0, 10, "shortlimit") == 0)) || (token.compare(0, 9, "longlimit") == 0))
        {
            Ogre::StringVector fields = Ogre::StringUtil::split(token, ":");
            if (fields.size() > 1)
            {
                if (is_shortlimit)
                {
                    animator.short_limit = std::strtod(fields[1].c_str(), nullptr);
                    animator.flags |= AnimatorsLine::OPTION_SHORT_LIMIT;
                }
                else
                {
                    animator.long_limit = std::strtod(fields[1].c_str(), nullptr);
                    animator.flags |= AnimatorsLine::OPTION_LONG_LIMIT;
                }
            }
        }
        else
        {
            // Standalone keywords 
                 if (token == "vis")           animator.flags |= AnimatorsLine::OPTION_VISIBLE;
            else if (token == "inv")           animator.flags |= AnimatorsLine::OPTION_INVISIBLE;
            else if (token == "airspeed")      animator.flags |= AnimatorsLine::OPTION_AIRSPEED;
            else if (token == "vvi")           animator.flags |= AnimatorsLine::OPTION_VERTICAL_VELOCITY;
            else if (token == "altimeter100k") animator.flags |= AnimatorsLine::OPTION_ALTIMETER_100K;
            else if (token == "altimeter10k")  animator.flags |= AnimatorsLine::OPTION_ALTIMETER_10K;
            else if (token == "altimeter1k")   animator.flags |= AnimatorsLine::OPTION_ALTIMETER_1K;
            else if (token == "aoa")           animator.flags |= AnimatorsLine::OPTION_ANGLE_OF_ATTACK;
            else if (token == "flap")          animator.flags |= AnimatorsLine::OPTION_FLAP;
            else if (token == "airbrake")      animator.flags |= AnimatorsLine::OPTION_AIR_BRAKE;
            else if (token == "roll")          animator.flags |= AnimatorsLine::OPTION_ROLL;
            else if (token == "pitch")         animator.flags |= AnimatorsLine::OPTION_PITCH;
            else if (token == "brakes")        animator.flags |= AnimatorsLine::OPTION_BRAKES;
            else if (token == "accel")         animator.flags |= AnimatorsLine::OPTION_ACCEL;
            else if (token == "clutch")        animator.flags |= AnimatorsLine::OPTION_CLUTCH;
            else if (token == "speedo")        animator.flags |= AnimatorsLine::OPTION_SPEEDO;
            else if (token == "tacho")         animator.flags |= AnimatorsLine::OPTION_TACHO;
            else if (token == "turbo")         animator.flags |= AnimatorsLine::OPTION_TURBO;
            else if (token == "parking")       animator.flags |= AnimatorsLine::OPTION_PARKING;
            else if (token == "shifterman1")   animator.flags |= AnimatorsLine::OPTION_SHIFT_LEFT_RIGHT;
            else if (token == "shifterman2")   animator.flags |= AnimatorsLine::OPTION_SHIFT_BACK_FORTH;
            else if (token == "sequential")    animator.flags |= AnimatorsLine::OPTION_SEQUENTIAL_SHIFT;
            else if (token == "shifterlin")    animator.flags |= AnimatorsLine::OPTION_GEAR_SELECT;
            else if (token == "torque")        animator.flags |= AnimatorsLine::OPTION_TORQUE;
            else if (token == "difflock")      animator.flags |= AnimatorsLine::OPTION_DIFFLOCK;
            else if (token == "rudderboat")    animator.flags |= AnimatorsLine::OPTION_BOAT_RUDDER;
            else if (token == "throttleboat")  animator.flags |= AnimatorsLine::OPTION_BOAT_THROTTLE;
        }
    }

    m_document->animators.push_back(animator);
    m_document->lines.emplace_back(Line(KEYWORD_ANIMATORS, (int)m_document->animators.size() - 1));
}

void Parser::ParseAuthor()
{
    if (! this->CheckNumArguments(2)) { return; }

    AuthorLine author;
    if (m_num_args > 1) { author.type             = this->GetArgStr(1); }
    if (m_num_args > 2) { author.forum_account_id = this->GetArgInt(2); author._has_forum_account = true; }
    if (m_num_args > 3) { author.name             = this->GetArgStr(3); }
    if (m_num_args > 4) { author.email            = this->GetArgStr(4); }

    m_document->authors.push_back(author);
    m_document->lines.emplace_back(Line(KEYWORD_AUTHOR, (int)m_document->authors.size() - 1));
}

// -------------------------------------------------------------------------- 
//  Utilities
// -------------------------------------------------------------------------- 

void Parser::AddMessage(std::string const & line, Message::Type type, std::string const & message)
{
    RoR::Str<4000> txt;

    if (!m_document->name.empty())
    {
        txt << m_document->name;
    }
    else
    {
        txt << m_filename;
    }

    txt << " (line " << (size_t)m_current_line_number;
    if (m_current_block != KEYWORD_INVALID)
    {
        txt << " '" << File::KeywordToString(m_current_block) << "'";
    }
    txt << "): " << message;

    RoR::Console::MessageType cm_type;
    switch (type)
    {
    case Message::TYPE_FATAL_ERROR:
        cm_type = RoR::Console::MessageType::CONSOLE_SYSTEM_ERROR;
        break;

    case Message::TYPE_ERROR:
    case Message::TYPE_WARNING:
        cm_type = RoR::Console::MessageType::CONSOLE_SYSTEM_WARNING;
        break;

    default:
        cm_type = RoR::Console::MessageType::CONSOLE_SYSTEM_NOTICE;
        break;
    }

    RoR::App::GetConsole()->putMessage(RoR::Console::CONSOLE_MSGTYPE_ACTOR, cm_type, txt.ToCStr());
}

Keyword Parser::IdentifyKeywordInCurrentLine()
{
    // Quick check - keyword always starts with ASCII letter
    char c = tolower(m_current_line[0]); // Note: line comes in trimmed
    if (c > 'z' || c < 'a')
    {
        return KEYWORD_INVALID;
    }

    // Search with correct lettercase
    std::smatch results;
    std::string line(m_current_line);
    std::regex_search(line, results, Regexes::IDENTIFY_KEYWORD_RESPECT_CASE); // Always returns true.
    Keyword keyword = FindKeywordMatch(results);
    if (keyword != KEYWORD_INVALID)
    {
        return keyword;
    }

    // Search and ignore lettercase
    std::regex_search(line, results, Regexes::IDENTIFY_KEYWORD_IGNORE_CASE); // Always returns true.
    keyword = FindKeywordMatch(results);
    if (keyword != KEYWORD_INVALID)
    {
        this->AddMessage(line, Message::TYPE_WARNING,
            "Keyword has invalid lettercase. Correct form is: " + std::string(File::KeywordToString(keyword)));
    }
    return keyword;
}

Keyword Parser::FindKeywordMatch(std::smatch& search_results)
{
    // The 'results' array contains a complete match at positon [0] and sub-matches starting with [1], 
    //    so we get exact positions in Regexes::IDENTIFY_KEYWORD, which again match Keyword enum members

    for (unsigned int i = 1; i < search_results.size(); i++)
    {
        std::ssub_match sub  = search_results[i];
        if (sub.matched)
        {
            // Build enum value directly from result offset
            return Keyword(i);
        }
    }
    return KEYWORD_INVALID;
}

void Parser::BeginBlock(Keyword keyword)
{
    m_current_block = keyword;
    if (keyword != KEYWORD_INVALID)
    {
        Line line(keyword, -1);
        line.begins_block = true;
        m_document->lines.push_back(line);
    }
}

void Parser::EndBlock(Keyword keyword)
{
    m_current_block = KEYWORD_INVALID;
    if (keyword == KEYWORD_END_COMMENT ||
        keyword == KEYWORD_END_DESCRIPTION)
    {
        Line line(keyword, -1);
        line.begins_block = true;
        m_document->lines.push_back(line);
    }
}

std::string Parser::GetArgStr(int index)
{
    return std::string(m_args[index].start, m_args[index].length);
}

char Parser::GetArgChar(int index)
{
    return *(m_args[index].start);
}

WheelSide Parser::GetArgWheelSide(int index)
{
    char side_char = this->GetArgChar(index);
    if (side_char != 'r')
    {
        if (side_char != 'l')
        {
            char msg[200] = "";
            snprintf(msg, 200, "Bad arg~%d 'side' (value: %c), parsing as 'l' for backwards compatibility.", index + 1, side_char);
            this->AddMessage(Message::TYPE_WARNING, msg);
        }
        return WheelSide::SIDE_LEFT;
    }
    return WheelSide::SIDE_RIGHT;
}

long Parser::GetArgLong(int index)
{
    errno = 0;
    char* out_end = nullptr;
    const int MSG_LEN = 200;
    char msg[MSG_LEN];
    long res = std::strtol(m_args[index].start, &out_end, 10);
    if (errno != 0)
    {
        snprintf(msg, MSG_LEN, "Cannot parse argument [%d] as integer, errno: %d", index + 1, errno);
        this->AddMessage(Message::TYPE_ERROR, msg);
        return 0; // Compatibility
    }
    if (out_end == m_args[index].start)
    {
        snprintf(msg, MSG_LEN, "Argument [%d] is not valid integer", index + 1);
        this->AddMessage(Message::TYPE_ERROR, msg);
        return 0; // Compatibility
    }
    else if (out_end != (m_args[index].start + m_args[index].length))
    {
        snprintf(msg, MSG_LEN, "Integer argument [%d] has invalid trailing characters", index + 1);
        this->AddMessage(Message::TYPE_WARNING, msg);
    }
    return res;
}

int Parser::GetArgInt(int index)
{
    return static_cast<int>(this->GetArgLong(index));
}

NodeRef_t Parser::GetArgRigidityNode(int index)
{
    std::string rigidity_node = this->GetArgStr(index);
    if (rigidity_node != "9999") // Special null value
    {
        return this->GetArgNodeRef(index);
    }
    return NodeRef_t(); // Defaults to invalid ref
}

WheelPropulsion Parser::GetArgPropulsion(int index)
{
    int propulsion = this->GetArgInt(index);
    if (propulsion < 0 || propulsion > 2)
    {
        char msg[100] = "";
        snprintf(msg, 100, "Bad value of param ~%d (propulsion), using 0 (no propulsion)", index + 1);
        this->AddMessage(Message::TYPE_ERROR, msg);
        return WheelPropulsion::PROPULSION_NONE;
    }
    return WheelPropulsion(propulsion);
}

WheelBraking Parser::GetArgBraking(int index)
{
    int braking = this->GetArgInt(index);
    if (braking < 0 || braking > 4)
    {
        char msg[100] = "";
        snprintf(msg, 100, "Bad value of param ~%d (braking), using 0 (no braking)", index + 1);
        return WheelBraking::BRAKING_NO;
    }
    return WheelBraking(braking);
}

NodeRef_t Parser::GetArgNodeRef(int index)
{
    return NodeRef_t(this->GetArgStr(index));
}

NodeRef_t Parser::GetArgNullableNode(int index)
{
    if (! (Ogre::StringConverter::parseReal(this->GetArgStr(index)) == -1.f))
    {
        return this->GetArgNodeRef(index);
    }
    return NodeRef_t(); // Defaults to empty ref.
}

unsigned Parser::GetArgUint(int index)
{
    return static_cast<unsigned>(this->GetArgLong(index));
}

FlareType Parser::GetArgFlareType(int index)
{
    char in = this->GetArgChar(index);
    switch (in)
    {
        case (char)FlareType::HEADLIGHT:
        case (char)FlareType::BRAKE_LIGHT:
        case (char)FlareType::BLINKER_LEFT:
        case (char)FlareType::BLINKER_RIGHT:
        case (char)FlareType::REVERSE_LIGHT:
        case (char)FlareType::USER:
        case (char)FlareType::DASHBOARD:
            return FlareType(in);

        default:
            this->AddMessage(Message::TYPE_WARNING,
                fmt::format("Invalid flare type '{}', falling back to type 'f' (front light)...", in));
            return FlareType::HEADLIGHT;
    }
}

float Parser::GetArgFloat(int index)
{
    return (float) Ogre::StringConverter::parseReal(this->GetArgStr(index), 0.f);
}

float Parser::ParseArgFloat(const char* str)
{
    return (float) Ogre::StringConverter::parseReal(str, 0.f);
}

float Parser::ParseArgFloat(std::string const & str)
{
    return this->ParseArgFloat(str.c_str());
}

unsigned Parser::ParseArgUint(const char* str)
{
    errno = 0;
    long res = std::strtol(str, nullptr, 10);
    if (errno != 0)
    {
        char msg[200];
        snprintf(msg, 200, "Cannot parse argument '%s' as int, errno: %d", str, errno);
        this->AddMessage(Message::TYPE_ERROR, msg);
        return 0.f; // Compatibility
    }
    return static_cast<unsigned>(res);
}

unsigned Parser::ParseArgUint(std::string const & str)
{
    return this->ParseArgUint(str.c_str());
}

int Parser::ParseArgInt(const char* str)
{
    return static_cast<int>(this->ParseArgUint(str));
}

bool Parser::GetArgBool(int index)
{
    return Ogre::StringConverter::parseBool(this->GetArgStr(index));
}

WingControlSurface Parser::GetArgWingSurface(int index)
{
    std::string str = this->GetArgStr(index);
    size_t bad_pos = str.find_first_not_of(WingsLine::CONTROL_LEGAL_FLAGS);
    const int MSG_LEN = 300;
    char msg_buf[MSG_LEN] = "";
    if (bad_pos == 0)
    {
        snprintf(msg_buf, MSG_LEN, "Invalid argument ~%d 'control surface' (value: %s), allowed are: <%s>, ignoring...",
            index + 1, str.c_str(), WingsLine::CONTROL_LEGAL_FLAGS.c_str());
        this->AddMessage(Message::TYPE_ERROR, msg_buf);
        return WingControlSurface::n_NONE;
    }
    if (str.size() > 1)
    {
        snprintf(msg_buf, MSG_LEN, "Argument ~%d 'control surface' (value: %s), should be only 1 letter.", index, str.c_str());
        this->AddMessage(Message::TYPE_WARNING, msg_buf);
    }
    return WingControlSurface(str.at(0));
}

std::string Parser::GetArgManagedTex(int index)
{
    std::string tex_name = this->GetArgStr(index);
    return (tex_name.at(0) != '-') ? tex_name : "";
}

int Parser::TokenizeCurrentLine()
{
    int cur_arg = 0;
    const char* cur_char = m_current_line;
    int arg_len = 0;
    while ((*cur_char != '\0') && (cur_arg < Parser::LINE_MAX_ARGS))
    {
        const bool is_arg = !IsSeparator(*cur_char);
        if ((arg_len == 0) && is_arg)
        {
            m_args[cur_arg].start = cur_char;
            arg_len = 1;
        }
        else if ((arg_len > 0) && !is_arg)
        {
            m_args[cur_arg].length = arg_len;
            arg_len = 0;
            ++cur_arg;
        }
        else if (is_arg)
        {
            ++arg_len;
        }
        ++cur_char;
    }
    if (arg_len > 0)
    {
        m_args[cur_arg].length = arg_len;
        ++cur_arg;
    }

    m_num_args = cur_arg;
    return cur_arg;
}

void Parser::ProcessOgreStream(Ogre::DataStream* stream, Ogre::String resource_group)
{
    m_resource_group = resource_group;
    m_filename = stream->getName();

    char raw_line_buf[LINE_BUFFER_LENGTH];
    while (!stream->eof())
    {
        try
        {
            stream->readLine(raw_line_buf, LINE_BUFFER_LENGTH);
        }
        catch (Ogre::Exception &ex)
        {
            std::string msg = "Error reading truckfile! Message:\n";
            msg += ex.getFullDescription();
            this->AddMessage(Message::TYPE_FATAL_ERROR, msg.c_str());
            break;
        }

        this->ProcessRawLine(raw_line_buf);
    }
}

void Parser::ProcessRawLine(const char* raw_line_buf)
{
    const char* raw_start = raw_line_buf;
    const char* raw_end = raw_line_buf + strnlen(raw_line_buf, LINE_BUFFER_LENGTH);

    // Trim leading whitespace
    while (IsWhitespace(*raw_start) && (raw_start != raw_end))
    {
        ++raw_start;
    }

    // Skip empty/comment lines
    if ((raw_start == raw_end) || (*raw_start == ';') || (*raw_start == '/'))
    {
        ++m_current_line_number;
        return;
    }

    // Sanitize UTF-8
    memset(m_current_line, 0, LINE_BUFFER_LENGTH);
    char* out_start = m_current_line;
    utf8::replace_invalid(raw_start, raw_end, out_start, '?');

    // Process
    this->ProcessCurrentLine();
    ++m_current_line_number;
}

} // namespace RigDef
