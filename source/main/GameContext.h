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
/// @brief  Game state manager and message-queue provider

#pragma once

#include <list>
#include <mutex>
#include <queue>
#include <string>

namespace RoR {

/// All gameplay events are specified here (work in progress)
enum MsgType
{
    MSG_INVALID,
    // Application
    MSG_APP_SHUTDOWN_REQUESTED,
    MSG_APP_SCREENSHOT_REQUESTED,
    MSG_APP_DISPLAY_FULLSCREEN_REQUESTED,
    MSG_APP_DISPLAY_WINDOWED_REQUESTED,
    // Networking
    MSG_NET_CONNECT_REQUESTED,
    MSG_NET_CONNECT_STARTED,
    MSG_NET_CONNECT_PROGRESS,
    MSG_NET_CONNECT_SUCCESS,
    MSG_NET_CONNECT_FAILURE,
    MSG_NET_SERVER_KICK,
    MSG_NET_DISCONNECT_REQUESTED,
    MSG_NET_USER_DISCONNECT,
    MSG_NET_RECV_ERROR,
    MSG_NET_REFRESH_SERVERLIST_SUCCESS,
    MSG_NET_REFRESH_SERVERLIST_FAILURE,
    // Simulation
    MSG_SIM_PAUSE_REQUESTED,
    MSG_SIM_UNPAUSE_REQUESTED,
    MSG_SIM_LOAD_TERRN_REQUESTED,
    MSG_SIM_LOAD_SAVEGAME_REQUESTED,
    MSG_SIM_UNLOAD_TERRN_REQUESTED,
};

/// Unified game event system - all requests and state changes are reported using a message (work in progress)
struct Message
{
    Message(MsgType _type): type(_type) {}
    Message(MsgType _type, std::string const& _desc): type(_type), description(_desc) {}
    Message(MsgType _type, void* _data): type(_type), payload(_data) {}

    MsgType     type         = MSG_INVALID;
    std::string description;
    void*       payload      = nullptr;
};

typedef std::queue < Message, std::list<Message>> GameMsgQueue;

class GameContext
{
public:
    GameContext();

    // Message queue
    void                PushMessage(Message m);
    bool                HasMessages();
    Message             PopMessage();

private:
    // Message queue
    GameMsgQueue        m_msg_queue;
    std::mutex          m_msg_mutex;
};

} // namespace RoR