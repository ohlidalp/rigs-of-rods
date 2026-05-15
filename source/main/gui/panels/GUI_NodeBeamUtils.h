/*
    This source file is part of Rigs of Rods
    Copyright 2016-2020 Petr Ohlidal

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

#include "Application.h"
#include "OgreImGui.h"

#include <vector>
#include <stdint.h>

namespace RoR {
namespace GUI {

struct CachePaint // Visualization of `CalcBeams()` and it's node cache usage.
{
    // Config:
    int nodecache_cap = 10;
    float anim_speed = 0.05f; // seconds per beam
    int beams_start = 0;
    int beams_end = 0;
    // State:
    std::vector<NodeNum_t> nodecache; // FIFO cache of nodes; maximum size is `nodecache_cap`.
    std::vector<uint8_t> nodecache_hits; // 0 = initial miss, then number of hits until eviction; size is same as `nodecache`.
    int total_multihits = 0;
    int total_hits = 0;
    int total_misses = 0;
    int anim_curbeam = -1; // -1 = no animation, otherwise <beams_start - beams_end] progress of the animation
    bool anim_running = false;
    float anim_total_time = 0.f; // accumulated time since animation start

    void StartAnim(const int start, const int end)
    {
        anim_curbeam = start;
        beams_start = start;
        beams_end = end;
        anim_running = true;
        total_hits = 0;
        total_misses = 0;
        total_multihits = 0;
        nodecache.clear();
        nodecache_hits.clear();
        anim_total_time = 0.f;
    }

    void TouchNode(const NodeNum_t n)
    {
        auto it = std::find(nodecache.begin(), nodecache.end(), n);
        if (it != nodecache.end())
        {
            // Node is already in the cache, increment hit count
            size_t index = std::distance(nodecache.begin(), it);
            if (nodecache_hits[index] > 1)
            {
                total_multihits++;
            }
            nodecache_hits[index]++;
            total_hits++;
        }
        else
        {
            // Node is not in the cache, add it
            if (nodecache.size() >= nodecache_cap)
            {
                // Evict the oldest node
                nodecache.erase(nodecache.begin());
                nodecache_hits.erase(nodecache_hits.begin());
            }
            nodecache.push_back(n);
            nodecache_hits.push_back(0);
            total_misses++;
        }
    }

    void UpdateAnim(const ActorPtr& actor,float dt);
};

class NodeBeamUtils
{
public:
    void Draw(float dt);

    void SetVisible(bool visible);
    bool IsVisible() const { return m_is_visible; }
    bool IsHovered() const { return IsVisible() && m_is_hovered; }

private:
    bool m_is_visible = false;
    bool m_is_hovered = false;
    bool m_is_searching = false;
    CachePaint m_cache_paint;

    const ImVec4 GRAY_HINT_TEXT = ImVec4(0.62f, 0.62f, 0.61f, 1.f);

    void DrawCreateProjectBanner(ActorPtr actor, bool& window_open);
    void DrawMenubar(ActorPtr actor);
    void DrawMassTab(ActorPtr actor);
    void DrawSpringDampTab(ActorPtr actor);
    void DrawMemoryTab(ActorPtr actor);
};

} // namespace GUI
} // namespace RoR
