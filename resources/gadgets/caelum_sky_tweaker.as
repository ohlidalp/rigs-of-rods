/// \title Caelum Sky Tweaker
/// \brief Interactive tool to explore and tweak all Caelum sky system parameters
/// \author Based on engine_tool.as pattern, 2025

// Window [X] button handler
#include "imgui_utils.as"
imgui_utils::CloseWindowPrompt closeBtnHandler;

// #region Configuration and State

// Available Caelum components (sections)
const string SECTION_CAELUM_SYSTEM = "caelum_sky_system";
const string SECTION_POINT_STARFIELD = "point_starfield";
const string SECTION_SUN = "sun";
const string SECTION_MOON = "moon";
const string SECTION_GROUND_FOG = "ground_fog";
const string SECTION_PRECIPITATION = "precipitation";
const string SECTION_DEPTH_COMPOSER = "depth_composer";
const string SECTION_SKY_DOME = "sky_dome";
const string SECTION_VCLOUDS = "vclouds";

// Parameter editing state
string gFocusedSection = "";
string gFocusedParam = "";
array<float> gFocusedValues(4, 0.f);
array<float> gOriginalValues(4, 0.f);
bool gHasActiveFocus = false;

// UI Configuration
float cfgParamInputWidth = 100.f;
float cfgLabelWidth = 275.f;
color cfgUnfocusedBgColor = color(0.14, 0.14, 0.14, 1.0);
color cfgErrorColor = color(0.8, 0.2, 0.2, 1.0);
color cfgSuccessColor = color(0.2, 0.8, 0.2, 1.0);

// Parameter definitions for each component
enum NumComponents
{
    NUMCOMPONENTS_NONE = 0,
    NUMCOMPONENTS_FLOAT = 1,
    NUMCOMPONENTS_BOOL = -1,
    NUMCOMPONENTS_VEC2 = 2,
    NUMCOMPONENTS_VEC3 = 3,
    NUMCOMPONENTS_RGB = -3,
    NUMCOMPONENTS_VEC4 = 4,
    NUMCOMPONENTS_RGBA = -4
}
class ParamDef
{
    string name;
    string label;
    NumComponents numComponents;
    float minValue;
    float maxValue;
    string tooltip;
    
    ParamDef(string n, string l, /*NumComponents*/int nc, float minv, float maxv, string tt)
    {
        name = n;
        label = l;
        numComponents = NumComponents(nc);
        minValue = minv;
        maxValue = maxv;
        tooltip = tt;
    }
    
    bool isFocusable()
    {
        return numComponents > 0; // no focus for bools and RGB(A), just a checkbox or colorpicker
    }
}

// Parameter databases for each section
dictionary g_paramDefs;

