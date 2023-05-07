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
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

/// @file   SceneMouse.h
/// @author Thomas Fischer <thomas@thomasfischer.biz>
/// @date   11th of March 2011

#include "SceneMouse.h"

#include "Actor.h"
#include "Application.h"
#include "GameContext.h"
#include "GfxScene.h"
#include "GUIUtils.h"
#include "InputEngine.h"
#include "ScriptEngine.h"

#include <Ogre.h>

using namespace Ogre;
using namespace RoR;

#define MOUSE_GRAB_FORCE 30000.0f

SceneMouse::SceneMouse() :
    mouseGrabState(0),
    grab_truck(nullptr)
{
    this->reset();
}

void SceneMouse::InitializeVisuals()
{

}

void SceneMouse::DiscardVisuals()
{

}

void SceneMouse::releaseMousePick()
{
    if (App::sim_state->getEnum<SimState>() == SimState::PAUSED) { return; } // Do nothing when paused

    // remove forces
    if (grab_truck)
        grab_truck->clearNodeEffectForceTowardsPoint(minnode);

    this->reset();
}

void SceneMouse::reset()
{
    minnode = NODENUM_INVALID;
    grab_truck = nullptr;
    mindist = std::numeric_limits<float>::max();
    mouseGrabState = 0;
    lastgrabpos = Vector3::ZERO;
}

bool SceneMouse::mouseMoved(const OIS::MouseEvent& _arg)
{
    const OIS::MouseState ms = _arg.state;

    // experimental mouse hack
    if (ms.buttonDown(OIS::MB_Left) && mouseGrabState == 0)
    {

        // mouse selection is updated every frame in `update()`
        // check if we hit a node
        if (mintruck && minnode != NODENUM_INVALID)
        {
            grab_truck = mintruck;
            mouseGrabState = 1;            

            for (std::vector<hook_t>::iterator it = grab_truck->ar_hooks.begin(); it != grab_truck->ar_hooks.end(); it++)
            {
                if (it->hk_hook_node == minnode)
                {
                    //grab_truck->hookToggle(it->hk_group, MOUSE_HOOK_TOGGLE, minnode);
                    ActorLinkingRequest* rq = new ActorLinkingRequest();
                    rq->alr_type = ActorLinkingRequestType::HOOK_ACTION;
                    rq->alr_actor_instance_id = grab_truck->ar_instance_id;
                    rq->alr_hook_action = MOUSE_HOOK_TOGGLE;
                    rq->alr_hook_group = it->hk_group;
                    rq->alr_hook_mousenode = minnode;
                    App::GetGameContext()->PushMessage(Message(MSG_SIM_ACTOR_LINKING_REQUESTED, rq));
                }
            }
        }
    }
    else if (ms.buttonDown(OIS::MB_Left) && mouseGrabState == 1)
    {
        // force applying and so forth happens in update()

        // not fixed
        return false;
    }
    else if (!ms.buttonDown(OIS::MB_Left) && mouseGrabState == 1)
    {
        releaseMousePick();
        // not fixed
        return false;
    }

    return false;
}

Ogre::AxisAlignedBox InflateAABB(Ogre::AxisAlignedBox box, float extra)
{
    return Ogre::AxisAlignedBox(box.getMinimum() - extra, box.getMaximum() + extra);
}

void SceneMouse::updateMouseNodeHighlights(ActorPtr& actor)
{
    ROR_ASSERT(actor != nullptr);
    ROR_ASSERT(actor->ar_state == ActorState::LOCAL_SIMULATED);

    Ray mouseRay = getMouseRay();

    // check if our ray intersects with the bounding box of the truck
    AxisAlignedBox inflated_aabb = InflateAABB(actor->ar_bounding_box, App::GetGuiManager()->GetTheme().mouse_node_highlight_aabb_padding);
    std::pair<bool, Real> pair = mouseRay.intersects(inflated_aabb);
    if (!pair.first)
    {
        return;
    }

    for (int j = 0; j < actor->ar_num_nodes; j++)
    {
        // skip nodes with grabbing disabled
        if (actor->ar_nodes[j].nd_no_mouse_grab)
        {
            continue;
        }

        // check if our ray intersects with the node
        std::pair<bool, Real> pair = mouseRay.intersects(Sphere(actor->ar_nodes[j].AbsPosition, GRAB_SPHERE_SIZE));
        if (pair.first)
        {

            // we hit it, check if its the nearest node
            if (pair.second < mindist)
            {
                mindist = pair.second;
                minnode = static_cast<NodeNum_t>(j);
                mintruck = actor;
            }
        }

        // check if the node is close enough to be highlighted
        std::pair<bool, Real> highlight_result = mouseRay.intersects(Sphere(actor->ar_nodes[j].AbsPosition, this->HIGHLIGHT_SPHERE_SIZE));
        if (highlight_result.first)
        {
            highlightedTruck = actor;

            highlightedNodes.push_back({ highlight_result.second, static_cast<NodeNum_t>(j) });
            highlightedNodesTopDistance = std::max(highlightedNodesTopDistance, highlight_result.second);
        }
    }
}

