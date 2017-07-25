/*
    This source file is part of Rigs of Rods
    Copyright 2013-2017 Petr Ohlidal & contributors

    For more information, see http://www.rigsofrods.com/

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

/** 
    @file   RigEditor_Main.cpp
    @date   06/2014
    @author Petr Ohlidal
*/

#include "RigEditor_Main.h"

#include "Application.h"
#include "CacheSystem.h"
#include "ContentManager.h"
#include "GlobalEnvironment.h"
#include "GUI_RigEditorBeamsPanel.h"
#include "GUI_RigEditorCommands2Panel.h"
#include "GUI_RigEditorDeleteMenu.h"
#include "GUI_RigEditorFlexBodyWheelsPanel.h"
#include "GUI_RigEditorHelpWindow.h"
#include "GUI_RigEditorHydrosPanel.h"
#include "GUI_RigEditorLandVehiclePropertiesWindow.h"
#include "GUI_RigEditorMenubar.h"
#include "GUI_RigEditorMeshWheels2Panel.h"
#include "GUI_RigEditorNodePanel.h"
#include "GUI_RigEditorRigPropertiesWindow.h"
#include "GUI_RigEditorShocksPanel.h"
#include "GUI_RigEditorShocks2Panel.h"
#include "GUIManager.h"
#include "InputEngine.h"
#include "OgreSubsystem.h"
#include "RigDef_Parser.h"
#include "RigDef_Serializer.h"
#include "RigDef_Validator.h"
#include "RigEditor_CameraHandler.h"
#include "RigEditor_Config.h"
#include "RigEditor_InputHandler.h"
#include "RigEditor_LandVehicleWheel.h"
#include "RigEditor_Node.h"
#include "RigEditor_Rig.h"
#include "RigEditor_RigProperties.h"
#include "RigEditor_RigElementsAggregateData.h"
#include "RigEditor_RigWheelsAggregateData.h"
#include "Settings.h"

#include <OISKeyboard.h>
#include <OgreEntity.h>
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreMovableObject.h>
#include <OgreRoot.h>
#include <OgreRenderWindow.h>
#include <sstream>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

using namespace RoR;
using namespace RoR::RigEditor;

const MyGUI::UString Main::OpenSaveFileDialogMode::MODE_OPEN_TRUCK     ("RigEditor_OpenTruckFile");
const MyGUI::UString Main::OpenSaveFileDialogMode::MODE_SAVE_TRUCK_AS  ("RigEditor_SaveTruckFileAs");
const MyGUI::UString Main::OpenSaveFileDialogMode::MODE_OPEN_JSON      ("RigEditor_OpenJsonProject");
const MyGUI::UString Main::OpenSaveFileDialogMode::MODE_SAVE_JSON_AS   ("RigEditor_SaveJsonProjectAs");

Main::Main():
    m_scene_manager(nullptr),
    m_camera(nullptr),
    m_viewport(nullptr),
    m_rig_entity(nullptr),
    m_input_handler(nullptr),
    m_rig(nullptr),
    m_state_flags(0),
    m_gui_help_visible(false),
    m_gui_active_left_panel(GuiLeftSidePanel::NONE)
{
    // Load resources
    RoR::App::GetContentManager()->AddResourcePack(ContentManager::ResourcePack::RIG_EDITOR);
    Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup("RigEditor");

    // Load config file
    GStr<300> cfg_path;
    cfg_path << App::sys_config_dir.GetActive() << PATH_SLASH << "rig_editor.cfg";
    m_config = new RigEditor::Config(cfg_path.ToCStr());

    /* Setup 3D engine */
    OgreSubsystem* ror_ogre_subsystem = App::GetOgreSubsystem();
    assert(ror_ogre_subsystem != nullptr);
    m_scene_manager = ror_ogre_subsystem->GetOgreRoot()->createSceneManager(Ogre::ST_GENERIC, "rig_editor_scene_manager");
    m_scene_manager->setAmbientLight(m_config->scene_ambient_light_color);

    /* Camera */
    m_camera = m_scene_manager->createCamera("rig_editor_camera");
    m_camera->setNearClipDistance(m_config->camera_near_clip_distance);
    m_camera->setFarClipDistance(m_config->camera_far_clip_distance);
    m_camera->setFOVy(Ogre::Degree(m_config->camera_FOVy_degrees));
    m_camera->setAutoAspectRatio(true);

    /* Setup input */
    m_input_handler = new InputHandler();

    /* Camera handling */
    m_camera_handler = new CameraHandler(m_camera);
    m_camera_handler->SetOrbitTarget(m_scene_manager->getRootSceneNode());	
    m_camera_handler->SetOrthoZoomRatio(m_config->ortho_camera_zoom_ratio);
    m_camera->setPosition(Ogre::Vector3(10,5,10));
}

Main::~Main()
{
    // GUI objects are cleaned up by std::unique_ptr

    if (m_rig != nullptr)
    {
        m_rig->DetachFromScene();
        delete m_rig;
        m_rig = nullptr;
    }
}

void Main::EnterEditorLoop()
{
    Ogre::RenderWindow* const window     = App::GetOgreSubsystem()->GetRenderWindow();
    Ogre::Root* const         ogre_root  = App::GetOgreSubsystem()->GetOgreRoot();
    Ogre::Timer timer; // TODO: Last time I checked, this timer was very slow, research and replace ~ ony_a_ptr, 07/2017

    while (App::app_state.GetPending() == RoR::AppState::RIG_EDITOR)
    {
        float elapsed_sec = timer.getMilliseconds() * 1000.f;
        timer.reset();
        App::GetGuiManager()->NewImGuiFrame(elapsed_sec);
        this->UpdateEditorLoop();

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32 || OGRE_PLATFORM == OGRE_PLATFORM_LINUX
        RoRWindowEventUtilities::messagePump();
#endif

        if (window->isClosed())
        {
            App::app_state.SetPending(RoR::AppState::SHUTDOWN);
            break;
        }

        ogre_root->renderOneFrame();

        if (!window->isActive() && window->isVisible())
        {
            window->update(); // update even when in background !
        }
    }
}

void Main::BringUp()
{
    // Setup 3D engine
    OgreSubsystem* ror_ogre_subsystem = App::GetOgreSubsystem();
    assert(ror_ogre_subsystem != nullptr);
    m_viewport = ror_ogre_subsystem->GetRenderWindow()->addViewport(nullptr);
    int viewport_width = m_viewport->getActualWidth();
    m_viewport->setBackgroundColour(m_config->viewport_background_color);
    m_camera->setAspectRatio(m_viewport->getActualHeight() / viewport_width);
    m_viewport->setCamera(m_camera);

    // Setup GUI
    this->InitializeOrRestoreGui();
    App::GetGuiManager()->GetImGui().StartRendering(m_scene_manager);

    // Setup input
    App::GetInputEngine()->SetKeyboardListener(m_input_handler);
    App::GetInputEngine()->SetMouseListener(m_input_handler);
}

