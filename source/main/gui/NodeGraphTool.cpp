

#include "NodeGraphTool.h"
#include "Beam.h" // aka 'the actor'
#include "BeamFactory.h"
#include "as_addon/scriptmath.h" // Part of codebase; located in "/source/main/scripting/as_addon"
#include "Euler.h" // from OGRE wiki


#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/prettywriter.h"


#include <map>
#include <sstream>
#include <unordered_map>
#include <angelscript.h>
#include <OgreMatrix3.h>

// ====================================================================================================================

static inline bool operator!=(const ImVec2& lhs, const ImVec2& rhs)              { return (lhs.x != rhs.x) && (lhs.y != rhs.y); }
static inline bool operator==(const ImVec2& lhs, const ImVec2& rhs)              { return (lhs.x == rhs.x) && (lhs.y == rhs.y); }

RoR::NodeGraphTool::NodeGraphTool():
    m_scroll(0.0f, 0.0f),
    m_scroll_offset(0.0f, 0.0f),
    m_mouse_resize_node(nullptr),
    m_mouse_arrange_node(nullptr),
    m_mouse_move_node(nullptr),
    m_link_mouse_src(nullptr),
    m_link_mouse_dst(nullptr),
    m_hovered_slot_node(nullptr),
    m_hovered_node(nullptr),
    m_context_menu_node(nullptr),
    m_header_mode(HeaderMode::NORMAL),
    m_hovered_slot_input(-1),
    m_hovered_slot_output(-1),
    m_free_id(0),
    m_fake_mouse_node(this, ImVec2()), // Used for dragging links with mouse
    m_mouse_arrange_show(false),
    m_shared_script_window_open(false),

    udp_position_node(this, ImVec2(-300.f, 100.f), "UDP position",     "(world XYZ)"),
    udp_velocity_node(this, ImVec2(-300.f, 200.f), "UDP velocity",     "(world XYZ)"),
    udp_accel_node   (this, ImVec2(-300.f, 300.f), "UDP acceleration", "(world XYZ)"),
    udp_orient_node  (this, ImVec2(-300.f, 400.f))
{
    memset(m_filename, 0, sizeof(m_filename));
    m_fake_mouse_node.id = MOUSEDRAG_NODE_ID;
    udp_position_node.id = UDP_POS_NODE_ID;
    udp_velocity_node.id = UDP_VELO_NODE_ID;
    udp_accel_node   .id = UDP_ACC_NODE_ID;
    udp_orient_node  .id = UDP_ANGLES_NODE_ID;
    memset(m_shared_script, 0, sizeof(m_shared_script));
}

RoR::NodeGraphTool::Link* RoR::NodeGraphTool::FindLinkByDestination(Node* node, const int slot)
{
    for (Link* link: m_links)
    {
        if (link->node_dst == node && link->slot_dst == slot)
            return link;
    }
    return nullptr;
}

RoR::NodeGraphTool::Style::Style()
{
    color_grid                = ImColor(200,200,200,40);
    grid_line_width           = 1.f;
    grid_size                 = 64.f;
    color_node                = ImColor(30,30,35);
    color_node_frame          = ImColor(100,100,100);
    color_node_frame_active   = ImColor(100,100,100);
    color_node_hovered        = ImColor(45,45,49);
    color_node_frame_hovered  = ImColor(125,125,125);
    node_rounding             = 4.f;
    node_window_padding       = ImVec2(8.f,8.f);
    slot_hoverbox_extent      = ImVec2(15.f, 10.f);
    color_input_slot          = ImColor(150,150,150,150);
    color_output_slot         = ImColor(150,150,150,150);
    color_input_slot_hover    = ImColor(144,155,222,245);
    color_output_slot_hover   = ImColor(144,155,222,245);
    node_slots_radius         = 5.f;
    color_link                = ImColor(200,200,100);
    color_link_hover          = ImColor( 88,222,188);
    link_line_width                 = 3.f;
    scaler_size                     = ImVec2(20, 20);
    display2d_rough_line_color      = ImColor(225,225,225,255);
    display2d_smooth_line_color     = ImColor(125,175,255,255);
    display2d_rough_line_width      = 1.f;
    display2d_smooth_line_width     = 1.f;
    display2d_grid_line_color       = ImColor(100,125,110,255);
    display2d_grid_line_width       = 1.2f;
    node_arrangebox_color           = ImColor(66,222,111,255);
    node_arrangebox_mouse_color     = ImColor(222,122,88,255);
    node_arrangebox_thickness       = 4.f;
    node_arrangebox_mouse_thickness = 3.2f;
    arrange_widget_color            = ImColor(22,175,75,255);
    arrange_widget_color_hover      = ImColor(111,255,148,255);
    arrange_widget_size             = ImVec2(20.f, 15.f);
    arrange_widget_margin           = ImVec2(5.f, 5.f);
    arrange_widget_thickness        = 2.f;
    node_arrangebox_inner_color     = ImColor(244,244,244,255);
    node_arrangebox_inner_thickness = 1.6f;
}

RoR::NodeGraphTool::Link* RoR::NodeGraphTool::FindLinkBySource(Node* node, const int slot)
{
    for (Link* link: m_links)
    {
        if (link->node_src == node && link->buff_src->slot == slot)
            return link;
    }
    return nullptr;
}

