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


#include "GUI_NodeBeamUtils.h"

#include "Application.h"
#include "Actor.h"
#include "GameContext.h"
#include "GUIManager.h"
#include "GUIUtils.h"
#include "Language.h"
#include "Utils.h"

using namespace RoR;
using namespace GUI;

void NodeBeamUtils::Draw(float dt)
{
    ActorPtr actor = App::GetGameContext()->GetPlayerActor();
    if (!actor)
    {
        this->SetVisible(false);
        return;
    }
    const bool is_project = actor->getUsedActorEntry()->resource_bundle_type != "Zip";

    ImGui::SetNextWindowPosCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600.f, 675.f), ImGuiCond_FirstUseEver);
    int flags = ImGuiWindowFlags_NoCollapse;
    if (is_project)
    {
        flags |= ImGuiWindowFlags_MenuBar;
    }
    bool keep_open = true;
    ImGui::Begin(_LC("NodeBeamUtils", "Node/Beam Utils"), &keep_open, flags);

    if (!is_project)
    {
        this->DrawCreateProjectBanner(actor, keep_open);
    }
    else
    {
        this->DrawMenubar(actor);
    }

    if (ImGui::BeginTabBar("NodeBeamUtilsTabBar", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem(_LC("NodeBeamUtils", "Mass")))
        {
            this->DrawMassTab(actor);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(_LC("NodeBeamUtils", "Spring/Damp")))
        {
            this->DrawSpringDampTab(actor);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(_LC("NodeBeamUtils", "Memory")))
        {
            this->DrawMemoryTab(actor);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (m_is_searching)
    {
        actor->searchBeamDefaults();
    }

    m_is_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    App::GetGuiManager()->RequestGuiCaptureKeyboard(m_is_hovered);

    ImGui::End();

    m_cache_paint.UpdateAnim(actor, dt);

    if (!keep_open)
    {
        this->SetVisible(false);
    }
}

void NodeBeamUtils::DrawSpringDampTab(ActorPtr actor)
{
    ImGui::PushItemWidth(500.f); // Width includes [+/-] buttons

    ImGui::TextColored(GRAY_HINT_TEXT, _LC("NodeBeamUtils", "Beams:"));
    if (ImGui::SliderFloat("Spring##Beams", &actor->ar_nb_beams_scale.first, 0.1f, 10.0f, "%.5f"))
    {
        actor->applyNodeBeamScales();
    }
    if (ImGui::SliderFloat("Damping##Beams", &actor->ar_nb_beams_scale.second, 0.1f, 10.0f, "%.5f"))
    {
        actor->applyNodeBeamScales();
    }
    ImGui::Separator();
    ImGui::TextColored(GRAY_HINT_TEXT, _LC("NodeBeamUtils", "Shocks:"));
    if (ImGui::SliderFloat("Spring##Shocks", &actor->ar_nb_shocks_scale.first, 0.1f, 10.0f, "%.5f"))
    {
        actor->applyNodeBeamScales();
    }
    if (ImGui::SliderFloat("Damping##Shocks", &actor->ar_nb_shocks_scale.second, 0.1f, 10.0f, "%.5f"))
    {
        actor->applyNodeBeamScales();
    }
    ImGui::Separator();

    ImGui::TextColored(GRAY_HINT_TEXT, _LC("NodeBeamUtils", "Wheels:"));
    const float WBASE_WIDTH = 125.f;
    const float WSCALE_WIDTH = 225.f;
    const float WLABEL_GAP = 20.f;
    // WHEEL-SPECIFIC: assume all wheels have same spring/damp and use wheel [0] as the master record

    // wheel spring
    ImGui::SetNextItemWidth(WBASE_WIDTH);
    if (ImGui::InputFloat("Base##wheels-spring", &actor->ar_wheels[0].wh_arg_simple_spring))
    {
        actor->applyNodeBeamScales();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(WSCALE_WIDTH);
    if (ImGui::SliderFloat("Scale##Wheels-spring", &actor->ar_nb_wheels_scale.first, 0.1f, 10.0f, "%.5f"))
    {
        actor->applyNodeBeamScales();
    }
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + WLABEL_GAP);
    ImGui::Text("Spring (%.2f)", actor->ar_nb_wheels_scale.first * actor->ar_wheels[0].wh_arg_simple_spring);

    // wheel damping
    ImGui::SetNextItemWidth(WBASE_WIDTH);
    if (ImGui::InputFloat("Base##wheels-damping", &actor->ar_wheels[0].wh_arg_simple_damping))
    {
        actor->applyNodeBeamScales();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(WSCALE_WIDTH);
    if (ImGui::SliderFloat("Scale##Wheels-damping", &actor->ar_nb_wheels_scale.second, 0.1f, 10.0f, "%.5f"))
    {
        actor->applyNodeBeamScales();
    }
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + WLABEL_GAP);
    ImGui::Text("Damping (%.2f)", actor->ar_nb_wheels_scale.second * actor->ar_wheels[0].wh_arg_simple_damping);

    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button(_LC("NodeBeamUtils", "Reset to default settings"), ImVec2(280.f, 25.f)))
    {
        actor->ar_nb_beams_scale = { 1.0f, 1.0f };
        actor->ar_nb_shocks_scale = { 1.0f, 1.0f };
        actor->ar_nb_wheels_scale = { 1.0f, 1.0f };
        actor->SyncReset(true);
    }
    ImGui::SameLine();
    if (ImGui::Button(_LC("NodeBeamUtils", "Update initial node positions"), ImVec2(280.f, 25.f)))
    {
        actor->updateInitPosition();
    }
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(235.f); // Width includes [+/-] buttons
    ImGui::TextColored(GRAY_HINT_TEXT, "%s", _LC("NodeBeamUtils", "Physics steps:"));
    ImGui::SliderInt("Skip##BeamsInt", &actor->ar_nb_skip_steps, 0, 2000);
    ImGui::SameLine();
    ImGui::SliderInt("Measure##BeamsInt", &actor->ar_nb_measure_steps, 2, 6000);
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(138.f); // Width includes [+/-] buttons
    ImGui::Separator();
    ImGui::TextColored(GRAY_HINT_TEXT, "%s", _LC("NodeBeamUtils", "Beams (spring & damping search interval):"));
    ImGui::SliderFloat("##BSL", &actor->ar_nb_beams_k_interval.first, 0.1f, actor->ar_nb_beams_k_interval.second);
    ImGui::SameLine();
    ImGui::SliderFloat("##BSU", &actor->ar_nb_beams_k_interval.second, actor->ar_nb_beams_k_interval.first, 10.0f);
    ImGui::SameLine();
    ImGui::SliderFloat("##BDL", &actor->ar_nb_beams_d_interval.first, 0.1f, actor->ar_nb_beams_d_interval.second);
    ImGui::SameLine();
    ImGui::SliderFloat("##BDU", &actor->ar_nb_beams_d_interval.second, actor->ar_nb_beams_d_interval.first, 10.0f);
    ImGui::TextColored(GRAY_HINT_TEXT, "%s", _LC("NodeBeamUtils", "Shocks (spring & damping search interval):"));
    ImGui::SliderFloat("##SSL", &actor->ar_nb_shocks_k_interval.first, 0.1f, actor->ar_nb_shocks_k_interval.second);
    ImGui::SameLine();
    ImGui::SliderFloat("##SSU", &actor->ar_nb_shocks_k_interval.second, actor->ar_nb_shocks_k_interval.first, 10.0f);
    ImGui::SameLine();
    ImGui::SliderFloat("##SDL", &actor->ar_nb_shocks_d_interval.first, 0.1f, actor->ar_nb_shocks_d_interval.second);
    ImGui::SameLine();
    ImGui::SliderFloat("##SDU", &actor->ar_nb_shocks_d_interval.second, actor->ar_nb_shocks_d_interval.first, 10.0f);
    ImGui::TextColored(GRAY_HINT_TEXT, "%s", _LC("NodeBeamUtils", "Wheels (spring & damping search interval):"));
    ImGui::SliderFloat("##WSL", &actor->ar_nb_wheels_k_interval.first, 0.1f, actor->ar_nb_wheels_k_interval.second);
    ImGui::SameLine();
    ImGui::SliderFloat("##WSU", &actor->ar_nb_wheels_k_interval.second, actor->ar_nb_wheels_k_interval.first, 10.0f);
    ImGui::SameLine();
    ImGui::SliderFloat("##WDL", &actor->ar_nb_wheels_d_interval.first, 0.1f, actor->ar_nb_wheels_d_interval.second);
    ImGui::SameLine();
    ImGui::SliderFloat("##WDU", &actor->ar_nb_wheels_d_interval.second, actor->ar_nb_wheels_d_interval.first, 10.0f);
    ImGui::PopItemWidth();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button(m_is_searching ? _LC("NodeBeamUtils", "Stop searching") : actor->ar_nb_initialized ? _LC("NodeBeamUtils", "Continue searching") : _LC("NodeBeamUtils", "Start searching"),
        ImVec2(280.f, 25.f)))
    {
        m_is_searching = !m_is_searching;
        if (!m_is_searching)
        {
            actor->SyncReset(true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(_LC("NodeBeamUtils", "Reset search"), ImVec2(280.f, 25.f)))
    {
        actor->ar_nb_initialized = false;
        m_is_searching = false;
    }
    ImGui::Separator();
    ImGui::Spacing();
    if (actor->ar_nb_initialized)
    {
        ImGui::Columns(2, _LC("NodeBeamUtils", "Search results"));
        ImGui::SetColumnOffset(1, 290.f);
        ImGui::Text("%s", _LC("NodeBeamUtils", "Reference"));
        ImGui::NextColumn();
        ImGui::Text("%s", _LC("NodeBeamUtils", "Optimum"));
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text("%s: %f (%f)", _LC("NodeBeamUtils", "Movement"), actor->ar_nb_reference[5] / actor->ar_num_nodes, actor->ar_nb_reference[4]);
        ImGui::Text("%s: %.2f (%.2f)", _LC("NodeBeamUtils", "Stress"), actor->ar_nb_reference[1] / actor->ar_num_beams, actor->ar_nb_reference[0]);
        ImGui::Text("%s:   %f (%f)", _LC("NodeBeamUtils", "Yitter"), actor->ar_nb_reference[3] / actor->ar_num_beams, actor->ar_nb_reference[2]);
        ImGui::NextColumn();
        ImGui::Text("%s: %f (%f)", _LC("NodeBeamUtils", "Movement"), actor->ar_nb_optimum[5] / actor->ar_num_nodes, actor->ar_nb_optimum[4]);
        ImGui::Text("%s: %.2f (%.2f)", _LC("NodeBeamUtils", "Stress"), actor->ar_nb_optimum[1] / actor->ar_num_beams, actor->ar_nb_optimum[0]);
        ImGui::Text("%s:   %f (%f)", _LC("NodeBeamUtils", "Yitter"), actor->ar_nb_optimum[3] / actor->ar_num_beams, actor->ar_nb_optimum[2]);
        ImGui::Columns(1);
    }
}

void NodeBeamUtils::SetVisible(bool v)
{
    m_is_visible = v;
    m_is_hovered = false;
    if (!v)
    {
        m_is_searching = false;
        m_cache_paint = CachePaint(); // reset
    }
}

void NodeBeamUtils::DrawCreateProjectBanner(ActorPtr actor, bool& window_open)
{
    // Show [[ "read only files - create writeable project?" ]] banner.
    // If [yes], unpack the project files, unload current actor and show hint box.
    // ---------------------------------------------------------------------------

    GUIManager::GuiTheme& theme = App::GetGuiManager()->GetTheme();

    // Draw a banner
    const ImVec2 PAD(3, 3);
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 rect_min =  cursor - PAD;
    ImVec2 rect_max = cursor + PAD + ImVec2(ImGui::GetWindowContentRegionMax().x, ImGui::GetTextLineHeightWithSpacing());
    ImGui::GetWindowDrawList()->AddRectFilled(rect_min, rect_max, ImColor(theme.tip_panel_bg_color));
    ImGui::AlignTextToFramePadding();
    ImGui::Text(_LC("NodeBeamUtils", "This mod is read only (ZIP archive)"));
    ImGui::SameLine();
    if (ImGui::Button(_LC("NodeBeamUtils", "Create writable project (if not existing)")))
    {
        // Unzip the mod
        RoR::CreateProjectRequest* req = new RoR::CreateProjectRequest();
        req->cpr_name = "nbutil_" + actor->getUsedActorEntry()->fname_without_uid;
        req->cpr_description = "Node/Beam Utils project for " + actor->getUsedActorEntry()->dname;
        req->cpr_source_entry = actor->getUsedActorEntry();
        req->cpr_type = RoR::CreateProjectRequestType::ACTOR_PROJECT;
        App::GetGameContext()->PushMessage(Message(MSG_EDI_CREATE_PROJECT_REQUESTED, req));

        // Show a message box "please load the project"
        // - it cannot be loaded automatically because it's not in modcache yet so there's no way to locate it.
        RoR::GUI::MessageBoxConfig* box = new RoR::GUI::MessageBoxConfig();
        box->mbc_title = _LC("NodeBeamUtils", "Project created");
        box->mbc_text = fmt::format(_LC("NodeBeamUtils", "Project created successfully as \n\"{}\"\n\nPlease load it and open the N/B utility again"), req->cpr_name);
        box->mbc_allow_close = true;
        App::GetGameContext()->ChainMessage(Message(MSG_GUI_SHOW_MESSAGE_BOX_REQUESTED, box));

        // Unload current actor
        App::GetGameContext()->ChainMessage(Message(MSG_SIM_DELETE_ACTOR_REQUESTED, new ActorPtr(actor)));

        window_open = false;
    }
    ImGui::Dummy(ImVec2(1.f, 6.f));
}

void NodeBeamUtils::DrawMenubar(ActorPtr actor)
{
    if (ImGui::BeginMenuBar())
    {
        ImGui::TextDisabled(_LC("NodeBeamUtils", "Project:"));
        ImGui::SameLine();
        ImGui::Text("%s", actor->getUsedActorEntry()->dname.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(_LC("NodeBeamUtils", "Save and reload")))
        {
            RoR::ModifyProjectRequest* req = new RoR::ModifyProjectRequest();
            req->mpr_type = RoR::ModifyProjectRequestType::ACTOR_UPDATE_DEF_DOCUMENT;
            req->mpr_target_actor = actor;
            App::GetGameContext()->PushMessage(Message(MSG_EDI_MODIFY_PROJECT_REQUESTED, req));
        }

        ImGui::EndMenuBar();
    }
}

void NodeBeamUtils::DrawMassTab(ActorPtr actor)
{
    ImGui::PushID("drymass"); // To disambiguate the 'reset' buttons.
    ImGui::TextDisabled(_LC("NodeBeamUtils", "User-defined values:"));
    if (ImGui::SliderFloat(_LC("NodeBeamUtils", "Dry mass"), &actor->ar_dry_mass,
        actor->ar_original_dry_mass * 0.4f, actor->ar_original_dry_mass * 1.6f, "%.2f Kg"))
    {
        actor->recalculateNodeMasses();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(_LC("NodeBeamUtils", "Reset")))
    {
        actor->ar_dry_mass = actor->ar_original_dry_mass;
        actor->recalculateNodeMasses();
    }
    ImGui::PopID();

    ImGui::PushID("loadmass"); // To disambiguate the 'reset' buttons.
    if (ImGui::SliderFloat(_LC("NodeBeamUtils", "Load mass"), &actor->ar_load_mass,
        actor->ar_original_load_mass * 0.4f, actor->ar_original_load_mass * 1.6f, "%.2f Kg"))
    {
        actor->recalculateNodeMasses();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(_LC("NodeBeamUtils", "Reset")))
    {
        actor->ar_load_mass = actor->ar_original_load_mass;
        actor->recalculateNodeMasses();
    }
    ImGui::PopID();

    ImGui::PushID("minimass"); // To disambiguate the 'reset' buttons.
    if (ImGui::SliderFloat(_LC("NodeBeamUtils", "Minimum node mass scale"), &actor->ar_nb_minimass_scale, 0.4, 1.6))
    {
        for (int i = 0; i < actor->ar_num_nodes; i++)
        {
            actor->ar_minimass[i] = actor->ar_nb_minimass_scale * actor->ar_orig_minimass[i];
        }
        actor->recalculateNodeMasses();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(_LC("NodeBeamUtils", "Reset")))
    {
        for (int i = 0; i < actor->ar_num_nodes; i++)
        {
            actor->ar_minimass[i] = actor->ar_orig_minimass[i];
        }
        actor->ar_nb_minimass_scale = 1.0f;
        actor->recalculateNodeMasses();
    }
    ImGui::PopID();

    ImGui::Separator();
    ImGui::TextDisabled(_LC("NodeBeamUtils", "Calculated values:"));
    ImGui::Text("%s: %f", _LC("NodeBeamUtils", "Total mass"), actor->ar_total_mass);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Total nodes"), actor->ar_num_nodes);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Loaded nodes"), actor->ar_masscount);
}

