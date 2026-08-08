/// \title Extcamera attr tweaking
/// \brief Demonstrates new smoothing & antijitter args of 'extcamera'.
/// \author ohlidalp 2024-2025, see https://github.com/RigsOfRods/rigs-of-rods/pull/3177

// Window [X] button handler
#include "imgui_utils.as"
imgui_utils::CloseWindowPrompt closeBtnHandler;



// #region frameStep

void frameStep(float dt)
{
    
    if (ImGui::Begin("Extcamera demo", closeBtnHandler.windowOpen, 0))
    {
        // Draw the "Terminate this script?" prompt on the top (if not disabled by config).
        closeBtnHandler.draw();
        
        
        
        // force minimum width
        ImGui::Dummy(vector2(450, 1));
        
        BeamClass@ playerVehicle = game.getCurrentTruck();
        if (@playerVehicle == null)
        {
            ImGui::Text("You are on foot.");
        }
        else
        {
            
            drawExtcameraDiagUI(playerVehicle);
            
        }
        
        ImGui::End();
    }
}
// #endregion

// #region UI drawing
void drawTableRow(string key, float val)
{
    ImGui::TextDisabled(key); ImGui::NextColumn(); ImGui::Text(formatFloat(val, "", 0, 3)); ImGui::NextColumn();
}

void drawTableRow(string key, int val)
{
    ImGui::TextDisabled(key); ImGui::NextColumn(); ImGui::Text(''+val); ImGui::NextColumn();
}

void drawTableRow(string key, bool val)
{
    ImGui::TextDisabled(key); ImGui::NextColumn(); ImGui::Text(val ? 'true' : 'false'); ImGui::NextColumn();
}



// Attributes can be edited realtime (on every keystroke) or using the [Focus] button.
// Focused editing means all other inputs are disabled and [Apply/Reset] buttons must be used.
ActorSimAttr gFocusedEditingAttr = ACTORSIMATTR_NONE;
float gFocusedEditingValue = 0.f;
float cfgAttrInputboxWidth = 125.f;
color cfgAttrUnfocusedBgColor = color(0.14, 0.14, 0.14, 1.0);
void drawAttrInputRow(BeamClass@ actor, ActorSimAttr attr, string label)
{
    ImGui::PushID(label);
    ImGui::TextDisabled(label);
    ImGui::NextColumn();
    if (gFocusedEditingAttr == ACTORSIMATTR_NONE)
    {
        // Focused editing inactive - draw [Focus] button
        float val = actor.getSimAttribute(attr);
        ImGui::SetNextItemWidth(cfgAttrInputboxWidth);
        if (ImGui::InputFloat("", val))
        {
            actor.setSimAttribute(attr, val);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Focus"))
        {
            gFocusedEditingAttr = attr;
            gFocusedEditingValue = val;
        }
    }
    else if (gFocusedEditingAttr == attr)
    {
        // This attr is focused - draw [Apply/Reset] buttons
        ImGui::SetNextItemWidth(cfgAttrInputboxWidth);
        ImGui::InputFloat("##"+label, gFocusedEditingValue);
        ImGui::SameLine();
        if (ImGui::Button("Apply"))
        {
            actor.setSimAttribute(attr, gFocusedEditingValue);
            gFocusedEditingAttr = ACTORSIMATTR_NONE;
            gFocusedEditingValue = 0.f;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset"))
        {
            gFocusedEditingAttr = ACTORSIMATTR_NONE;
            gFocusedEditingValue = 0.f;
        }        
    }
    else
    {
        // Some other attr is focused - just draw a label padded to size of inputbox.
        string valStr = "" + actor.getSimAttribute(attr);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, cfgAttrUnfocusedBgColor);
        ImGui::BeginChildFrame(uint(attr), vector2(cfgAttrInputboxWidth, ImGui::GetTextLineHeight()) + 3 * 2);
        ImGui::Text(valStr);
        ImGui::EndChildFrame(); // Must be called either way - inconsistent with other End*** funcs.
        ImGui::PopStyleColor(); // FrameBg
    }
    ImGui::NextColumn();
    ImGui::PopID(); // label
}

void drawAttributesCommonHelp()
{
    if (ImGui::CollapsingHeader("How to read and edit attributes:"))
    {
        ImGui::TextDisabled("Attributes can be edited realtime (on every keystroke) or using the [Focus] button.");
        ImGui::TextDisabled("Focused editing means all other inputs are disabled and [Apply/Reset] buttons must be used.");
        ImGui::Separator();
        ImGui::Separator();
    }
}

//#endregion

// #region Main window
void drawExtcameraDiagUI(BeamClass@ actor)
{
    drawAttributesCommonHelp();
    
    ImGui::Columns(2);
    
    drawAttrInputRow(actor, ACTORSIMATTR_EXTCAMERA_MODE, "EXTCAMERA_MODE (0/1/2)");
    ImGui::Separator();
    drawAttrInputRow(actor, ACTORSIMATTR_EXTCAMERA_NODE, "EXTCAMERA_NODE (for mode 2)");
    ImGui::Separator();
    drawAttrInputRow(actor, ACTORSIMATTR_EXTCAMERA_SMOOTHING, "EXTCAMERA_SMOOTHING (0.1 - 0.9)");
    ImGui::Separator();
    drawAttrInputRow(actor, ACTORSIMATTR_EXTCAMERA_ANTIJITTER, "EXTCAMERA_ANTIJITTER (meters)");
    ImGui::Separator();
    
    
    ImGui::Columns(1);
}
//#endregion


float fmax(float a, float b)
{
    return (a > b) ? a : b;
}

float fabs(float a)
{
    return a > 0.f ? a : -a;
}

float fclamp(float val, float minv, float maxv)
{
    return val < minv ? minv : (val > maxv) ? maxv : val;
}

float fexp(float val)
{
    const float eConstant = 2.71828183;
    return pow(eConstant, val);
}