void RoR::NodeGraphTool::Draw(int net_send_state)
{
    // Create a window
    bool is_open = true;
    ImGui::Begin("MotionFeeder", &is_open);
    if (!is_open)
    {
        App::sim_motionfeeder_mode.SetPending(MotionFeederMode::HIDDEN);
    }

    // Debug outputs
    //ImGui::Text("MouseDrag - src: 0x%p, dst: 0x%p | mousenode - X:%.1f, Y:%.1f", m_link_mouse_src, m_link_mouse_dst, m_fake_mouse_node.pos.x, m_fake_mouse_node.pos.y);
    //ImGui::Text("SlotHover - node: 0x%p, input: %d, output: %d", m_hovered_slot_node, m_hovered_slot_input, m_hovered_slot_output);
    ImGui::SameLine();
    if (ImGui::Button("Open file"))
    {
        m_header_mode = HeaderMode::LOAD_FILE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save file"))
    {
        m_header_mode = HeaderMode::SAVE_FILE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear all"))
    {
        m_header_mode = HeaderMode::CLEAR_ALL;
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 25.f);
    ImGui::Text("Arrangement:");
    ImGui::SameLine();
    ImGui::Checkbox("Preview", &m_mouse_arrange_show);
    ImGui::SameLine();
    if (ImGui::Button("Lock!"))
    {
        App::sim_motionfeeder_mode.SetPending(MotionFeederMode::LOCKED);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset all"))
    {
        m_header_mode = HeaderMode::RESET_ARRANGE;
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 50.f);
    ImGui::Text("Scripting:");
    ImGui::SameLine();
    if (ImGui::Button("Edit shared"))
    {
        m_shared_script_window_open = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Upd. all nodes!"))
    {
        for (Node* n: m_nodes)
        {
            if (n->type == Node::Type::SCRIPT)
            {
                static_cast<ScriptNode*>(n)->Apply();
            }
        }
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 50.f);
    ImGui::Text("Net status: %s", (net_send_state >= 0) ? "OK" : "ERROR");

    if (m_header_mode != HeaderMode::NORMAL)
    {
        if (ImGui::Button("Cancel"))
        {
            m_header_mode = HeaderMode::NORMAL;
        }
        if ((m_header_mode == HeaderMode::CLEAR_ALL))
        {
            ImGui::SameLine();
            ImGui::Text("Really clear all?");
            ImGui::SameLine();
            if (ImGui::Button("Confirm"))
            {
                this->ClearAll();
                m_header_mode = HeaderMode::NORMAL;
            }
        }
        else if ((m_header_mode == HeaderMode::RESET_ARRANGE))
        {
            ImGui::SameLine();
            ImGui::Text("Really reset arrangement of all nodes?");
            ImGui::SameLine();
            if (ImGui::Button("Confirm"))
            {
                this->ResetAllArrangements();
                m_header_mode = HeaderMode::NORMAL;
            }
        }
        else
        {
            ImGui::SameLine();
            ImGui::InputText("Filename", m_filename, IM_ARRAYSIZE(m_filename));
            ImGui::SameLine();
            if ((m_header_mode == HeaderMode::SAVE_FILE) && ImGui::Button("Save now"))
            {
                this->SaveAsJson();
                m_header_mode = HeaderMode::NORMAL;
            }
            else if ((m_header_mode == HeaderMode::LOAD_FILE) && ImGui::Button("Load now"))
            {
                this->LoadFromJson();
                m_header_mode = HeaderMode::NORMAL;
            }
        }
    }

    // Scripting engine messages
    if (! m_messages.empty())
    {
        if (ImGui::CollapsingHeader("Messages"))
        {
            for (std::string& msg: m_messages)
            {
                ImGui::BulletText(msg.c_str());
            }
        }
        if (ImGui::Button("Clear messages"))
        {
            m_messages.clear();
        }
    }

    m_scroll_offset = ImGui::GetCursorScreenPos() - m_scroll;
    m_is_any_slot_hovered = false;

    this->DrawNodeGraphPane();

    ImGui::End(); // Finalize the window

    this->DrawNodeArrangementBoxes(); // Full display drawing

    if (m_shared_script_window_open)
    {
        ImGui::SetNextWindowSize(ImVec2(200.f, 400.f), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Shared script", &m_shared_script_window_open);
        const int flags = ImGuiInputTextFlags_AllowTabInput;
        ImVec2 input_size = ImGui::GetWindowSize()-ImVec2(40.f, 40.f); // dummy padding...whatever, no time!
        ImGui::InputTextMultiline("##shared_script", m_shared_script, IM_ARRAYSIZE(m_shared_script), input_size, flags );
        ImGui::End();
    }
}

void RoR::NodeGraphTool::PhysicsTick(Beam* actor)
{
    for (Node* node: m_nodes)
    {
        if (node->type == Node::Type::GENERATOR)
        {

            GeneratorNode* gen_node = static_cast<GeneratorNode*>(node);
            gen_node->elapsed_sec += PHYSICS_DT;

            float result = cosf((gen_node->elapsed_sec * 2.f) * 3.14f * gen_node->frequency) * gen_node->amplitude;

            // add noise
            if (gen_node->noise_max != 0)
            {
                int r = rand() % gen_node->noise_max;
                result += static_cast<float>((r*2)-r) * 0.1f;
            }

            // save to buffer
            gen_node->buffer_out.Push(result);
        }
        else if (node->type == Node::Type::READING)
        {
            ReadingNode* rnode = static_cast<ReadingNode*>(node);
            if (rnode->softbody_node_id >= 0)
            {
                const node_t& node = actor->nodes[rnode->softbody_node_id];
                rnode->PushPosition(node.AbsPosition);
                rnode->PushForces  (node.Forces);
                rnode->PushVelocity(node.Velocity);
            }
        }
    }
    this->CalcGraph();
}

ImDrawList* DummyWholeDisplayWindowBegin(const char* window_name) // Internal helper
{
    int window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar| ImGuiWindowFlags_NoInputs 
                     | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowFocus(); // Necessary to keep window drawn on top of others
    ImGui::Begin(window_name, NULL, ImGui::GetIO().DisplaySize, 0, window_flags);
    return ImGui::GetWindowDrawList();
}

void RoR::NodeGraphTool::DrawNodeArrangementBoxes()
{
    // Check if we should draw
    if (!m_mouse_arrange_show && (m_mouse_arrange_node == nullptr))
        return;

    ImDrawList* drawlist = DummyWholeDisplayWindowBegin("RoR/Nodegraph/arrange");
    ImGui::End();

    // Iterate nodes and draw boxes if applicable
    for (Node* node: m_nodes)
    {
        if (node->arranged_pos == Node::ARRANGE_DISABLED || node->arranged_pos == Node::ARRANGE_EMPTY)
            continue;

        // Outer rectangle
        const bool highlight = (m_mouse_arrange_show && (m_hovered_node == node)) || (m_mouse_arrange_node == node);
        float outer_thick = (highlight) ? m_style.node_arrangebox_thickness : m_style.node_arrangebox_mouse_thickness;
        ImU32 outer_color = (highlight) ? m_style.node_arrangebox_color     : m_style.node_arrangebox_mouse_color;
        drawlist->AddRect(node->arranged_pos, node->arranged_pos + node->user_size, outer_color, 0.f, -1, outer_thick);

        // Inner rectangle
        drawlist->AddRect(node->arranged_pos, node->arranged_pos + node->user_size,
            m_style.node_arrangebox_inner_color, 0.f, -1, m_style.node_arrangebox_inner_thickness);
    }
}

void RoR::NodeGraphTool::DrawGrid()
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->ChannelsSetCurrent(0); // background + curves
    const ImVec2 win_pos = ImGui::GetCursorScreenPos();
    const ImVec2 offset = ImGui::GetCursorScreenPos() - m_scroll;
    const ImVec2 canvasSize = ImGui::GetWindowSize();

    for (float x = fmodf(offset.x,m_style.grid_size); x < canvasSize.x; x += m_style.grid_size)
        draw_list->AddLine(ImVec2(x,0.0f)+win_pos, ImVec2(x,canvasSize.y)+win_pos, m_style.color_grid, m_style.grid_line_width);
    for (float y = fmodf(offset.y,m_style.grid_size); y < canvasSize.y; y += m_style.grid_size)
        draw_list->AddLine(ImVec2(0.0f,y)+win_pos, ImVec2(canvasSize.x,y)+win_pos, m_style.color_grid, m_style.grid_line_width);
}

void RoR::NodeGraphTool::DrawLockedMode()
{
    ImDrawList* drawlist = DummyWholeDisplayWindowBegin("RoR/Nodegraph/locked");
    drawlist->ChannelsSplit(2); // 0= backgrounds, 1=items.

    for (Node* node: m_nodes) // Iterate nodes and draw contents if applicable
    {
        if (node->arranged_pos == Node::ARRANGE_DISABLED || node->arranged_pos == Node::ARRANGE_EMPTY 
            || (node->type != Node::Type::DISPLAY && node->type != Node::Type::DISPLAY_2D && node->type != Node::Type::DISPLAY_NUM))
        {
            continue;
        }

        node->DrawLockedMode();
    }
    drawlist->ChannelsMerge();
    ImGui::End();
}

bool RoR::NodeGraphTool::ClipTest(ImRect r)
{
    return ImGui::GetCurrentWindow()->Rect().Overlaps(r);
}

bool RoR::NodeGraphTool::ClipTestNode(Node* n)
{
    n->draw_rect_min = m_scroll_offset + n->pos;
    // NOTE: We're using value from previous update; `calc_size` is updated by DrawNodeFinalize(); --> We must add a safety minimum size in case the node was never displayed before
    return this->ClipTest(ImRect(n->draw_rect_min, n->draw_rect_min + n->calc_size + ImVec2(50.f,25.f)));
}

void RoR::NodeGraphTool::DrawLink(Link* link)
{
    // Perform clipping test
    ImVec2 p1 = m_scroll_offset + link->node_src->GetOutputSlotPos(link->buff_src->slot);
    ImVec2 p2 = m_scroll_offset + link->node_dst->GetInputSlotPos(link->slot_dst);
    ImRect window = ImGui::GetCurrentWindow()->Rect();
    if (!this->IsInside(window.Min, window.Max, p1) && !this->IsInside(window.Min, window.Max, p2)) // very basic clipping
        return;

    // Determine color
    ImU32 color = (link->node_dst == m_hovered_node || link->node_src == m_hovered_node) ? m_style.color_link_hover : m_style.color_link;

    // Draw curve
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->ChannelsSetCurrent(0); // background + curves
    float bezier_pt_dist = fmin(50.f, fmin(fabs(p1.x - p2.x)*0.75f, fabs(p1.y - p2.y)*0.75f)); // Maximum: 50; minimum: 75% of shorter-axis distance between p1 and p2
    draw_list->AddBezierCurve(p1, p1+ImVec2(+bezier_pt_dist,0), p2+ImVec2(-bezier_pt_dist,0), p2, color, m_style.link_line_width);
}

void RoR::NodeGraphTool::DrawSlotUni(Node* node, const int index, const bool input)
{
    ImDrawList* drawlist = ImGui::GetWindowDrawList();
    drawlist->ChannelsSetCurrent(2);
    ImVec2 slot_center_pos =  ((input) ? node->GetInputSlotPos(index) : (node->GetOutputSlotPos(index)));

    // Clip test
    ImVec2 clip_pos = slot_center_pos + m_scroll_offset;
    ImVec2 clip_min(clip_pos.x - m_style.node_slots_radius, clip_pos.y - m_style.node_slots_radius);
    ImVec2 clip_max(clip_pos.x + m_style.node_slots_radius, clip_pos.y + m_style.node_slots_radius);
    if (!this->ClipTest(ImRect(clip_min, clip_max)))
    {
        return;
    }

    ImGui::SetCursorScreenPos((slot_center_pos + m_scroll_offset) - m_style.slot_hoverbox_extent);
    ImU32 color = (input) ? m_style.color_input_slot : m_style.color_output_slot;

    DragType drag = this->DetermineActiveDragType();
    if (((drag == DragType::NONE) || (this->IsLinkDragInProgress() && ((drag == DragType::LINK_DST) == input)))
        && this->IsSlotHovered(slot_center_pos))
    {
        m_is_any_slot_hovered = true;
        m_hovered_slot_node = node;
        if (input)
            m_hovered_slot_input = static_cast<int>(index);
        else
            m_hovered_slot_output = static_cast<int>(index);
        color = (input) ? m_style.color_input_slot_hover : m_style.color_output_slot_hover;
        if (ImGui::IsMouseDragging(0, 0.f) && (this->DetermineActiveDragType() == DragType::NONE))
        {
            // Start link drag!
            Link* link = (input) ? this->FindLinkByDestination(node, index) : this->FindLinkBySource(node, index);
            if (link)
            {
                // drag existing link
                node->DetachLink(link);
                if (input)
                {
                    if (!m_fake_mouse_node.BindDst(link, 0))
                    {
                        this->AddMessage("DEBUG: NodeGraphTool::DrawSlotUni() failed to BindDst() link to fake_mouse_node");
                        // cancel drag = re-bind the link
                        if (input)
                            node->BindDst(link, index);
                        else
                            node->BindSrc(link, index);
                    }
                    else
                    {
                        m_link_mouse_dst = link;
                    }
                }
                else
                {
                    m_fake_mouse_node.BindSrc(link, 0);
                    m_link_mouse_src = link;
                }
            }
            else
            {
                // Create a new link
                if (input)
                {
                    m_link_mouse_src = this->AddLink(&m_fake_mouse_node, node, 0, index);
                }
                else
                {
                    m_link_mouse_dst = this->AddLink(node, &m_fake_mouse_node, index, 0);
                }
            }
        }
    }
    drawlist->AddCircleFilled(slot_center_pos+m_scroll_offset, m_style.node_slots_radius, color);
}

RoR::NodeGraphTool::Link* RoR::NodeGraphTool::AddLink(Node* src, Node* dst, int src_slot, int dst_slot)
{
    Link* link = new Link();
    if (!dst->BindDst(link, dst_slot))
    {
        this->AddMessage("DEBUG: NodeGraphTool::AddLink(): Failed to BindDst() to node %d", dst->id);
        delete link;
        return nullptr;
    }
    src->BindSrc(link, src_slot);
    m_links.push_back(link);
    return link;
}

void RoR::NodeGraphTool::DrawNodeBegin(Node* node)
{
    ImGui::PushID(node->id);
    // Draw content
    ImDrawList* drawlist = ImGui::GetWindowDrawList();
    drawlist->ChannelsSetCurrent(2);
    ImGui::SetCursorScreenPos(node->draw_rect_min + m_style.node_window_padding);
    ImGui::BeginGroup(); // Locks horizontal position
}

void RoR::NodeGraphTool::DrawNodeFinalize(Node* node)
{
    ImGui::EndGroup();
    node->calc_size = ImGui::GetItemRectSize() + (m_style.node_window_padding * 2.f);

    // Draw slots: 0 inputs, 3 outputs (XYZ)
    for (int i = 0; i<node->num_inputs; ++i)
        this->DrawInputSlot(node, i);
    for (int i = 0; i<node->num_outputs; ++i)
        this->DrawOutputSlot(node, i);

    // Handle mouse dragging
    bool is_hovered = false;
    bool start_mouse_drag = false;
    if (!m_is_any_slot_hovered && (this->DetermineActiveDragType() == DragType::NONE))
    {
        ImGui::SetCursorScreenPos(node->draw_rect_min);
        ImGui::InvisibleButton("node", node->calc_size);
            // NOTE: Using 'InvisibleButton' enables dragging by node body but not by contained widgets
            // NOTE: This MUST be done AFTER widgets are drawn, otherwise their input is blocked by the invis. button
        is_hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemActive())
        {
            start_mouse_drag = true;
        }
    }
    // Draw outline
    ImDrawList* drawlist = ImGui::GetWindowDrawList();
    drawlist->ChannelsSetCurrent(1);
    ImU32 bg_color = (is_hovered) ? m_style.color_node_hovered : m_style.color_node;
    ImU32 border_color = (is_hovered) ? m_style.color_node_frame_hovered : m_style.color_node_frame;
    ImVec2 draw_rect_max = node->draw_rect_min + node->calc_size;
    drawlist->AddRectFilled(node->draw_rect_min, draw_rect_max, bg_color, m_style.node_rounding);
    drawlist->AddRect(node->draw_rect_min, draw_rect_max, border_color, m_style.node_rounding);

    // Resizing
    if (node->is_scalable)
    {
        // Handle resize
        ImVec2 scaler_mouse_max = node->pos + node->calc_size;
        ImVec2 scaler_mouse_min = scaler_mouse_max - m_style.scaler_size;
        bool scaler_hover = false;
        if ((this->DetermineActiveDragType() == DragType::NONE) && this->IsInside(scaler_mouse_min, scaler_mouse_max, m_nodegraph_mouse_pos))
        {
            start_mouse_drag = false;
            scaler_hover = true;
            if (ImGui::IsMouseDragging(0, 0.f))
            {
                m_mouse_resize_node = node;
            }
        }

        // Draw
        ImColor scaler_color = ImGui::GetStyle().Colors[ImGuiCol_Button];
        if (scaler_hover || (node == m_mouse_resize_node))
        {
            scaler_color = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
        }
        drawlist->AddTriangleFilled(draw_rect_max,
                                    ImVec2(draw_rect_max.x, draw_rect_max.y - m_style.scaler_size.y),
                                    ImVec2(draw_rect_max.x - m_style.scaler_size.x, draw_rect_max.y),
                                    scaler_color);
    }

    // Handle arranging
    if (node->arranged_pos != Node::ARRANGE_DISABLED)
    {
        ImVec2 ara_mouse_min, ara_mouse_max;
        ara_mouse_max.x = (node->pos.x + node->calc_size.x) - m_style.arrange_widget_margin.x;
        ara_mouse_min.x = ara_mouse_max.x - m_style.arrange_widget_size.x;
        ara_mouse_min.y = node->pos.y + m_style.arrange_widget_margin.y;
        ara_mouse_max.y = ara_mouse_min.y + m_style.arrange_widget_size.y;
        const bool ara_hover = ((this->DetermineActiveDragType() == DragType::NONE) && this->IsInside(ara_mouse_min, ara_mouse_max, m_nodegraph_mouse_pos));
        if (ara_hover)
            start_mouse_drag = false;

        if (ara_hover && ImGui::IsMouseDragging(0, 0.f))
        {
            m_mouse_arrange_node = node;
            node->arranged_pos = node->pos + m_scroll_offset; // Initialize the screen position
            start_mouse_drag = false;
        }

        // Draw arrange widget
        const ImU32 ara_color = (ara_hover) ? m_style.arrange_widget_color_hover : m_style.arrange_widget_color;
        drawlist->AddRect(ara_mouse_min + m_scroll_offset, ara_mouse_max + m_scroll_offset, ara_color, 0.5f, -1, m_style.arrange_widget_thickness);
    }

    ImGui::PopID();

    if (start_mouse_drag)
        m_mouse_move_node = node;

    if (is_hovered)
        m_hovered_node = node;
}

void RoR::NodeGraphTool::DetachAndDeleteLink(Link* link)
{
    if (link->node_dst != nullptr)
        link->node_dst->DetachLink(link);
    if (link->node_src != nullptr)
        link->node_src->DetachLink(link);
    this->DeleteLink(link);
}

void RoR::NodeGraphTool::DeleteLink(Link* link)
{
    auto itor = m_links.begin();
    auto endi = m_links.end();
    for (; itor != endi; ++itor)
    {
        if (link == *itor)
        {
            delete link;
            m_links.erase(itor);
            return;
        }
    }
    this->Assert(false , "NodeGraphTool::DeleteLink(): stray link - not in list");
}

void RoR::NodeGraphTool::DrawNodeGraphPane()
{
    const bool draw_border = false;
    const int window_flags = ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::BeginChild("scroll-region", ImVec2(0,0), draw_border, window_flags))
        return; // Nothing more to do.

    const float baseNodeWidth = 120.f; // same as reference, but hardcoded
    float currentNodeWidth = baseNodeWidth;
    ImGui::PushItemWidth(currentNodeWidth);
    ImDrawList* drawlist = ImGui::GetWindowDrawList();
    drawlist->ChannelsSplit(3); // 0 = background (grid, curves); 1 = node rectangle/slots; 2 = node content

    // Update mouse drag
    m_nodegraph_mouse_pos = (ImGui::GetIO().MousePos - m_scroll_offset);
    const DragType active_drag = this->DetermineActiveDragType();
    if (ImGui::IsMouseDragging(0, 0.f) && this->IsLinkDragInProgress())
    {
        m_fake_mouse_node.pos = m_nodegraph_mouse_pos;
    }
    else // drag ended
    {
        if (m_link_mouse_src != nullptr)
        {
            if (m_hovered_slot_node != nullptr && m_hovered_slot_output != -1)
            {
                m_fake_mouse_node.DetachLink(m_link_mouse_src); // Detach from mouse node
                m_hovered_slot_node->BindSrc(m_link_mouse_src, m_hovered_slot_output); // Bind to target
            }
            else
            {
                this->DetachAndDeleteLink(m_link_mouse_src);
            }
            m_link_mouse_src = nullptr;
        }
        else if (m_link_mouse_dst != nullptr)
        {
            m_fake_mouse_node.DetachLink(m_link_mouse_dst); // Detach from mouse node
            const bool should_bind = (m_hovered_slot_node != nullptr && m_hovered_slot_input != -1);
            if (!should_bind || !m_hovered_slot_node->BindDst(m_link_mouse_dst, m_hovered_slot_input)) // Try binding to target
            {
                this->DetachAndDeleteLink(m_link_mouse_dst);
            }
            m_link_mouse_dst = nullptr;
        }
    }

    // Update node resize
    if (ImGui::IsMouseDragging(0, 0.f) && (active_drag == DragType::NODE_RESIZE))
    {
        m_mouse_resize_node->user_size += ImGui::GetIO().MouseDelta;
    }
    else // Resize ended
    {
        m_mouse_resize_node = nullptr;
    }

    // Update node screen arranging
    if (ImGui::IsMouseDragging(0, 0.f) && (active_drag == DragType::NODE_ARRANGE))
    {
        m_mouse_arrange_node->arranged_pos += ImGui::GetIO().MouseDelta;
    }
    else // Arranging ended
    {
        m_mouse_arrange_node = nullptr;
    }

    // Update node positioning
    if (ImGui::IsMouseDragging(0, 0.f) && (active_drag == DragType::NODE_MOVE))
    {
        m_mouse_move_node->pos += ImGui::GetIO().MouseDelta;
    }
    else // Move ended
    {
        m_mouse_move_node = nullptr;
    }

    // Draw grid
    this->DrawGrid();

    // DRAW LINKS

    drawlist->ChannelsSetCurrent(0);
    for (Link* link: m_links)
    {
        this->DrawLink(link);
    }

    // DRAW NODES
    m_hovered_node = nullptr;
    for (Node* node: m_nodes)
    {
        node->Draw();
    }

    // DRAW SPECIAL NODES
    udp_accel_node   .Draw();
    udp_velocity_node.Draw();
    udp_orient_node  .Draw();
    udp_position_node.Draw();

    // Slot hover cleanup
    if (!m_is_any_slot_hovered)
    {
        m_hovered_slot_node = nullptr;
        m_hovered_slot_input = -1;
        m_hovered_slot_output = -1;
    }

    // Open context menu
    if (ImGui::IsMouseHoveringWindow() && ImGui::IsMouseClicked(1))
    {
        if (!ImGui::IsAnyItemHovered())
        {
            ImGui::OpenPopup("context_menu");
            m_context_menu_node = nullptr;
        }
        else if (m_hovered_node != nullptr)
        {
            m_context_menu_node = m_hovered_node;
            ImGui::OpenPopup("context_menu");
        }
    }

    // Draw context menu
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,8));
    if (ImGui::BeginPopup("context_menu"))
    {
        ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - m_scroll_offset;
        if (m_context_menu_node != nullptr)
        {
            if (m_context_menu_node == &udp_accel_node ||
                m_context_menu_node == &udp_velocity_node ||
                m_context_menu_node == &udp_position_node ||
                m_context_menu_node == &udp_orient_node 
                )
            {
                ImGui::Text("UDP node:");
                ImGui::Text("~ no actions ~");
            }
            else
            {
                ImGui::Text("Existing node:");
                if (ImGui::MenuItem("Delete"))
                {
                    this->DetachAndDeleteNode(m_context_menu_node);
                    m_context_menu_node = nullptr;
                }
            }
        }
        else
        {
            ImGui::Text("-- Create new node --");
            if (ImGui::MenuItem("Reading"))           { m_nodes.push_back(new ReadingNode        (this, scene_pos)); }
            if (ImGui::MenuItem("Generator"))         { m_nodes.push_back(new GeneratorNode      (this, scene_pos)); }
            if (ImGui::MenuItem("Display (plot)"))    { m_nodes.push_back(new DisplayPlotNode    (this, scene_pos)); }
            if (ImGui::MenuItem("Display (number)"))  { m_nodes.push_back(new DisplayNumberNode  (this, scene_pos)); }
            if (ImGui::MenuItem("Display (2D)"))      { m_nodes.push_back(new Display2DNode      (this, scene_pos)); }
            if (ImGui::MenuItem("Script"))            { m_nodes.push_back(new ScriptNode         (this, scene_pos)); }
            ImGui::Text("-- Fetch UDP node --");
            if (ImGui::MenuItem("Position")) { udp_position_node.pos = scene_pos; }
            if (ImGui::MenuItem("Velocity")) { udp_velocity_node.pos = scene_pos; }
            if (ImGui::MenuItem("Accel."))   { udp_accel_node.pos = scene_pos; }
            if (ImGui::MenuItem("Rotation")) { udp_orient_node.pos = scene_pos; }
        }
        ImGui::EndPopup();
    }
    else
    {
        m_context_menu_node = nullptr;
    }
    ImGui::PopStyleVar();

    // Scrolling
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(2, 0.0f))
    {
        m_scroll = m_scroll - ImGui::GetIO().MouseDelta;
    }

    ImGui::EndChild();
    drawlist->ChannelsMerge();
}