void SceneMouse::updateMouseEffectHighlights(ActorPtr& actor)
{
    Ray mouseRay = getMouseRay();

    for (size_t i = 0; i < actor->ar_node_effects_constant_force.size(); i++)
    {
        const NodeEffectConstantForce& e = actor->ar_node_effects_constant_force[i];
        Vector3 pointWorldPos = actor->ar_nodes[e.nodenum].AbsPosition + (e.force * FORCE_NEWTONS_TO_LINE_LENGTH_RATIO);
        std::pair<bool, Real> result = mouseRay.intersects(Sphere(pointWorldPos, this->FORCE_UNPIN_SPHERE_SIZE));
        if (result.first && result.second < cfEffect_mindist)
        {
            cfEffect_minnode = e.nodenum;
            cfEffect_mindist = result.second;
            cfEffect_mintruck = actor;
        }
    }

    for (size_t i = 0; i < actor->ar_node_effects_force_towards_point.size(); i++)
    {
        const NodeEffectForceTowardsPoint& e = actor->ar_node_effects_force_towards_point[i];
        std::pair<bool, Real> result = mouseRay.intersects(Sphere(e.point, this->FORCE_UNPIN_SPHERE_SIZE));
        if (result.first && result.second < f2pEffect_mindist)
        {
            f2pEffect_minnode = e.nodenum;
            f2pEffect_mindist = result.second;
            f2pEffect_mintruck = actor;
        }
    }
}

void SceneMouse::updateMouseBeamHighlights()
{
    highlightedBeamIDs.clear();

    if (mintruck)
    {
        highlightedBeamsNodeProximity.clear();
        highlightedBeamsNodeProximity.assign(mintruck->ar_num_nodes, 0);

        // Fire up the recursive update
        const GUIManager::GuiTheme& theme = App::GetGuiManager()->GetTheme();
        highlightedBeamsNodeProximity[minnode] = 255;
        this->updateMouseBeamHighlightsRecursive(
            minnode, 0.f, theme.mouse_beam_traversal_length);
    }
}

void SceneMouse::updateMouseBeamHighlightsRecursive(NodeNum_t nodenum, float traversalLen, float maxTraversalLen)
{
    ROR_ASSERT(mintruck);

    // Traverse connected beams and for each node visited, record it's proximity
    for (int beamID: mintruck->ar_node_to_beam_connections[nodenum])
    {
        // Always record beams, even if their far node is out of reach.
        highlightedBeamIDs.push_back(beamID);

        // Check if node is within reach
        const beam_t& b = mintruck->ar_beams[beamID];
        NodeNum_t nodenumFar = (b.p1->pos == nodenum) ? b.p2->pos : b.p1->pos;
        float traversalLenFar = traversalLen + b.L;

        if (traversalLenFar < maxTraversalLen)
        {
            // Update node proximity - always record higher result
            uint8_t proximity = static_cast<uint8_t>(255.f * (traversalLenFar - maxTraversalLen)/maxTraversalLen); // the further the smaller
            if (proximity > highlightedBeamsNodeProximity[nodenumFar])
            {
                highlightedBeamsNodeProximity[nodenumFar] = proximity;
                this->updateMouseBeamHighlightsRecursive(nodenumFar, traversalLenFar, maxTraversalLen);
            }
        }
    }
}

void SceneMouse::UpdateInputEvents()
{
    if (App::GetInputEngine()->getEventBoolValueBounce(EV_ROAD_EDITOR_POINT_INSERT))
    {
        if (mouseGrabState == 1 && grab_truck)
        {
            // Reset mouse grab but do not remove node forces
            this->reset();
        }
    }
}