void Main::PutOff()
{
    // Hide GUI
    if (m_gui_open_save_file_dialog->isModal())
    {
        m_gui_open_save_file_dialog->endModal(); // Hides the dialog
    }
    m_gui_delete_menu->Hide();
    App::GetGuiManager()->GetImGui().StopRendering();

    // Supress node/beam panels (if visible)
    m_nodes_panel    ->HideTemporarily();
    m_beams_panel    ->HideTemporarily();
    m_hydros_panel   ->HideTemporarily();
    m_commands2_panel->HideTemporarily();
    m_shocks_panel   ->HideTemporarily();
    m_shocks2_panel  ->HideTemporarily();
    m_meshwheels2_panel     ->HideTemporarily();
    m_flexbodywheels_panel  ->HideTemporarily();
}

void Main::UpdateEditorLoop()
{
    // Update input events
    m_input_handler->ResetEvents();
    App::GetInputEngine()->Capture(); // Also injects input to GUI (through RigEditor::InputHandler)
    this->DrawGui();

    // Handle key presses
    bool camera_ortho_toggled = false;
    bool camera_view_changed = false;
    if (m_input_handler->WasEventFired(InputHandler::Event::CAMERA_VIEW_TOGGLE_PERSPECTIVE))
    {
        m_camera_handler->ToggleOrtho();
        camera_ortho_toggled = true;
    }

    // Orientation:
    // Front->Back = X axis
    // Right->Left = Z axis
    // Top->Down   = Y axis negative
    // Inspired by:
    // * Gavril MZ2
    // * Tatra-T813-Dakar.truck

    OIS::Keyboard* ois_keyboard = App::GetInputEngine()->GetOisKeyboard();
    bool ctrl_is_down = ois_keyboard->isKeyDown(OIS::KC_RCONTROL) || ois_keyboard->isKeyDown(OIS::KC_LCONTROL);
    if (m_input_handler->WasEventFired(InputHandler::Event::CAMERA_VIEW_FRONT))
    {
        m_camera_handler->LookInDirection(ctrl_is_down ? Ogre::Vector3::NEGATIVE_UNIT_X : Ogre::Vector3::UNIT_X);
        camera_view_changed = true;
    }
    if (m_input_handler->WasEventFired(InputHandler::Event::CAMERA_VIEW_SIDE))
    {
        m_camera_handler->LookInDirection(ctrl_is_down ? Ogre::Vector3::UNIT_Z : Ogre::Vector3::NEGATIVE_UNIT_Z);
        camera_view_changed = true;
    }
    if (m_input_handler->WasEventFired(InputHandler::Event::CAMERA_VIEW_TOP))
    {
        m_camera_handler->TopView(! ctrl_is_down);
        camera_view_changed = true;
    }

    /* Handle camera control */
    
    if (m_input_handler->GetMouseMotionEvent().HasMoved() || m_input_handler->GetMouseMotionEvent().HasScrolled())
    {
        bool res = m_camera_handler->InjectMouseMove(
            m_input_handler->GetMouseButtonEvent().IsRightButtonDown(), /* (bool do_orbit) */
            m_input_handler->GetMouseMotionEvent().rel_x,
            m_input_handler->GetMouseMotionEvent().rel_y,
            m_input_handler->GetMouseMotionEvent().rel_wheel
        );
        if (res)
        {
            camera_view_changed = true;
        }
    }

    // RIG MANIPULATION \\

    if (m_rig != nullptr)
    {
        bool node_selection_changed = false;
        bool node_hover_changed = false;
        bool node_mouse_selecting_disabled = m_gui_delete_menu->IsVisible();// || m_gui_rig_properties_window->IsVisible();
        bool rig_updated = false;
        bool all_nodes_selection_toggled = false;
        bool rig_must_deselect_all_nodes = false;
        RigEditor::Node* node_to_set_selected = nullptr;
        Vector2int mouse_screen_position = m_input_handler->GetMouseMotionEvent().GetAbsolutePosition();

        // Handle event 'extrude selected', because it changes selection and invokes Mode::GRAB_NODES
        // TODO: Make this work with GUI panel updates. It will require implementing array of nodes to set selected.
        if	(	(m_input_handler->WasEventFired(InputHandler::Event::NODES_EXTRUDE_SELECTED))
            &&	(! m_input_handler->IsModeActive(InputHandler::Mode::CREATE_NEW_NODE))
            &&	(! m_input_handler->IsModeActive(InputHandler::Mode::GRAB_NODES))
            )
        {
            m_rig->ExtrudeSelectedNodes();
            m_input_handler->EnterMode(InputHandler::Mode::GRAB_NODES);
        }
        
        if	(	(m_input_handler->WasModeEntered(InputHandler::Mode::CREATE_NEW_NODE))
            ||	(m_input_handler->WasModeEntered(InputHandler::Mode::GRAB_NODES))
            )
        {
            m_rig->ClearMouseHoveredNode();
            node_hover_changed = true;
            node_mouse_selecting_disabled = true;
        }

        // The 'delete menu' dialog
        if (m_input_handler->WasEventFired(InputHandler::Event::GUI_SHOW_DELETE_MENU))
        {
            m_gui_delete_menu->Show();
            m_gui_delete_menu->SetPosition(
                    mouse_screen_position.x + m_config->gui_dialog_delete_placement_x_px,
                    mouse_screen_position.y + m_config->gui_dialog_delete_placement_y_px
                );
        }
        if (m_gui_delete_menu->IsVisible())
        {
            if (! m_gui_delete_menu->TestCursorInRange(mouse_screen_position.x, mouse_screen_position.y, m_config->gui_dialog_delete_cursor_fence_px))
            {
                m_gui_delete_menu->Hide();
            }
        }

        // Deselect/select all nodes
        if	(	(m_input_handler->WasEventFired(InputHandler::Event::NODES_DESELECT_OR_SELECT_ALL))
            &&	(! m_input_handler->IsModeActive(InputHandler::Mode::CREATE_NEW_NODE))
            &&	(! m_input_handler->IsModeActive(InputHandler::Mode::GRAB_NODES))
            )
        {
            all_nodes_selection_toggled = true;
            node_selection_changed = true;
        }

        // Creating new nodes with mouse
        if (m_input_handler->IsModeActive(InputHandler::Mode::CREATE_NEW_NODE))
        {
            node_mouse_selecting_disabled = true;
            if (m_input_handler->GetMouseButtonEvent().WasLeftButtonPressed())
            {
                if (! ctrl_is_down)
                {
                    rig_must_deselect_all_nodes = true;
                    node_selection_changed = true;
                }
                node_to_set_selected = &m_rig->CreateNewNode(
                        m_camera_handler->ConvertScreenToWorldPosition(mouse_screen_position, Ogre::Vector3::ZERO)
                    );
                m_rig->RefreshNodeScreenPosition(*node_to_set_selected, m_camera_handler);
                node_selection_changed = true;
                rig_updated = true;
            }
        }

        // Grabbing nodes with mouse
        if (m_input_handler->IsModeActive(InputHandler::Mode::GRAB_NODES))
        {
            if (m_input_handler->GetMouseMotionEvent().HasMoved())
            {
                // Translate selected nodes
                Ogre::Vector3 mouse_world_pos = m_camera_handler->ConvertScreenToWorldPosition(mouse_screen_position, Ogre::Vector3::ZERO);
                Ogre::Vector3 previous_world_pos = m_camera_handler->ConvertScreenToWorldPosition(
                    m_input_handler->GetMouseMotionEvent().GetPreviousAbsolutePosition(),
                    Ogre::Vector3::ZERO
                );

                m_rig->TranslateSelectedNodes(mouse_world_pos - previous_world_pos, m_camera_handler);
                rig_updated = true;
            }

            if (m_input_handler->GetMouseButtonEvent().WasLeftButtonPressed())
            {
                // Exit grab mode and commit changes
                m_input_handler->ExitMode(InputHandler::Mode::GRAB_NODES);
                m_rig->SelectedNodesCommitPositionUpdates();
            }
            else if (m_input_handler->WasEventFired(InputHandler::Event::ESCAPE))
            {
                // Exit grab mode and revert changes
                m_input_handler->ExitMode(InputHandler::Mode::GRAB_NODES);
                m_rig->SelectedNodesCancelPositionUpdates();
                rig_updated = true;
            }
        }
        

        if (camera_view_changed || camera_ortho_toggled)
        {
            m_rig->RefreshAllNodesScreenPositions(m_camera_handler);
        }
        
        if	(	(m_input_handler->WasModeExited(InputHandler::Mode::CREATE_NEW_NODE))
            ||	(m_input_handler->WasModeExited(InputHandler::Mode::GRAB_NODES))
            ||	(	(! m_input_handler->IsModeActive(InputHandler::Mode::CREATE_NEW_NODE))
                &&	((m_input_handler->GetMouseMotionEvent().HasMoved() || camera_view_changed || camera_ortho_toggled))
                )
            )
        {
            if (m_rig->RefreshMouseHoveredNode(mouse_screen_position))
            {
                node_hover_changed = true;
            }
        }

        // Selecting nodes with LMB
        if	(	(! node_mouse_selecting_disabled) 
            &&	(! node_hover_changed) 
            &&	(! m_input_handler->WasModeExited(InputHandler::Mode::GRAB_NODES))
            &&	(m_input_handler->GetMouseButtonEvent().WasLeftButtonPressed())
            )
        {
            if (! ctrl_is_down)
            {
                rig_must_deselect_all_nodes = true;
                node_selection_changed = true;
            }
            if (m_rig->GetMouseHoveredNode() != nullptr)
            {
                node_selection_changed = true;
                node_to_set_selected = m_rig->GetMouseHoveredNode();
            }
        }

        if (node_selection_changed)
        {
            // ==== Apply changes from GUI to rig ====

            if (m_nodes_panel->GetData()->num_selected != 0)
            {
                m_rig->SelectedNodesUpdateAttributes(m_nodes_panel->GetData());
                if (m_rig->GetNumSelectedBeams() != 0)
                {
                    if (m_beams_panel->HasMixedBeamTypes())
                    {
                        MixedBeamsAggregateData data;
                        m_beams_panel->GetMixedBeamsData(&data);
                        m_rig->SelectedMixedBeamsUpdateAttributes(&data);
                    }
                    else
                    {
                        if (m_beams_panel->GetPlainBeamsData()->num_selected != 0)
                        {
                            m_rig->SelectedPlainBeamsUpdateAttributes(m_beams_panel->GetPlainBeamsData());
                        }
                        else if (m_shocks_panel->GetShocksData()->num_selected != 0)
                        {
                            m_rig->SelectedShocksUpdateAttributes(m_shocks_panel->GetShocksData());
                        }
                        else if (m_shocks2_panel->GetShocks2Data()->num_selected != 0)
                        {
                            m_rig->SelectedShocks2UpdateAttributes(m_shocks2_panel->GetShocks2Data());
                        }
                        else if (m_hydros_panel->GetHydrosData()->num_selected != 0)
                        {
                            m_rig->SelectedHydrosUpdateAttributes(m_hydros_panel->GetHydrosData());
                        }
                        else if (m_commands2_panel->GetCommands2Data()->num_selected != 0)
                        {
                            m_rig->SelectedCommands2UpdateAttributes(m_commands2_panel->GetCommands2Data());
                        }
                    }		
                }
            }

            HideAllNodeBeamGuiPanels(); // Reset GUI

            // ==== Perform rig selection updates ====
            if (all_nodes_selection_toggled)
            {
                m_rig->DeselectOrSelectAllNodes();
            }
            if (rig_must_deselect_all_nodes)
            {
                m_rig->DeselectAllNodes();
            }
            if (node_to_set_selected != nullptr)
            {
                node_to_set_selected->SetSelected(true);
            }

            // ==== Query updated data ====

            // Update "nodes" panel
            RigAggregateNodesData query;
            m_rig->QuerySelectedNodesData(&query);

            if (query.num_selected != 0)
            {
                m_nodes_panel->UpdateNodeData(&query);
                m_nodes_panel->Show();
            }

            // Update BEAM panels
            if (query.num_selected >= 2)
            {
                RigAggregateBeams2Data beam_query;
                m_rig->QuerySelectedBeamsData(&beam_query);
                if (beam_query.GetTotalNumSelectedBeams() != 0)
                {
                    if (beam_query.HasMixedBeamTypes())
                    {
                        m_beams_panel->UpdateMixedBeamData(&beam_query);
                        m_beams_panel->Show();
                    }
                    else if (beam_query.plain_beams.num_selected != 0)
                    {
                        m_beams_panel->UpdatePlainBeamData(&beam_query.plain_beams);
                        m_beams_panel->Show();
                    }
                    else if (beam_query.hydros.num_selected != 0)
                    {
                        m_hydros_panel->UpdateHydrosData(&beam_query.hydros);
                        m_hydros_panel->Show();
                    }
                    else if (beam_query.commands2.num_selected != 0)
                    {
                        m_commands2_panel->UpdateCommand2Data(&beam_query.commands2);
                        m_commands2_panel->Show();
                    }
                    else if (beam_query.shocks.num_selected != 0)
                    {
                        m_shocks_panel->UpdateShockData(&beam_query.shocks);
                        m_shocks_panel->Show();
                    }
                    else if (beam_query.shocks2.num_selected != 0)
                    {
                        m_shocks2_panel->UpdateShock2Data(&beam_query.shocks2);
                        m_shocks2_panel->Show();
                    }
                }
            } // if (query.num_selected >= 2)
        } // if (node_selection_changed)

        bool must_refresh_wheels_mesh = false;

        if (this->IsAnyWheelSelectionChangeScheduled())
        {
            // Apply changes from panels
            AllWheelsAggregateData wheel_update;
            wheel_update.flexbodywheels_data = *m_flexbodywheels_panel->GetFlexBodyWheelsData();
            wheel_update.num_elements += wheel_update.flexbodywheels_data.num_elements;
            wheel_update.meshwheels2_data = *m_meshwheels2_panel->GetMeshWheel2Data();
            wheel_update.num_elements += wheel_update.meshwheels2_data.num_elements;
            must_refresh_wheels_mesh = m_rig->UpdateSelectedWheelsData(&wheel_update);

            // Update selection
            const bool any_change = m_rig->PerformScheduledWheelSelectionUpdates(this);
            this->ResetAllScheduledWheelSelectionChanges();

            if (any_change)
            {
                HideAllWheelGuiPanels();
            
                // Query selected wheels data
                AllWheelsAggregateData wheel_query;
                m_rig->QuerySelectedWheelsData(&wheel_query);
                if (wheel_query.num_elements != 0)
                {
                    if(wheel_query.ContainsMultipleWheelTypes())
                    {
                        // To be done.
                    }
                    else if (wheel_query.meshwheels2_data.num_elements != 0)
                    {
                        m_meshwheels2_panel->UpdateMeshWheels2Data(&wheel_query.meshwheels2_data);
                        m_meshwheels2_panel->Show();
                    }
                    else if (wheel_query.flexbodywheels_data.num_elements != 0)
                    {
                        m_flexbodywheels_panel->UpdateFlexBodyWheelsData(&wheel_query.flexbodywheels_data);
                        m_flexbodywheels_panel->Show();
                    }
                }
            }
        }
        else
        {
            AllWheelsAggregateData wheel_update;
            if (m_flexbodywheels_panel->IsImmediateRigUpdateNeeded())
            {
                wheel_update.flexbodywheels_data = *m_flexbodywheels_panel->GetFlexBodyWheelsData();
                wheel_update.num_elements += wheel_update.flexbodywheels_data.num_elements;
                m_flexbodywheels_panel->SetIsImmediateRigUpdateNeeded(false);
            }
            if (m_meshwheels2_panel->IsImmediateRigUpdateNeeded())
            {
                wheel_update.meshwheels2_data = *m_meshwheels2_panel->GetMeshWheel2Data();
                wheel_update.num_elements += wheel_update.meshwheels2_data.num_elements;
                m_meshwheels2_panel->SetIsImmediateRigUpdateNeeded(false);
            }
            if (wheel_update.num_elements != 0)
            {
                must_refresh_wheels_mesh = m_rig->UpdateSelectedWheelsData(&wheel_update);
            }
        }

        // ==== Update visuals ====
        Ogre::SceneNode* parent_scene_node = m_scene_manager->getRootSceneNode();
        if (rig_updated || node_selection_changed || node_hover_changed)
        {
            m_rig->RefreshNodesDynamicMeshes(parent_scene_node);
        }
        if (rig_updated)
        {
            m_rig->RefreshBeamsDynamicMesh();
        }
        bool force_refresh_wheel_selection_boxes = false;

        if (must_refresh_wheels_mesh)
        {
            m_rig->RefreshWheelsDynamicMesh(parent_scene_node, this);
            force_refresh_wheel_selection_boxes = true;
        }
        m_rig->CheckAndRefreshWheelsSelectionHighlights(this, parent_scene_node, force_refresh_wheel_selection_boxes);
        m_rig->CheckAndRefreshWheelsMouseHoverHighlights(this, parent_scene_node);
    }

    // Mouse node info box = small, headless window at the bottom center of screen.
    if (m_rig != nullptr && m_rig->GetMouseHoveredNode() != nullptr)
    {
        ImVec2 box_size(300.f, 35.f);
        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x / 2.f) - (box_size.x / 2.f), ImGui::GetIO().DisplaySize.y - box_size.y - 20.f));
        ImGui::SetNextWindowContentSize(box_size);
        const int flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;
        ImGui::Begin("Mouse node info", nullptr, flags);
        GStr<64> node_id_str;
        node_id_str << m_rig->GetMouseHoveredNode()->GetId().ToString().c_str();
        ImVec2 text_size = ImGui::CalcTextSize(node_id_str);
        ImGui::SetCursorPos((box_size/2.f) - (text_size/2.f));
        ImGui::Text("%s", node_id_str.GetBuffer());
        ImGui::End();
    }
}

