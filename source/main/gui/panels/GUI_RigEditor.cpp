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
#include "InputEngine.h"
#include "Language.h"
#include "RigDef_File.h"
#include "RigDef_Serializer.h"

#include <fmt/format.h>
#include <imgui.h>

using namespace RoR;
using namespace GUI;

void RigEditor::Draw()
{
    ActorPtr actor = App::GetGameContext()->GetPlayerActor();
    if (!actor)
    {
   //     m_is_visible = false;
        return;
    }
    m_node_selected.resize(actor->ar_num_nodes);

    ImGui::SetNextWindowContentWidth(500.f);
    if (!ImGui::Begin(actor->getTruckName().c_str(), &m_is_visible))
    {
        ImGui::End(); // The window is collapsed
        return;
    }

    RoR::GfxActor* gfx_actor = actor->GetGfxActor();
    // Find this actor in modcache
    CacheEntry* cache_entry = App::GetCacheSystem()->FindEntryByFilename(RoR::LT_AllBeam, /*partial:*/false, actor->getTruckFileName());

    NodeNum_t highlightNode = NODENUM_INVALID;

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

    App::GetGuiManager()->RequestGuiCaptureKeyboard(ImGui::IsWindowHovered());
    ImGui::End();
}

void RigEditor::UpdateInputEvents(float dt)
{
    if (App::sim_state->getEnum<SimState>() == SimState::TRUCK_EDITOR)
    {
        // truck editor toggle
        if (App::GetGameContext()->GetPlayerActor() && App::GetInputEngine()->getEventBoolValueBounce(EV_COMMON_TOGGLE_TRUCK_EDITOR))
        {
            App::GetGameContext()->PushMessage(MSG_EDI_LEAVE_TRUCK_EDITOR_REQUESTED);
        }
    }
}

void RigEditor::DrawNodesTable(ActorPtr& actor, CacheEntry* cache_entry)
{
    ImGui::PushID("RigEditorNodes");

    ImGui::Columns(9); // pos, from, name, num, X, Y, Z, options, loadweight

    /* ImGui::SetColumnWidth(0, ImGui::CalcTextSize("000").x);
     ImGui::SetColumnWidth(1, ImGui::CalcTextSize("0000000").x);
     static float nameColWidth = static_cast<float>(std::max(ImGui::CalcTextSize("0").x * actor->ar_nodes_name_top_length, ImGui::CalcTextSize("00000").x));
     ImGui::SetColumnWidth(2, nameColWidth);
     ImGui::SetColumnWidth(3, ImGui::CalcTextSize("000").x);
     ImGui::SetColumnWidth(4, ImGui::CalcTextSize("-000.00").x);
     ImGui::SetColumnWidth(5, ImGui::CalcTextSize("-000.00").x);
     ImGui::SetColumnWidth(6, ImGui::CalcTextSize("-000.00").x);
     ImGui::SetColumnWidth(7, ImGui::CalcTextSize("abcd").x);
     ImGui::SetColumnWidth(8, ImGui::CalcTextSize("000").x);*/

    ImGui::Text("###");

    ImGui::NextColumn();
    ImGui::Text("From");

    ImGui::NextColumn();
    ImGui::Text("Name");

    ImGui::NextColumn();
    ImGui::Text("Num");

    ImGui::NextColumn();
    ImGui::Text("X");
    ImGui::NextColumn();
    ImGui::Text("Y");
    ImGui::NextColumn();
    ImGui::Text("Z");

    ImGui::NextColumn();
    ImGui::Text("Opt");

    ImGui::NextColumn();
    ImGui::Text("Kg");

    for (int i = 0; i < actor->ar_num_nodes; i++)
    {
        ROR_ASSERT(actor->ar_nodes[i].pos == (NodeNum_t)i);
        ImGui::PushID(i);

        ImGui::NextColumn(); // Pos    
        Str<50> num;
        num << i;
        bool selected = m_node_selected[i];
        // The selection highlight for the whole line
        if (ImGui::Selectable("", &selected, ImGuiSelectableFlags_SpanAllColumns))
        {
            m_node_selected[i] = selected;
        }
        ImGui::SameLine();
        // The selection checkbox - just informational
        ImGui::Checkbox(num.ToCStr(), &selected);

        ImGui::NextColumn(); // From
        ImGui::Text(RigDef::KeywordToString(actor->ar_nodes_aux[i].nda_source_keyword));

        ImGui::NextColumn(); // Name
        ImGui::Text(actor->ar_nodes_name[i].c_str());

        ImGui::NextColumn(); // Id
        ImGui::Text("%d", actor->ar_nodes_id[i]);

        // Display definition, only for nodes from `nodes` or `nodes2`!
        if (actor->ar_nodes_aux[i].nda_source_keyword == RigDef::Keyword::NODES)
        {

            RigDef::Node& def = cache_entry->actor_def->root_module->nodes[actor->ar_nodes_aux[i].nda_source_datapos];

            ImGui::NextColumn(); // X

            ImGui::Text("%f", def.position.x);
            ImGui::NextColumn(); // Y
            ImGui::Text("%f", def.position.y);
            ImGui::NextColumn(); // Z
            ImGui::Text("%f", def.position.z);

            ImGui::NextColumn(); // options
            ImGui::Text(RigDef::Serializer::ProcessNodeOptions(def.options).c_str());

            ImGui::NextColumn(); // load weight
        }
        else
        {
            ImGui::NextColumn(); // X
            ImGui::NextColumn(); // Y
            ImGui::NextColumn(); // Z
            ImGui::NextColumn(); // options
            ImGui::NextColumn(); // load weight
        }

        ImGui::PopID(); // i
    }

    ImGui::Columns(1);

    ImGui::PopID();// "RigEditorNodes"
}