RoR::NodeGraphTool::DragType RoR::NodeGraphTool::DetermineActiveDragType()
{
    if      (m_link_mouse_src != nullptr)     { return DragType::LINK_SRC;     }
    else if (m_link_mouse_dst != nullptr)     { return DragType::LINK_DST;     }
    else if (m_mouse_resize_node != nullptr)  { return DragType::NODE_RESIZE;  }
    else if (m_mouse_arrange_node != nullptr) { return DragType::NODE_ARRANGE; }
    else if (m_mouse_move_node != nullptr)    { return DragType::NODE_MOVE;    }

    return DragType::NONE;
}

void RoR::NodeGraphTool::CalcGraph()
{
    // Reset states
    for (Node* n: m_nodes)
    {
        n->done = false;
    }

    bool all_done = true;
    do
    {
        all_done = true;
        for (Node* n: m_nodes)
        {
            if (! n->done)
            {
                all_done &= n->Process();
            }
        }
    }
    while (!all_done);
}

void RoR::NodeGraphTool::ScriptMessageCallback(const AngelScript::asSMessageInfo *msg, void *param)
{
    const char *type = "error  ";
    if( msg->type == AngelScript::asMSGTYPE_WARNING )
        type = "warning";
    else if( msg->type == AngelScript::asMSGTYPE_INFORMATION )
        type = "info   ";

    this->AddMessage("[Script %s] %s (line: %d, pos: %d)", type, msg->message, msg->row, msg->col);
}