void Main::CommandShowDialogOpenRigFile()
{
    m_gui_open_save_file_dialog->setDialogInfo(MyGUI::UString("Open rig file"), MyGUI::UString("Open"), false);
    m_gui_open_save_file_dialog->eventEndDialog = MyGUI::newDelegate(this, &Main::NotifyFileSelectorEnded);
    m_gui_open_save_file_dialog->setMode(OpenSaveFileDialogMode::MODE_OPEN_TRUCK);
    m_gui_open_save_file_dialog->doModal(); // Shows the dialog
}

void Main::CommandShowDialogSaveRigFileAs()
{
    if (m_rig != nullptr)
    {
        m_gui_open_save_file_dialog->setDialogInfo(MyGUI::UString("Save rig file"), MyGUI::UString("Save"), false);
        m_gui_open_save_file_dialog->eventEndDialog = MyGUI::newDelegate(this, &Main::NotifyFileSelectorEnded);
        m_gui_open_save_file_dialog->setMode(OpenSaveFileDialogMode::MODE_SAVE_TRUCK_AS);
        m_gui_open_save_file_dialog->doModal(); // Shows the dialog
    }
}

void Main::CommandShowDialogOpenJsonProject()
{
    m_gui_open_save_file_dialog->setDialogInfo(MyGUI::UString("Open JSON project file"), MyGUI::UString("Open"), false);
    m_gui_open_save_file_dialog->eventEndDialog = MyGUI::newDelegate(this, &Main::NotifyFileSelectorEnded);
    m_gui_open_save_file_dialog->setMode(OpenSaveFileDialogMode::MODE_OPEN_JSON);
    m_gui_open_save_file_dialog->doModal(); // Shows the dialog
}

