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
    grab_truck(nullptr),
    pickLine(nullptr),
    pickLineNode(nullptr)
{
    this->reset();
}

void SceneMouse::InitializeVisuals()
{
    // load 3d line for mouse picking
    pickLine = App::GetGfxScene()->GetSceneManager()->createManualObject("PickLineObject");
    pickLineNode = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode("PickLineNode");

    MaterialPtr pickLineMaterial = MaterialManager::getSingleton().getByName("PickLineMaterial", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    if (pickLineMaterial.isNull())
    {
        pickLineMaterial = MaterialManager::getSingleton().create("PickLineMaterial", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    }
    pickLineMaterial->setReceiveShadows(false);
    pickLineMaterial->getTechnique(0)->setLightingEnabled(true);
    pickLineMaterial->getTechnique(0)->getPass(0)->setDiffuse(0, 0, 1, 0);
    pickLineMaterial->getTechnique(0)->getPass(0)->setAmbient(0, 0, 1);
    pickLineMaterial->getTechnique(0)->getPass(0)->setSelfIllumination(0, 0, 1);

    pickLine->begin("PickLineMaterial", RenderOperation::OT_LINE_LIST);
    pickLine->position(0, 0, 0);
    pickLine->position(0, 0, 0);
    pickLine->end();
    pickLineNode->attachObject(pickLine);
    pickLineNode->setVisible(false);
}

void SceneMouse::DiscardVisuals()
{
    if (pickLineNode != nullptr)
    {
        App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->removeAndDestroyChild("PickLineNode");
        pickLineNode = nullptr;
    }

    if (pickLine != nullptr)
    {
        App::GetGfxScene()->GetSceneManager()->destroyManualObject("PickLineObject");
        pickLine = nullptr;
    }
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

void SceneMouse::updateMouseHighlights(ActorPtr actor)
{
    ROR_ASSERT(actor != nullptr);
    ROR_ASSERT(actor->ar_state == ActorState::LOCAL_SIMULATED);

    ImGui::Text("DBG updateMouseHighlights()");

    Ray mouseRay = getMouseRay();

    // check if our ray intersects with the bounding box of the truck
    std::pair<bool, Real> pair = mouseRay.intersects(actor->ar_bounding_box);
    if (!pair.first)
    {
        ImGui::Text("DBG AABB intersection fail!");
        return;
    }

    int numGrabHits = 0;
    int numHighlightHits = 0;
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
            numGrabHits++;

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
            numHighlightHits++;
            highlightedTruck = actor;

            highlightedNodes.push_back({ highlight_result.second, static_cast<NodeNum_t>(j) });
            highlightedNodesTopDistance = std::max(highlightedNodesTopDistance, highlight_result.second);
        }
    }
    ImGui::Text("DBG numGrabHits: %4d, numHighlightHits: %4d", numGrabHits, numHighlightHits);
}

void SceneMouse::UpdateSimulation()
{
    Ray mouseRay = getMouseRay();
    highlightedNodes.clear(); // clear every frame - highlights are not displayed when grabbing
    highlightedTruck = nullptr;

    if (mouseGrabState == 1 && grab_truck)
    {
        // get values
        lastgrabpos = mouseRay.getPoint(mindist);

        // add forces
        grab_truck->clearNodeEffectForceTowardsPoint(minnode);
        grab_truck->addNodeEffectForceTowardsPoint(minnode, lastgrabpos, MOUSE_GRAB_FORCE);
    }
    else
    {
        // refresh mouse highlight
        mintruck = nullptr;
        minnode = NODENUM_INVALID;
        mindist = std::numeric_limits<float>::max();
        for (ActorPtr& actor : App::GetGameContext()->GetActorManager()->GetActors())
        {
            if (actor->ar_state == ActorState::LOCAL_SIMULATED)
            {
                this->updateMouseHighlights(actor);
            }
        }
    }
}

void SceneMouse::drawMouseHighlights()
{
    ActorPtr actor = (mintruck != nullptr) ? mintruck : highlightedTruck;
    if (!actor)
        return; // Nothing to draw

    ImDrawList* drawlist = GetImDummyFullscreenWindow("Mouse-grab node highlights");
    const int LAYER_HIGHLIGHTS = 0;
    const int LAYER_MINNODE = 1;
    drawlist->ChannelsSplit(2);

    Vector2 screenPos;
    for (const HighlightedNode& hnode : highlightedNodes)
    {
        if (GetScreenPosFromWorldPos(actor->ar_nodes[hnode.nodenum].AbsPosition, /*out:*/screenPos))
        {
            if (hnode.nodenum == minnode)
            {
                drawlist->ChannelsSetCurrent(LAYER_MINNODE);
                drawlist->AddCircle(ImVec2(screenPos.x, screenPos.y), MINNODE_RADIUS, ImColor(MINNODE_COLOR));
            }

            float animRatio = 1.f - (hnode.distance / highlightedNodesTopDistance); // the closer the bigger
            float radius = (HIGHLIGHTED_NODE_RADIUS_MAX - HIGHLIGHTED_NODE_RADIUS_MIN) * animRatio;
            ImVec4 color = HIGHLIGHTED_NODE_COLOR * animRatio;
            color.w = 1.f; // no transparency
            drawlist->ChannelsSetCurrent(LAYER_HIGHLIGHTS);
            drawlist->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), radius, ImColor(color));
        }
    }

    drawlist->ChannelsMerge();
}

void SceneMouse::UpdateVisuals()
{
    if (grab_truck == nullptr)
    {
        pickLineNode->setVisible(false);   // Hide the line     

        this->drawMouseHighlights();
    }
    else
    {
        pickLineNode->setVisible(true);   // Show the line
        // update visual line
        pickLine->beginUpdate(0);
        pickLine->position(grab_truck->GetGfxActor()->GetSimNodeBuffer()[minnode].AbsPosition);
        pickLine->position(lastgrabpos);
        pickLine->end();
    }
}

bool SceneMouse::mousePressed(const OIS::MouseEvent& _arg, OIS::MouseButtonID _id)
{
    if (App::sim_state->getEnum<SimState>() == SimState::PAUSED) { return true; } // Do nothing when paused

    const OIS::MouseState ms = _arg.state;

    if (ms.buttonDown(OIS::MB_Middle))
    {
        Ray mouseRay = getMouseRay();

        if (App::sim_state->getEnum<SimState>() == SimState::EDITOR_MODE)
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