const float BEAM_THICKNESS           (1.2f);
const ImVec4 BEAM_COLOR_VISITED(0.2f, 0.6f, 0.45f, 1.f);
const ImVec4 BEAM_COLOR_NONVISITED(0.25f, 0.45f, 0.55f, 1.f);

const float NODE_RADIUS              (2.f);
const ImVec4 NODE_COLOR_MISS(0.8f, 0.2f, 0.2f, 1.f);
const ImVec4 NODE_COLOR_HIT(0.9f, 0.8f, 0.2f, 1.f);
const ImVec4 NODE_COLOR_MULTIHIT(0.2f, 0.8f, 0.3f, 1.f);

void NodeBeamUtils::DrawMemoryTab(ActorPtr actor)
{
    ImGui::TextDisabled(_LC("NodeBeamUtils", "Beam counts per type:")); // see `BeamRangesByOrigin` and `Actor::ar_beam_ranges_by_origin`

    /*
    // beams at the start are hookbeams from 'nodes' with option 'h'
    int wheelbeams_start = 0; // '*wheels*'
    int unboundedbeams_start = 0; // 'beams' without flags 'r' or 's'
    int shock1beams_start = 0; // 'shocks'
    int shock2beams_start = 0; // 'shocks2'
    int shock3beams_start = 0; // 'shocks3'
    int commandbeams_start = 0; // 'commands*'
    int hydrobeams_start = 0; // 'hydros'
    int triggerbeams_start = 0; // 'triggers'
    int ropebeams_start = 0; // 'ropes' and 'beams' with 'r' flag - both have identical physics
    int supportbeams_start = 0; // 'beams' with 's' flag
    */

    BeamRangesByOrigin& r = actor->ar_beam_ranges_by_origin;
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Hookbeams"), r.wheelbeams_start);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Wheels"), r.unboundedbeams_start - r.wheelbeams_start);
    ImGui::SameLine();
    ImGui::PushID("wheel"); // To disambiguate the 'Visualize' buttons.
    if (ImGui::SmallButton(_LC("NodeBeamUtils","Visualize!")))
    {
        m_cache_paint.StartAnim(r.wheelbeams_start, r.unboundedbeams_start);
    }
    ImGui::PopID(); // "wheel"
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Unbounded"), r.shock1beams_start - r.unboundedbeams_start);
    ImGui::SameLine();
    ImGui::PushID("unbo"); // To disambiguate the 'Visualize' buttons.
    if (ImGui::SmallButton(_LC("NodeBeamUtils","Visualize!")))
    {
        m_cache_paint.StartAnim(r.unboundedbeams_start, r.shock1beams_start);
    }
    ImGui::PopID(); // "unbo"
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Shocks1"), r.shock2beams_start - r.shock1beams_start);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Shocks2"), r.shock3beams_start - r.shock2beams_start);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Shocks3"), r.commandbeams_start - r.shock3beams_start);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Commands"), r.hydrobeams_start - r.commandbeams_start);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Hydros"), r.triggerbeams_start - r.hydrobeams_start);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Triggers"), r.ropebeams_start - r.triggerbeams_start);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Ropes"), r.supportbeams_start - r.ropebeams_start);
    ImGui::Text("%s: %d", _LC("NodeBeamUtils", "Supports"), actor->ar_num_beams - r.supportbeams_start);

    ImGui::Separator();

    ImGui::TextDisabled(_LC("NodeBeamUtils", "Cache visualization settings:"));
    ImGui::SliderFloat(_LC("NodeBeamUtils", "Animation speed"), &m_cache_paint.anim_speed, 0.005f, 0.5f, "%.2f seconds per beam");
    ImGui::SliderInt(_LC("NodeBeamUtils", "Number of nodes in cache"), &m_cache_paint.nodecache_cap, 1, actor->ar_num_nodes);

    ImGui::TextDisabled(_LC("NodeBeamUtils", "Cache visualization stats:"));
    ImGui::TextColored(ImColor(NODE_COLOR_MISS), "%s: %d", _LC("NodeBeamUtils", "misses"), m_cache_paint.total_misses);
    ImGui::TextColored(ImColor(NODE_COLOR_HIT), "%s: %d", _LC("NodeBeamUtils", "hits"), m_cache_paint.total_hits);
    ImGui::SameLine();
    ImGui::TextColored(ImColor(NODE_COLOR_MULTIHIT), "(%s: %d)", _LC("NodeBeamUtils", "multihits"), m_cache_paint.total_multihits);
}