void Main::CommandShowDialogSaveJsonProjectAs()
{
    if (m_rig != nullptr)
    {
        m_gui_open_save_file_dialog->setDialogInfo(MyGUI::UString("Save JSON project file"), MyGUI::UString("Save"), false);
        m_gui_open_save_file_dialog->eventEndDialog = MyGUI::newDelegate(this, &Main::NotifyFileSelectorEnded);
        m_gui_open_save_file_dialog->setMode(OpenSaveFileDialogMode::MODE_SAVE_JSON_AS);
        m_gui_open_save_file_dialog->doModal(); // Shows the dialog
    }
}

void Main::CommandSaveRigFile()
{
    // TODO
}

void Main::CommandCloseCurrentRig()
{
    if (m_rig != nullptr)
    {
        // Remove rig
        m_rig->DetachFromScene();
        delete m_rig;
        m_rig = nullptr;

        // Restore GUI
        HideAllNodeBeamGuiPanels();
    }
}

void Main::NotifyFileSelectorEnded(GUI::Dialog* dialog, bool result)
{
    if (result)
    {
        const MyGUI::UString & mode = m_gui_open_save_file_dialog->getMode();
        auto const & folder = m_gui_open_save_file_dialog->getCurrentFolder();
        auto const & filename = m_gui_open_save_file_dialog->getFileName();

        if (mode == OpenSaveFileDialogMode::MODE_OPEN_TRUCK)
        {
            this->LoadRigDefFile(folder, filename);
        }
        else if (mode == OpenSaveFileDialogMode::MODE_SAVE_TRUCK_AS)
        {
            this->ExportRigDefFile(folder, filename);
        }
        else if (mode == OpenSaveFileDialogMode::MODE_SAVE_JSON_AS)
        {
            this->SaveJsonProject(folder, filename);
        }
        else if (mode == OpenSaveFileDialogMode::MODE_OPEN_JSON)
        {
            this->LoadJsonProject(folder, filename);
        }
    }
    dialog->endModal(); // Hides the dialog
}

