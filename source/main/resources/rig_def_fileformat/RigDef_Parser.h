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
/// @brief Checks the rig-def file syntax and loads data to memory

#pragma once

#include "RigDef_File.h"

#include <memory>
#include <string>
#include <regex>

namespace RigDef
{

class Parser
{

public:

    static const int LINE_BUFFER_LENGTH = 2000;
    static const int LINE_MAX_ARGS = 100;

    struct Message // TODO: remove, use console API directly
    {
        enum Type
        {
            TYPE_WARNING,
            TYPE_ERROR,
            TYPE_FATAL_ERROR,

            TYPE_INVALID = 0xFFFFFFFF
        };
    };

    struct Token
    {
        const char* start;
        int         length;
    };

    void ProcessOgreStream(Ogre::DataStream* stream, Ogre::String resource_group);
    void ProcessRawLine(const char* line);

    std::shared_ptr<RigDef::File> GetFile()
    {
        return m_document;
    }

private:

// --------------------------------------------------------------------------
//  Directive parsers
// --------------------------------------------------------------------------

    void ParseDirectiveAddAnimation();
    void ParseDirectiveBackmesh();
    void ParseDirectiveDetacherGroup();
    void ParseDirectiveFlexbodyCameraMode();
    void ParseDirectivePropCameraMode();
    void ParseDirectiveSection();
    void ParseDirectiveSectionConfig();
    void ParseDirectiveSetBeamDefaults();
    void ParseDirectiveSetBeamDefaultsScale();
    void ParseDirectiveSetDefaultMinimass();
    void ParseDirectiveSetInertiaDefaults();
    void ParseDirectiveSetManagedMaterialsOptions();
    void ParseDirectiveSetNodeDefaults();

// --------------------------------------------------------------------------
//  Section parsers
// --------------------------------------------------------------------------

    void ParseAirbrakes();
    void ParseAnimators();
    void ParseAntiLockBrakes();
    void ParseAuthor();
    void ParseAxles();
    void ParseBeams();
    void ParseBrakes();
    void ParseCab();
    void ParseCameras();
    void ParseCamerarails();
    void ParseCinecam();
    void ParseCollisionboxes();
    void ParseCommands();
    void ParseCommands2();
    void ParseContacters();
    void ParseCruiseControl();
    void ParseDescription();
    void ParseEngine();
    void ParseEngoption();
    void ParseEngturbo();
    void ParseExhausts();
    void ParseExtCamera();
    void ParseFileFormatVersion();
    void ParseFileinfo();
    void ParseFixes();
    void ParseFlares();
    void ParseFlares2();
    void ParseFlexbodies();
    void ParseFlexbodywheels();
    void ParseForset();
    void ParseFusedrag();
    void ParseGlobals();
    void ParseGuid();
    void ParseGuiSettings();
    void ParseHelp();
    void ParseHook();
    void ParseHydros();
    void ParseInterAxles();
    void ParseLockgroups();
    void ParseManagedMaterials();
    void ParseMaterialFlareBindings();
    void ParseMeshwheels();
    void ParseMeshwheels2();
    void ParseMinimass();
    void ParseNodes();
    void ParseNodes2();
    void ParseNodeCollision();
    void ParseParticles();
    void ParsePistonprops();
    void ParseProps();
    void ParseRailGroups();
    void ParseRopables();
    void ParseRopes();
    void ParseRotators();
    void ParseRotators2();
    void ParseScrewprops();
    void ParseSetCollisionRange();
    void ParseSetSkeletonSettings();
    void ParseShock();
    void ParseShock2();
    void ParseShock3();
    void ParseSlidenodes();
    void ParseSlopeBrake();
    void ParseSoundsources();
    void ParseSoundsources2();
    void ParseSpeedLimiter();
    void ParseSubmeshGroundModel();
    void ParseTexcoords();
    void ParseTies();
    void ParseTorqueCurve();
    void ParseTractionControl();
    void ParseTransferCase();
    void ParseTriggers();
    void ParseTurbojets();
    void ParseTurboprops();
    void ParseTurboprops2();
    void ParseVideoCamera();
    void ParseWheelDetachers();
    void ParseWheel();
    void ParseWheel2();
    void ParseWing();

// --------------------------------------------------------------------------
//  Utilities
// --------------------------------------------------------------------------

    void             ProcessCurrentLine();
    int              TokenizeCurrentLine();
    bool             CheckNumArguments(int num_required_args);
    void             BeginBlock(RigDef::Keyword keyword);
    void             EndBlock(RigDef::Keyword keyword);

    std::string        GetArgStr          (int index);
    int                GetArgInt          (int index);
    unsigned           GetArgUint         (int index);
    long               GetArgLong         (int index);
    float              GetArgFloat        (int index);
    char               GetArgChar         (int index);
    bool               GetArgBool         (int index);
    WheelPropulsion    GetArgPropulsion   (int index);
    WheelBraking       GetArgBraking      (int index);
    NodeRef_t          GetArgNodeRef      (int index);
    NodeRef_t          GetArgRigidityNode (int index);
    NodeRef_t          GetArgNullableNode (int index);
    WheelSide          GetArgWheelSide    (int index);
    WingControlSurface GetArgWingSurface  (int index);
    RoR::FlareType     GetArgFlareType    (int index);
    std::string        GetArgManagedTex   (int index);

    float              ParseArgFloat      (const char* str);
    int                ParseArgInt        (const char* str);
    unsigned           ParseArgUint       (const char* str);

    unsigned           ParseArgUint       (const std::string& s);
    float              ParseArgFloat      (const std::string& s);

    void _CheckInvalidTrailingText(Ogre::String const & line, std::smatch const & results, unsigned int index);

    /// Keyword scan function. 
    Keyword IdentifyKeywordInCurrentLine();

    /// Keyword scan utility function. 
    Keyword FindKeywordMatch(std::smatch& search_results);

    /// Adds a message to console
    void AddMessage(std::string const & line, Message::Type type, std::string const & message);
    void AddMessage(Message::Type type, const char* msg)
    {
        this->AddMessage(m_current_line, type, msg);
    }
    void AddMessage(Message::Type type, std::string const & msg)
    {
        this->AddMessage(m_current_line, type, msg);
    }

    static void _TrimTrailingComments(std::string const & line_in, std::string & line_out);

    void _ParseCameraSettings(CameraModeCommon & camera_settings, Ogre::String input_str);

    void ParseOptionalInertia(InertiaCommon& inertia, int index);

    void ParseCommandOptions(CommandsCommon& command, std::string const& options);

// --------------------------------------------------------------------------

    // Parser state
    unsigned int                         m_current_line_number = 1;
    char                                 m_current_line[LINE_BUFFER_LENGTH] = {};
    Token                                m_args[LINE_MAX_ARGS] = {};    //!< Tokens of current line.
    int                                  m_num_args = 0;               //!< Number of tokens on current line.
    Keyword                              m_current_block = KEYWORD_INVALID;

    Ogre::String                         m_filename; // Logging
    Ogre::String                         m_resource_group;

    std::shared_ptr<RigDef::File>        m_document = std::shared_ptr<File>(new File());
};

} // namespace RigDef