void CachePaint::UpdateAnim(const ActorPtr& actor, float dt)
{
    if (anim_running)
    {
        anim_total_time += dt;
        int projected_progress = std::min(beams_start + static_cast<int>(anim_total_time / anim_speed), beams_end);
        while (anim_curbeam < projected_progress)
        {
            anim_curbeam++;
            if (anim_curbeam == beams_end)
            {
                anim_running = false;
            }
            else
            {
                this->TouchNode(actor->ar_beams[anim_curbeam].p1num);
                this->TouchNode(actor->ar_beams[anim_curbeam].p2num);
            }
        }
    }

    // Var
    ImVec2 screen_size = ImGui::GetIO().DisplaySize;
    World2ScreenConverter world2screen(
        App::GetCameraManager()->GetCamera()->getViewMatrix(true), App::GetCameraManager()->GetCamera()->getProjectionMatrix(), Ogre::Vector2(screen_size.x, screen_size.y));

    ImDrawList* drawlist = GetImDummyFullscreenWindow();

    // Beams
    const beam_t* beams = actor->ar_beams;
    for (int i = beams_start; i < beams_end; ++i)
    {
        node_t& node_p1 = actor->ar_nodes[beams[i].p1num];
        node_t& node_p2 = actor->ar_nodes[beams[i].p2num];

        Ogre::Vector3 pos1 = world2screen.Convert(node_p1.AbsPosition);
        Ogre::Vector3 pos2 = world2screen.Convert(node_p2.AbsPosition);

        if ((pos1.z < 0.f) && (pos2.z < 0.f))
        {
            ImVec2 pos1xy(pos1.x, pos1.y);
            ImVec2 pos2xy(pos2.x, pos2.y);

            ImU32 color = (i <= anim_curbeam) ? ImColor(BEAM_COLOR_VISITED) : ImColor(BEAM_COLOR_NONVISITED);

            drawlist->AddLine(pos1xy, pos2xy, color, BEAM_THICKNESS);
            
        }
    }

    // Nodes
    const node_t* nodes = actor->ar_nodes;
    for (size_t i = 0; i < nodecache.size(); ++i)
    {
        const NodeNum_t n = nodecache[i];

        Ogre::Vector3 pos_xyz = world2screen.Convert(nodes[n].AbsPosition);

        if (pos_xyz.z < 0.f)
        {
            ImVec2 pos(pos_xyz.x, pos_xyz.y);
            ImU32 color = 0;
            switch(nodecache_hits[i])
            {
            case 0: color = ImColor(NODE_COLOR_MISS); break;
            case 1: color = ImColor(NODE_COLOR_HIT); break;
            default: color = ImColor(NODE_COLOR_MULTIHIT); break;
            }

            drawlist->AddCircleFilled(pos, NODE_RADIUS, color);
                
        }
    }
}