void RoR::NodeGraphTool::AddMessage(const char* format, ...)
{
    char buffer[2000] = {};

    va_list args;
    va_start(args, format);
        vsprintf(buffer, format, args);
    va_end(args);

    m_messages.push_back(buffer);
}

void RoR::NodeGraphTool::NodeToJson(rapidjson::Value& j_data, Node* node, rapidjson::Document& doc)
{
    j_data.AddMember("pos_x",           node->pos.x,           doc.GetAllocator());
    j_data.AddMember("pos_y",           node->pos.y,           doc.GetAllocator());
    j_data.AddMember("arranged_pos_x",  node->arranged_pos.x,  doc.GetAllocator());
    j_data.AddMember("arranged_pos_y",  node->arranged_pos.y,  doc.GetAllocator());
    j_data.AddMember("user_size_x",     node->user_size.x,     doc.GetAllocator());
    j_data.AddMember("user_size_y",     node->user_size.y,     doc.GetAllocator());
    j_data.AddMember("id",              node->id,              doc.GetAllocator());
    j_data.AddMember("type_id",         static_cast<int>(node->type),  doc.GetAllocator());
}

void RoR::NodeGraphTool::JsonToNode(Node* node, const rapidjson::Value& j_object)
{
    node->pos          = ImVec2(j_object["pos_x"]         .GetFloat(), j_object["pos_y"]         .GetFloat());
    node->user_size    = ImVec2(j_object["user_size_x"]   .GetFloat(), j_object["user_size_y"]   .GetFloat());
    node->id           = j_object["id"].GetInt();
    if (j_object.HasMember("arranged_pos_x") && j_object.HasMember("arranged_pos_y"))
    {
        node->arranged_pos = ImVec2(j_object["arranged_pos_x"].GetFloat(), j_object["arranged_pos_y"].GetFloat());
    }
    this->UpdateFreeId(node->id);
}