void SceneMouse::UpdateSimulation()
{
    Ray mouseRay = getMouseRay();
    highlightedNodes.clear(); // clear every frame - highlights are not displayed when grabbing
    highlightedNodesTopDistance = 0.f;
    highlightedTruck = nullptr;

    if (mouseGrabState == 1 && grab_truck)
    {
        // get values
        lastgrabpos = mouseRay.getPoint(mindist);

        // add forces
        grab_truck->clearNodeEffectForceTowardsPoint(minnode);
        grab_truck->addNodeEffectForceTowardsPoint(minnode, lastgrabpos, MOUSE_GRAB_FORCE);
    }
    else if (!App::GetGuiManager()->IsGuiCaptureKeyboardRequested()
        && !ImGui::GetIO().WantCaptureKeyboard
        && !ImGui::GetIO().WantCaptureMouse
        && App::GetGuiManager()->AreStaticMenusAllowed())
    {
        // refresh mouse highlight of nodes
        mintruck = nullptr;
        minnode = NODENUM_INVALID;
        mindist = std::numeric_limits<float>::max();
        for (ActorPtr& actor : App::GetGameContext()->GetActorManager()->GetActors())
        {
            if (actor->ar_state == ActorState::LOCAL_SIMULATED)
            {
                this->updateMouseNodeHighlights(actor);
            }
        }
        this->updateMouseBeamHighlights();

        // refresh mouse highlight of effects
        cfEffect_mintruck = nullptr;
        cfEffect_minnode = NODENUM_INVALID;
        cfEffect_mindist = std::numeric_limits<float>::max();
        f2pEffect_mintruck = nullptr;
        f2pEffect_minnode = NODENUM_INVALID;
        f2pEffect_mindist = std::numeric_limits<float>::max();
        for (ActorPtr& actor : App::GetGameContext()->GetActorManager()->GetActors())
        {
            if (actor->ar_state == ActorState::LOCAL_SIMULATED)
            {
                this->updateMouseEffectHighlights(actor);
            }
        }
    }
}

void SceneMouse::drawMouseBeamHighlights()
{
    if (!mintruck)
        return; // nothing to draw

    ImDrawList* drawlist = GetImDummyFullscreenWindow("Mouse-grab beam highlights");

    const GUIManager::GuiTheme& theme = App::GetGuiManager()->GetTheme();

    for (int beamID: highlightedBeamIDs)
    {
        const beam_t& beam = mintruck->ar_beams[beamID];

        Vector2 screenPos1, screenPos2;
        if (GetScreenPosFromWorldPos(beam.p1->AbsPosition, screenPos1)
            && GetScreenPosFromWorldPos(beam.p2->AbsPosition, screenPos2))
        {
            const float t1 = static_cast<float>(highlightedBeamsNodeProximity[beam.p1->pos] / 255.f);
            const float t2 = static_cast<float>(highlightedBeamsNodeProximity[beam.p2->pos] / 255.f);

            const ImVec4 color1 = ImLerp(theme.mouse_beam_close_color, theme.mouse_beam_far_color, 1.f - t1);
            const ImVec4 color2 = ImLerp(theme.mouse_beam_close_color, theme.mouse_beam_far_color, 1.f - t2);

            ImAddLineColorGradient(drawlist, screenPos1, screenPos2, color1, color2, theme.mouse_beam_thickness);
        }
    }
}

void SceneMouse::drawMouseNodeHighlights()
{
    ActorPtr actor = (mintruck != nullptr) ? mintruck : highlightedTruck;
    if (!actor)
        return; // Nothing to draw

    ImDrawList* drawlist = GetImDummyFullscreenWindow("Mouse-grab node highlights");
    const int LAYER_HIGHLIGHTS = 0;
    const int LAYER_MINNODE = 1;
    drawlist->ChannelsSplit(2);

    Vector2 screenPos;
    const GUIManager::GuiTheme& theme = App::GetGuiManager()->GetTheme();
    for (const HighlightedNode& hnode : highlightedNodes)
    {
        if (GetScreenPosFromWorldPos(actor->ar_nodes[hnode.nodenum].AbsPosition, /*out:*/screenPos))
        {
            float animRatio = (1.f - hnode.distance / theme.mouse_node_highlight_ref_distance); // the closer the bigger
            float radius = (theme.mouse_highlighted_node_radius_max - theme.mouse_highlighted_node_radius_min) * animRatio;

            if (hnode.nodenum == minnode)
            {
                drawlist->ChannelsSetCurrent(LAYER_MINNODE);
                drawlist->AddCircle(ImVec2(screenPos.x, screenPos.y),
                    radius, ImColor(theme.mouse_minnode_color),
                    theme.node_circle_num_segments, theme.mouse_minnode_thickness);
            }

            ImVec4 color = theme.mouse_highlighted_node_color * animRatio;
            color.w = 1.f; // no transparency
            drawlist->ChannelsSetCurrent(LAYER_HIGHLIGHTS);
            drawlist->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), radius, ImColor(color), theme.node_circle_num_segments);
        }
    }

    drawlist->ChannelsMerge();
}