void Main::ExportRigDefFile(MyGUI::UString const & directory, MyGUI::UString const & filename)
{
    using namespace RigDef;

    if (m_rig == nullptr)
    {
        LOG("RigEditor: [WARNING] ExportRigDefFile(): Nothing to export.");
        return;
    }

    auto out_path = directory + '/' + filename;

    auto rig_def = m_rig->Export();
    
    Serializer serializer(rig_def, out_path);
    serializer.Serialize();
    LOG("RigEditor: Rig saved as: " + out_path);
}

void Main::SaveJsonProject(MyGUI::UString const & directory, MyGUI::UString const & filename)
{
    if (m_rig == nullptr)
    {
        LOG("RigEditor: [WARNING] SaveJsonProject(): Nothing to save.");
        return;
    }

    auto out_path = directory + '/' + filename;
    m_rig->SaveJsonProject(out_path);
}

void Main::LoadJsonProject(MyGUI::UString const & directory, MyGUI::UString const & filename)
{
    this->CommandCloseCurrentRig();

    auto src_path = directory + '/' + filename;
    m_rig = new RigEditor::Rig(m_config);

    Ogre::SceneNode* parent_scene_node = m_scene_manager->getRootSceneNode();
    m_rig->LoadJsonProject(src_path, this, parent_scene_node);
    this->OnNewRigCreatedOrLoaded(parent_scene_node);
}

void RigEditor_LogParserMessages(RigDef::Parser & parser)
{
    if (parser.GetMessages().size() == 0)
    {
        LOG("RigEditor: Parsing done OK");
        return;
    }

    std::stringstream report;
    report << "RigEditor: Parsing done, report:" << std::endl <<std::endl;

    for (auto iter = parser.GetMessages().begin(); iter != parser.GetMessages().end(); iter++)
    {
        switch (iter->type)
        {
            case (RigDef::Parser::Message::TYPE_FATAL_ERROR): 
                report << "FATAL_ERROR"; 
                break;

            case (RigDef::Parser::Message::TYPE_ERROR): 
                report << "ERROR"; 
                break;

            case (RigDef::Parser::Message::TYPE_WARNING): 
                report << "WARNING"; 
                break;

            default:
                report << "INFO"; 
                break;
        }
        report << " (Section " << RigDef::File::SectionToString(iter->section) << ")" << std::endl;
        report << "\tLine (# " << iter->line_number << "): " << iter->line << std::endl;
        report << "\tMessage: " << iter->message << std::endl;
    }

    Ogre::LogManager::getSingleton().logMessage(report.str());
}

void RigEditor_LogValidatorMessages(RigDef::Validator & validator)
{
    if (validator.GetMessages().empty())
    {
        Ogre::LogManager::getSingleton().logMessage("RigEditor: Validating done OK");
        return;
    }

    std::ostringstream report;
    report << "RigEditor: Validating done, report:" <<std::endl << std::endl;

    for(auto itor = validator.GetMessages().begin(); itor != validator.GetMessages().end(); itor++)
    {
        switch (itor->type)
        {
            case (RigDef::Validator::Message::TYPE_FATAL_ERROR):
                report << "FATAL ERROR";
                break;
            case (RigDef::Validator::Message::TYPE_ERROR):
                report << "ERROR";
                break;
            case (RigDef::Validator::Message::TYPE_WARNING):
                report << "WARNING";
                break;
            default:
                report << "INFO";
        }

        report << ": " << itor->text << std::endl;
    }

    Ogre::LogManager::getSingleton().logMessage(report.str());
}

bool Main::LoadRigDefFile(MyGUI::UString const & directory, MyGUI::UString const & filename)
{
    /* CLOSE PREVIOUS RIG */

    if (m_rig != nullptr)
    {
        this->CommandCloseCurrentRig();
    }

    /* LOAD */

    Ogre::DataStreamPtr stream = Ogre::DataStreamPtr();
    Ogre::String resource_group_name("RigEditor_CurrentProject");

    try
    {
        Ogre::ResourceGroupManager::getSingleton().addResourceLocation(directory, "FileSystem", resource_group_name);
        stream = Ogre::ResourceGroupManager::getSingleton().openResource(filename, resource_group_name);
    } 
    catch (Ogre::Exception& e)
    {
        // TODO: Report error to user

        LOG("RigEditor: Failed to retrieve rig file" + filename + ", Ogre::Exception was thrown with message: " + e.what());
        return false;
    }

    /* PARSE */

    LOG("RigEditor: Parsing rig file: " + filename);

    RigDef::Parser parser;
    parser.Prepare();
    parser.ProcessOgreStream(stream.get());
    parser.Finalize();

    RigEditor_LogParserMessages(parser);

    /* VALIDATE */

    LOG("RigEditor: Validating vehicle: " + parser.GetFile()->name);

    RigDef::Validator validator;
    validator.Setup(parser.GetFile());
    bool valid = validator.Validate();

    RigEditor_LogValidatorMessages(validator);

    if (! valid)
    {
        // TODO: Report error to user

        LOG("RigEditor: Validating failed!");
        return false;
    }

    /* BUILD RIG MESH */

    m_rig = new RigEditor::Rig(m_config);
    RigEditor::RigBuildingReport rig_build_report;
    Ogre::SceneNode* parent_scene_node = m_scene_manager->getRootSceneNode();
    m_rig->Build(parser.GetFile(), this, parent_scene_node, &rig_build_report);

    /* SHOW MESH */
    this->OnNewRigCreatedOrLoaded(parent_scene_node);

    LOG(Ogre::String("RigEditor: Finished loading rig, report:\n") + rig_build_report.ToString());

    return true;
}