void RoR::NodeGraphTool::SaveAsJson()
{
    rapidjson::Document doc(rapidjson::kObjectType);
    auto& j_alloc = doc.GetAllocator();

    // EXPORT NODES

    rapidjson::Value j_nodes(rapidjson::kArrayType);
    for (Node* node: m_nodes)
    {
        rapidjson::Value j_data(rapidjson::kObjectType); // Common properties....
        this->NodeToJson(j_data, node, doc);

        switch (node->type) // Specifics...
        {
        case Node::Type::GENERATOR:
            j_data.AddMember("amplitude", static_cast<GeneratorNode*>(node)->amplitude, j_alloc);
            j_data.AddMember("frequency", static_cast<GeneratorNode*>(node)->frequency, j_alloc);
            j_data.AddMember("noise_max", static_cast<GeneratorNode*>(node)->noise_max, j_alloc);
            break;

        case Node::Type::SCRIPT:
            j_data.AddMember("source_code", rapidjson::StringRef(static_cast<ScriptNode*>(node)->code_buf), j_alloc);
            break;

        case Node::Type::READING:
            j_data.AddMember("softbody_node_id", static_cast<ReadingNode*>(node)->softbody_node_id, j_alloc); // Int
            break;

        case Node::Type::DISPLAY:
            j_data.AddMember("scale", static_cast<DisplayPlotNode*>(node)->plot_extent, j_alloc);
            break;

        case Node::Type::DISPLAY_2D:
            j_data.AddMember("zoom",      static_cast<Display2DNode*>(node)->zoom, j_alloc);
            j_data.AddMember("grid_size", static_cast<Display2DNode*>(node)->grid_size, j_alloc);
            break;

        default:
            break;

        } // end switch
        j_nodes.PushBack(j_data, j_alloc);
    }

    // EXPORT UDP NODES
    rapidjson::Value j_udp_pos   (rapidjson::kObjectType);
    rapidjson::Value j_udp_acc   (rapidjson::kObjectType);
    rapidjson::Value j_udp_orient(rapidjson::kObjectType);
    rapidjson::Value j_udp_velo  (rapidjson::kObjectType);

    this->NodeToJson(j_udp_pos,    &this->udp_position_node, doc);
    this->NodeToJson(j_udp_acc,    &this->udp_accel_node,    doc);
    this->NodeToJson(j_udp_orient, &this->udp_orient_node,   doc);
    this->NodeToJson(j_udp_velo,   &this->udp_velocity_node, doc);

    // EXPORT LINKS

    rapidjson::Value j_links(rapidjson::kArrayType);
    for (Link* link: m_links)
    {
        rapidjson::Value j_data(rapidjson::kObjectType);
        j_data.AddMember("node_src_id",  link->node_src->id,    j_alloc);
        j_data.AddMember("node_dst_id",  link->node_dst->id,    j_alloc);
        j_data.AddMember("slot_src",     link->buff_src->slot,  j_alloc);
        j_data.AddMember("slot_dst",     link->slot_dst,        j_alloc);
        j_links.PushBack(j_data, j_alloc);
    }

    // COMBINE

    doc.AddMember("nodes", j_nodes, j_alloc);
    doc.AddMember("links", j_links, j_alloc);
    doc.AddMember("udp_pos_node",    j_udp_pos   , j_alloc);
    doc.AddMember("udp_acc_node",    j_udp_acc   , j_alloc);
    doc.AddMember("udp_orient_node", j_udp_orient, j_alloc);
    doc.AddMember("udp_velo_node",   j_udp_velo  , j_alloc);
    doc.AddMember("shared_script", rapidjson::StringRef(m_shared_script), j_alloc);

    // SAVE FILE

    FILE* file = nullptr;
    errno_t fopen_result = 0;
#ifdef _WIN32
    // Binary mode recommended by RapidJSON tutorial: http://rapidjson.org/md_doc_stream.html#FileWriteStream
    fopen_result = fopen_s(&file, m_filename, "wb");
#else
    fopen_result = fopen_s(&file, m_filename, "w");
#endif
    if ((fopen_result != 0) || (file == nullptr))
    {
        std::stringstream msg;
        msg << "[RoR|RigEditor] Failed to save JSON project file (path: "<< m_filename << ")";
        if (fopen_result != 0)
        {
            msg<<" Tech details: function [fopen_s()] returned ["<<fopen_result<<"]";
        }
        this->AddMessage(msg.str().c_str());
        return; 
    }

    char* buffer = new char[100000]; // 100kb
    rapidjson::FileWriteStream j_out_stream(file, buffer, sizeof(buffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> j_writer(j_out_stream);
    doc.Accept(j_writer);
    fclose(file);
    delete buffer;
}

RoR::NodeGraphTool::Node* ResolveNodeByIdHelper(RoR::NodeGraphTool* graph,
                                                std::unordered_map<int, RoR::NodeGraphTool::Node*>& lookup,
                                                int node_id)
{
    switch (node_id)
    {
        case RoR::NodeGraphTool::UDP_POS_NODE_ID   : return &graph->udp_position_node; 
        case RoR::NodeGraphTool::UDP_VELO_NODE_ID  : return &graph->udp_velocity_node; 
        case RoR::NodeGraphTool::UDP_ACC_NODE_ID   : return &graph->udp_accel_node;    
        case RoR::NodeGraphTool::UDP_ANGLES_NODE_ID: return &graph->udp_orient_node;   
        default:
        {
            auto found_itor = lookup.find(node_id);
            if (found_itor != lookup.end())
                return found_itor->second;
        }
    }
    return nullptr;
}

void RoR::NodeGraphTool::LoadFromJson()
{
    this->ClearAll();

#ifdef _WIN32
    // Binary mode recommended by RapidJSON tutorial: http://rapidjson.org/md_doc_stream.html#FileReadStream
    FILE* fp = fopen(m_filename, "rb");
#else
    FILE* fp = fopen(m_filename, "r");
#endif
    if (fp == nullptr)
    {
        this->AddMessage("failed to open file '%s'", m_filename);
        return;
    }

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // IMPORT NODES
    std::unordered_map<int, Node*> lookup;

    if (d.HasMember("nodes") && d["nodes"].IsArray())
    {
        rapidjson::Value::ConstValueIterator itor = d["nodes"].Begin();
        rapidjson::Value::ConstValueIterator endi = d["nodes"].End();
        for (; itor != endi; ++itor)
        {
            Node::Type type = static_cast<Node::Type>((*itor)["type_id"].GetInt());
            Node* node = nullptr;
            switch(type)
            {
            case Node::Type::DISPLAY:
            {
                DisplayPlotNode* dnode = new DisplayPlotNode  (this, ImVec2());
                dnode->plot_extent = (*itor)["scale"].GetFloat();
                node = dnode;
                break;
            }
            case Node::Type::DISPLAY_2D:
            {
                Display2DNode* dnode = new Display2DNode  (this, ImVec2());
                dnode->zoom      = (*itor)["zoom"].GetFloat();
                dnode->grid_size = (*itor)["grid_size"].GetFloat();
                node = dnode;
                break;
            }
            case Node::Type::READING:
            {
                ReadingNode* rnode = new ReadingNode  (this, ImVec2());
                rnode->softbody_node_id = (*itor)["softbody_node_id"].GetInt();
                node = rnode;
                break;
            }
            case Node::Type::GENERATOR:
            {
                GeneratorNode* gnode = new GeneratorNode(this, ImVec2());
                gnode->amplitude = (*itor)["amplitude"].GetFloat();
                gnode->frequency = (*itor)["frequency"].GetFloat();
                gnode->noise_max = (*itor)["noise_max"].GetInt();
                node = gnode;
                break;
            }
            case Node::Type::SCRIPT:
            {
                ScriptNode* gnode = new ScriptNode(this, ImVec2());
                strncpy(gnode->code_buf, (*itor)["source_code"].GetString(), IM_ARRAYSIZE(gnode->code_buf));
                node = gnode;
                break;
            }
            case Node::Type::DISPLAY_NUM:
            {
                node = new DisplayNumberNode(this, ImVec2());
                break;
            }
            //case Node::Type::UDP: // special, saved separately
            //case Node::Type::ORIENT_UDP: // special, saved separately
            }
            if (node != nullptr)
            {
                this->JsonToNode(node, *itor);
                lookup.insert(std::make_pair(node->id, node));
                m_nodes.push_back(node);
            }
        }
    }
    else this->Assert(false, "LoadFromJson(): No 'nodes' array in JSON");

    // IMPORT special UDP nodes

    this->JsonToNode(&this->udp_position_node, d["udp_pos_node"]);
    this->JsonToNode(&this->udp_accel_node,    d["udp_acc_node"]);
    this->JsonToNode(&this->udp_orient_node,   d["udp_orient_node"]);
    this->JsonToNode(&this->udp_velocity_node, d["udp_velo_node"]);

    // IMPORT SHARED SCRIPT SOURCECODE

    if (d["shared_script"].IsString())
    {
        strncpy(m_shared_script, d["shared_script"].GetString(), sizeof(m_shared_script));
    }

    // IMPORT LINKS

    if (d.HasMember("links") && d["links"].IsArray())
    {
        rapidjson::Value::ConstValueIterator l_itor = d["links"].Begin();
        rapidjson::Value::ConstValueIterator l_endi = d["links"].End();
        for (; l_itor != l_endi; ++l_itor)
        {
            Link* link = new Link();
            int src_id = (*l_itor)["node_src_id"].GetInt();
            int dst_id = (*l_itor)["node_dst_id"].GetInt();
            Node* src_node = ResolveNodeByIdHelper(this, lookup, src_id);
            Node* dst_node = ResolveNodeByIdHelper(this, lookup, dst_id);

            if (src_node == nullptr)
            {
                this->AddMessage("JSON load error: failed to resolve link src node %d", src_id);
                delete link;
                continue;
            }
            if (dst_node == nullptr)
            {
                this->AddMessage("JSON load error: failed to resolve link dst node %d", dst_id);
                delete link;
                continue;
            }
            src_node->BindSrc(link, (*l_itor)["slot_src"].GetInt());
            if (!dst_node->BindDst(link, (*l_itor)["slot_dst"].GetInt()))
            {
                this->AddMessage("JSON load FATAL ERROR: link %d --> %d failed to bind.",src_id, dst_id);
                delete link;
            }
            else
            {
                m_links.push_back(link);
            }
        }
    }
    else this->Assert(false, "LoadFromJson(): No 'links' array in JSON");
}

void RoR::NodeGraphTool::ClearAll()
{
    while (!m_links.empty())
        this->DetachAndDeleteLink(m_links.back());

    while (!m_nodes.empty())
        this->DetachAndDeleteNode(m_nodes.back());

}

void RoR::NodeGraphTool::ResetAllArrangements()
{
    for (Node* n: m_nodes)
    {
        if (n->arranged_pos != Node::ARRANGE_DISABLED)
        {
            n->arranged_pos = Node::ARRANGE_EMPTY;
        }
    }
}

template<typename N> void DeleteNodeFromVector(std::vector<N*>& vec, RoR::NodeGraphTool::Node* node)
{
    for (auto itor = vec.begin(); itor != vec.end(); ++itor)
    {
        if (*itor == node)
        {
            delete node;
            vec.erase(itor);
            return;
        }
    }
}

void RoR::NodeGraphTool::DeleteNode(Node* node)
{
    auto itor = m_nodes.begin();
    auto endi = m_nodes.end();
    for (; itor != endi; ++itor)
    {
        if (*itor == node)
        {
            m_nodes.erase(itor);
            delete node;
            return;
        }
    }
}

void RoR::NodeGraphTool::DetachAndDeleteNode(Node* node)
{
    // Disconnect inputs
    for (int i = 0; i<node->num_inputs; ++i)
    {
        Link* found_link = this->FindLinkByDestination(node, i);
        if (found_link != nullptr)
        {
            this->DetachAndDeleteLink(found_link);
        }
    }
    // Disconnect outputs
    for (int i=0; i< node->num_outputs; ++i)
    {
        Link* found_link = this->FindLinkBySource(node, i);
        if (found_link != nullptr)
        {
            this->DetachAndDeleteLink(found_link);
        }
    }
    // Erase the node
    this->DeleteNode(node);
}

const ImVec2 RoR::NodeGraphTool::Node::ARRANGE_EMPTY = ImVec2(-1.f, -1.f);    // Static member
const ImVec2 RoR::NodeGraphTool::Node::ARRANGE_DISABLED = ImVec2(-2.f, -2.f); // Static member

// -------------------------------- Buffer object -----------------------------------

void RoR::NodeGraphTool::Buffer::CopyKeepOffset(Buffer* src) // Copies source buffer as-is, including the offset; fastest
{
    memcpy(this->data, src->data, Buffer::SIZE*sizeof(float));
    this->offset = src->offset;
}

void RoR::NodeGraphTool::Buffer::CopyResetOffset(Buffer* src) // Copies source buffer with 0-offset
{
    int src_upper_size = (Buffer::SIZE - src->offset);
    memcpy(this->data, src->data + src->offset, src_upper_size*sizeof(float)); // Upper portion
    memcpy(this->data + src_upper_size, src->data, src->offset*sizeof(float)); // Lower portion
    this->offset = 0; // Reset offset
}

void RoR::NodeGraphTool::Buffer::CopyReverse(Buffer* src) // Copies source buffer, resets offset to 0 and reverts ordering (0=last, SIZE=first)
{
    Buffer tmp(-1);
    tmp.CopyResetOffset(src);
    int tmp_index = Buffer::SIZE - 1;
    for (int i = 0; i < Buffer::SIZE; ++i)
    {
        data[i] = tmp.data[tmp_index];
        --tmp_index;
    }
}

void RoR::NodeGraphTool::Buffer::Fill(const float* const src, int offset, int len) // offset: default=0; len: default=Buffer::SIZE
{
    memcpy(this->data + offset, src, len);
}

float RoR::NodeGraphTool::Buffer::Read(int offset_mod) const
{
    const int pos = ((offset-1) + offset_mod) % SIZE;
    const int pos_clamp = (pos < 0) ? (SIZE + pos) : pos;

    return data[pos_clamp];
}

// -------------------------------- Display2D node -----------------------------------

RoR::NodeGraphTool::Display2DNode::Display2DNode(NodeGraphTool* nodegraph, ImVec2 _pos):
    UserNode(nodegraph, Type::DISPLAY_2D, _pos),
    input_rough_x(nullptr),
    input_rough_y(nullptr),
    input_smooth_x(nullptr),
    input_smooth_y(nullptr),
    input_scroll_x(nullptr),
    input_scroll_y(nullptr),
    zoom(1.5f),
    grid_size(10.f)
{
    num_outputs = 0;
    num_inputs = 6;
    user_size = ImVec2(200.f, 200.f);
    done = false; // Irrelevant for this node type - no outputs
    is_scalable = true;
    arranged_pos = Node::ARRANGE_EMPTY; // Enables arranging
}

void RoR::NodeGraphTool::Display2DNode::DetachLink(Link* link)
{
    graph->Assert (link->node_src != this, "Display2DNode::DetachLink() discrepancy - this node has no outputs!");
    if (link->node_dst != this)
    {
        graph->Assert (false, "Display2DNode::DetachLink() called with unrelated link");
        return;
    }

    //  Resolve link    -----     Detach link                             --------   Detach node    ------    done
    if (link == input_rough_x ) { link->node_dst = nullptr; link->slot_dst = -1;    input_rough_x  = nullptr; return; }
    if (link == input_rough_y ) { link->node_dst = nullptr; link->slot_dst = -1;    input_rough_y  = nullptr; return; }
    if (link == input_smooth_x) { link->node_dst = nullptr; link->slot_dst = -1;    input_smooth_x = nullptr; return; }
    if (link == input_smooth_y) { link->node_dst = nullptr; link->slot_dst = -1;    input_smooth_y = nullptr; return; }
    if (link == input_scroll_x) { link->node_dst = nullptr; link->slot_dst = -1;    input_scroll_x = nullptr; return; }
    if (link == input_scroll_y) { link->node_dst = nullptr; link->slot_dst = -1;    input_scroll_y = nullptr; return; }

    graph->Assert(false, "Display2DNode::DetachLink() discrepancy in link: node_dst attached, link_in not");
}

bool RoR::NodeGraphTool::Display2DNode::BindDstSingle(Link*& slot_ptr, int slot_index, Link* link)
{
    if (slot_ptr != nullptr)
        return false; // Occupied!

    slot_ptr = link;
    link->node_dst = this;
    link->slot_dst = slot_index;
    return true;
}

bool RoR::NodeGraphTool::Display2DNode::BindDst(Link* link, int slot)
{
    const bool slot_ok = (slot >= 0 && slot <= num_inputs);
    if (!slot_ok)
    {
        graph->Assert(false , "Display2DNode::BindDst() called with bad slot");
        return false;
    }

    switch (slot)
    {
        case 0: return this->BindDstSingle(input_rough_x , slot, link);
        case 1: return this->BindDstSingle(input_rough_y , slot, link);
        case 2: return this->BindDstSingle(input_smooth_x, slot, link);
        case 3: return this->BindDstSingle(input_smooth_y, slot, link);
        case 4: return this->BindDstSingle(input_scroll_x, slot, link);
        case 5: return this->BindDstSingle(input_scroll_y, slot, link);
    }
    return false;
}

#define TOOLTIP_NODE_DISPLAY_2D \
    "Inputs:\n" \
    "-------\n" \
    "rough X\nrough Y\n" \
    "smooth X\nsmooth Y\n" \
    "scroll X\nscroll Y (center)"

void RoR::NodeGraphTool::Display2DNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    graph->DrawNodeBegin(this);

    // --- Draw header ----
    ImGui::TextDisabled("<?>");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(TOOLTIP_NODE_DISPLAY_2D);
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    ImGui::Text("2D display");

    // ---- Create sub panel ----
    const int window_flags = ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollWithMouse;
    const bool draw_border = false;
    ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    ImGui::BeginChild("display-node-2D", this->user_size, draw_border, window_flags);

    if (graph->IsLinkAttached(input_scroll_x) && graph->IsLinkAttached(input_scroll_y) && this->zoom != 0)
    {
        ImDrawList* drawlist = ImGui::GetWindowDrawList();
        const ImVec2 canvas_screen_min    = ImGui::GetCursorScreenPos();
        const ImVec2 canvas_screen_max    = canvas_screen_min + this->user_size;
        const ImVec2 canvas_world_center  = ImVec2(input_scroll_x->buff_src->Read(), input_scroll_y->buff_src->Read());
        const ImVec2 canvas_world_min     = canvas_world_center - ((this->user_size / 2.f) / this->zoom);

        // --- Draw grid ----
        if (this->grid_size != 0)
        {
            const float grid_screen_spacing = this->grid_size * this->zoom;
            ImVec2 grid_screen_min(((this->grid_size - fmodf(canvas_world_center.x, this->grid_size)) * this->zoom),
                                   ((this->grid_size - fmodf(canvas_world_center.y, this->grid_size)) * this->zoom));
            if (grid_screen_min.x < 0.f)
                grid_screen_min.x += grid_screen_spacing;
            if (grid_screen_min.y < 0.f)
                grid_screen_min.y += grid_screen_spacing;
            grid_screen_min += canvas_screen_min;

            for (float x = grid_screen_min.x; x < canvas_screen_max.x; x += grid_screen_spacing)
            {
                drawlist->AddLine(ImVec2(x, canvas_screen_min.y),               ImVec2(x, canvas_screen_max.y),
                                  graph->m_style.display2d_grid_line_color,     graph->m_style.display2d_grid_line_width);
            }
            for (float y = grid_screen_min.y; y < canvas_screen_max.y; y += grid_screen_spacing)
            {
                drawlist->AddLine(ImVec2(canvas_screen_min.x, y),               ImVec2(canvas_screen_max.x, y),
                                  graph->m_style.display2d_grid_line_color,     graph->m_style.display2d_grid_line_width);
            }
        }

        if (graph->IsLinkAttached(input_rough_x) && graph->IsLinkAttached(input_rough_y))
        {
            this->DrawPath(input_rough_x->buff_src, input_rough_y->buff_src, graph->m_style.display2d_rough_line_width,
                           graph->m_style.display2d_rough_line_color, canvas_world_min, canvas_screen_min, canvas_screen_max);
        }

        if (graph->IsLinkAttached(input_smooth_x) && graph->IsLinkAttached(input_smooth_y))
        {
            this->DrawPath(input_smooth_x->buff_src, input_smooth_y->buff_src, graph->m_style.display2d_smooth_line_width,
                           graph->m_style.display2d_smooth_line_color, canvas_world_min, canvas_screen_min, canvas_screen_max);
        }

    }
    else
    {
        if (this->zoom == 0.f)
            ImGui::Text("~~ Invalid zoom! ~~");
        else
            ImGui::Text("~~ No scroll input! ~~");
    }

    // ---- Close sub panel ----
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::InputFloat("Zoom (px/m)", &this->zoom);
    ImGui::InputFloat("Grid size(m)", &this->grid_size);

    graph->DrawNodeFinalize(this);
}

void RoR::NodeGraphTool::Display2DNode::DrawLockedMode()
{
    // ## TODO: This is a copypaste of `Draw()` - refactor and unify!
    ImDrawList* drawlist = ImGui::GetWindowDrawList();

    // --- background ---
    drawlist->ChannelsSetCurrent(0);
    drawlist->AddRectFilled(this->arranged_pos, this->arranged_pos+this->user_size, ImColor(ImGui::GetStyle().Colors[ImGuiCol_FrameBg]));

    if (graph->IsLinkAttached(input_scroll_x) && graph->IsLinkAttached(input_scroll_y) && this->zoom != 0)
    {
        drawlist->ChannelsSetCurrent(1);
        const ImVec2 canvas_screen_min    = this->arranged_pos;
        const ImVec2 canvas_screen_max    = canvas_screen_min + this->user_size;
        const ImVec2 canvas_world_center  = ImVec2(input_scroll_x->buff_src->Read(), input_scroll_y->buff_src->Read());
        const ImVec2 canvas_world_min     = canvas_world_center - ((this->user_size / 2.f) / this->zoom);

        // --- Draw grid ----
        if (this->grid_size != 0)
        {
            const float grid_screen_spacing = this->grid_size * this->zoom;
            ImVec2 grid_screen_min(((this->grid_size - fmodf(canvas_world_center.x, this->grid_size)) * this->zoom),
                                   ((this->grid_size - fmodf(canvas_world_center.y, this->grid_size)) * this->zoom));
            if (grid_screen_min.x < 0.f)
                grid_screen_min.x += grid_screen_spacing;
            if (grid_screen_min.y < 0.f)
                grid_screen_min.y += grid_screen_spacing;
            grid_screen_min += canvas_screen_min;

            for (float x = grid_screen_min.x; x < canvas_screen_max.x; x += grid_screen_spacing)
            {
                drawlist->AddLine(ImVec2(x, canvas_screen_min.y),               ImVec2(x, canvas_screen_max.y),
                                  graph->m_style.display2d_grid_line_color,     graph->m_style.display2d_grid_line_width);
            }
            for (float y = grid_screen_min.y; y < canvas_screen_max.y; y += grid_screen_spacing)
            {
                drawlist->AddLine(ImVec2(canvas_screen_min.x, y),               ImVec2(canvas_screen_max.x, y),
                                  graph->m_style.display2d_grid_line_color,     graph->m_style.display2d_grid_line_width);
            }
        }

        if (graph->IsLinkAttached(input_rough_x) && graph->IsLinkAttached(input_rough_y))
        {
            this->DrawPath(input_rough_x->buff_src, input_rough_y->buff_src, graph->m_style.display2d_rough_line_width,
                           graph->m_style.display2d_rough_line_color, canvas_world_min, canvas_screen_min, canvas_screen_max);
        }

        if (graph->IsLinkAttached(input_smooth_x) && graph->IsLinkAttached(input_smooth_y))
        {
            this->DrawPath(input_smooth_x->buff_src, input_smooth_y->buff_src, graph->m_style.display2d_smooth_line_width,
                           graph->m_style.display2d_smooth_line_color, canvas_world_min, canvas_screen_min, canvas_screen_max);
        }

    }
    else
    {
        if (this->zoom == 0.f)
            ImGui::Text("~~ Invalid zoom! ~~");
        else
            ImGui::Text("~~ No scroll input! ~~");
    }

}

void RoR::NodeGraphTool::Display2DNode::DrawPath(Buffer* const buff_x, Buffer* const buff_y, float line_width, ImU32 color, ImVec2 canvas_world_min, ImVec2 canvas_screen_min, ImVec2 canvas_screen_max)
{
    ImDrawList* const drawlist = ImGui::GetWindowDrawList();
    Buffer buf_copy_x(-1), buf_copy_y(-1);
    buf_copy_x.CopyResetOffset(buff_x);
    buf_copy_y.CopyResetOffset(buff_y);
    for (int i = 1; i < Buffer::SIZE ; ++i) // Starts at 1 --> line is drawn from end to start
    {
        ImVec2 start(((buf_copy_x.data[i - 1] - canvas_world_min.x) * this->zoom),
                     ((buf_copy_y.data[i - 1] - canvas_world_min.y) * this->zoom));
        ImVec2 end(((buf_copy_x.data[i]     - canvas_world_min.x) * this->zoom),
                   ((buf_copy_y.data[i]     - canvas_world_min.y) * this->zoom));
        start += canvas_screen_min;
        end   += canvas_screen_min;
        if (NodeGraphTool::IsInside(canvas_screen_min, canvas_screen_max, start) &&
            NodeGraphTool::IsInside(canvas_screen_min, canvas_screen_max, end)) // Clipping test
        {
            drawlist->AddLine(start, end, color, line_width);
        }
    }
}

// -------------------------------- DisplayNumber node -----------------------------------

RoR::NodeGraphTool::DisplayNumberNode::DisplayNumberNode(NodeGraphTool* nodegraph, ImVec2 _pos):
    UserNode(nodegraph, Type::DISPLAY_NUM, _pos), link_in(nullptr)
{
    num_outputs = 0;
    num_inputs = 1;
    user_size = ImVec2(100.f, 30.f); // Rough estimate
    done = false; // Irrelevant for this node type - no outputs
    arranged_pos = Node::ARRANGE_EMPTY; // Enables arranging
}

void RoR::NodeGraphTool::DisplayNumberNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    graph->DrawNodeBegin(this);

    if (graph->IsLinkAttached(link_in))
        ImGui::Text("%f", this->link_in->buff_src->Read());
    else
        ImGui::Text("~offline~");
    ImGui::Button("    ", ImVec2(user_size.x, 1.f)); // Spacer (hacky)

    graph->DrawNodeFinalize(this);
}