void initializeParamDefs()
{
    // Caelum System parameters
    array<ParamDef@> caelumSystemParams = {
        ParamDef("julian_day", "Julian Day", 1, 0.0, 1000000.0, ""),
        ParamDef("latitude", "Latitude (degrees)", 1, -90.0, 90.0, ""),
        ParamDef("longitude", "Longitude (degrees)", 1, -180.0, 180.0, ""),
        ParamDef("global_fog_density_multiplier", "Global Fog Density", 1, 0.0, 10.0, ""),
        ParamDef("global_fog_colour_multiplier", "Global Fog Color", 4, 0.0, 2.0, ""),
        ParamDef("scene_fog_density_multiplier", "Scene Fog Density", 1, 0.0, 10.0, ""),
        ParamDef("scene_fog_colour_multiplier", "Scene Fog Color", 4, 0.0, 2.0, ""),
        ParamDef("ground_fog_density_multiplier", "Ground Fog Density", 1, 0.0, 10.0, ""),
        ParamDef("ground_fog_colour_multiplier", "Ground Fog Color", 4, 0.0, 2.0, ""),
        ParamDef("manage_ambient_light", "Manage Ambient Light", 1, 0.0, 1.0, ""),
        ParamDef("minimum_ambient_light", "Min Ambient Light", 4, 0.0, 1.0, ""),
        ParamDef("ensure_single_light_source", "Single Light Source", 1, 0.0, 1.0, ""),
        ParamDef("ensure_single_shadow_source", "Single Shadow Source", 1, 0.0, 1.0, "")
    };
    g_paramDefs[SECTION_CAELUM_SYSTEM] = caelumSystemParams;
    
    // Point Starfield parameters
    array<ParamDef@> starfieldParams = {
        ParamDef("magnitude_scale", "Magnitude Scale", 1, 0.0, 10.0, ""),
        ParamDef("mag0_pixel_size", "Mag0 Pixel Size", 1, 0.0, 20.0, ""),
        ParamDef("min_pixel_size", "Min Pixel Size", 1, 0.0, 10.0, ""),
        ParamDef("max_pixel_size", "Max Pixel Size", 1, 0.0, 20.0, ""),
        ParamDef("latitude", "Latitude (degrees)", 1, -90.0, 90.0, ""),
        ParamDef("longitude", "Longitude (degrees)", 1, -180.0, 180.0, "")
    };
    g_paramDefs[SECTION_POINT_STARFIELD] = starfieldParams;
    
    // Sun/Moon (BaseSkyLight) parameters
    array<ParamDef@> skyLightParams = {
        ParamDef("ambient_multiplier", "Ambient Multiplier", 4, 0.0, 5.0, ""),
        ParamDef("specular_multiplier", "Specular Multiplier", 4, 0.0, 5.0, ""),
        ParamDef("diffuse_multiplier", "Diffuse Multiplier", 4, 0.0, 5.0, ""),
        ParamDef("light_colour", "Light Color", 4, 0.0, 1.0, ""),
        ParamDef("body_colour", "Body Color", 4, 0.0, 1.0, ""),
        ParamDef("auto_disable_threshold", "Auto Disable Threshold", 1, 0.0, 1.0, ""),
        ParamDef("auto_disable", "Auto Disable", 1, 0.0, 1.0, "")
    };
    g_paramDefs[SECTION_SUN] = skyLightParams;
    g_paramDefs[SECTION_MOON] = skyLightParams;
    
    // Ground Fog parameters
    array<ParamDef@> groundFogParams = {
        ParamDef("density", "Density", 1, 0.0, 1.0, ""),
        ParamDef("vertical_decay", "Vertical Decay", 1, 0.0, 1.0, ""),
        ParamDef("ground_level", "Ground Level", 1, -1000.0, 1000.0, ""),
        ParamDef("colour", "Color", 4, 0.0, 1.0, "")
    };
    g_paramDefs[SECTION_GROUND_FOG] = groundFogParams;
    
    // Depth Composer parameters
    array<ParamDef@> depthComposerParams = {
        ParamDef("debug_depth_render", "Debug Depth Render", 1, 0.0, 1.0, ""),
        ParamDef("haze_enabled", "Haze Enabled", 1, 0.0, 1.0, ""),
        ParamDef("haze_colour", "Haze Color", 4, 0.0, 1.0, ""),
        ParamDef("ground_fog_enabled", "Ground Fog Enabled", 1, 0.0, 1.0, ""),
        ParamDef("ground_fog_density", "Ground Fog Density", 1, 0.0, 1.0, ""),
        ParamDef("ground_fog_vertical_decay", "Fog Vertical Decay", 1, 0.0, 1.0, ""),
        ParamDef("ground_fog_base_level", "Fog Base Level", 1, -1000.0, 1000.0, ""),
        ParamDef("ground_fog_colour", "Ground Fog Color", 4, 0.0, 1.0, "")
    };
    g_paramDefs[SECTION_DEPTH_COMPOSER] = depthComposerParams;
    
    // Sky Dome parameters
    array<ParamDef@> skyDomeParams = {
        ParamDef("haze_enabled", "Haze Enabled", NUMCOMPONENTS_BOOL, 0.0, 1.0, "Enable dome haze effect")
    };
    g_paramDefs[SECTION_SKY_DOME] = skyDomeParams;
    
    // Volumetric Clouds parameters
    array<ParamDef@> vcloudsParams = {
        // the clouds
        ParamDef("wind_speed", "Wind Speed", 1, 0.0, 100.0, ""),
        ParamDef("wind_direction", "Wind Direction (deg)", 1, 0.0, 6.28, "Wind direction in degrees (0=eastwards, then clockwise)"),
        ParamDef("vertical_bounds", "Vertical Bounds", 2, 0.0, 10000.0, "x=altitude, y=thickness"),
        ParamDef("ambient_color", "Ambient Color", NUMCOMPONENTS_RGB, 0.0, 1.0, ""),
        ParamDef("light_response", "Light Response", 4, 0.0, 5.0, ""),
        ParamDef("ambient_factors", "Ambient Factors", 4, 0.0, 5.0, ""),
        ParamDef("cloudiness", "Cloudiness", 2, 0.0, 1.0, ""),
        ParamDef("radius", "Radius", NUMCOMPONENTS_FLOAT, 10.0, 10000.0, ""),
        
        // lightnings
        ParamDef("enable_lightnings", "Enable Lightnings", NUMCOMPONENTS_BOOL, 0.0, 1.0, ""),
        ParamDef("average_lightning_appartition_time", "Lightning Interval", 1, 0.0, 60.0, ""),
        ParamDef("lightning_color", "Lightning Color", 3, 0.0, 1.0, ""),
        ParamDef("lightning_time_multiplier", "Lightning Duration", 1, 0.0, 10.0, "")
    };
    g_paramDefs[SECTION_VCLOUDS] = vcloudsParams;
}