void Main::OnNewRigCreatedOrLoaded(Ogre::SceneNode* parent_scene_node)
{
    m_rig->AttachToScene(parent_scene_node);
    /* Handle mouse selection of nodes */
    m_rig->RefreshAllNodesScreenPositions(m_camera_handler);
    if (m_rig->RefreshMouseHoveredNode(m_input_handler->GetMouseMotionEvent().GetAbsolutePosition()))
    {
        m_rig->RefreshNodesDynamicMeshes(parent_scene_node);
    }
}

void Main::CommandCurrentRigDeleteSelectedNodes()
{
    m_gui_delete_menu->Hide();
    HideAllNodeBeamGuiPanels();
    assert(m_rig != nullptr);
    m_rig->DeleteSelectedNodes();
    m_rig->RefreshBeamsDynamicMesh();
    m_rig->RefreshNodesDynamicMeshes(m_scene_manager->getRootSceneNode());
}

void Main::HideAllNodeBeamGuiPanels()
{
    m_nodes_panel    ->HideAndReset();
    m_beams_panel    ->HideAndReset();
    m_hydros_panel   ->HideAndReset();
    m_commands2_panel->HideAndReset();
    m_shocks2_panel  ->HideAndReset();
    m_shocks_panel   ->HideAndReset();
}

void Main::CommandCurrentRigDeleteSelectedBeams()
{
    m_gui_delete_menu->Hide();
    HideAllNodeBeamGuiPanels();
    assert(m_rig != nullptr);
    m_rig->DeleteBeamsBetweenSelectedNodes();
    m_rig->RefreshBeamsDynamicMesh();
    m_rig->RefreshNodesDynamicMeshes(m_scene_manager->getRootSceneNode());
}

void Main::CommandQuitRigEditor()
{
    if (App::app_state.GetActive() == RoR::AppState::RIG_EDITOR)
    {
        App::app_state.SetPending(AppState::MAIN_MENU);
    }
    else // AppState::SIMULATION, SimState::RIG_EDITOR
    {
        App::sim_state.SetPending(SimState::RUNNING);
    }
}

void Main::CommandShowRigPropertiesWindow()
{
    if (m_rig != nullptr)
    {
        m_gui_rig_properties_window->Import(m_rig->GetProperties());
        m_gui_rig_properties_window->Show();
        m_gui_rig_properties_window->CenterToScreen();
    }
}

void Main::CommandSaveContentOfRigPropertiesWindow()
{
    if (m_rig != nullptr && m_gui_rig_properties_window->IsVisible())
    {
        m_gui_rig_properties_window->Export(m_rig->GetProperties());
    }
}

void Main::CommandShowLandVehiclePropertiesWindow()
{
    if (m_rig != nullptr)
    {
        m_gui_land_vehicle_properties_window->Import(
            m_rig->GetProperties()->GetEngine(),
            m_rig->GetProperties()->GetEngoption()
            );
        m_gui_land_vehicle_properties_window->Show();
        m_gui_land_vehicle_properties_window->CenterToScreen();		
    }
}

void Main::CommandSaveLandVehiclePropertiesWindowData()
{
    if (m_rig != nullptr && m_gui_land_vehicle_properties_window->IsVisible())
    {
        m_rig->GetProperties()->SetEngine(m_gui_land_vehicle_properties_window->ExportEngine());
        m_rig->GetProperties()->SetEngoption(m_gui_land_vehicle_properties_window->ExportEngoption());
    }
}

void Main::CommandShowHelpWindow()
{
    m_gui_help_visible = true;
}

#define INIT_OR_RESTORE_RIG_ELEMENT_PANEL(VAR, CLASSNAME) \
    if ((VAR).get() == nullptr) \
        (VAR) = std::unique_ptr<GUI::CLASSNAME>(new GUI::CLASSNAME(this, m_config)); \
    else \
        (VAR)->ShowIfHiddenTemporarily();

void Main::InitializeOrRestoreGui()
{
    INIT_OR_RESTORE_RIG_ELEMENT_PANEL( m_nodes_panel,            RigEditorNodePanel);
    INIT_OR_RESTORE_RIG_ELEMENT_PANEL( m_beams_panel,            RigEditorBeamsPanel);
    INIT_OR_RESTORE_RIG_ELEMENT_PANEL( m_hydros_panel,           RigEditorHydrosPanel);
    INIT_OR_RESTORE_RIG_ELEMENT_PANEL( m_commands2_panel,        RigEditorCommands2Panel);
    INIT_OR_RESTORE_RIG_ELEMENT_PANEL( m_shocks_panel,           RigEditorShocksPanel);
    INIT_OR_RESTORE_RIG_ELEMENT_PANEL( m_shocks2_panel,          RigEditorShocks2Panel);
    INIT_OR_RESTORE_RIG_ELEMENT_PANEL( m_meshwheels2_panel,      RigEditorMeshWheels2Panel);
    INIT_OR_RESTORE_RIG_ELEMENT_PANEL( m_flexbodywheels_panel,   RigEditorFlexBodyWheelsPanel);

    App::GetGuiManager()->SetSceneManagerForGuiRendering(m_scene_manager);

    if (m_gui_open_save_file_dialog.get() == nullptr)
    {
        m_gui_open_save_file_dialog = std::unique_ptr<GUI::OpenSaveFileDialog>(new GUI::OpenSaveFileDialog());
    }
    if (m_gui_delete_menu.get() == nullptr)
    {
        m_gui_delete_menu = std::unique_ptr<GUI::RigEditorDeleteMenu>(new GUI::RigEditorDeleteMenu(this));
    }
    if (m_gui_rig_properties_window.get() == nullptr)
    {
        m_gui_rig_properties_window 
            = std::unique_ptr<GUI::RigEditorRigPropertiesWindow>(new GUI::RigEditorRigPropertiesWindow(this));
    }
    if (m_gui_land_vehicle_properties_window.get() == nullptr)
    {
        m_gui_land_vehicle_properties_window 
            = std::unique_ptr<GUI::RigEditorLandVehiclePropertiesWindow>(new GUI::RigEditorLandVehiclePropertiesWindow(this));
    }
}

void Main::HideAllWheelGuiPanels()
{
    m_meshwheels2_panel->Hide();
    m_flexbodywheels_panel->Hide();
}

// ----------------------------------------------------------------------------
// Rig Updaters
// ----------------------------------------------------------------------------