void RoR::NodeGraphTool::DisplayNumberNode::DrawLockedMode()
{
    ImGui::SetCursorPos(ImVec2(this->arranged_pos.x, this->arranged_pos.y));

    if (graph->IsLinkAttached(link_in))
        ImGui::Text("%f", this->link_in->buff_src->Read());
    else
        ImGui::Text("~offline~");
}

void RoR::NodeGraphTool::DisplayNumberNode::DetachLink(Link* link)
{
    graph->Assert (link->node_src != this, "DisplayNumberNode::DetachLink() discrepancy - this node has no outputs!");

    if (link->node_dst == this)
    {
        graph->Assert(this->link_in == link, "DisplayNumberNode::DetachLink() discrepancy in link: node_dst attached, link_in not");
        link->node_dst = nullptr;
        link->slot_dst = -1;
        link_in = nullptr;
    }
    else graph->Assert (false, "DisplayPlDisplayNumberNodeotNode::DetachLink() called with unrelated link");
}

bool RoR::NodeGraphTool::DisplayNumberNode::BindDst(Link* link, int slot)
{
    graph->Assert(slot == 0, "DisplayNumberNode::BindDst() called with bad slot");

    if ((slot == 0) && (link_in == nullptr))
    {
        link->node_dst = this;
        link->slot_dst = slot;
        link_in = link;
        return true;
    }
    return false;
}

// -------------------------------- DisplayPlot node -----------------------------------

RoR::NodeGraphTool::DisplayPlotNode::DisplayPlotNode(NodeGraphTool* nodegraph, ImVec2 _pos):
    UserNode(nodegraph, Type::DISPLAY, _pos), link_in(nullptr)
{
    num_outputs = 0;
    num_inputs = 1;
    user_size = ImVec2(250.f, 85.f);
    done = false; // Irrelevant for this node type - no outputs
    plot_extent = 1.5f;
    is_scalable = true;
    arranged_pos = Node::ARRANGE_EMPTY; // Enables arranging
}

static const float DUMMY_PLOT[] = {0,0,0,0,0};

void RoR::NodeGraphTool::DisplayPlotNode::DrawPlot()
{
    const float* data_ptr = DUMMY_PLOT;;
    int data_length = IM_ARRAYSIZE(DUMMY_PLOT);
    int data_offset = 0;
    int stride =  static_cast<int>(sizeof(float));
    const char* title = "~~ disconnected ~~";
    Buffer buf_rev(-1);
    if (graph->IsLinkAttached(this->link_in))
    {
        buf_rev.CopyResetOffset(this->link_in->buff_src);
        data_ptr    = buf_rev.data;
        stride      =  static_cast<int>(sizeof(float));
        title       = "";
        data_length = Buffer::SIZE;
    }
    ImGui::PlotLines("", data_ptr, data_length, data_offset, title, -this->plot_extent, this->plot_extent, this->user_size, stride);
}

void RoR::NodeGraphTool::DisplayPlotNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    graph->DrawNodeBegin(this);
    this->DrawPlot();

    ImGui::InputFloat("Scale", &this->plot_extent);

    graph->DrawNodeFinalize(this);
}

void RoR::NodeGraphTool::DisplayPlotNode::DrawLockedMode()
{
    ImGui::SetCursorPos(ImVec2(this->arranged_pos.x, this->arranged_pos.y));

    this->DrawPlot();
}

void RoR::NodeGraphTool::DisplayPlotNode::DetachLink(Link* link)
{
    graph->Assert (link->node_src != this, "DisplayPlotNode::DetachLink() discrepancy - this node has no outputs!");

    if (link->node_dst == this)
    {
        graph->Assert(this->link_in == link, "DisplayPlotNode::DetachLink() discrepancy in link: node_dst attached, link_in not");
        link->node_dst = nullptr;
        link->slot_dst = -1;
        link_in = nullptr;
    }
    else graph->Assert (false, "DisplayPlotNode::DetachLink() called with unrelated link");
}

bool RoR::NodeGraphTool::DisplayPlotNode::BindDst(Link* link, int slot)
{
    graph->Assert(slot == 0, "DisplayPlotNode::BindDst() called with bad slot");

    if ((slot == 0) && (link_in == nullptr))
    {
        link->node_dst = this;
        link->slot_dst = slot;
        link_in = link;
        return true;
    }
    return false;
}