// #endregion

// #region Parameter Access Helpers

bool getCaelumParam(string section, string param, array<float>@ outValues)
{
    float v1=0, v2=0, v3=0, v4=0;
    if (game.getCaelumParameter(section, param, v1, v2, v3, v4))
    {
        outValues[0] = v1;
        outValues[1] = v2;
        outValues[2] = v3;
        outValues[3] = v4;
        return true;
    }
    return false;
}

bool setCaelumParam(string section, string param, array<float>@ values)
{
    bool res= game.setCaelumParameter(section, param, values[0], values[1], values[2], values[3]);
if (!res) { game.message("Error setting Caelum parameter", "", 0.f,false); }
    return res;
}

// #endregion

// #region UI Drawing Helpers

void drawHelpHeader()
{
    if (ImGui::CollapsingHeader("How to use this tool:"))
    {
        ImGui::TextDisabled("This tool lets you explore and modify all Caelum sky system parameters in real-time.");
        ImGui::Dummy(vector2(10,5));
        ImGui::TextDisabled("REALTIME MODE: Edit values directly - changes apply on every keystroke.");
        ImGui::TextDisabled("FOCUSED MODE: Click [Focus] to lock editing. Use [Apply] to commit or [Reset] to cancel.");
        ImGui::Dummy(vector2(10,5));
        ImGui::TextDisabled("Parameter components:");
        ImGui::TextDisabled("  • 1 value = Scalar (float, bool, angle)");
        ImGui::TextDisabled("  • 2 values = Vector2 (X, Y)");
        ImGui::TextDisabled("  • 3 values = Vector3 or RGB color");
        ImGui::TextDisabled("  • 4 values = Vector4 or RGBA color");
        ImGui::Dummy(vector2(10,5));
        ImGui::PushStyleColor(ImGuiCol_Text, color(1.0, 0.8, 0.2, 1.0));
        ImGui::TextWrapped("TIP: Hover over parameter names to see tooltips with descriptions!");
        ImGui::PopStyleColor();
        ImGui::Separator();
    }
}