void Main::CommandRigSelectedNodesUpdateAttributes(const RigAggregateNodesData* data)
{
    m_rig->SelectedNodesUpdateAttributes(data);
}

void Main::CommandRigSelectedPlainBeamsUpdateAttributes(const RigAggregatePlainBeamsData* data)
{
    m_rig->SelectedPlainBeamsUpdateAttributes(data);
}

void Main::CommandRigSelectedShocksUpdateAttributes(const RigAggregateShocksData*     data)
{
    m_rig->SelectedShocksUpdateAttributes(data);
}

void Main::CommandRigSelectedShocks2UpdateAttributes(const RigAggregateShocks2Data*    data)
{
    m_rig->SelectedShocks2UpdateAttributes(data);
}

void Main::CommandRigSelectedHydrosUpdateAttributes(const RigAggregateHydrosData*     data)
{
    m_rig->SelectedHydrosUpdateAttributes(data);
}

void Main::CommandRigSelectedCommands2UpdateAttributes(const RigAggregateCommands2Data*  data)
{
    m_rig->SelectedCommands2UpdateAttributes(data);
}

void Main::CommandScheduleSetWheelSelected(LandVehicleWheel* wheel_ptr, int wheel_index, bool state_selected)
{
    // TODO: I have no idea what I was smoking when I wrote this 3 years ago *blushes* ~ only_a_ptr, 07/2017
    if (m_rig == nullptr)
    {
        return;
    }
    const bool selection_changes = m_rig->ScheduleSetWheelSelected(wheel_ptr, wheel_index, state_selected, this);
    if (selection_changes)
    {
        if (state_selected)
        {
            this->SetIsSelectWheelScheduled(true); // TODO: having separate 'select!' and 'deselect!' flags is dumb ~ only_a_ptr, 07/2017
            this->SetIsDeselectWheelScheduled(false);
        }
        else
        {
            this->SetIsSelectWheelScheduled(false);
            this->SetIsDeselectWheelScheduled(true);
        }
    }
}

void Main::CommandSetWheelHovered (LandVehicleWheel* wheel_ptr, int wheel_index, bool state_hovered)
{
    if (m_rig == nullptr)
    {
        return;
    }
    m_rig->SetWheelHovered(wheel_ptr, wheel_index, state_hovered, this);
}

void Main::CommandScheduleSetAllWheelsSelected(bool state_selected)
{
    if (m_rig == nullptr)
    {
        return;
    }
    if (m_rig->ScheduleSetAllWheelsSelected(state_selected, this))
    {
        if (state_selected)
        {
            this->SetIsSelectAllWheelsScheduled(true);
            this->SetIsDeselectAllWheelsScheduled(false);
        }
        else
        {
            this->SetIsSelectAllWheelsScheduled(false);
            this->SetIsDeselectAllWheelsScheduled(true);
        }
    }
}

void Main::CommandSetAllWheelsHovered(bool state_hovered)
{
    if (m_rig != nullptr)
    {
        m_rig->SetAllWheelsHovered(state_hovered, this);
    }
}

// Utility macros for creating fresh empty rig + initial cube
#define ADD_NODE(MODULENAME, NODENAME, X, Y, Z) \
{ \
    RigDef::Node n; \
    n.id.SetStr(NODENAME); \
    n.position = Ogre::Vector3(X, Y, Z); \
    MODULENAME->nodes.push_back(n); \
}
#define LINK_NODES(MODULE, NODE1, NODE2)\
{\
    RigDef::Beam b;\
    unsigned flags = RigDef::Node::Ref::REGULAR_STATE_IS_VALID | RigDef::Node::Ref::REGULAR_STATE_IS_NAMED; \
    b.nodes[0] = RigDef::Node::Ref(NODE1, 0u, flags, 0);\
    b.nodes[1] = RigDef::Node::Ref(NODE2, 0u, flags, 0);\
    MODULE->beams.push_back(b);\
}

void Main::CommandCreateNewEmptyRig()
{
    if (m_rig != nullptr)
    {
        return;
    }

    m_rig = new RigEditor::Rig(m_config);
    // Create definition
    auto def = std::shared_ptr<RigDef::File>(new RigDef::File());
    def->name = "Unnamed rig (created in editor)";
    auto module = std::shared_ptr<RigDef::File::Module>(new RigDef::File::Module(RigDef::File::Module::ROOT_MODULE_NAME));
    def->root_module = module;
    // Create special node 0 in _Root_ module
    RigDef::Node node_0;
    node_0.id.SetStr("ZERO");
    node_0.position = Ogre::Vector3::ZERO;
    module->nodes.push_back(node_0);
    // Create cube around node 0
    float size = m_config->new_rig_initial_box_half_size;
    ADD_NODE(module, "CUBE_bottom_oo", -size, -size, -size);
    ADD_NODE(module, "CUBE_bottom_xo",  size, -size, -size);
    ADD_NODE(module, "CUBE_bottom_ox", -size, -size,  size);
    ADD_NODE(module, "CUBE_bottom_xx",  size, -size,  size);
    ADD_NODE(module, "CUBE_top_oo",    -size,  size, -size);
    ADD_NODE(module, "CUBE_top_xo",     size,  size, -size);
    ADD_NODE(module, "CUBE_top_ox",    -size,  size,  size);
    ADD_NODE(module, "CUBE_top_xx",     size,  size,  size);
    // Link nodes                         
    LINK_NODES(module, "CUBE_bottom_oo", "CUBE_bottom_xo"); // Bottom plane...
    LINK_NODES(module, "CUBE_bottom_xo", "CUBE_bottom_xx");
    LINK_NODES(module, "CUBE_bottom_xx", "CUBE_bottom_ox");
    LINK_NODES(module, "CUBE_bottom_ox", "CUBE_bottom_oo");
    LINK_NODES(module, "CUBE_top_oo", "CUBE_top_xo"); // Top plane...
    LINK_NODES(module, "CUBE_top_xo", "CUBE_top_xx");
    LINK_NODES(module, "CUBE_top_xx", "CUBE_top_ox");
    LINK_NODES(module, "CUBE_top_ox", "CUBE_top_oo");
    LINK_NODES(module, "CUBE_bottom_oo", "CUBE_top_oo"); // Top to bottom...
    LINK_NODES(module, "CUBE_bottom_xo", "CUBE_top_xo");
    LINK_NODES(module, "CUBE_bottom_ox", "CUBE_top_ox");
    LINK_NODES(module, "CUBE_bottom_xx", "CUBE_top_xx");
    // Link node 0
    LINK_NODES(module, "CUBE_bottom_oo", "ZERO" );
    LINK_NODES(module, "CUBE_bottom_xo", "ZERO" );
    LINK_NODES(module, "CUBE_bottom_ox", "ZERO" );
    LINK_NODES(module, "CUBE_bottom_xx", "ZERO" );
    LINK_NODES(module, "CUBE_top_oo"   , "ZERO" );
    LINK_NODES(module, "CUBE_top_xo"   , "ZERO" );
    LINK_NODES(module, "CUBE_top_ox"   , "ZERO" );
    LINK_NODES(module, "CUBE_top_xx"   , "ZERO" );
    // Build
    m_rig->Build(def, this, this->m_scene_manager->getRootSceneNode(), nullptr);
    // Accomodate
    this->OnNewRigCreatedOrLoaded(m_scene_manager->getRootSceneNode());
}