// -------------------------------- UDP node -----------------------------------

RoR::NodeGraphTool::UdpNode::UdpNode(NodeGraphTool* nodegraph, ImVec2 _pos, const char* _title, const char* _desc):
    Node(nodegraph, Type::UDP, _pos)
{
    num_outputs = 0;
    num_inputs = 3;
    inputs[0]=nullptr;
    inputs[1]=nullptr;
    inputs[2]=nullptr;
    title = _title;
    desc = _desc;
}

void RoR::NodeGraphTool::UdpNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    graph->DrawNodeBegin(this);
    ImGui::Text(title);
    ImGui::Text(" ---------- ");
    ImGui::Text(desc);
    graph->DrawNodeFinalize(this);
}

void RoR::NodeGraphTool::UdpNode::DetachLink(Link* link)
{
    assert (link->node_src != this); // Check discrepancy - this node has no outputs!

    if (link->node_dst == this)
    {
        for (int i=0; i<num_inputs; ++i)
        {
            if (inputs[i] == link)
            {
                inputs[i] = nullptr;
                link->node_dst = nullptr;
                link->slot_dst = -1;
                return;
            }
            assert(false && "UdpNode::DetachLink(): Discrepancy! link points to node but node doesn't point to link");
        }
    }
}

bool RoR::NodeGraphTool::UdpNode::BindDst(Link* link, int slot)
{
    const bool slot_ok = (slot >= 0 && slot < num_inputs);
    if (slot_ok && inputs[slot] == nullptr)
    {
        inputs[slot] = link;
        link->node_dst = this;
        link->slot_dst = slot;
        return true;
    }
    else
    {
        this->graph->AddMessage("UdpNode::BindDst() called with bad slot");
        return false;
    }
}

// -------------------------------- UDP orientation node -----------------------------------

RoR::NodeGraphTool::UdpOrientNode::UdpOrientNode(NodeGraphTool* nodegraph, ImVec2 _pos):
    Node(nodegraph, Type::UDP_ORIENT, _pos),
    m_back_vector(Ogre::Vector3::ZERO),
    m_left_vector(Ogre::Vector3::ZERO),
    m_up_vector(Ogre::Vector3::ZERO),
    m_ogre_back_vec(Ogre::Vector3::ZERO),
    m_ogre_left_vec(Ogre::Vector3::ZERO),
    m_ogre_up_vec(Ogre::Vector3::ZERO)
{
    num_outputs = 0;
    num_inputs = 3;
    in_world_yaw  =nullptr;
    in_world_pitch=nullptr;
    in_world_roll =nullptr;
}

void RoR::NodeGraphTool::UdpOrientNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    graph->DrawNodeBegin(this);
    ImGui::Text(" Orietation UDP Node ");
    ImGui::Text(" ------------------- ");
    ImGui::Text("Inputs:              ");
    ImGui::Text(" * Yaw     | separate");
    ImGui::Text(" * Pitch   | world   ");
    ImGui::Text(" * Roll    | angles  ");
    // DEBUG
    ImGui::Text(" ------debug xyz (carth.RH)------- ");
    
    ImGui::Text(" Back axis: %10.3f  %10.3f  %10.3f", m_back_vector.x, m_back_vector.y, m_back_vector.z);
    ImGui::Text(" Left axis: %10.3f  %10.3f  %10.3f", m_left_vector.x, m_left_vector.y, m_left_vector.z);
    ImGui::Text(" Up axis:   %10.3f  %10.3f  %10.3f", m_up_vector.x,   m_up_vector.y,   m_up_vector.z);

    ImGui::Text(" ------debug xyz (OGRE)------- ");
    
    ImGui::Text(" Back axis: %10.3f  %10.3f  %10.3f", m_ogre_back_vec.x, m_ogre_back_vec.y, m_ogre_back_vec.z);
    ImGui::Text(" Left axis: %10.3f  %10.3f  %10.3f", m_ogre_left_vec.x, m_ogre_left_vec.y, m_ogre_left_vec.z);
    ImGui::Text(" Up axis:   %10.3f  %10.3f  %10.3f", m_ogre_up_vec.x,   m_ogre_up_vec.y,   m_ogre_up_vec.z);
    graph->DrawNodeFinalize(this);
}

void RoR::NodeGraphTool::UdpOrientNode::DetachLink(Link* link)
{
    if (link->node_src == this)
        this->graph->AddMessage(" DEBUG: discrepancy - this node has no outputs!");

    if (link->node_dst == this)
    {
        if (in_world_yaw == link)
        {
            if (link->slot_dst != 0) graph->AddMessage("UdpOrientNode::DetachLink()    DEBUG -- discrepancy -- link is attached to 'in_world_yaw' but slot ID is '%d'", link->slot_dst);
            in_world_yaw = nullptr;
            link->node_dst = nullptr;
            link->slot_dst = -1;
            return;
        }
        if (in_world_pitch == link)
        {
            if (link->slot_dst != 0) graph->AddMessage("UdpOrientNode::DetachLink()    DEBUG -- discrepancy -- link is attached to 'in_world_pitch' but slot ID is '%d'", link->slot_dst);
            in_world_pitch = nullptr;
            link->node_dst = nullptr;
            link->slot_dst = -1;
            return;
        }
        if (in_world_roll == link)
        {
            if (link->slot_dst != 0) graph->AddMessage("UdpOrientNode::DetachLink()    DEBUG -- discrepancy -- link is attached to 'in_world_roll' but slot ID is '%d'", link->slot_dst);
            in_world_roll = nullptr;
            link->node_dst = nullptr;
            link->slot_dst = -1;
            return;
        }
        // We shouldn't get here.
        this->graph->AddMessage("DEBUG -- UdpOrientNode::DetachLink(): Discrepancy! link points to node but node doesn't point to link");
    }
}

bool RoR::NodeGraphTool::UdpOrientNode::BindDstSingle(Link*& slot_ptr, int slot_index, Link* link)
{
    if (slot_ptr != nullptr)
        return false; // Occupied!

    slot_ptr = link;
    link->node_dst = this;
    link->slot_dst = slot_index;
    return true;
}

bool RoR::NodeGraphTool::UdpOrientNode::BindDst(Link* link, int slot)
{
    if (slot < 0 || slot > 2)
    {
        this->graph->AddMessage("DEBUG: UdpOrientNode::BindDst(): bad slot: %d", slot);
        return false;
    }

    if (slot == 0) { return this->BindDstSingle(in_world_yaw,   slot, link); }
    if (slot == 1) { return this->BindDstSingle(in_world_pitch, slot, link); }
    if (slot == 2) { return this->BindDstSingle(in_world_roll,  slot, link); }
}

Ogre::Vector3 RoR::NodeGraphTool::UdpOrientNode::CalcUdpOutput()
{
    if (!graph->IsLinkAttached(in_world_yaw) ||
        !graph->IsLinkAttached(in_world_pitch) ||
        !graph->IsLinkAttached(in_world_roll))
    {
        return Ogre::Vector3::ZERO;
    }

    Ogre::Euler euler(in_world_yaw->buff_src->Read(), in_world_pitch->buff_src->Read(), in_world_roll->buff_src->Read()); // Input = carthesian RH

    m_back_vector = euler.forward() * -1; // the roll axis, carthesian RH
    m_left_vector = euler.right() * -1;   // the pitch axis, carthesian RH
    m_up_vector   = euler.up();           // the yaw axis, carthesian RH

    m_ogre_back_vec = CoordsRhCarthesianToOgre(m_back_vector);
    m_ogre_left_vec = CoordsRhCarthesianToOgre(m_left_vector);
    m_ogre_up_vec   = CoordsRhCarthesianToOgre(m_up_vector);

    // Below is an exact reproduction of transformation done for the "proof of concept"

    Ogre::Matrix3 orient_mtx;
    orient_mtx.FromAxes(m_left_vector, m_up_vector, m_back_vector); // NOTE: totally swapped, args are (X, Y, Z)
    Ogre::Radian yaw, pitch, roll;
    orient_mtx.ToEulerAnglesYXZ(yaw, roll, pitch); // NOTE: totally swapped... Function args are(Y, P, R)
    Ogre::Vector3 output_vec;
    output_vec.x = pitch.valueRadians(); // Wrong again.
    output_vec.y = roll.valueRadians();
    output_vec.z = yaw.valueRadians();
    return output_vec;
}


// -------------------------------- Generator node -----------------------------------

RoR::NodeGraphTool::GeneratorNode::GeneratorNode(NodeGraphTool* _graph, ImVec2 _pos):
            UserNode(_graph, Type::GENERATOR, _pos), amplitude(1.f), frequency(1.f), noise_max(0), elapsed_sec(0.f), buffer_out(0)
{
    num_inputs = 0;
    num_outputs = 1;
    is_scalable = true;
    user_size = ImVec2(100.f, 50.f);
}

void RoR::NodeGraphTool::GeneratorNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    this->graph->DrawNodeBegin(this);

    ImGui::Text("Sine generator");

    // raw data display
    const float GRAPH_MINMAX(this->amplitude + 0.1f);
    ImGui::PlotLines("Raw",                   buffer_out.data,  buffer_out.SIZE,                     0, nullptr, -GRAPH_MINMAX, GRAPH_MINMAX, this->user_size);

    Buffer display_buf(-1);
    display_buf.CopyResetOffset(&buffer_out);
    //     PlotLines(const char* label, const float* values, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0,0), int stride = sizeof(float));
    ImGui::PlotLines("Out",                   display_buf.data,  buffer_out.SIZE,                     0, nullptr, -GRAPH_MINMAX, GRAPH_MINMAX, this->user_size);

    float freq = this->frequency;
    if (ImGui::InputFloat("Freq", &freq))
    {
        this->frequency = freq;
    }

    float ampl = this->amplitude;
    if (ImGui::InputFloat("Ampl", &ampl))
    {
        this->amplitude = ampl;
    }

    int noise = this->noise_max;
    if (ImGui::InputInt("Noise", &noise))
    {
        this->noise_max = noise;
    }

    this->graph->DrawNodeFinalize(this);
}

void RoR::NodeGraphTool::GeneratorNode::DetachLink(Link* link)
{
    assert(link->node_dst != this); // discrepancy - no inputs in this node

    if (link->node_src == this)
    {
        assert(link->buff_src == &this->buffer_out); // check discrepancy
        link->buff_src = nullptr;
        link->node_src = nullptr;
    }
}

void RoR::NodeGraphTool::GeneratorNode::BindSrc(Link* link, int slot)
{
    assert(slot == 0); // Check invalid input
    if (slot == 0)
    {
        link->node_src = this;
        link->buff_src = &buffer_out;
    }
}

// -------------------------------- Reading node -----------------------------------

RoR::NodeGraphTool::ReadingNode::ReadingNode(NodeGraphTool* _graph, ImVec2 _pos):
    UserNode(_graph, Type::READING, _pos),
    buffer_pos_x(0), buffer_pos_y(1), buffer_pos_z(2),
    buffer_forces_x(3), buffer_forces_y(4), buffer_forces_z(5),
    buffer_velo_x(6), buffer_velo_y(7), buffer_velo_z(8),
    softbody_node_id(-1)
{
    num_inputs = 0;
    num_outputs = 9;
}