void drawParamInputRow(string section, ParamDef@ param)
{
    ImGui::PushID(section + "_" + param.name);
    
    array<float> currentValues(4, 0.f);
    bool getParamOk = getCaelumParam(section, param.name, currentValues);
    
    bool isFocused = (gHasActiveFocus && gFocusedSection == section && gFocusedParam == param.name);
    
    // Draw label with tooltip
    ImGui::TextDisabled(param.name); // the labels are AI slop, show internal name
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(param.tooltip);
        ImGui::Text("Parameter: " + param.name);
        ImGui::Text("Components: " + param.numComponents);
        ImGui::EndTooltip();
    }
    
    if (!gHasActiveFocus && param.isFocusable())
    {
        // not focusable
        ImGui::SameLine();
        if (ImGui::SmallButton("Focus"))
        {
            gFocusedSection = section;
            gFocusedParam = param.name;
            gHasActiveFocus = true;
            for (int i = 0; i < 4; i++)
            {
                gFocusedValues[i] = currentValues[i];
                gOriginalValues[i] = currentValues[i];
            }
        }
    }
    
    if (isFocused) 
    {
        ImGui::SameLine();
        if (ImGui::Button("Apply"))
        {
            if (setCaelumParam(section, param.name, gFocusedValues))
            {
                gHasActiveFocus = false;
            }
        }
        
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset"))
        {
            gHasActiveFocus = false;
        }
    }
    ImGui::NextColumn();
    
    
    bool otherIsFocused = (gHasActiveFocus && !isFocused);
    
    if (!gHasActiveFocus)
    {
        // REALTIME MODE - Direct editing
        if (getParamOk)
        {
            bool changed = false;
            
            
            for (int i = 0; i < param.numComponents; i++)
            {
                if (i > 0) ImGui::SameLine();
                
                float val = currentValues[i];
                string label = "##val" + i;
                                ImGui::SetNextItemWidth(cfgParamInputWidth);
                if (ImGui::InputFloat(label, val))
                {
                    val = fclamp(val, param.minValue, param.maxValue);
                    currentValues[i] = val;
                    changed = true;
                }
            }
            
            if (param.numComponents == NUMCOMPONENTS_BOOL)
            {
                bool val = currentValues[0] == 1.f;
                changed = ImGui::Checkbox("##bool", val);
                currentValues[0] = val ? 1.f : 0.f;
            }
            
            if (param.numComponents == NUMCOMPONENTS_RGB)
            {
                color val(currentValues[0],currentValues[1],currentValues[2],1);
                changed = ImGui::ColorEdit3("##rgb", val);
                currentValues[0] = val.r;
                currentValues[1] = val.g;
                currentValues[2] = val.b;
            }
            
            
            if (param.numComponents == NUMCOMPONENTS_RGBA)
            {
                color val(currentValues[0],currentValues[1],currentValues[2],currentValues[3]);
                changed = ImGui::ColorEdit4("##rgba", val);
                currentValues[0] = val.r;
                currentValues[1] = val.g;
                currentValues[2] = val.b;
                currentValues[3] = val.a;
            }
            
            if (changed)
            {
                setCaelumParam(section, param.name, currentValues);
            }
            
            
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, cfgErrorColor);
            ImGui::Text("[READ ERROR]");
            ImGui::PopStyleColor();
        }
    }
    else if (isFocused)
    {
        // FOCUSED MODE - This parameter is being edited
        
        for (int i = 0; i < param.numComponents; i++)
        {
            if (i > 0) ImGui::SameLine();
            string label = "##focused" + i;
            ImGui::SetNextItemWidth(cfgParamInputWidth);
            ImGui::InputFloat(label, gFocusedValues[i]);
            gFocusedValues[i] = fclamp(gFocusedValues[i], param.minValue, param.maxValue);
        }
        
        
    }
    else
    {
        // UNFOCUSED MODE - Another parameter is being edited
        getCaelumParam(section, param.name, currentValues);
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, cfgUnfocusedBgColor);
        ImGui::BeginChildFrame(42, vector2(cfgParamInputWidth * param.numComponents + 5 * (param.numComponents-1), ImGui::GetTextLineHeight() + 6));
        
        string valueStr = "";
        for (int i = 0; i < param.numComponents; i++)
        {
            if (i > 0) valueStr += ", ";
            valueStr += formatFloat(currentValues[i], "", 0, 3);
        }
        ImGui::Text(valueStr);
        
        ImGui::EndChildFrame();
        ImGui::PopStyleColor();
    }
    
    ImGui::NextColumn();
    ImGui::PopID();
}