void Main::DrawHelpGui()
{
    if (!m_gui_help_visible)
        return; // Nothing to do

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiSetCond_Once);
    ImGui::SetNextWindowPosCenter(ImGuiSetCond_Once);
    ImGui::Begin("RigEditor Help", &m_gui_help_visible, ImGuiWindowFlags_NoCollapse);

    ImGui::TextWrapped("RigEditor lets you create and modify softbody actors of Rigs of Rods\n"
        "These can be anything from soda can to the most advanced vehicles, be it land, air or nautical ones.\n"
        "It has Blender-like controls and behavior.");

    ImGui::Text("*** UNDER CONSTRUCTION ***\nThis tool is very early in development - expect bugs and quirks");

    if (ImGui::CollapsingHeader("Features"))
    {
        ImGui::BulletText("Import/export of truckfiles (.truck .car etc...)");
        ImGui::BulletText("Saving/loading of RigEditor project files (.json) - experimental");
        ImGui::BulletText("Blender-like camera controls: perspective/ortho switching, axis-aligned views");
        ImGui::BulletText("Moving nodes with G key (Blender-like)");
        ImGui::BulletText("Adding/deleting nodes with mouse");
        ImGui::BulletText("Adding/deleting beams with mouse");

        ImGui::Text("Supports following elements:");
        ImGui::BulletText("Nodes: nodes, nodes2");
        ImGui::BulletText("Beams: beams, shocks[2], hydros, commands[2]");
        ImGui::BulletText("Engines: engine, engoption (NOTE: import only)");
        ImGui::BulletText("Physics: globals, minimass, rollon, enable_advanced_deformation");
        ImGui::BulletText("Visuals: set_skeleton_settings, guisettings, help");
        ImGui::BulletText("Global: fileinfo, author, [forward/import]commands, rescuer");
    }

    if (ImGui::CollapsingHeader("Controls"))
    {
        ImGui::Text("Camera:");
        ImGui::BulletText("RMB        = Hold and move mouse to rotate camera");
        ImGui::BulletText("MouseWheel = Zoom camera");
        ImGui::BulletText("Numpad 5   = Toggle perspective/ortho view");
        ImGui::BulletText("Numpad 1   = Front view");
        ImGui::BulletText("Numpad 3   = Left view");
        ImGui::BulletText("Numpad 7   = Top view");

        ImGui::Text("Editing:");
        ImGui::BulletText("LMB        = Select/deselect node");
        ImGui::BulletText("CTRL + LMB = Select multiple nodes");
        ImGui::BulletText("G          = Grab node[s] (LMB to release)");
        ImGui::BulletText("N + LMB    = Add new node");
        ImGui::BulletText("DELETE     = Show 'delete menu'\n>"
            " > 'Nodes & beams' => del. selected nodes and all attached beams.\n"
            " > 'Beams only' => del. only beams between selected nodes.");
    }

    ImGui::End();
}

void Main::DrawTopMenubarGui()
{
    ImGui::BeginMainMenuBar();

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New project"))        { this->CommandCreateNewEmptyRig();            }
        if (ImGui::MenuItem("Load project"))       { this->CommandShowDialogOpenJsonProject();    }
        if (ImGui::MenuItem("Save project as...")) { this->CommandShowDialogSaveJsonProjectAs();  }
        if (ImGui::MenuItem("Close project"))      { this->CommandCloseCurrentRig();              }
        ImGui::Separator();
        if (ImGui::MenuItem("Import truckfile"))   { this->CommandShowDialogOpenRigFile();        }
        if (ImGui::MenuItem("Export truckfile"))   { this->CommandShowDialogSaveRigFileAs();      }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))               { this->CommandQuitRigEditor();                }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Project"))
    {
        if (ImGui::MenuItem("General properties"))        { this->CommandShowRigPropertiesWindow();         }
        if (ImGui::MenuItem("Land vehicle properties"))   { this->CommandShowLandVehiclePropertiesWindow(); }
        ImGui::Separator();
        ImGui::MenuItem("Left-side panel:", nullptr, false, false); // Just a caption - force disabled
        if (ImGui::MenuItem("None"))                      { m_gui_active_left_panel = GuiLeftSidePanel::NONE;   }
        if (ImGui::MenuItem("Wheels"))                    { m_gui_active_left_panel = GuiLeftSidePanel::WHEELS; }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        ImGui::EndMenu();
        this->CommandShowHelpWindow();
    }

    ImGui::EndMainMenuBar();
}

void Main::DrawLandWheelsGui()
{
    const int window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;
    ImGui::SetNextWindowPos(ImVec2(10.f, 30.f));
    ImGui::SetNextWindowSize(ImVec2(175.f, ImGui::GetIO().DisplaySize.y - 40.f), ImGuiSetCond_Once);
    bool is_window_open = true;
    ImGui::Begin("Wheels", &is_window_open, window_flags);

    if (!is_window_open)
    {
        m_gui_active_left_panel = GuiLeftSidePanel::NONE;
        return;
    }

    if (ImGui::Button("[ + ] Select all"))
    {
        this->CommandScheduleSetAllWheelsSelected(true);
    }
    this->CommandSetAllWheelsHovered(ImGui::IsItemHovered());
    if (ImGui::Button("[ - ] Deselect all"))
    {
        this->CommandScheduleSetAllWheelsSelected(false);
    }

    ImGui::Separator();

    int index = 0;
    for (LandVehicleWheel* wheel: m_rig->GetWheels())
    {
        GStr<50> title; // TODO: Put editable wheel names directly to `LandVehicleWheel` objects. ~ only_a_ptr, 07/2017
        title << "Wheel_" << index;
        ++index;

        // We make our own multi-select list from checkboxes
        bool is_selected = wheel->IsSelected();
        if (ImGui::Checkbox(title, &is_selected))
        {
            this->CommandScheduleSetWheelSelected(wheel, index, is_selected); // TODO: The contained logic is insane, simplify it! ~ only_a_ptr, 07/2017
        }
        wheel->SetIsHovered(ImGui::IsItemHovered());
    }

    ImGui::End();
}

void Main::DrawGui()
{
    this->DrawTopMenubarGui();
    this->DrawHelpGui();

    switch (m_gui_active_left_panel)
    {
    case GuiLeftSidePanel::WHEELS: this->DrawLandWheelsGui(); break;
    default: break; // No action
    }
}