void RoR::NodeGraphTool::ReadingNode::BindSrc(Link* link, int slot)
{
    switch (slot)
    {
    case 0:    link->buff_src = &buffer_pos_x;       link->node_src = this;     return;
    case 1:    link->buff_src = &buffer_pos_y;       link->node_src = this;     return;
    case 2:    link->buff_src = &buffer_pos_z;       link->node_src = this;     return;
    case 3:    link->buff_src = &buffer_forces_x;    link->node_src = this;     return;
    case 4:    link->buff_src = &buffer_forces_y;    link->node_src = this;     return;
    case 5:    link->buff_src = &buffer_forces_z;    link->node_src = this;     return;
    case 6:    link->buff_src = &buffer_velo_x;      link->node_src = this;     return;
    case 7:    link->buff_src = &buffer_velo_y;      link->node_src = this;     return;
    case 8:    link->buff_src = &buffer_velo_z;      link->node_src = this;     return;
    default: assert(false && "ReadingNode::BindSrc(): invalid slot index");
    }
}

void RoR::NodeGraphTool::ReadingNode::DetachLink(Link* link)
{
    assert(link->node_dst != this); // Check discrepancy - this node has no inputs

    if (link->node_src == this)
    {
        link->buff_src = nullptr;
        link->node_src = nullptr;
    }
}

void RoR::NodeGraphTool::ReadingNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    this->graph->DrawNodeBegin(this);
    ImGui::Text("SoftBody reading");
    ImGui::InputInt("Node", &softbody_node_id);
    ImGui::Text(" --- Outputs ---  "); // Filler text to make node tall enough for all the outputs :)
    ImGui::Text("           Pos XYZ");
    ImGui::Text("        Forces XYZ");
    ImGui::Text("      Velocity XYZ");
    this->graph->DrawNodeFinalize(this);
}

// -------------------------------- Script node -----------------------------------

const char* SCRIPTNODE_EXAMPLE_CODE =
    "// Static variables here"     "\n"
    ""                             "\n"
    "// Update func. (mandatory)"  "\n"
    "void step() {"                "\n"
    "    // Pass-thru"             "\n"
    "    Write(0,Read(0,0));"      "\n"
    "}";

RoR::NodeGraphTool::ScriptNode::ScriptNode(NodeGraphTool* _graph, ImVec2 _pos):
    UserNode(_graph, Type::SCRIPT, _pos), 
    script_func(nullptr), script_engine(nullptr), script_context(nullptr), enabled(false),
    outputs{{0},{1},{2},{3},{4},{5},{6},{7},{8}} // C++11 mandatory :)
{
    num_outputs = 9;
    num_inputs = 9;
    memset(code_buf, 0, sizeof(code_buf));
    sprintf(code_buf, SCRIPTNODE_EXAMPLE_CODE);
    memset(inputs, 0, sizeof(inputs));
    user_size = ImVec2(250, 200);
    snprintf(node_name, 10, "Node %d", id);
    this->InitScripting();
    is_scalable = true;
}

void RoR::NodeGraphTool::ScriptNode::InitScripting()
{
    script_engine = AngelScript::asCreateScriptEngine(ANGELSCRIPT_VERSION);
    if (script_engine == nullptr)
    {
        graph->AddMessage("%s: failed to create scripting engine", node_name);
        return;
    }

    AngelScript::RegisterScriptMath(script_engine);

    int result = script_engine->SetMessageCallback(AngelScript::asMETHOD(NodeGraphTool, ScriptMessageCallback), graph, AngelScript::asCALL_THISCALL);
    if (result < 0)
    {
        graph->AddMessage("%s: failed to register message callback function, res: %d", node_name, result);
        return;
    }

    result = script_engine->RegisterGlobalFunction("void Write(int, float)", AngelScript::asMETHOD(RoR::NodeGraphTool::ScriptNode, Write), AngelScript::asCALL_THISCALL_ASGLOBAL, this);
    if (result < 0)
    {
        graph->AddMessage("%s: failed to register function `Write`, res: %d", node_name, result);
        return;
    }

    result = script_engine->RegisterGlobalFunction("float Read(int, int)", AngelScript::asMETHOD(RoR::NodeGraphTool::ScriptNode, Read), AngelScript::asCALL_THISCALL_ASGLOBAL, this);
    if (result < 0)
    {
        graph->AddMessage("%s: failed to register function `Read`, res: %d", node_name, result);
        return;
    }
}

void RoR::NodeGraphTool::ScriptNode::Apply()
{
    AngelScript::asIScriptModule* module = script_engine->GetModule(nullptr, AngelScript::asGM_ALWAYS_CREATE);
    if (module == nullptr)
    {
        graph->AddMessage("%s: Failed to create module", node_name);
        module->Discard();
        return;
    }

    int result = module->AddScriptSection("local_body", code_buf, strlen(code_buf));
    if (result < 0)
    {
        graph->AddMessage("%s: failed to `AddScriptSection() for local sourcecode`, res: %d", node_name, result);
        module->Discard();
        return;
    }

    result = module->AddScriptSection("shared_body", graph->m_shared_script, strlen(graph->m_shared_script));
    if (result < 0)
    {
        graph->AddMessage("%s: failed to `AddScriptSection() for shared sourcecode`, res: %d", node_name, result);
        module->Discard();
        return;
    }

    result = module->Build();
    if (result < 0)
    {
        graph->AddMessage("%s: failed to compile the script.", node_name); // Details provided by script via message callback fn.
        module->Discard();
        return;
    }

    script_func = module->GetFunctionByDecl("void step()");
    if (script_func == nullptr)
    {
        graph->AddMessage("%s: failed to `GetFunctionByDecl()`", node_name);
        module->Discard();
        return;
    }

    script_context = script_engine->CreateContext();
    if (script_context == nullptr)
    {
        graph->AddMessage("%s: failed to `CreateContext()`", node_name);
        module->Discard();
        return;
    }

    enabled = true;
}

        // Script functions
float RoR::NodeGraphTool::ScriptNode::Read(int slot, int offset_mod)
{
    if (slot < 0 || slot > (num_inputs - 1) || !graph->IsLinkAttached(inputs[slot]))
        return 0.f;

    return inputs[slot]->buff_src->Read(offset_mod);
}

void RoR::NodeGraphTool::ScriptNode::Write(int slot, float val)
{
    if (slot < 0 || slot > (num_inputs - 1))
        return;
    this->outputs[slot].Push(val);
}

bool RoR::NodeGraphTool::ScriptNode::Process()
{
    if (! enabled)
    {
        this->done = true;
        return true;
    }

    bool ready = true; // If completely disconnected, we're good to go. Otherwise, all inputs must be ready.
    for (int i=0; i<num_inputs; ++i)
    {
        if ((graph->IsLinkAttached(inputs[i])) && (! inputs[i]->node_src->done))
            ready = false;
    }

    if (! ready)
        return false;

    int prep_result = script_context->Prepare(script_func);
    if (prep_result < 0)
    {
        graph->AddMessage("%s: failed to `Prepare()`, res: %d", node_name, prep_result);
        script_engine->ReturnContext(script_context);
        script_context = nullptr;
        enabled = false;
        done = true;
        return true;
    }

    int exec_result = script_context->Execute();
    if (exec_result != AngelScript::asEXECUTION_FINISHED)
    {
        graph->AddMessage("%s: failed to `Execute()`, res: %d", node_name, exec_result);
        script_engine->ReturnContext(script_context);
        script_context = nullptr;
        enabled = false;
        done = true;
    }

    done = true;
    return true;
}

void RoR::NodeGraphTool::ScriptNode::BindSrc(Link* link, int slot)
{
    assert(slot >= 0 && slot < num_outputs); // Check input
    assert(link != nullptr); // Check input
    link->node_src = this;
    link->buff_src = &outputs[slot];
}

bool RoR::NodeGraphTool::ScriptNode::BindDst(Link* link, int slot)
{
    const bool slot_ok = (slot >= 0 && slot < num_inputs);
    if (!slot_ok)
    {
        this->graph->AddMessage("ScriptNode::BindDst(): bad slot number");
        return false;
    }

    if (inputs[slot] == nullptr)
    {
        inputs[slot] = link;
        link->node_dst = this;
        link->slot_dst = slot;
        return true;
    }
    return false;
}

void RoR::NodeGraphTool::ScriptNode::DetachLink(Link* link)
{
    if (link->node_dst == this)
    {
        graph->Assert(inputs[link->slot_dst] == link, "ScriptNode::DetachLink(): Discrepancy: inputs[link->slot_dst] != link "); // Check discrepancy
        inputs[link->slot_dst] = nullptr;
        link->node_dst = nullptr;
        link->slot_dst = -1;
    }
    else if (link->node_src == this)
    {
        graph->Assert((link->buff_src != nullptr), "ScriptNode::DetachLink(): Discrepancy, buff_src is NULL"); // Check discrepancy
        link->buff_src = nullptr;
        link->node_src = nullptr;
    }
    else graph->Assert(false,"ScriptNode::DetachLink() called on unrelated node");
}

void RoR::NodeGraphTool::ScriptNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    graph->DrawNodeBegin(this);
    const int flags = ImGuiInputTextFlags_AllowTabInput;
    const ImVec2 size = this->user_size;
    ImGui::Text((this->enabled)? "Enabled" : "Disabled");
    ImGui::SameLine();
    if (ImGui::SmallButton("Update"))
    {
        this->Apply();
    }
    ImGui::InputTextMultiline("##source", this->code_buf, IM_ARRAYSIZE(this->code_buf), size, flags);
    graph->DrawNodeFinalize(this);
}

// -------------------------------- MouseDragNode -----------------------------------

RoR::NodeGraphTool::MouseDragNode::MouseDragNode(NodeGraphTool* _graph, ImVec2 _pos):
    Node(_graph, Type::MOUSE, _pos), buffer_out(0), link_in(nullptr)
{
    num_inputs = 1;
    num_outputs = 1;
    user_size.x = 200.f;
}

void RoR::NodeGraphTool::MouseDragNode::Draw()
{
    if (!graph->ClipTestNode(this))
        return;
    graph->DrawNodeBegin(this);
    ImGui::PushItemWidth(this->user_size.x);
    ImGui::Text("Transform");
    ImGui::Text("No options");
    graph->DrawNodeFinalize(this);
}

void RoR::NodeGraphTool::MouseDragNode::DetachLink(Link* link)
{
    if (link->node_dst == this)
    {
        if (this->link_in != link)
        link->node_dst = nullptr;
        link->slot_dst = -1;
        this->link_in = nullptr;
    }
    else if (link->node_src == this)
    {
        assert(&this->buffer_out == link->buff_src); // Check discrepancy
        link->node_src = nullptr;
        link->buff_src = nullptr;
    }
    else assert(false && "MouseDragNode::DetachLink() called on unrelated node");
}
