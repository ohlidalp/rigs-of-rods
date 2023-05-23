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

#include "GUI_RigEditor.h"

#include "Application.h"
#include "Actor.h"
#include "GameContext.h"
#include "GUIManager.h"
#include "GUIUtils.h"
#include "InputEngine.h"
#include "Language.h"
#include "RigDef_File.h"
#include "RigDef_Serializer.h"

#include <fmt/format.h>
#include <imgui.h>

using namespace RoR;
using namespace GUI;
using namespace Ogre;

void RigEditor::DrawSidePanel()
{
    ActorPtr actor = App::GetGameContext()->GetPlayerActor();
    if (!actor) // should never happen
    {
        m_is_visible = false;
        return;
    }
    m_node_selected.resize(actor->ar_nodes.size());

    ImGui::SetNextWindowContentWidth(500.f);
    ImGui::SetNextWindowPos(ImVec2(10.f, 10.f));
    if (!ImGui::Begin(actor->getTruckName().c_str(), &m_is_visible))
    {
        ImGui::End(); // The window is collapsed
        return;
    }

    RoR::GfxActor* gfx_actor = actor->GetGfxActor();
    // Find this actor in modcache
    CacheEntry* cache_entry = App::GetCacheSystem()->FindEntryByFilename(RoR::LT_AllBeam, /*partial:*/false, actor->getTruckFileName());

    m_hovered_node = NODENUM_INVALID;

    if (cache_entry && ImGui::CollapsingHeader(_LC("RigEditor", "Nodes"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        this->DrawNodesTable(actor, cache_entry);
    }

    if (ImGui::CollapsingHeader(_LC("VehicleDescription", "Description"), 0))
    {
        if (gfx_actor->GetHelpTex())
        {
            ImTextureID im_tex = reinterpret_cast<ImTextureID>(gfx_actor->GetHelpTex()->getHandle());
            ImGui::Image(im_tex, ImVec2(HELP_TEXTURE_WIDTH, HELP_TEXTURE_HEIGHT));
        }

        for (auto line : actor->getDescription())
        {
            ImGui::TextWrapped("%s", line.c_str());
        }
    }

    if (ImGui::CollapsingHeader(_LC("VehicleDescription", "Commands"), 0))
    {
        ImGui::Columns(2, /*id=*/nullptr, /*border=*/true);
        for (int i = 1; i < MAX_COMMANDS; i += 2)
        {
            if (actor->ar_command_key[i].description == "hide")
                continue;
            if (actor->ar_command_key[i].beams.empty() && actor->ar_command_key[i].rotators.empty())
                continue;

            int eventID = RoR::InputEngine::resolveEventName(fmt::format("COMMANDS_{:02d}", i));
            Ogre::String keya = RoR::App::GetInputEngine()->getEventCommand(eventID);
            eventID = RoR::InputEngine::resolveEventName(fmt::format("COMMANDS_{:02d}", i + 1));
            Ogre::String keyb = RoR::App::GetInputEngine()->getEventCommand(eventID);

            // cut off expl
            if (keya.size() > 6 && keya.substr(0, 5) == "EXPL+")
                keya = keya.substr(5);
            if (keyb.size() > 6 && keyb.substr(0, 5) == "EXPL+")
                keyb = keyb.substr(5);

            ImGui::Text("%s/%s", keya.c_str(), keyb.c_str());
            ImGui::NextColumn();

            if (!actor->ar_command_key[i].description.empty())
            {
                ImGui::Text("%s", actor->ar_command_key[i].description.c_str());
            }
            else
            {
                ImGui::TextDisabled("%s", _LC("VehicleDescription", "unknown function"));
            }
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }

    if (ImGui::CollapsingHeader(_LC("VehicleDescription", "Authors")))
    {
        for (authorinfo_t const& author: actor->getAuthors())
        {
            ImGui::Text("%s: %s", author.type.c_str(), author.name.c_str());
        }

        if (actor->getAuthors().empty())
        {
            ImGui::TextDisabled("%s", _LC("VehicleDescription", "(no author information available) "));
        }
    }
    m_is_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    App::GetGuiManager()->RequestGuiCaptureKeyboard(m_is_hovered);
    ImGui::End();

    if (m_is_hovered)
    {
        App::GetGameContext()->GetSceneMouse().SetMouseHoveredNode(actor, m_hovered_node);
    }
}

void RigEditor::UpdateInputEvents(float dt)
{
    if (App::sim_state->getEnum<SimState>() == SimState::TRUCK_EDITOR)
    {
        // truck editor toggle (logs a warning if not driving a vehicle)
        if (App::GetInputEngine()->getEventBoolValueBounce(EV_COMMON_TOGGLE_TRUCK_EDITOR))
        {
            App::GetGameContext()->PushMessage(MSG_EDI_LEAVE_TRUCK_EDITOR_REQUESTED);
        }
    }

    ActorPtr player_actor = App::GetGameContext()->GetPlayerActor();
    if (player_actor)
    {
        if (App::GetInputEngine()->getEventBoolValueBounce(EV_COMMON_TOGGLE_DEBUG_VIEW))
        {
            player_actor->GetGfxActor()->ToggleDebugView();
            for (ActorPtr actor : player_actor->ar_linked_actors)
            {
                actor->GetGfxActor()->SetDebugView(player_actor->GetGfxActor()->GetDebugView());
            }
        }

        if (App::GetInputEngine()->getEventBoolValueBounce(EV_COMMON_CYCLE_DEBUG_VIEWS))
        {
            player_actor->GetGfxActor()->CycleDebugViews();
            for (ActorPtr actor : player_actor->ar_linked_actors)
            {
                actor->GetGfxActor()->SetDebugView(player_actor->GetGfxActor()->GetDebugView());
            }
        }
    }
}

void RigEditor::DrawNodesTable(ActorPtr& actor, CacheEntry* cache_entry)
{
    ImGui::PushID("RigEditorNodes");
    m_num_nodes_selected = 0;

    ImGui::TextDisabled("This is a dump from memory, including all generated nodes");
    ImGui::TextDisabled("Columns marked `*` contain live data (other values are from definition file)");
    ImGui::TextDisabled("The '*Opt' column only supports flags `l` and `m`");
    ImGui::TextDisabled("Note that 'set_node_defaults' values are merged into the nodes.");

    ImGui::Columns(8); // live pos, live Kg, live options, from, name, id, options, `l`Kg (load weight override)

    ImGui::Text("*###");

    ImGui::NextColumn();
    ImGui::Text("*Kg");

    ImGui::NextColumn();
    ImGui::Text("*Opt");

    ImGui::NextColumn();
    ImGui::Text("From");

    ImGui::NextColumn();
    ImGui::Text("Name");

    ImGui::NextColumn();
    ImGui::Text("Id");

    ImGui::NextColumn();
    ImGui::Text("Opt");

    ImGui::NextColumn();
    ImGui::Text("`l`Kg");

    for (size_t i = 0; i < actor->ar_nodes.size(); i++)
    {
        ROR_ASSERT(actor->ar_nodes[i].pos == (NodeNum_t)i);
        ImGui::PushID(i);

        ImGui::NextColumn(); // *Pos    
        Str<50> num;
        num << i;
        bool selected = m_node_selected[i];
        // The selection highlight for the whole line
        if (ImGui::Selectable("", &selected, ImGuiSelectableFlags_SpanAllColumns))
        {
            m_node_selected[i] = selected;
        }
        if (ImGui::IsItemHovered())
        {
            m_hovered_node = (NodeNum_t)i;
        }
        ImGui::SameLine();
        if (m_node_selected[i])
            m_num_nodes_selected++;
        // The selection checkbox - just informational
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.f));
        ImGui::Checkbox(num.ToCStr(), &selected);
        ImGui::PopStyleVar(); // ImGuiStyleVar_FramePadding

        ImGui::NextColumn(); // *Kg
        ImGui::Text("%.2f", actor->ar_nodes[i].mass);

        ImGui::NextColumn(); // *Opt
        Str<25> liveOpt;
        if (actor->ar_nodes_aux[i].nda_loaded_mass) liveOpt << "l";
        if (actor->ar_nodes[i].nd_no_mouse_grab) liveOpt << "m";
        ImGui::Text(liveOpt.ToCStr());

        ImGui::NextColumn(); // From
        ImGui::Text(RigDef::KeywordToString(actor->ar_nodes_aux[i].nda_source_keyword));

        ImGui::NextColumn(); // Name
        ImGui::Text(actor->ar_nodes_aux[i].nda_source_name.c_str());

        ImGui::NextColumn(); // Id
        ImGui::Text("%d", actor->ar_nodes_aux[i].nda_source_id);

        // Display definition, only for nodes from `nodes` or `nodes2`!
        if (actor->ar_nodes_aux[i].nda_source_keyword == RigDef::Keyword::NODES)
        {

            RigDef::Node& def = cache_entry->actor_def->root_module->nodes[actor->ar_nodes_aux[i].nda_source_datapos];

            ImGui::NextColumn(); // options
            ImGui::Text(RigDef::Serializer::ProcessNodeOptions(def.options).c_str());

            ImGui::NextColumn(); // load weight override
            if (def._has_load_weight_override)
            {
                ImGui::Text("%.2f", def.load_weight_override);
            }
        }
        else
        {
            ImGui::NextColumn(); // options
            ImGui::NextColumn(); // load weight override
        }

        ImGui::PopID(); // i
    }

    ImGui::Columns(1);

    ImGui::PopID();// "RigEditorNodes"
}

void RigEditor::DrawSelectedNodeHighlights()
{
    if (m_node_selected.size() == 0)
        return; // Nothing to draw yet

    ActorPtr actor = App::GetGameContext()->GetPlayerActor();
    m_node_highlight_drawn.clear();
    m_node_highlight_drawn.resize(actor->ar_nodes.size(), /*val:*/false); 
    auto& theme = App::GetGuiManager()->GetTheme();

    ImDrawList* drawlist = GetImDummyFullscreenWindow("RigEditorNodeHighlights");
    for (size_t i = 0; i < actor->ar_beams.size(); i++)
    {
        beam_t& beam = actor->ar_beams[i];

        if (m_node_selected[beam.p1num]
            || m_node_selected[beam.p2num])
        {
            Vector2 p1screen, p2screen;
            if (GetScreenPosFromWorldPos(actor->ar_nodes[beam.p1num].AbsPosition, p1screen)
                && GetScreenPosFromWorldPos(actor->ar_nodes[beam.p2num].AbsPosition, p2screen))
            {
                // Draw the beam highlight (faded if only 1 node is selected)

                ImVec4 p1color = theme.editor_selected_node_color;
                if (!m_node_selected[beam.p1num])
                    p1color.w = 0.f; // Unselected node = full transparency
                ImVec4 p2color = theme.editor_selected_node_color;
                if (!m_node_selected[beam.p2num])
                    p2color.w = 0.f; // Unselected node = full transparency

                ImAddLineColorGradient(drawlist, p1screen, p2screen, p1color, p2color, theme.editor_selected_node_beam_thickness);

                // Draw node circles (if not already drawn)

                ImVec2 im_p1screen(p1screen.x, p1screen.y);
                if (!m_node_highlight_drawn[actor->ar_nodes[beam.p1num].pos])
                {
                    if (m_node_selected[beam.p1num])
                    {
                        drawlist->AddCircleFilled(im_p1screen, theme.editor_selected_node_radius, ImColor(theme.editor_selected_node_color), theme.node_circle_num_segments);
                    }
                    m_node_highlight_drawn[actor->ar_nodes[beam.p1num].pos] = true;
                }

                ImVec2 im_p2screen(p2screen.x, p2screen.y);
                if (!m_node_highlight_drawn[actor->ar_nodes[beam.p2num].pos])
                {
                    if (m_node_selected[beam.p2num])
                    {
                        drawlist->AddCircleFilled(im_p2screen, theme.editor_selected_node_radius, ImColor(theme.editor_selected_node_color), theme.node_circle_num_segments);
                    }
                    m_node_highlight_drawn[actor->ar_nodes[beam.p2num].pos] = true;
                }
            }
        }
    }
}
