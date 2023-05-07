/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2023 Petr Ohlidal

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

#pragma once

#include "SimData.h"

#include <imgui.h>

#include <vector>

namespace RoR {
namespace GUI {

class RigEditor
{
public:
    const float HELP_TEXTURE_WIDTH = 512.f;
    const float HELP_TEXTURE_HEIGHT = 128.f;

    void SetVisible(bool vis) { m_is_visible = vis; }
    bool IsVisible() const { return m_is_visible; }
    bool IsHovered() const { return IsVisible() && m_is_hovered; }

    void DrawSidePanel();
    void DrawSelectedNodeHighlights();
    void UpdateInputEvents(float dt);

private:
    bool m_is_visible = false;
    bool m_is_hovered = false;

    void DrawNodesTable(ActorPtr& actor, CacheEntry* cache_entry);

    std::vector<bool> m_node_selected;
    std::vector<bool> m_node_highlight_drawn;
    int m_num_nodes_selected = 0;
    NodeNum_t m_hovered_node = NODENUM_INVALID;
};

} // namespace GUI
} // namespace RoR