void SceneMouse::drawNodeEffects()
{
    const GUIManager::GuiTheme& theme = App::GetGuiManager()->GetTheme();
    ImDrawList* drawlist = GetImDummyFullscreenWindow("Node effect view");
    const int LAYER_LINES = 0;
    const int LAYER_CIRCLES = 1;
    drawlist->ChannelsSplit(2);

    for (ActorPtr& actor : App::GetGameContext()->GetActorManager()->GetActors())
    {
        if (actor->ar_state != ActorState::LOCAL_SIMULATED)
            continue;

        for (size_t i = 0; i < actor->ar_node_effects_constant_force.size(); i++)
        {
            const NodeEffectConstantForce& e = actor->ar_node_effects_constant_force[i];
            Vector2 nodeScreenPos, pointScreenPos;
            Vector3 pointWorldPos = actor->ar_nodes[e.nodenum].AbsPosition + (e.force * FORCE_NEWTONS_TO_LINE_LENGTH_RATIO);
            if (GetScreenPosFromWorldPos(actor->ar_nodes[e.nodenum].AbsPosition, nodeScreenPos)
                && GetScreenPosFromWorldPos(pointWorldPos, nodeScreenPos))
            {
                drawlist->ChannelsSetCurrent(LAYER_LINES);
                const bool highlight = (e.nodenum == cfEffect_minnode) && actor == cfEffect_mintruck;
                const ImColor color = (highlight)
                    ? ImColor(theme.node_effect_highlight_line_color)
                    : ImColor(theme.node_effect_force_line_color);
                drawlist->AddLine(ImVec2(nodeScreenPos.x, nodeScreenPos.y), ImVec2(pointScreenPos.x, pointScreenPos.y),
                    color, theme.node_effect_force_line_thickness);
            }
        }

        for (size_t i = 0; i < actor->ar_node_effects_force_towards_point.size(); i++)
        {
            const NodeEffectForceTowardsPoint& e = actor->ar_node_effects_force_towards_point[i];
            Vector2 nodeScreenPos, pointScreenPos;
            if (GetScreenPosFromWorldPos(actor->ar_nodes[e.nodenum].AbsPosition, nodeScreenPos)
                && GetScreenPosFromWorldPos(e.point, pointScreenPos))
            {
                drawlist->ChannelsSetCurrent(LAYER_LINES);
                drawlist->AddLine(ImVec2(nodeScreenPos.x, nodeScreenPos.y), ImVec2(pointScreenPos.x, pointScreenPos.y),
                    ImColor(theme.node_effect_force_line_color), theme.node_effect_force_line_thickness);
                drawlist->ChannelsSetCurrent(LAYER_CIRCLES);
                const bool highlight = (e.nodenum == f2pEffect_minnode) && actor == f2pEffect_mintruck;
                const ImColor color = (highlight)
                    ? ImColor(theme.node_effect_highlight_line_color)
                    : ImColor(theme.node_effect_force_line_color);
                drawlist->AddCircleFilled(ImVec2(pointScreenPos.x, pointScreenPos.y),
                    theme.node_effect_force_circle_radius, color);
            }
        }
    }

    drawlist->ChannelsMerge();
}

void SceneMouse::UpdateVisuals()
{
    if (grab_truck == nullptr)
    {
        this->drawMouseNodeHighlights();
    }

    this->drawNodeEffects();
    this->drawMouseBeamHighlights();
}

