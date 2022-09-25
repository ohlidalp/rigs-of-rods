/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2017-2022 Petr Ohlidal

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

#pragma once

#include "CharacterFileFormat.h"
#include "ForwardDeclarations.h"
#include "SimBuffers.h"

#include <OgreUTFString.h>
#include <OgreMeshManager.h>
#include <OgreTimer.h>
#include <string>

namespace RoR {

/// @addtogroup Gameplay
/// @{

/// @addtogroup Character
/// @{
/// 

/// Character uses simplified physics and occupies single point in space.
/// Note on animations: 
///     This object decides what animations are played and how fast, but doesn't apply it to visual scene.
///     Visual 3D model and animations are loaded and updated by `RoR::GfxCharacter` using data from sim buffers (see file 'SimBuffers.h')
class Character
{
    friend struct GfxCharacter; // visual counterpart.

public:

    // Action flags
    static const BitMask_t ACTION_MOVE_FORWARD   = BITMASK(1);
    static const BitMask_t ACTION_MOVE_BACKWARD  = BITMASK(2);
    static const BitMask_t ACTION_TURN_RIGHT     = BITMASK(3);
    static const BitMask_t ACTION_TURN_LEFT      = BITMASK(4);
    static const BitMask_t ACTION_SIDESTEP_RIGHT = BITMASK(5);
    static const BitMask_t ACTION_SIDESTEP_LEFT  = BITMASK(6);
    static const BitMask_t ACTION_RUN            = BITMASK(7);
    static const BitMask_t ACTION_JUMP           = BITMASK(8);
    static const BitMask_t ACTION_SLOW_TURN      = BITMASK(9);
    static const BitMask_t ACTION_CUSTOM_01      = BITMASK(10);
    static const BitMask_t ACTION_CUSTOM_02      = BITMASK(11);
    static const BitMask_t ACTION_CUSTOM_03      = BITMASK(12);
    static const BitMask_t ACTION_CUSTOM_04      = BITMASK(13);
    static const BitMask_t ACTION_CUSTOM_05      = BITMASK(14);
    static const BitMask_t ACTION_CUSTOM_06      = BITMASK(15);
    static const BitMask_t ACTION_CUSTOM_07      = BITMASK(16);
    static const BitMask_t ACTION_CUSTOM_08      = BITMASK(17);
    static const BitMask_t ACTION_CUSTOM_09      = BITMASK(18);
    static const BitMask_t ACTION_CUSTOM_10      = BITMASK(19);

    static const char* ActionFlagToString(BitMask_t single_action_flag);
    static BitMask_t ActionFlagFromString(std::string const& single_action_flag_str);

    // Situation flags
    static const BitMask_t SITUATION_ON_SOLID_GROUND  = BITMASK(1);
    static const BitMask_t SITUATION_IN_SHALLOW_WATER = BITMASK(2);
    static const BitMask_t SITUATION_IN_DEEP_WATER    = BITMASK(3);
    static const BitMask_t SITUATION_IN_AIR           = BITMASK(4);
    static const BitMask_t SITUATION_DRIVING          = BITMASK(5);
    static const BitMask_t SITUATION_CUSTOM_MODE_01   = BITMASK(10);
    static const BitMask_t SITUATION_CUSTOM_MODE_02   = BITMASK(11);
    static const BitMask_t SITUATION_CUSTOM_MODE_03   = BITMASK(12);
    static const BitMask_t SITUATION_CUSTOM_MODE_04   = BITMASK(13);
    static const BitMask_t SITUATION_CUSTOM_MODE_05   = BITMASK(14);
    static const BitMask_t SITUATION_CUSTOM_MODE_06   = BITMASK(15);
    static const BitMask_t SITUATION_CUSTOM_MODE_07   = BITMASK(16);
    static const BitMask_t SITUATION_CUSTOM_MODE_08   = BITMASK(17);
    static const BitMask_t SITUATION_CUSTOM_MODE_09   = BITMASK(18);
    static const BitMask_t SITUATION_CUSTOM_MODE_10   = BITMASK(19);

    static const char* SituationFlagToString(BitMask_t single_situation_flag);
    static BitMask_t SituationFlagFromString(std::string const& single_situation_flag_str);

    Character(CharacterDocumentPtr def, int source = -1, unsigned int streamid = 0, Ogre::UTFString playerName = "", int color_number = 0, bool is_remote = true);
    ~Character();

    // get info
    CharacterDocumentPtr getCharacterDocument() { return m_character_def; }
       
    // get state
    Ogre::Vector3  getPosition();
    Ogre::Radian   getRotation() const                  { return m_character_rotation; }
    ActorPtr       GetActorCoupling();
    
    void           setPosition(Ogre::Vector3 position);
    void           setRotation(Ogre::Radian rotation);
    void           move(Ogre::Vector3 offset);
    void           updateLocal(float dt);
    void           updateCharacterRotation();
    void           SetActorCoupling(bool enabled, ActorPtr actor);

    // network
    void           receiveStreamData(unsigned int& type, int& source, unsigned int& streamid, char* buffer);
    void           SendStreamData();
    int            getSourceID() const                  { return m_source_id; }
    bool           isRemote() const                     { return m_is_remote; }
    int            GetColorNum() const                  { return m_color_number; }
    void           setColour(int color)                 { this->m_color_number = color; }
    Ogre::UTFString const& GetNetUsername()             { return m_net_username; }

private:

    void           ReportError(const char* detail);
    void           SendStreamSetup();

    // attributes
    CharacterDocumentPtr  m_character_def;
    std::string      m_instance_name;

    // transforms
    Ogre::Vector3    m_character_position;
    Ogre::Vector3    m_prev_position;
    Ogre::Radian     m_character_rotation;
    float            m_character_h_speed;
    float            m_character_v_speed;

    // state
    BitMask_t        m_action_flags = 0;
    BitMask_t        m_situation_flags = 0;
    ActorPtr         m_actor_coupling; //!< The vehicle or machine which the character occupies

    // network
    bool             m_is_remote;
    int              m_color_number;
    Ogre::UTFString  m_net_username;
    Ogre::Timer      m_net_timer;
    unsigned long    m_net_last_update_time;
    int              m_stream_id;
    int              m_source_id;
};

/// @} // addtogroup Character
/// @} // addtogroup Gameplay

} // namespace RoR