void drawSectionTab(string section, string title)
{
    if (ImGui::BeginTabItem(title))
    {
        ImGui::Dummy(vector2(500, 1)); // Force minimum width
        
        if (!game.getCaelumAvailable())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, cfgErrorColor);
            ImGui::Text("Caelum sky system is not available!");
            ImGui::PopStyleColor();
            ImGui::EndTabItem();
            return;
        }
        
        // Get parameters for this section
        array<ParamDef@>@ params;
        if (!g_paramDefs.get(section, @params))
        {
            ImGui::Text("No parameters defined for this section.");
            ImGui::EndTabItem();
            return;
        }
        
        ImGui::Columns(2);
        ImGui::SetColumnOffset(1, cfgLabelWidth);
        
        for (uint i = 0; i < params.length(); i++)
        {
            drawParamInputRow(section, params[i]);
            if (i < params.length() - 1)
            {
                ImGui::Separator();
            }
        }
        
        ImGui::Columns(1);
        ImGui::EndTabItem();
    }
}

// #endregion

// #region Main Window

void drawMainWindow()
{
    if (ImGui::Begin("Caelum Sky Tweaker", closeBtnHandler.windowOpen, 0))
    {
        closeBtnHandler.draw();
        
        drawHelpHeader();
        
        float preTabBarCursorY = ImGui::GetCursorPosY();
        if (ImGui::BeginTabBar("caelumTweakerTabs"))
        {
            drawSectionTab(SECTION_CAELUM_SYSTEM, "Caelum System");
            drawSectionTab(SECTION_POINT_STARFIELD, "Starfield");
            drawSectionTab(SECTION_SUN, "Sun");
            drawSectionTab(SECTION_MOON, "Moon");
            /* FIXME: These components are disabled in the game
            drawSectionTab(SECTION_GROUND_FOG, "Ground Fog");
            drawSectionTab(SECTION_DEPTH_COMPOSER, "Depth Composer");
            */
            drawSectionTab(SECTION_SKY_DOME, "Sky Dome");
            drawSectionTab(SECTION_VCLOUDS, "Volumetric Clouds");
            
            ImGui::EndTabBar();
        }
        
        vector2 postTabBarCursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos(vector2(500, preTabBarCursorY));
        if (ImGui::SmallButton("reload sky"))
        {
        game.pushMessage(MSG_EDI_REINIT_SKY_REQUESTED, {});
        }
        ImGui::SetCursorPos(postTabBarCursor);
        
        ImGui::End();
    }
}

// #endregion

// #region frameStep

CVarClass@ g_mp_state = console.cVarFind("mp_state");
bool g_initialized = false;

void frameStep(float dt)
{
    if (!g_initialized)
    {
        initializeParamDefs();
        g_initialized = true;
    }
    
    drawMainWindow();
}

// #endregion

// #region Utility Functions

float fmax(float a, float b)
{
    return (a > b) ? a : b;
}

float fmin(float a, float b)
{
    return (a < b) ? a : b;
}

float fabs(float a)
{
    return a > 0.f ? a : -a;
}

float fclamp(float val, float minv, float maxv)
{
    return val < minv ? minv : (val > maxv) ? maxv : val;
}

int iabs(int val)
{
    return val < 0 ? -val : val;
}

// #endregion