bool SceneMouse::mousePressed(const OIS::MouseEvent& _arg, OIS::MouseButtonID _id)
{
    if (App::sim_state->getEnum<SimState>() == SimState::PAUSED) { return true; } // Do nothing when paused

    const OIS::MouseState ms = _arg.state;

    if (ms.buttonDown(OIS::MB_Middle))
    {
        Ray mouseRay = getMouseRay();

        if (App::sim_state->getEnum<SimState>() == SimState::TERRN_EDITOR)
        {
            return true;
        }

        ActorPtr player_actor = App::GetGameContext()->GetPlayerActor();

        // Reselect the player actor
        {
            Real nearest_ray_distance = std::numeric_limits<float>::max();

            for (ActorPtr& actor : App::GetGameContext()->GetActorManager()->GetActors())
            {
                if (actor != player_actor)
                {
                    Vector3 pos = actor->getPosition();
                    std::pair<bool, Real> pair = mouseRay.intersects(Sphere(pos, actor->getMinCameraRadius()));
                    if (pair.first)
                    {
                        Real ray_distance = mouseRay.getDirection().crossProduct(pos - mouseRay.getOrigin()).length();
                        if (ray_distance < nearest_ray_distance)
                        {
                            nearest_ray_distance = ray_distance;
                            App::GetGameContext()->PushMessage(Message(MSG_SIM_SEAT_PLAYER_REQUESTED, static_cast<void*>(new ActorPtr(actor))));
                        }
                    }
                }
            }
        }

        // Reselect the vehicle orbit camera center
        if (player_actor && App::GetCameraManager()->GetCurrentBehavior() == CameraManager::CAMERA_BEHAVIOR_VEHICLE)
        {
            Real nearest_camera_distance = std::numeric_limits<float>::max();
            Real nearest_ray_distance = std::numeric_limits<float>::max();
            NodeNum_t nearest_node_index = NODENUM_INVALID;

            for (int i = 0; i < static_cast<int>(player_actor->ar_nodes.size()); i++)
            {
                Vector3 pos = player_actor->ar_nodes[i].AbsPosition;
                std::pair<bool, Real> pair = mouseRay.intersects(Sphere(pos, 0.25f));
                if (pair.first)
                {
                    Real ray_distance = mouseRay.getDirection().crossProduct(pos - mouseRay.getOrigin()).length();
                    if (ray_distance < nearest_ray_distance || (ray_distance == nearest_ray_distance && pair.second < nearest_camera_distance))
                    {
                        nearest_camera_distance = pair.second;
                        nearest_ray_distance = ray_distance;
                        nearest_node_index = (NodeNum_t)i;
                    }
                }
            }
            if (player_actor->ar_custom_camera_node != nearest_node_index)
            {
                player_actor->ar_custom_camera_node = nearest_node_index;
                player_actor->calculateAveragePosition();
                App::GetCameraManager()->NotifyContextChange(); // Reset last 'look at' pos
            }
        }
    }

    if (ms.buttonDown(OIS::MB_Right) && cfEffect_minnode != NODENUM_INVALID)
    {
        cfEffect_mintruck->clearNodeEffectConstantForce(cfEffect_minnode);
        cfEffect_minnode = NODENUM_INVALID;
        cfEffect_mintruck = nullptr;
    }
    else if (ms.buttonDown(OIS::MB_Right) && f2pEffect_minnode != NODENUM_INVALID)
    {
        f2pEffect_mintruck->clearNodeEffectForceTowardsPoint(f2pEffect_minnode);
        f2pEffect_minnode = NODENUM_INVALID;
        f2pEffect_mintruck = nullptr;
    }

    return true;
}

bool SceneMouse::mouseReleased(const OIS::MouseEvent& _arg, OIS::MouseButtonID _id)
{
    if (App::sim_state->getEnum<SimState>() == SimState::PAUSED) { return true; } // Do nothing when paused

    if (mouseGrabState == 1)
    {
        releaseMousePick();
    }

    return true;
}

Ray SceneMouse::getMouseRay()
{
    int lastMouseX = App::GetInputEngine()->getMouseState().X.abs;
    int lastMouseY = App::GetInputEngine()->getMouseState().Y.abs;

    Viewport* vp = App::GetCameraManager()->GetCamera()->getViewport();

    return App::GetCameraManager()->GetCamera()->getCameraToViewportRay((float)lastMouseX / (float)vp->getActualWidth(), (float)lastMouseY / (float)vp->getActualHeight());
}

void SceneMouse::SetMouseHoveredNode(ActorPtr& actor, NodeNum_t nodenum)
{
    this->reset();
    minnode = nodenum;
    mintruck = actor;
    if (minnode != NODENUM_INVALID)
    {
        this->updateMouseBeamHighlights();
    }
}
