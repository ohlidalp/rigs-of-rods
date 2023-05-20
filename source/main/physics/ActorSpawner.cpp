/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2020 Petr Ohlidal

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


///    @file
///    @brief  Vehicle spawning logic.
///    @author Petr Ohlidal
///    @date   12/2013


#include "ActorSpawner.h"

#include "AppContext.h"
#include "AirBrake.h"
#include "Airfoil.h"
#include "Application.h"
#include "ApproxMath.h"
#include "AutoPilot.h"
#include "Actor.h"
#include "ActorManager.h"
#include "BitFlags.h"
#include "Buoyance.h"
#include "CacheSystem.h"
#include "CameraManager.h"
#include "CmdKeyInertia.h"
#include "Collisions.h"
#include "DashBoardManager.h"
#include "Differentials.h"
#include "EngineSim.h"
#include "FlexAirfoil.h"
#include "FlexBody.h"
#include "FlexMesh.h"
#include "FlexMeshWheel.h"
#include "FlexObj.h"
#include "GameContext.h"
#include "GfxActor.h"
#include "GfxScene.h"
#include "Console.h"
#include "InputEngine.h"
#include "MeshObject.h"
#include "PointColDetector.h"
#include "Renderdash.h"
#include "ScrewProp.h"
#include "Skidmark.h"
#include "SkinFileFormat.h"
#include "SlideNode.h"
#include "SoundScriptManager.h"
#include "Terrain.h"
#include "TorqueCurve.h"
#include "TurboJet.h"
#include "TurboProp.h"
#include "Utils.h"
#include "VehicleAI.h"

#include <OgreMaterialManager.h>
#include <OgreSceneManager.h>
#include <OgreMovableObject.h>
#include <OgreParticleSystem.h>
#include <OgreEntity.h>
#include <climits>
#include <fmt/format.h>

const char* ACTOR_ID_TOKEN = "@Actor_"; // Appended to material name, followed by actor ID (aka 'trucknum')

using namespace RoR;

/* -------------------------------------------------------------------------- */
// Prepare for loading
/* -------------------------------------------------------------------------- */

void ActorSpawner::ConfigureSections(Ogre::String const & sectionconfig, RigDef::DocumentPtr def)
{   
    m_selected_modules.push_back(def->root_module);
    if (sectionconfig != "")
    {
        auto result = def->user_modules.find(sectionconfig);

        if (result != def->user_modules.end())
        {
            m_selected_modules.push_back(result->second);
            LOG(" == ActorSpawner: Module added to configuration: " + sectionconfig);
        }
        else
        {
            this->AddMessage(Message::TYPE_WARNING, "Selected module not found: " + sectionconfig);
        }
    }
}

void ActorSpawner::CalcMemoryRequirements(ActorMemoryRequirements& req, RigDef::Document::Module* module_def)
{
    // 'nodes'
    req.num_nodes += module_def->nodes.size();
    for (auto& def: module_def->nodes)
    {
        if (BITMASK_IS_1(def.options, RigDef::Node::OPTION_h_HOOK_POINT))
        {
            req.num_beams += 1;
        }
    }

    // 'beams'
    req.num_beams += module_def->beams.size();

    // 'ties'
    req.num_beams += module_def->ties.size();

    // 'ropes'
    req.num_beams += module_def->ropes.size();

    // 'hydros'
    req.num_beams += module_def->hydros.size();

    // 'triggers'
    req.num_beams  += module_def->triggers.size();
    req.num_shocks += module_def->triggers.size();

    // 'animators'
    req.num_beams += module_def->animators.size();

    // 'cinecam'
    req.num_nodes += module_def->cinecam.size();
    req.num_beams += module_def->cinecam.size() * 8;

    // 'shocks', 'shocks2', 'shocks3'
    req.num_beams  += module_def->shocks.size();
    req.num_shocks += module_def->shocks.size();
    req.num_beams  += module_def->shocks2.size();
    req.num_shocks += module_def->shocks2.size();
    req.num_beams  += module_def->shocks3.size();
    req.num_shocks += module_def->shocks3.size();

    // 'commands' and 'commands2' (unified)
    req.num_beams += module_def->commands2.size();

    // 'rotators'
    req.num_rotators += module_def->rotators.size();
    req.num_rotators += module_def->rotators2.size();

    // 'wings'
    req.num_wings += module_def->wings.size();

    // 'wheels'
    for (RigDef::Wheel& wheel: module_def->wheels)
    {
        req.num_nodes += wheel.num_rays * 2; // BuildWheelObjectAndNodes()
        req.num_beams += wheel.num_rays * ((wheel.rigidity_node.IsValidAnyState()) ? 9 : 8); // BuildWheelBeams()
    }

    // 'wheels2'
    for (RigDef::Wheel2& wheel2: module_def->wheels2)
    {
        req.num_nodes += wheel2.num_rays * 4;
        // Rim beams:  num_rays*10 (*11 with valid rigidity_node)
        // Tyre beams: num_rays*14
        req.num_beams += wheel2.num_rays * ((wheel2.rigidity_node.IsValidAnyState()) ? 25 : 24);
    }

    // 'meshwheels' & 'meshwheels2'
    for (RigDef::MeshWheel& meshwheel: module_def->meshwheels)
    {
        req.num_nodes += meshwheel.num_rays * 2; // BuildWheelObjectAndNodes()
        req.num_beams += meshwheel.num_rays * ((meshwheel.rigidity_node.IsValidAnyState()) ? 9 : 8); // BuildWheelBeams()
    }
    for (RigDef::MeshWheel2& meshwheel2: module_def->meshwheels2)
    {
        req.num_nodes += meshwheel2.num_rays * 2; // BuildWheelObjectAndNodes()
        req.num_beams += meshwheel2.num_rays * ((meshwheel2.rigidity_node.IsValidAnyState()) ? 9 : 8); // BuildWheelBeams()
    }

    // 'flexbodywheels'
    for (RigDef::FlexBodyWheel& flexwheel: module_def->flexbodywheels)
    {
        req.num_nodes += flexwheel.num_rays * 4;
        // Rim beams:      num_rays*8
        // Tyre beams:     num_rays*10 (num_rays*11 with valid rigidity_node)
        // Support beams:  num_rays*2
        req.num_beams += flexwheel.num_rays * ((flexwheel.rigidity_node.IsValidAnyState()) ? 21 : 20);
    }

    // 'airbrakes'
    req.num_airbrakes += module_def->airbrakes.size();

    // 'fixes'
    req.num_fixes += module_def->fixes.size();
}

void ActorSpawner::InitializeRig()
{
    ActorMemoryRequirements & req = m_memory_requirements;
    for (auto module: m_selected_modules) // _Root_ module is included
    {
        this->CalcMemoryRequirements(req, module.get());
    }

    // Allocate memory as needed

    if (req.num_rotators > 0)
        m_actor->ar_rotators = new rotator_t[req.num_rotators];

    // commands contain complex data structures, do not memset them ...
    for (int i=0;i<MAX_COMMANDS+1;i++)
    {
        m_actor->ar_command_key[i].commandValue=0;
        m_actor->ar_command_key[i].beams.clear();
        m_actor->ar_command_key[i].rotators.clear();
        m_actor->ar_command_key[i].description="";
    }

    m_actor->exhausts.clear();
    memset(m_actor->ar_custom_particles, 0, sizeof(cparticle_t) * MAX_CPARTICLES);
    m_actor->ar_num_custom_particles = 0;
    memset(m_actor->ar_collcabs, 0, sizeof(int) * MAX_CABS);
    memset(m_actor->ar_inter_collcabrate, 0, sizeof(collcab_rate_t) * MAX_CABS);
    m_actor->ar_num_collcabs = 0;
    memset(m_actor->ar_intra_collcabrate, 0, sizeof(collcab_rate_t) * MAX_CABS);
    memset(m_actor->ar_buoycabs, 0, sizeof(int) * MAX_CABS);
    m_actor->ar_num_buoycabs = 0;
    memset(m_actor->ar_buoycab_types, 0, sizeof(int) * MAX_CABS);
    memset(m_actor->m_skid_trails, 0, sizeof(Skidmark *) * (MAX_WHEELS*2));

    m_actor->authors.clear();

    m_actor->m_odometer_total = 0;
    m_actor->m_odometer_user  = 0;

    m_actor->m_masscount=0;
    m_actor->m_disable_smoke = App::gfx_particles_mode->getInt() == 0;
    m_actor->m_beam_break_debug_enabled  = App::diag_log_beam_break->getBool();
    m_actor->m_beam_deform_debug_enabled = App::diag_log_beam_deform->getBool();
    m_actor->m_trigger_debug_enabled    = App::diag_log_beam_trigger->getBool();
    m_actor->ar_origin=Ogre::Vector3::ZERO;
    m_actor->m_slidenodes.clear();
    m_actor->ar_num_cinecams=0;
    m_actor->m_deletion_scene_nodes.clear();

    m_actor->ar_state = ActorState::LOCAL_SLEEPING;
    m_actor->m_fusealge_airfoil = nullptr;
    m_actor->m_fusealge_width=0;
    m_actor->ar_brake_force=30000.0;
    m_actor->m_handbrake_force = 2 * m_actor->ar_brake_force;

    m_actor->m_num_proped_wheels=0;

    m_actor->ar_speedo_max_kph=140;

    // Camera at index 0 is main camera which uses nodenums 0 as fallback.
    m_actor->ar_cameras.push_back(camera_t(NodeNum_t(0), NodeNum_t(0), NodeNum_t(0)));

#ifdef USE_ANGELSCRIPT
    m_actor->ar_vehicle_ai = new VehicleAI(m_actor);
#endif // USE_ANGELSCRIPT

    m_actor->ar_airbrake_intensity = 0;
    m_actor->alb_minspeed = 0.0f;
    m_actor->alb_mode = false;
    m_actor->alb_nodash = true;
    m_actor->alb_notoggle = false;
    m_actor->alb_pulse_time = 2000.0f;
    m_actor->alb_pulse_state = false;
    m_actor->alb_ratio = 1.0f;
    m_actor->alb_timer = 0.0f;
    m_actor->ar_anim_shift_timer = 0.0f;

    m_actor->cc_mode = false;
    m_actor->cc_can_brake = false;
    m_actor->cc_target_rpm = 0.0f;
    m_actor->cc_target_speed = 0.0f;
    m_actor->cc_target_speed_lower_limit = 0.0f;

    m_actor->ar_collision_relevant = false;

    m_actor->ar_hide_in_actor_list = false;

    m_actor->ar_anim_previous_crank = 0.f;

    m_actor->sl_enabled = false;
    m_actor->sl_speed_limit = 0.f;

    m_actor->tc_mode = false;
    m_actor->tc_nodash = true;
    m_actor->tc_notoggle = false;
    m_actor->tc_pulse_time = 2000.0f;
    m_actor->tc_pulse_state = false;
    m_actor->tc_ratio = 1.f;
    m_actor->tc_timer = 0.f;

    m_actor->ar_dashboard = new DashBoardManager();

    /* Collisions */

    if (!App::sim_no_collisions->getBool())
    {
        m_actor->m_inter_point_col_detector = new PointColDetector(m_actor);
    }

    if (!App::sim_no_self_collisions->getBool())
    {
        m_actor->m_intra_point_col_detector = new PointColDetector(m_actor);
    }

    m_actor->ar_submesh_ground_model = App::GetGameContext()->GetTerrain()->GetCollisions()->defaultgm;

    // Lights mode
    m_actor->m_flares_mode = App::gfx_flares_mode->getEnum<GfxFlaresMode>();

    m_actor->m_definition = m_file;

    m_flex_factory = RoR::FlexFactory(this);

    m_flex_factory.CheckAndLoadFlexbodyCache();

    m_placeholder_managedmat = Ogre::MaterialManager::getSingleton().getByName("rigsofrods/managedmaterial-placeholder"); // Built-in

    m_apply_simple_materials = App::diag_simple_materials->getBool();
    if (m_apply_simple_materials)
    {
        m_simple_material_base = Ogre::MaterialManager::getSingleton().getByName("tracks/simple"); // Built-in material
        if (m_simple_material_base.isNull())
        {
            App::GetConsole()->putMessage(Console::CONSOLE_MSGTYPE_ACTOR, Console::CONSOLE_SYSTEM_WARNING,
                "Failed to load built-in material 'tracks/simple'; disabling 'SimpleMaterials'");
            m_apply_simple_materials = false;
        }
    }

    m_curr_mirror_prop_type = CustomMaterial::MirrorPropType::MPROP_NONE;
    m_curr_mirror_prop_scenenode = nullptr;
}

void ActorSpawner::FinalizeRig()
{
    // we should post-process the torque curve if existing
    if (m_actor->ar_engine)
    {
        int result = m_actor->ar_engine->getTorqueCurve()->spaceCurveEvenly(m_actor->ar_engine->getTorqueCurve()->getUsedSpline());
        if (result)
        {
            m_actor->ar_engine->getTorqueCurve()->setTorqueModel("default");
            if (result == 1)
            {
                AddMessage(Message::TYPE_ERROR, "TorqueCurve: Points (rpm) must be in an ascending order. Using default curve");
            }
        }

        //Gearbox
        m_actor->ar_engine->SetAutoMode(App::sim_gearbox_mode->getEnum<SimGearboxMode>());
    }
    
    // Sanitize trigger_cmdshort and trigger_cmdlong
    for (size_t i=0; i<m_actor->ar_beams.size(); i++)
    {
        if (m_actor->ar_beams[i].bm_shockid != SHOCKID_INVALID)
        {
            shock_t& shock = m_actor->ar_shocks[m_actor->ar_beams[i].bm_shockid];
            if ((shock.sk_flags & SHOCK_FLAG_TRG_BLOCKER) || (shock.sk_flags & SHOCK_FLAG_TRG_BLOCKER_A))
            {
                shock.trigger_cmdshort = std::min(shock.trigger_cmdshort, static_cast<int>(m_actor->ar_beams.size() - i - 1));
                shock.trigger_cmdlong = std::min(shock.trigger_cmdlong, static_cast<int>(m_actor->ar_beams.size() - i - 1));
            }
        }
    }

    //calculate gwps height offset
    //get a starting value
    m_actor->ar_posnode_spawn_height=m_actor->ar_nodes[0].RelPosition.y;
    //start at 0 to avoid a crash whith a 1-node truck
    for (int i=0; i<static_cast<int>(m_actor->ar_nodes.size()); i++)
    {
        // scan and store the y-coord for the lowest node of the truck
        if (m_actor->ar_nodes[i].RelPosition.y <= m_actor->ar_posnode_spawn_height)
        {
            m_actor->ar_posnode_spawn_height = m_actor->ar_nodes[i].RelPosition.y;
        }
    }

    // Fix up main camera (index 0)
    camera_t& maincam = m_actor->ar_cameras[0];
    if (maincam.camera_node_pos  == NODENUM_INVALID) { maincam.camera_node_pos  = NodeNum_t(0); }
    if (maincam.camera_node_dir  == NODENUM_INVALID) { maincam.camera_node_dir  = NodeNum_t(0); }
    if (maincam.camera_node_roll == NODENUM_INVALID) { maincam.camera_node_roll = NodeNum_t(0); }

    m_actor->m_has_axles_section = m_actor->m_num_wheel_diffs > 0;

    // Calculate mass of each wheel (without rim)
    for (int i = 0; i < m_actor->ar_num_wheels; i++)
    {
        m_actor->ar_wheels[i].wh_mass = 0.0f;
        for (size_t j = 0; j < m_actor->ar_wheels[i].wh_tire_nodes.size(); j++)
        {
            m_actor->ar_wheels[i].wh_mass += m_actor->ar_nodes[m_actor->ar_wheels[i].wh_tire_nodes[j]].mass;
        }
    }

    if (m_actor->m_num_proped_wheels > 0)
    {
        float proped_wheels_radius_sum = 0.0f;
        for (int i = 0; i < m_actor->ar_num_wheels; i++)
        {
            if (m_actor->ar_wheels[i].wh_propulsed > 0)
            {
                proped_wheels_radius_sum += m_actor->ar_wheels[i].wh_radius;
            }
        }
        m_actor->m_avg_proped_wheel_radius = proped_wheels_radius_sum / m_actor->m_num_proped_wheels;
    }

    // Automatically build interwheel differentials from proped wheel pairs
    if (m_actor->m_num_wheel_diffs == 0)
    {
        for (int i = 1; i < m_actor->m_num_proped_wheels; i++)
        {
            if (i % 2)
            {
                Differential *diff = new Differential();

                diff->di_idx_1 = m_actor->m_proped_wheel_pairs[i - 1];
                diff->di_idx_2 = m_actor->m_proped_wheel_pairs[i - 0];

                diff->AddDifferentialType(VISCOUS_DIFF);

                m_actor->m_wheel_diffs[m_actor->m_num_wheel_diffs] = diff;
                m_actor->m_num_wheel_diffs++;
            }
        }
    }

    // Automatically build interaxle differentials from interwheel differentials pairs
    if (m_actor->m_num_axle_diffs == 0)
    {
        for (int i = 1; i < m_actor->m_num_wheel_diffs; i++)
        {
            if (m_actor->m_transfer_case)
            {
                int a1 = std::min(m_actor->m_transfer_case->tr_ax_1, m_actor->m_transfer_case->tr_ax_2);
                int a2 = std::max(m_actor->m_transfer_case->tr_ax_1, m_actor->m_transfer_case->tr_ax_2);
                if ((a1 == i - 1) && (a2 == i - 0))
                    continue;
            }

            Differential *diff = new Differential();

            diff->di_idx_1 = i - 1;
            diff->di_idx_2 = i - 0;

            if (m_actor->m_has_axles_section)
                diff->AddDifferentialType(LOCKED_DIFF);
            else
                diff->AddDifferentialType(VISCOUS_DIFF);

            m_actor->m_axle_diffs[m_actor->m_num_axle_diffs] = diff;
            m_actor->m_num_axle_diffs++;
        }
    }

    // Automatically build an additional interaxle differential for the transfer case
    if (m_actor->m_transfer_case && m_actor->m_transfer_case->tr_ax_2 >= 0)
    {
        Differential *diff = new Differential();
        diff->di_idx_1 = m_actor->m_transfer_case->tr_ax_1;
        diff->di_idx_2 = m_actor->m_transfer_case->tr_ax_2;
        diff->AddDifferentialType(LOCKED_DIFF);
        m_actor->m_axle_diffs[m_actor->m_num_axle_diffs] = diff;
    }

    if (maincam.camera_node_dir == 0 || maincam.camera_node_dir == NODENUM_INVALID)
    {
        Ogre::Vector3 ref = m_actor->ar_nodes[maincam.camera_node_pos].RelPosition;
        // Step 1: Find a suitable camera node dir
        float max_dist = 0.0f;
        NodeNum_t furthest_node = 0;
        for (int i = 0; i < static_cast<int>(m_actor->ar_nodes.size()); i++)
        {
            float dist = m_actor->ar_nodes[i].RelPosition.squaredDistance(ref);
            if (dist > max_dist)
            {
                max_dist = dist;
                furthest_node = (NodeNum_t)i;
            }
        }
        maincam.camera_node_dir = furthest_node;
        // Step 2: Correct the misalignment
        Ogre::Vector3 dir = m_actor->ar_nodes[furthest_node].RelPosition - ref;
        float offset = atan2(dir.dotProduct(Ogre::Vector3::UNIT_Z), dir.dotProduct(Ogre::Vector3::UNIT_X));
        maincam.camera_dir_corr = Ogre::Quaternion(Ogre::Radian(offset), Ogre::Vector3::UNIT_Y);
    }

    if (maincam.camera_node_pos != NODENUM_INVALID)
    {
        // store the y-difference between the trucks lowest node and the campos-node for the gwps system
        m_actor->ar_posnode_spawn_height = m_actor->ar_nodes[maincam.camera_node_pos].RelPosition.y - m_actor->ar_posnode_spawn_height;
    } 
    else
    {
        //this can not be an airplane, just set it to 0.
        m_actor->ar_posnode_spawn_height = 0.0f;
    }

    //cameras workaround
    for (size_t i=0; i<m_actor->ar_cameras.size(); i++)
    {
        Ogre::Vector3 dir_node_offset = m_actor->ar_nodes[m_actor->ar_cameras[i].camera_node_dir].RelPosition - m_actor->ar_nodes[m_actor->ar_cameras[i].camera_node_pos].RelPosition;
        Ogre::Vector3 roll_node_offset = m_actor->ar_nodes[m_actor->ar_cameras[i].camera_node_roll].RelPosition - m_actor->ar_nodes[m_actor->ar_cameras[i].camera_node_pos].RelPosition;
        Ogre::Vector3 cross = dir_node_offset.crossProduct(roll_node_offset);
        
        m_actor->ar_cameras[i].camera_node_roll_inv=cross.y > 0;
        if (m_actor->ar_cameras[i].camera_node_roll_inv)
        {
            AddMessage(Message::TYPE_WARNING, "camera definition is probably invalid and has been corrected. It should be center, back, left");
        }
    }
    
    //wing closure
    if (m_first_wing_index!=WINGID_INVALID)
    {
        if (m_actor->ar_autopilot != nullptr) 
        {
            m_actor->ar_autopilot->setInertialReferences(
                m_airplane_left_light,
                m_airplane_right_light,
                m_actor->m_fusealge_back,
                m_actor->ar_cameras[0].camera_node_pos
                );
        }
        //inform wing segments
        float span=m_actor->ar_nodes[m_actor->ar_wings[m_first_wing_index].fa->nfrd].RelPosition.distance(m_actor->ar_nodes[m_actor->ar_wings[m_actor->ar_wings.size()-1].fa->nfld].RelPosition);
        
        m_actor->ar_wings[m_first_wing_index].fa->enableInducedDrag(span,m_wing_area, false);
        m_actor->ar_wings[m_actor->ar_wings.size()-1].fa->enableInducedDrag(span,m_wing_area, true);
        //wash calculator
        WashCalculator();
    }

    this->UpdateCollcabContacterNodes();

    m_flex_factory.SaveFlexbodiesToCache();

    m_actor->GetGfxActor()->SortFlexbodies();
}

/* -------------------------------------------------------------------------- */
// Processing functions and utilities.
/* -------------------------------------------------------------------------- */

void ActorSpawner::WashCalculator()
{
    //we will compute wash
    int w,p;
    for (p=0; p<m_actor->ar_num_aeroengines; p++)
    {
        Ogre::Vector3 prop=m_actor->ar_nodes[m_actor->ar_aeroengines[p]->getNoderef()].RelPosition;
        float radius=m_actor->ar_aeroengines[p]->getRadius();
        for (w=0; w<m_actor->ar_wings.size(); w++)
        {
            //left wash
            Ogre::Vector3 wcent=((m_actor->ar_nodes[m_actor->ar_wings[w].fa->nfld].RelPosition+m_actor->ar_nodes[m_actor->ar_wings[w].fa->nfrd].RelPosition)/2.0);
            //check if wing is near enough along X (less than 15m back)
            if (wcent.x>prop.x && wcent.x<prop.x+15.0)
            {
                //check if it's okay vertically
                if (wcent.y>prop.y-radius && wcent.y<prop.y+radius)
                {
                    //okay, compute wash coverage ratio along Z
                    float wleft=(m_actor->ar_nodes[m_actor->ar_wings[w].fa->nfld].RelPosition).z;
                    float wright=(m_actor->ar_nodes[m_actor->ar_wings[w].fa->nfrd].RelPosition).z;
                    float pleft=prop.z+radius;
                    float pright=prop.z-radius;
                    float aleft=wleft;
                    if (pleft<aleft) aleft=pleft;
                    float aright=wright;
                    if (pright>aright) aright=pright;
                    if (aright<aleft)
                    {
                        //we have a wash
                        float wratio=(aleft-aright)/(wleft-wright);
                        m_actor->ar_wings[w].fa->addwash(p, wratio);
                        Ogre::String msg = "Wing "+TOSTRING(w)+" is washed by prop "+TOSTRING(p)+" at "+TOSTRING((float)(wratio*100.0))+"%";
                        AddMessage(Message::TYPE_INFO, msg);
                    }
                }
            }
        }
    }
}

void ActorSpawner::ProcessTurbojet(RigDef::Turbojet & def)
{
    NodeNum_t front,back,ref;
    front = GetNodeIndexOrThrow(def.front_node);
    back  = GetNodeIndexOrThrow(def.back_node);
    ref   = GetNodeIndexOrThrow(def.side_node);

    Turbojet *tj = new Turbojet(m_actor, front, back, ref, def);

    // Visuals
    std::string nozzle_name = this->ComposeName("TurbojetNozzle", m_actor->ar_num_aeroengines);
    Ogre::Entity* nozzle_ent = App::GetGfxScene()->GetSceneManager()->createEntity(nozzle_name, "nozzle.mesh", m_custom_resource_group);
    this->SetupNewEntity(nozzle_ent, Ogre::ColourValue(1, 0.5, 0.5));
    Ogre::Entity* afterburn_ent = nullptr;
    if (def.wet_thrust > 0.f)
    {
        std::string flame_name = this->ComposeName("AfterburnerFlame", m_actor->ar_num_aeroengines);
        afterburn_ent = App::GetGfxScene()->GetSceneManager()->createEntity(flame_name, "abflame.mesh", m_custom_resource_group);
        this->SetupNewEntity(afterburn_ent, Ogre::ColourValue(1, 1, 0));
    }
    std::string propname = this->ComposeName("Turbojet", m_actor->ar_num_aeroengines);
    tj->tjet_visual.SetNodes(front, back, ref);
    tj->tjet_visual.SetupVisuals(def, m_actor->ar_num_aeroengines,
        propname, nozzle_ent, afterburn_ent);
    if (!m_actor->m_disable_smoke)
    {
        tj->tjet_visual.SetVisible(true);
    }

    m_actor->ar_aeroengines[m_actor->ar_num_aeroengines]=tj;
    m_actor->ar_driveable=AIRPLANE;
    if (m_actor->ar_autopilot == nullptr && m_actor->ar_state != ActorState::NETWORKED_OK)
    {
        m_actor->ar_autopilot=new Autopilot(m_actor);
    }

    m_actor->ar_num_aeroengines++;
}

std::string ActorSpawner::ComposeName(const char* type, int number)
{
    char buf[500];
    snprintf(buf, 500, "%s_%d%s%d", type, number, ACTOR_ID_TOKEN, m_actor->ar_instance_id);
    return buf;
}

void ActorSpawner::ProcessScrewprop(RigDef::Screwprop & def)
{
    if (! CheckScrewpropLimit(1))
    {
        return;
    }

    m_actor->ar_screwprops[m_actor->ar_num_screwprops] = new Screwprop(
        m_actor,
        GetNodeIndexOrThrow(def.prop_node),
        GetNodeIndexOrThrow(def.back_node),
        GetNodeIndexOrThrow(def.top_node),
        def.power
    );
    m_actor->ar_driveable=BOAT;
    m_actor->ar_num_screwprops++;
}

void ActorSpawner::ProcessFusedrag(RigDef::Fusedrag & def)
{
    //parse fusedrag
    NodeNum_t front_node_idx = GetNodeIndexOrThrow(def.front_node);
    float width = 1.f;
    float factor = 1.f;
    char fusefoil[256];
    strncpy(fusefoil, def.airfoil_name.c_str(), 255);

    if (def.autocalc)
    {
        // fusedrag autocalculation

        // calculate fusedrag by truck size
        factor = def.area_coefficient;
        width  =  (m_fuse_z_max - m_fuse_z_min) * (m_fuse_y_max - m_fuse_y_min) * factor;

        m_actor->m_fusealge_airfoil = new Airfoil(fusefoil);

        m_actor->m_fusealge_front   = front_node_idx;
        m_actor->m_fusealge_back    = front_node_idx; // This equals v0.38 / v0.4.0.7, but it's probably a bug
        m_actor->m_fusealge_width   = width;
        AddMessage(Message::TYPE_INFO, "Fusedrag autocalculation size: "+TOSTRING(width)+" m^2");
    } 
    else
    {
        // original fusedrag calculation

        width  = def.approximate_width;

        m_actor->m_fusealge_airfoil = new Airfoil(fusefoil);

        m_actor->m_fusealge_front   = front_node_idx;
        m_actor->m_fusealge_back    = front_node_idx; // This equals v0.38 / v0.4.0.7, but it's probably a bug
        m_actor->m_fusealge_width   = width;
    }
}

void ActorSpawner::BuildAeroEngine(
    NodeNum_t ref_node_index,
    NodeNum_t back_node_index,
    NodeNum_t blade_1_node_index,
    NodeNum_t blade_2_node_index,
    NodeNum_t blade_3_node_index,
    NodeNum_t blade_4_node_index,
    NodeNum_t couplenode_index,
    bool is_turboprops,
    Ogre::String const & airfoil,
    float power,
    float pitch
    )
{
    int aeroengine_index = m_actor->ar_num_aeroengines;

    Turboprop *turbo_prop = new Turboprop(
        m_actor,
        this->ComposeName("Turboprop", aeroengine_index).c_str(),
        ref_node_index,
        back_node_index,
        blade_1_node_index,
        blade_2_node_index,
        blade_3_node_index,
        blade_4_node_index,
        couplenode_index,
        power,
        airfoil,
        m_actor->m_disable_smoke,
        ! is_turboprops,
        pitch
    );

    m_actor->ar_aeroengines[m_actor->ar_num_aeroengines] = turbo_prop;
    m_actor->ar_num_aeroengines++;
    m_actor->ar_driveable = AIRPLANE;

    /* Autopilot */
    if (m_actor->ar_autopilot == nullptr && m_actor->ar_state != ActorState::NETWORKED_OK)
    {
        m_actor->ar_autopilot = new Autopilot(m_actor);
    }

    /* Visuals */
    float scale = m_actor->ar_nodes[ref_node_index].RelPosition.distance(m_actor->ar_nodes[blade_1_node_index].RelPosition) / 2.25f;
    for (RoR::Prop& prop: m_actor->m_gfx_actor->m_props)
    {
        if ((prop.pp_node_ref == ref_node_index) && (prop.pp_aero_propeller_blade || prop.pp_aero_propeller_spin))
        {
            prop.pp_scene_node->scale(scale, scale, scale);
            prop.pp_aero_engine_idx = aeroengine_index;
        }
    }
}

void ActorSpawner::ProcessTurboprop2(RigDef::Turboprop2 & def)
{
    const NodeNum_t p3_node_index = (def.blade_tip_nodes[2].IsValidAnyState()) ? GetNodeIndexOrThrow(def.blade_tip_nodes[2]) : -1;
    const NodeNum_t p4_node_index = (def.blade_tip_nodes[3].IsValidAnyState()) ? GetNodeIndexOrThrow(def.blade_tip_nodes[3]) : -1;
    const NodeNum_t couple_node_index = (def.couple_node.IsValidAnyState()) ? GetNodeIndexOrThrow(def.couple_node) : -1;

    BuildAeroEngine(
        GetNodeIndexOrThrow(def.reference_node),
        GetNodeIndexOrThrow(def.axis_node),
        GetNodeIndexOrThrow(def.blade_tip_nodes[0]),
        GetNodeIndexOrThrow(def.blade_tip_nodes[1]),
        p3_node_index,
        p4_node_index,
        couple_node_index,
        true,
        def.airfoil,
        def.turbine_power_kW,
        -10
    );
}

void ActorSpawner::ProcessDescription(Ogre::String const& line)
{
    m_actor->m_description.push_back(line);
}

void ActorSpawner::ProcessPistonprop(RigDef::Pistonprop & def)
{
    const NodeNum_t p3_node_index = (def.blade_tip_nodes[2].IsValidAnyState()) ? GetNodeIndexOrThrow(def.blade_tip_nodes[2]) : -1;
    const NodeNum_t p4_node_index = (def.blade_tip_nodes[3].IsValidAnyState()) ? GetNodeIndexOrThrow(def.blade_tip_nodes[3]) : -1;
    const NodeNum_t couple_node_index = (def.couple_node.IsValidAnyState()) ? GetNodeIndexOrThrow(def.couple_node) : -1;

    BuildAeroEngine(
        GetNodeIndexOrThrow(def.reference_node),
        GetNodeIndexOrThrow(def.axis_node),
        GetNodeIndexOrThrow(def.blade_tip_nodes[0]),
        GetNodeIndexOrThrow(def.blade_tip_nodes[1]),
        p3_node_index,
        p4_node_index,
        couple_node_index,
        false,
        def.airfoil,
        def.turbine_power_kW,
        def.pitch
    );
}

void ActorSpawner::ProcessAirbrake(RigDef::Airbrake & def)
{
    const int airbrake_idx = static_cast<int>(m_actor->ar_airbrakes.size());
    Airbrake* ab = new Airbrake(
        m_actor,
        this->ComposeName("Airbrake", airbrake_idx).c_str(),
        airbrake_idx,
        ResolveNodeRef(def.reference_node),
        ResolveNodeRef(def.x_axis_node),
        ResolveNodeRef(def.y_axis_node),
        ResolveNodeRef(def.aditional_node),
        def.offset,
        def.width,
        def.height,
        def.max_inclination_angle,
        m_cab_material_name,
        def.texcoord_x1,
        def.texcoord_y1,
        def.texcoord_x2,
        def.texcoord_y2,
        def.lift_coefficient
    );
    m_actor->ar_airbrakes.push_back(ab);

    AirbrakeGfx abx;
    // entity
    abx.abx_entity = ab->ec;
    ab->ec = nullptr;
    // mesh
    abx.abx_mesh = ab->msh;
    ab->msh.setNull();
    // scenenode
    abx.abx_scenenode = ab->snode;
    ab->snode = nullptr;
    // offset
    abx.abx_offset = ab->offset;
    ab->offset = Ogre::Vector3::ZERO;
    // Nodes - just copy
    abx.abx_ref_node = ab->noderef;
    abx.abx_x_node = ab->nodex;
    abx.abx_y_node = ab->nodey;

    m_actor->m_gfx_actor->m_gfx_airbrakes.push_back(abx);
}

void ActorSpawner::ProcessWing(RigDef::Wing & def)
{
    if ((m_first_wing_index != WINGID_INVALID) && (m_actor->ar_wings[m_actor->ar_wings.size() - 1].fa == nullptr))
    {
        this->AddMessage(Message::TYPE_ERROR, "Unable to process wing, previous wing has no Airfoil");
        return;
    }

    if (def.airfoil == "") // May happen for malformed `wings` entry in truckfile
    {
        this->AddMessage(Message::TYPE_ERROR, "Unable to process wing, no Airfoil defined");
        return;
    }

    m_actor->GetGfxActor()->UpdateSimDataBuffer(); // fill all current nodes - needed to setup flexing meshes

    NodeNum_t node1 = this->GetNodeIndexOrThrow(def.nodes[1]);

    const std::string wing_name = this->ComposeName("Wing", m_actor->ar_wings.size());
    auto flex_airfoil = new FlexAirfoil(
        wing_name,
        m_actor,
        this->GetNodeIndexOrThrow(def.nodes[0]),
        node1,
        this->GetNodeIndexOrThrow(def.nodes[2]),
        this->GetNodeIndexOrThrow(def.nodes[3]),
        this->GetNodeIndexOrThrow(def.nodes[4]),
        this->GetNodeIndexOrThrow(def.nodes[5]),
        this->GetNodeIndexOrThrow(def.nodes[6]),
        this->GetNodeIndexOrThrow(def.nodes[7]),
        m_cab_material_name,
        Ogre::Vector2(def.tex_coords[0], def.tex_coords[1]),
        Ogre::Vector2(def.tex_coords[2], def.tex_coords[3]),
        Ogre::Vector2(def.tex_coords[4], def.tex_coords[5]),
        Ogre::Vector2(def.tex_coords[6], def.tex_coords[7]),
        (char)def.control_surface,
        def.chord_point,
        def.min_deflection,
        def.max_deflection,
        def.airfoil,
        def.efficacy_coef,
        m_actor->ar_state != ActorState::NETWORKED_OK
    );

    Ogre::Entity* entity = nullptr;
    try
    {
        const std::string wing_instance_name = this->ComposeName("WingEntity", m_actor->ar_wings.size());
        entity = App::GetGfxScene()->GetSceneManager()->createEntity(wing_instance_name, wing_name);
        m_actor->m_deletion_entities.emplace_back(entity);
        this->SetupNewEntity(entity, Ogre::ColourValue(0.5, 1, 0));
    }
    catch (...)
    {
        this->AddMessage(Message::TYPE_ERROR, std::string("Failed to load mesh (flexbody wing): ") + wing_name);
        delete flex_airfoil;
        return;
    }

    // induced drag
    if (m_first_wing_index == WINGID_INVALID)
    {
        m_first_wing_index = static_cast<WingID_t>(m_actor->ar_wings.size());
        m_wing_area=ComputeWingArea(
            m_actor->ar_nodes[flex_airfoil->nfld].AbsPosition,    m_actor->ar_nodes[flex_airfoil->nfrd].AbsPosition,
            m_actor->ar_nodes[flex_airfoil->nbld].AbsPosition,    m_actor->ar_nodes[flex_airfoil->nbrd].AbsPosition
        );
    }
    else
    {
        wing_t & previous_wing = m_actor->ar_wings[m_actor->ar_wings.size() - 1];

        if (node1 != previous_wing.fa->nfld)
        {
            wing_t & start_wing    = m_actor->ar_wings[m_first_wing_index];

            //discontinuity
            //inform wing segments
            float span = m_actor->ar_nodes[start_wing.fa->nfrd].RelPosition.distance(m_actor->ar_nodes[previous_wing.fa->nfld].RelPosition );
            
            start_wing.fa->enableInducedDrag(span, m_wing_area, false);
            previous_wing.fa->enableInducedDrag(span, m_wing_area, true);

            //we want also to add positional lights for first wing
            if (m_generate_wing_position_lights && (m_actor->m_flares_mode != GfxFlaresMode::NONE))
            {
                //Left green
                m_airplane_left_light=previous_wing.fa->nfld;
                RoR::Prop left_green_prop;

                left_green_prop.pp_node_ref=previous_wing.fa->nfld;
                left_green_prop.pp_node_x=previous_wing.fa->nflu;
                left_green_prop.pp_node_y=previous_wing.fa->nfld; //ignored
                left_green_prop.pp_offset.x=0.5;
                left_green_prop.pp_offset.y=0.0;
                left_green_prop.pp_offset.z=0.0;
                left_green_prop.pp_beacon_rot_angle[0]=0.0;
                left_green_prop.pp_beacon_rot_rate[0]=1.0;
                left_green_prop.pp_beacon_type='L';
                left_green_prop.pp_beacon_light[0]=nullptr; //no light
                //the flare billboard
                left_green_prop.pp_beacon_scene_node[0] = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
                left_green_prop.pp_beacon_bbs[0]=App::GetGfxScene()->GetSceneManager()->createBillboardSet(this->ComposeName("Prop", static_cast<int>(m_actor->m_gfx_actor->m_props.size())+1),1);
                left_green_prop.pp_beacon_bbs[0]->createBillboard(0,0,0);
                if (left_green_prop.pp_beacon_bbs[0])
                {
                    left_green_prop.pp_beacon_bbs[0]->setVisibilityFlags(DEPTHMAP_DISABLED);
                    left_green_prop.pp_beacon_bbs[0]->setMaterialName("tracks/greenflare");
                    left_green_prop.pp_beacon_scene_node[0]->attachObject(left_green_prop.pp_beacon_bbs[0]);
                }
                left_green_prop.pp_beacon_scene_node[0]->setVisible(false);
                left_green_prop.pp_beacon_bbs[0]->setDefaultDimensions(0.5, 0.5);
                m_actor->m_gfx_actor->m_props.push_back(left_green_prop);
                
                //Left flash
                RoR::Prop left_flash_prop;

                left_flash_prop.pp_node_ref=previous_wing.fa->nbld;
                left_flash_prop.pp_node_x=previous_wing.fa->nblu;
                left_flash_prop.pp_node_y=previous_wing.fa->nbld; //ignored
                left_flash_prop.pp_offset.x=0.5;
                left_flash_prop.pp_offset.y=0.0;
                left_flash_prop.pp_offset.z=0.0;
                left_flash_prop.pp_beacon_rot_angle[0]=0.5; //alt
                left_flash_prop.pp_beacon_rot_rate[0]=1.0;
                left_flash_prop.pp_beacon_type='w';
                //light
                std::string prop_name = this->ComposeName("Prop", static_cast<int>(m_actor->m_gfx_actor->m_props.size())+1);
                left_flash_prop.pp_beacon_light[0]=App::GetGfxScene()->GetSceneManager()->createLight(prop_name);
                left_flash_prop.pp_beacon_light[0]->setType(Ogre::Light::LT_POINT);
                left_flash_prop.pp_beacon_light[0]->setDiffuseColour( Ogre::ColourValue(1.0, 1.0, 1.0));
                left_flash_prop.pp_beacon_light[0]->setSpecularColour( Ogre::ColourValue(1.0, 1.0, 1.0));
                left_flash_prop.pp_beacon_light[0]->setAttenuation(50.0, 1.0, 0.3, 0.0);
                left_flash_prop.pp_beacon_light[0]->setCastShadows(false);
                left_flash_prop.pp_beacon_light[0]->setVisible(false);
                //the flare billboard
                left_flash_prop.pp_beacon_scene_node[0] = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
                left_flash_prop.pp_beacon_bbs[0]=App::GetGfxScene()->GetSceneManager()->createBillboardSet(prop_name,1);
                left_flash_prop.pp_beacon_bbs[0]->createBillboard(0,0,0);
                if (left_flash_prop.pp_beacon_bbs[0])
                {
                    left_flash_prop.pp_beacon_bbs[0]->setVisibilityFlags(DEPTHMAP_DISABLED);
                    left_flash_prop.pp_beacon_bbs[0]->setMaterialName("tracks/flare");
                    left_flash_prop.pp_beacon_scene_node[0]->attachObject(left_flash_prop.pp_beacon_bbs[0]);
                }
                left_flash_prop.pp_beacon_scene_node[0]->setVisible(false);
                left_flash_prop.pp_beacon_bbs[0]->setDefaultDimensions(1.0, 1.0);
                m_actor->m_gfx_actor->m_props.push_back(left_flash_prop);
                
                //Right red
                m_airplane_right_light=previous_wing.fa->nfrd;
                RoR::Prop right_red_prop;

                
                right_red_prop.pp_node_ref=start_wing.fa->nfrd;
                right_red_prop.pp_node_x=start_wing.fa->nfru;
                right_red_prop.pp_node_y=start_wing.fa->nfrd; //ignored
                right_red_prop.pp_offset.x=0.5;
                right_red_prop.pp_offset.y=0.0;
                right_red_prop.pp_offset.z=0.0;
                right_red_prop.pp_beacon_rot_angle[0]=0.0;
                right_red_prop.pp_beacon_rot_rate[0]=1.0;
                right_red_prop.pp_beacon_type='R';
                right_red_prop.pp_beacon_light[0]=nullptr; /* No light */
                //the flare billboard
                right_red_prop.pp_beacon_scene_node[0] = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
                right_red_prop.pp_beacon_bbs[0]=App::GetGfxScene()->GetSceneManager()->createBillboardSet(this->ComposeName("Prop", static_cast<int>(m_actor->m_gfx_actor->m_props.size())+1),1);
                right_red_prop.pp_beacon_bbs[0]->createBillboard(0,0,0);
                if (right_red_prop.pp_beacon_bbs[0])
                {
                    right_red_prop.pp_beacon_bbs[0]->setVisibilityFlags(DEPTHMAP_DISABLED);
                    right_red_prop.pp_beacon_bbs[0]->setMaterialName("tracks/redflare");
                    right_red_prop.pp_beacon_scene_node[0]->attachObject(right_red_prop.pp_beacon_bbs[0]);
                }
                right_red_prop.pp_beacon_scene_node[0]->setVisible(false);
                right_red_prop.pp_beacon_bbs[0]->setDefaultDimensions(0.5, 0.5);
                m_actor->m_gfx_actor->m_props.push_back(right_red_prop);
                
                //Right flash
                RoR::Prop right_flash_prop;

                right_flash_prop.pp_node_ref=start_wing.fa->nbrd;
                right_flash_prop.pp_node_x=start_wing.fa->nbru;
                right_flash_prop.pp_node_y=start_wing.fa->nbrd; //ignored
                right_flash_prop.pp_offset.x=0.5;
                right_flash_prop.pp_offset.y=0.0;
                right_flash_prop.pp_offset.z=0.0;
                right_flash_prop.pp_beacon_rot_angle[0]=0.5; //alt
                right_flash_prop.pp_beacon_rot_rate[0]=1.0;
                right_flash_prop.pp_beacon_type='w';
                //light
                prop_name = this->ComposeName("Prop", static_cast<int>(m_actor->m_gfx_actor->m_props.size())+1);
                right_flash_prop.pp_beacon_light[0]=App::GetGfxScene()->GetSceneManager()->createLight(prop_name);
                right_flash_prop.pp_beacon_light[0]->setType(Ogre::Light::LT_POINT);
                right_flash_prop.pp_beacon_light[0]->setDiffuseColour( Ogre::ColourValue(1.0, 1.0, 1.0));
                right_flash_prop.pp_beacon_light[0]->setSpecularColour( Ogre::ColourValue(1.0, 1.0, 1.0));
                right_flash_prop.pp_beacon_light[0]->setAttenuation(50.0, 1.0, 0.3, 0.0);
                right_flash_prop.pp_beacon_light[0]->setCastShadows(false);
                right_flash_prop.pp_beacon_light[0]->setVisible(false);
                //the flare billboard
                right_flash_prop.pp_beacon_scene_node[0] = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
                right_flash_prop.pp_beacon_bbs[0]=App::GetGfxScene()->GetSceneManager()->createBillboardSet(prop_name,1);
                right_flash_prop.pp_beacon_bbs[0]->createBillboard(0,0,0);
                if (right_flash_prop.pp_beacon_bbs[0] != nullptr)
                {
                    right_flash_prop.pp_beacon_bbs[0]->setVisibilityFlags(DEPTHMAP_DISABLED);
                    right_flash_prop.pp_beacon_bbs[0]->setMaterialName("tracks/flare");
                    right_flash_prop.pp_beacon_scene_node[0]->attachObject(right_flash_prop.pp_beacon_bbs[0]);
                }
                right_flash_prop.pp_beacon_scene_node[0]->setVisible(false);
                right_flash_prop.pp_beacon_bbs[0]->setDefaultDimensions(1.0, 1.0);
                m_actor->m_gfx_actor->m_props.push_back(right_flash_prop);
                
                m_generate_wing_position_lights = false; // Already done
            }

            m_first_wing_index = m_actor->ar_wings.size();
            m_wing_area=ComputeWingArea(
                m_actor->ar_nodes[flex_airfoil->nfld].AbsPosition,    m_actor->ar_nodes[flex_airfoil->nfrd].AbsPosition,
                m_actor->ar_nodes[flex_airfoil->nbld].AbsPosition,    m_actor->ar_nodes[flex_airfoil->nbrd].AbsPosition
            );
        }
        else 
        {
            m_wing_area+=ComputeWingArea(
                m_actor->ar_nodes[flex_airfoil->nfld].AbsPosition,    m_actor->ar_nodes[flex_airfoil->nfrd].AbsPosition,
                m_actor->ar_nodes[flex_airfoil->nbld].AbsPosition,    m_actor->ar_nodes[flex_airfoil->nbrd].AbsPosition
            );
        }
    }

    // Add new wing to rig
    wing_t wing;
    wing.fa = flex_airfoil;
    wing.cnode = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
    wing.cnode->attachObject(entity);
    m_actor->ar_wings.push_back(wing);
}

float ActorSpawner::ComputeWingArea(Ogre::Vector3 const & ref, Ogre::Vector3 const & x, Ogre::Vector3 const & y, Ogre::Vector3 const & aref)
{
    return (((x-ref).crossProduct(y-ref)).length()+((x-aref).crossProduct(y-aref)).length())*0.5f;
}

void ActorSpawner::ProcessSoundSource2(RigDef::SoundSource2 & def)
{
#ifdef USE_OPENAL
    NodeNum_t node = ResolveNodeRef(def.node);
    if (node == NODENUM_INVALID)
    {
        return;
    }
    AddSoundSource(
            m_actor,
            App::GetSoundScriptManager()->createInstance(def.sound_script_name, m_actor->ar_instance_id), 
            node,
            def.mode
        );
#endif // USE_OPENAL
}

void ActorSpawner::AddSoundSourceInstance(ActorPtr const& vehicle, Ogre::String const & sound_script_name, int node_index, int type)
{
#ifdef USE_OPENAL
    AddSoundSource(vehicle, App::GetSoundScriptManager()->createInstance(sound_script_name, vehicle->ar_instance_id), (NodeNum_t)node_index);
#endif // USE_OPENAL
}

void ActorSpawner::AddSoundSource(ActorPtr const& vehicle, SoundScriptInstancePtr ssi, NodeNum_t node_index, int type)
{
    if (ssi)
    {
        vehicle->ar_soundsources.emplace_back(ssi, node_index, type);
    }
}

void ActorSpawner::ProcessSoundSource(RigDef::SoundSource & def)
{
#ifdef USE_OPENAL
    AddSoundSource(
            m_actor,
            App::GetSoundScriptManager()->createInstance(def.sound_script_name, m_actor->ar_instance_id), 
            GetNodeIndexOrThrow(def.node),
            -2
        );
#endif // USE_OPENAL
}

void ActorSpawner::ProcessCameraRail(RigDef::CameraRail & def)
{
    auto itor = def.nodes.begin();
    auto end  = def.nodes.end();
    for(; itor != end; ++itor)
    {
        if (! CheckCameraRailLimit(1))
        {
            return;
        }
        m_actor->ar_camera_rail[m_actor->ar_num_camera_rails] = GetNodeIndexOrThrow(*itor);
        m_actor->ar_num_camera_rails++;
    }
}

void ActorSpawner::ProcessExtCamera(RigDef::ExtCamera & def)
{
    m_actor->ar_extern_camera_mode = def.mode;
    if (def.node.IsValidAnyState())
    {
        m_actor->ar_extern_camera_node = GetNodeIndexOrThrow(def.node);
    }
}

void ActorSpawner::ProcessGuiSettings(RigDef::GuiSettings & def)
{
    if (def.key == "helpMaterial")
    {
        m_help_material_name = (def.value != "") ? def.value : m_help_material_name;
    }
    else if (def.key == "speedoMax")
    {
        float maxKph = PARSEREAL(def.value);
        if (maxKph > 10 && maxKph < 32000)
        {
            m_actor->ar_speedo_max_kph = maxKph;
        }
        else
        {
            this->AddMessage(Message::TYPE_ERROR,
                fmt::format("Invalid 'speedoMax' ({}), allowed range is <10 -32000>, using default ({})", maxKph, DEFAULT_SPEEDO_MAX_KPH));
            m_actor->ar_speedo_max_kph = DEFAULT_SPEEDO_MAX_KPH;
        }
    }
    else if (def.key == "useMaxRPM")
    {
        m_actor->ar_gui_use_engine_max_rpm = true;
    }

    // NOTE: Dashboard layouts are processed later
}

void ActorSpawner::ProcessFixedNode(RigDef::Node::Ref node_ref)
{
    NodeNum_t node = GetNodeIndexOrThrow(node_ref);
    m_actor->ar_nodes[node].nd_immovable = true;
}

void ActorSpawner::ProcessExhaust(RigDef::Exhaust & def)
{
    if (m_actor->m_disable_smoke)
    {
        return;
    }

    exhaust_t exhaust;
    exhaust.emitterNode   = this->GetNodeIndexOrThrow(def.reference_node);
    exhaust.directionNode = this->GetNodeIndexOrThrow(def.direction_node);

    std::string template_name = def.particle_name;
    if (template_name.empty() || template_name == "default")
    {
        template_name = "tracks/Smoke"; // defined in `particles/smoke.particle`
    }

    std::string name = this->ComposeName(template_name.c_str(), static_cast<int>(m_actor->exhausts.size()));
    exhaust.smoker = this->CreateParticleSystem(name, template_name);
    if (exhaust.smoker == nullptr)
    {
        std::stringstream msg;
        msg << "Failed to create particle system '" << name << "' (template: '" << template_name <<"')";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return;
    }

    exhaust.smokeNode = m_particles_parent_scenenode->createChildSceneNode();
    exhaust.smokeNode->attachObject(exhaust.smoker);
    exhaust.smokeNode->setPosition(m_actor->ar_nodes[exhaust.emitterNode].AbsPosition);

    m_actor->m_gfx_actor->SetNodeHot(exhaust.emitterNode, true);
    m_actor->m_gfx_actor->SetNodeHot(exhaust.directionNode, true);

    m_actor->exhausts.push_back(exhaust);
}

std::string ActorSpawner::GetSubmeshGroundmodelName()
{
    auto module_itor = m_selected_modules.begin();
    auto module_end  = m_selected_modules.end();
    for (; module_itor != module_end; ++module_itor)
    {
        if (! module_itor->get()->submesh_groundmodel.empty())
        {
            return module_itor->get()->submesh_groundmodel[0];
        }
    }
    return std::string();
};

void ActorSpawner::ProcessSubmesh(RigDef::Submesh & def)
{
    if (! CheckSubmeshLimit(1))
    {
        return;
    }

    /* TEXCOORDS */

    std::vector<RigDef::Texcoord>::iterator texcoord_itor = def.texcoords.begin();
    for ( ; texcoord_itor != def.texcoords.end(); texcoord_itor++)
    {
        if (! CheckTexcoordLimit(1))
        {
            break;
        }

        CabTexcoord texcoord;
        texcoord.node_id    = GetNodeIndexOrThrow(texcoord_itor->node);
        texcoord.texcoord_u = texcoord_itor->u;
        texcoord.texcoord_v = texcoord_itor->v;
        m_oldstyle_cab_texcoords.push_back(texcoord);
    }

    /* CAB */

    auto cab_itor = def.cab_triangles.begin();
    auto cab_itor_end = def.cab_triangles.end();
    for ( ; cab_itor != cab_itor_end; ++cab_itor)
    {
        if (! CheckCabLimit(1))
        {
            return;
        }
        else if (m_actor->ar_num_collcabs >= MAX_CABS)
        {
            std::stringstream msg;
            msg << "Collcab limit (" << MAX_CABS << ") exceeded";
            AddMessage(Message::TYPE_ERROR, msg.str());
            return;
        }

        bool mk_buoyance = false;

        m_actor->ar_cabs[m_actor->ar_num_cabs*3]=GetNodeIndexOrThrow(cab_itor->nodes[0]);
        m_actor->ar_cabs[m_actor->ar_num_cabs*3+1]=GetNodeIndexOrThrow(cab_itor->nodes[1]);
        m_actor->ar_cabs[m_actor->ar_num_cabs*3+2]=GetNodeIndexOrThrow(cab_itor->nodes[2]);

        // TODO: Clean this up properly ~ ulteq 10/2018
        if (BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_c_CONTACT) ||
            BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_p_10xTOUGHER) ||
            BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_u_INVULNERABLE))
        {
            m_actor->ar_collcabs[m_actor->ar_num_collcabs]=m_actor->ar_num_cabs;
            m_actor->ar_num_collcabs++;
        }
        if (BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_b_BUOYANT))
        {
            m_actor->ar_buoycabs[m_actor->ar_num_buoycabs]=m_actor->ar_num_cabs; 
            m_actor->ar_buoycab_types[m_actor->ar_num_buoycabs]=Buoyance::BUOY_NORMAL; 
            m_actor->ar_num_buoycabs++;   
            mk_buoyance = true;
        }
        if (BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_r_BUOYANT_ONLY_DRAG))
        {
            m_actor->ar_buoycabs[m_actor->ar_num_buoycabs]=m_actor->ar_num_cabs; 
            m_actor->ar_buoycab_types[m_actor->ar_num_buoycabs]=Buoyance::BUOY_DRAGONLY; 
            m_actor->ar_num_buoycabs++; 
            mk_buoyance = true;
        }
        if (BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_s_BUOYANT_NO_DRAG))
        {
            m_actor->ar_buoycabs[m_actor->ar_num_buoycabs]=m_actor->ar_num_cabs; 
            m_actor->ar_buoycab_types[m_actor->ar_num_buoycabs]=Buoyance::BUOY_DRAGLESS; 
            m_actor->ar_num_buoycabs++; 
            mk_buoyance = true;
        }

        if (BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_D_CONTACT_BUOYANT) ||
            BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_F_10xTOUGHER_BUOYANT) ||
            BITMASK_IS_1(cab_itor->options, RigDef::Cab::OPTION_S_INVULNERABLE_BUOYANT))
        {

            if (m_actor->ar_num_collcabs >= MAX_CABS)
            {
                std::stringstream msg;
                msg << "Collcab limit (" << MAX_CABS << ") exceeded";
                AddMessage(Message::TYPE_ERROR, msg.str());
                return;
            }
            else if (m_actor->ar_num_buoycabs >= MAX_CABS)
            {
                std::stringstream msg;
                msg << "Buoycab limit (" << MAX_CABS << ") exceeded";
                AddMessage(Message::TYPE_ERROR, msg.str());
                return;
            }

            m_actor->ar_collcabs[m_actor->ar_num_collcabs]=m_actor->ar_num_cabs;
            m_actor->ar_num_collcabs++;
            m_actor->ar_buoycabs[m_actor->ar_num_buoycabs]=m_actor->ar_num_cabs; 
            m_actor->ar_buoycab_types[m_actor->ar_num_buoycabs]=Buoyance::BUOY_NORMAL; 
            m_actor->ar_num_buoycabs++; 
            mk_buoyance = true;
        }

        if (mk_buoyance && (m_actor->m_buoyance == nullptr))
        {
            Buoyance* buoy = new Buoyance(App::GetGfxScene()->GetDustPool("splash"), App::GetGfxScene()->GetDustPool("ripple"));
            m_actor->m_buoyance.reset(buoy);
        }
        m_actor->ar_num_cabs++;
    }

    //close the current mesh
    CabSubmesh submesh;
    submesh.texcoords_pos = m_oldstyle_cab_texcoords.size();
    submesh.cabs_pos = static_cast<unsigned int>(m_actor->ar_num_cabs);
    submesh.backmesh_type = CabSubmesh::BACKMESH_NONE;
    m_oldstyle_cab_submeshes.push_back(submesh);

    /* BACKMESH */

    if (def.backmesh)
    {

        // Check limit
        if (! CheckCabLimit(1))
        {
            return;
        }

        // === add an extra front mesh ===
        //texcoords
        int uv_start = (m_oldstyle_cab_submeshes.size()==1) ? 0 : static_cast<int>((m_oldstyle_cab_submeshes.rbegin()+1)->texcoords_pos);
        for (size_t i=uv_start; i<m_oldstyle_cab_submeshes.back().texcoords_pos; i++)
        {
            m_oldstyle_cab_texcoords.push_back(m_oldstyle_cab_texcoords[i]);
        }
        //cab
        int cab_start =  (m_oldstyle_cab_submeshes.size()==1) ? 0 : static_cast<int>((m_oldstyle_cab_submeshes.rbegin()+1)->cabs_pos);
        for (size_t i=cab_start; i<m_oldstyle_cab_submeshes.back().cabs_pos; i++)
        {
            m_actor->ar_cabs[m_actor->ar_num_cabs*3]=m_actor->ar_cabs[i*3];
            m_actor->ar_cabs[m_actor->ar_num_cabs*3+1]=m_actor->ar_cabs[i*3+1];
            m_actor->ar_cabs[m_actor->ar_num_cabs*3+2]=m_actor->ar_cabs[i*3+2];
            m_actor->ar_num_cabs++;
        }
        // Finalize
        CabSubmesh submesh;
        submesh.backmesh_type = CabSubmesh::BACKMESH_TRANSPARENT;
        submesh.texcoords_pos = m_oldstyle_cab_texcoords.size();
        submesh.cabs_pos      = static_cast<unsigned int>(m_actor->ar_num_cabs);
        m_oldstyle_cab_submeshes.push_back(submesh);

        // === add an extra back mesh ===
        //texcoords
        uv_start = (m_oldstyle_cab_submeshes.size()==1) ? 0 : static_cast<int>((m_oldstyle_cab_submeshes.rbegin()+1)->texcoords_pos);
        for (size_t i=uv_start; i<m_oldstyle_cab_submeshes.back().texcoords_pos; i++)
        {
            m_oldstyle_cab_texcoords.push_back(m_oldstyle_cab_texcoords[i]);
        }

        //cab
        cab_start =  (m_oldstyle_cab_submeshes.size()==1) ? 0 : static_cast<int>((m_oldstyle_cab_submeshes.rbegin()+1)->cabs_pos);
        for (size_t i=cab_start; i<m_oldstyle_cab_submeshes.back().cabs_pos; i++)
        {
            m_actor->ar_cabs[m_actor->ar_num_cabs*3]=m_actor->ar_cabs[i*3+1];
            m_actor->ar_cabs[m_actor->ar_num_cabs*3+1]=m_actor->ar_cabs[i*3];
            m_actor->ar_cabs[m_actor->ar_num_cabs*3+2]=m_actor->ar_cabs[i*3+2];
            m_actor->ar_num_cabs++;
        }
    
        //close the current mesh
        CabSubmesh submesh2;
        submesh2.texcoords_pos = m_oldstyle_cab_texcoords.size();
        submesh2.cabs_pos = static_cast<unsigned int>(m_actor->ar_num_cabs);
        submesh2.backmesh_type = CabSubmesh::BACKMESH_OPAQUE;
        m_oldstyle_cab_submeshes.push_back(submesh2);
    }
}

void ActorSpawner::ProcessFlexbody(RigDef::Flexbody& def)
{
    // Collect nodes
    std::vector<unsigned int> node_indices;
    bool nodes_found = true;
    for (auto& node_def: def.node_list)
    {
        NodeNum_t node = this->ResolveNodeRef(node_def);
        if (node == NODENUM_INVALID)
        {
            nodes_found = false;
            break;
        }
        node_indices.push_back(node);
    }

    if (! nodes_found)
    {
        this->AddMessage(Message::TYPE_ERROR, "Failed to collect nodes from node-ranges, skipping flexbody: " + def.mesh_name);
        return;
    }

    const NodeNum_t reference_node = this->FindNodeIndex(def.reference_node);
    const NodeNum_t x_axis_node    = this->FindNodeIndex(def.x_axis_node);
    const NodeNum_t y_axis_node    = this->FindNodeIndex(def.y_axis_node);
    if (reference_node == -1 || x_axis_node == -1 || y_axis_node == -1)
    {
        this->AddMessage(Message::TYPE_ERROR, "Failed to find required nodes, skipping flexbody '" + def.mesh_name + "'");
        return;
    }

    Ogre::Quaternion rot=Ogre::Quaternion(Ogre::Degree(def.rotation.z), Ogre::Vector3::UNIT_Z);
    rot=rot*Ogre::Quaternion(Ogre::Degree(def.rotation.y), Ogre::Vector3::UNIT_Y);
    rot=rot*Ogre::Quaternion(Ogre::Degree(def.rotation.x), Ogre::Vector3::UNIT_X);

    m_actor->GetGfxActor()->UpdateSimDataBuffer(); // fill all current nodes - needed to setup flexing meshes

    try
    {
        auto* flexbody = m_flex_factory.CreateFlexBody(
            &def, reference_node, x_axis_node, y_axis_node, rot, node_indices, m_custom_resource_group);

        if (flexbody == nullptr)
            return; // Error already logged

        flexbody->setCameraMode(def.camera_settings.mode);

        m_actor->m_gfx_actor->m_flexbodies.emplace_back(flexbody);
    }
    catch (Ogre::Exception& e)
    {
        this->AddMessage(Message::TYPE_ERROR, 
            "Failed to create flexbody '" + def.mesh_name + "', reason:" + e.getFullDescription());
    }
}

void ActorSpawner::ProcessMinimass(RigDef::Minimass & def)
{
    m_state.global_minimass = def.global_min_mass_Kg;
    m_actor->ar_minimass_skip_loaded_nodes = (def.option == RigDef::MinimassOption::l_SKIP_LOADED);
}

void ActorSpawner::ProcessProp(RigDef::Prop & def)
{
    RoR::Prop prop;
    int prop_index = static_cast<int>(m_actor->m_gfx_actor->m_props.size());

    prop.pp_node_ref         = GetNodeIndexOrThrow(def.reference_node);
    prop.pp_node_x           = FindNodeIndex(def.x_axis_node);
    prop.pp_node_y           = FindNodeIndex(def.y_axis_node);
    if (prop.pp_node_x == NODENUM_INVALID || prop.pp_node_y == NODENUM_INVALID)
    {
        return; // Error alredy logged by `FindNodeIndex()`
    }
    prop.pp_offset       = def.offset;
    prop.pp_offset_orig  = def.offset;
    prop.pp_rot          = Ogre::Quaternion(Ogre::Degree(def.rotation.z), Ogre::Vector3::UNIT_Z)
                           * Ogre::Quaternion(Ogre::Degree(def.rotation.y), Ogre::Vector3::UNIT_Y)
                           * Ogre::Quaternion(Ogre::Degree(def.rotation.x), Ogre::Vector3::UNIT_X);
    prop.pp_rota         = def.rotation;
    prop.pp_camera_mode_active = def.camera_settings.mode; /* Handles default value */
    prop.pp_camera_mode_orig = def.camera_settings.mode; /* Handles default value */
    prop.pp_wheel_rot_degree  = 160.f; // ??

    /* SPECIAL PROPS */

    /* Rear view mirror (left) */
    if (def.special == RigDef::SpecialProp::MIRROR_LEFT)
    {
        m_curr_mirror_prop_type = CustomMaterial::MirrorPropType::MPROP_LEFT;
    }

    /* Rear view mirror (right) */
    if (def.special == RigDef::SpecialProp::MIRROR_RIGHT)
    {
        m_curr_mirror_prop_type = CustomMaterial::MirrorPropType::MPROP_RIGHT;
    }

    /* Custom steering wheel */
    Ogre::Vector3 steering_wheel_offset = Ogre::Vector3::ZERO;
    if (def.special == RigDef::SpecialProp::DASHBOARD_LEFT)
    {
        steering_wheel_offset = Ogre::Vector3(-0.67, -0.61,0.24);
    }
    if (def.special == RigDef::SpecialProp::DASHBOARD_RIGHT)
    {
        steering_wheel_offset = Ogre::Vector3(0.67, -0.61,0.24);
    }
    if (steering_wheel_offset != Ogre::Vector3::ZERO)
    {
        if (def.special_prop_dashboard._offset_is_set)
        {
            steering_wheel_offset = def.special_prop_dashboard.offset;
        }
        prop.pp_wheel_rot_degree = def.special_prop_dashboard.rotation_angle;
        prop.pp_wheel_scene_node = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
        prop.pp_wheel_pos = steering_wheel_offset;
        const std::string instance_name = this->ComposeName("SteeringWheelPropEntity", prop_index);
        prop.pp_wheel_mesh_obj = new MeshObject(
            def.special_prop_dashboard.mesh_name,
            m_custom_resource_group,
            instance_name,
            prop.pp_wheel_scene_node
            );
        this->SetupNewEntity(prop.pp_wheel_mesh_obj->getEntity(), Ogre::ColourValue(0, 0.5, 0.5));
    }

    /* CREATE THE PROP */

    prop.pp_scene_node = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
    const std::string instance_name = this->ComposeName("PropEntity", prop_index);
    prop.pp_mesh_obj = new MeshObject(def.mesh_name, m_custom_resource_group, instance_name, prop.pp_scene_node);

    prop.pp_mesh_obj->setCastShadows(true); // Orig code {{ prop.pp_mesh_obj->setCastShadows(shadowmode != 0); }}, shadowmode has default value 1 and changes with undocumented directive 'set_shadows'

    if (def.special == RigDef::SpecialProp::AERO_PROP_SPIN)
    {
        prop.pp_aero_propeller_spin = true;
        prop.pp_mesh_obj->setCastShadows(false);
        prop.pp_scene_node->setVisible(false);
    }
    else if(def.special == RigDef::SpecialProp::AERO_PROP_BLADE)
    {
        prop.pp_aero_propeller_blade = true;
    }
    else if(def.special == RigDef::SpecialProp::DRIVER_SEAT)
    {
        //driver seat, used to position the driver and make the seat translucent at times
        if (m_actor->m_gfx_actor->m_driverseat_prop_index == -1)
        {
            m_actor->m_gfx_actor->m_driverseat_prop_index = prop_index;
            prop.pp_mesh_obj->setMaterialName("driversseat");
        }
        else
        {
            this->AddMessage(Message::TYPE_INFO, "Found more than one 'seat[2]' special props. Only the first one will be the driver's seat.");
        }
    }
    else if(def.special == RigDef::SpecialProp::DRIVER_SEAT_2)
    {
        // Same as DRIVER_SEAT, except it doesn't force the "driversseat" material
        if (m_actor->m_gfx_actor->m_driverseat_prop_index == -1)
        {
            m_actor->m_gfx_actor->m_driverseat_prop_index = prop_index;
        }
        else
        {
            this->AddMessage(Message::TYPE_INFO, "Found more than one 'seat[2]' special props. Only the first one will be the driver's seat.");
        }
    }
    else if (m_actor->m_flares_mode != GfxFlaresMode::NONE)
    {
        if(def.special == RigDef::SpecialProp::BEACON)
        {
            prop.pp_beacon_type = 'b';
            prop.pp_beacon_rot_angle[0] = 2.0 * 3.14 * frand();
            prop.pp_beacon_rot_rate[0] = 4.0 * 3.14 + frand() - 0.5;
            /* the light */
            auto pp_beacon_light = App::GetGfxScene()->GetSceneManager()->createLight();
            pp_beacon_light->setType(Ogre::Light::LT_SPOTLIGHT);
            pp_beacon_light->setDiffuseColour(def.special_prop_beacon.color);
            pp_beacon_light->setSpecularColour(def.special_prop_beacon.color);
            pp_beacon_light->setAttenuation(50.0, 1.0, 0.3, 0.0);
            pp_beacon_light->setSpotlightRange( Ogre::Degree(35), Ogre::Degree(45) );
            pp_beacon_light->setCastShadows(false);
            pp_beacon_light->setVisible(false);
            /* the flare billboard */
            auto flare_scene_node = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
            auto flare_billboard_sys = App::GetGfxScene()->GetSceneManager()->createBillboardSet(1); //(propname,1);
            if (flare_billboard_sys)
            {
                flare_billboard_sys->createBillboard(0,0,0);
                flare_billboard_sys->setMaterialName(def.special_prop_beacon.flare_material_name);
                flare_billboard_sys->setVisibilityFlags(DEPTHMAP_DISABLED);
                flare_scene_node->attachObject(flare_billboard_sys);
            }
            flare_scene_node->setVisible(false);

            // Complete
            prop.pp_beacon_scene_node[0] = flare_scene_node;
            prop.pp_beacon_bbs[0] = flare_billboard_sys;
            prop.pp_beacon_light[0] = pp_beacon_light;
        }
        else if(def.special == RigDef::SpecialProp::REDBEACON)
        {
            prop.pp_beacon_rot_angle[0] = 0.f;
            prop.pp_beacon_rot_rate[0] = 1.0;
            prop.pp_beacon_type = 'r';
            //the light
            auto pp_beacon_light=App::GetGfxScene()->GetSceneManager()->createLight();//propname);
            pp_beacon_light->setType(Ogre::Light::LT_POINT);
            pp_beacon_light->setDiffuseColour( Ogre::ColourValue(1.0, 0.0, 0.0));
            pp_beacon_light->setSpecularColour( Ogre::ColourValue(1.0, 0.0, 0.0));
            pp_beacon_light->setAttenuation(50.0, 1.0, 0.3, 0.0);
            pp_beacon_light->setCastShadows(false);
            pp_beacon_light->setVisible(false);
            //the flare billboard
            auto flare_scene_node = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
            auto flare_billboard_sys = App::GetGfxScene()->GetSceneManager()->createBillboardSet(1); //propname,1);
            if (flare_billboard_sys)
            {
                flare_billboard_sys->createBillboard(0,0,0);
                flare_billboard_sys->setMaterialName("tracks/redbeaconflare");
                flare_billboard_sys->setVisibilityFlags(DEPTHMAP_DISABLED);
                flare_billboard_sys->setDefaultDimensions(1.0, 1.0);
                flare_scene_node->attachObject(flare_billboard_sys);
            }
            flare_scene_node->setVisible(false);

            // Finalize
            prop.pp_beacon_light[0] = pp_beacon_light;
            prop.pp_beacon_scene_node[0] = flare_scene_node;
            prop.pp_beacon_bbs[0] = flare_billboard_sys;
            
        }
        else if(def.special == RigDef::SpecialProp::LIGHTBAR)
        {
            m_actor->ar_is_police = true;
            prop.pp_beacon_type='p';
            for (int k=0; k<4; k++)
            {
                prop.pp_beacon_rot_angle[k] = 2.0 * 3.14 * frand();
                prop.pp_beacon_rot_rate[k] = 4.0 * 3.14 + frand() - 0.5;
                prop.pp_beacon_bbs[k] = nullptr;
                //the light
                prop.pp_beacon_light[k]=App::GetGfxScene()->GetSceneManager()->createLight();
                prop.pp_beacon_light[k]->setType(Ogre::Light::LT_SPOTLIGHT);
                if (k>1)
                {
                    prop.pp_beacon_light[k]->setDiffuseColour( Ogre::ColourValue(1.0, 0.0, 0.0));
                    prop.pp_beacon_light[k]->setSpecularColour( Ogre::ColourValue(1.0, 0.0, 0.0));
                }
                else
                {
                    prop.pp_beacon_light[k]->setDiffuseColour( Ogre::ColourValue(0.0, 0.5, 1.0));
                    prop.pp_beacon_light[k]->setSpecularColour( Ogre::ColourValue(0.0, 0.5, 1.0));
                }
                prop.pp_beacon_light[k]->setAttenuation(50.0, 1.0, 0.3, 0.0);
                prop.pp_beacon_light[k]->setSpotlightRange( Ogre::Degree(35), Ogre::Degree(45) );
                prop.pp_beacon_light[k]->setCastShadows(false);
                prop.pp_beacon_light[k]->setVisible(false);
                //the flare billboard
                prop.pp_beacon_scene_node[k] = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
                prop.pp_beacon_bbs[k]=App::GetGfxScene()->GetSceneManager()->createBillboardSet(1);
                prop.pp_beacon_bbs[k]->createBillboard(0,0,0);
                if (prop.pp_beacon_bbs[k])
                {
                    if (k>1)
                    {
                        prop.pp_beacon_bbs[k]->setMaterialName("tracks/brightredflare");
                    }
                    else
                    {
                        prop.pp_beacon_bbs[k]->setMaterialName("tracks/brightblueflare");
                    }

                    prop.pp_beacon_bbs[k]->setVisibilityFlags(DEPTHMAP_DISABLED);
                    prop.pp_beacon_scene_node[k]->attachObject(prop.pp_beacon_bbs[k]);
                }
                prop.pp_beacon_scene_node[k]->setVisible(false);
            }
        }

        if (m_curr_mirror_prop_type != CustomMaterial::MirrorPropType::MPROP_NONE)
        {
            m_curr_mirror_prop_scenenode = prop.pp_mesh_obj->GetSceneNode();
        }
    }

    this->SetupNewEntity(prop.pp_mesh_obj->getEntity(), Ogre::ColourValue(1.f, 1.f, 0.f));

    m_curr_mirror_prop_scenenode = nullptr;
    m_curr_mirror_prop_type = CustomMaterial::MirrorPropType::MPROP_NONE;

    /* PROCESS ANIMATIONS */

    for (RigDef::Animation& anim_def: def.animations)
    {
        PropAnim anim;

        /* Arg #1: ratio */
        anim.animratio = anim_def.ratio;
        if (anim_def.ratio == 0)
        {
            std::stringstream msg;
            msg << "Prop (mesh: " << def.mesh_name << ") has invalid animation ratio (0), using it anyway (compatibility)...";
            AddMessage(Message::TYPE_WARNING, msg.str());
        }

        /* Arg #2: option1 (lower limit) */
        anim.lower_limit = anim_def.lower_limit; /* Handles default */

        /* Arg #3: option2 (upper limit) */
        anim.upper_limit = anim_def.upper_limit; /* Handles default */

        /* Arg #4: source */
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_AIRSPEED)) { /* (NOTE: code formatting relaxed) */
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_AIRSPEED);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_VERTICAL_VELOCITY)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_VVI);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_ALTIMETER_100K)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_ALTIMETER);
            anim.animOpt3 = 1.f;
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_ALTIMETER_10K)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_ALTIMETER);
            anim.animOpt3 = 2.f;
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_ALTIMETER_1K)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_ALTIMETER);
            anim.animOpt3 = 3.f;
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_ANGLE_OF_ATTACK)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_AOA);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_FLAP)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_FLAP);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_AIR_BRAKE)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_AIRBRAKE);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_ROLL)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_ROLL);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_PITCH)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_PITCH);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_BRAKES)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_BRAKE);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_ACCEL)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_ACCEL);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_CLUTCH)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_CLUTCH);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_SPEEDO)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_SPEEDO);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_TACHO)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_TACHO);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_TURBO)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_TURBO);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_PARKING)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_PBRAKE);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_SHIFT_LEFT_RIGHT)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_SHIFTER);
            anim.animOpt3 = 1.0f;
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_SHIFT_BACK_FORTH)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_SHIFTER);
            anim.animOpt3 = 2.0f;
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_SEQUENTIAL_SHIFT)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_SHIFTER);
            anim.animOpt3 = 3.0f;
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_SHIFTERLIN)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_SHIFTER);
            anim.animOpt3 = 4.0f;
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_TORQUE)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_TORQUE);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_HEADING)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_HEADING);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_DIFFLOCK)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_DIFFLOCK);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_STEERING_WHEEL)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_STEERING);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_AILERON)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_AILERONS);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_ELEVATOR)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_ELEVATORS);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_AIR_RUDDER)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_ARUDDER);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_BOAT_RUDDER)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_BRUDDER);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_BOAT_THROTTLE)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_BTHROTTLE);
        }
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_PERMANENT)) {
            BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_PERMANENT);
        }

        /* Motor-indexed sources */
        std::list<RigDef::Animation::MotorSource>::iterator source_itor = anim_def.motor_sources.begin();
        for ( ; source_itor != anim_def.motor_sources.end(); source_itor++)
        {
            if (BITMASK_IS_1(source_itor->source, RigDef::Animation::MotorSource::SOURCE_AERO_THROTTLE)) {
                BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_THROTTLE);
                anim.animOpt3 = static_cast<float>(source_itor->motor);
            }
            if (BITMASK_IS_1(source_itor->source, RigDef::Animation::MotorSource::SOURCE_AERO_RPM)) {
                BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_RPM);
                anim.animOpt3 = static_cast<float>(source_itor->motor);
            }
            if (BITMASK_IS_1(source_itor->source, RigDef::Animation::MotorSource::SOURCE_AERO_TORQUE)) {
                BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_AETORQUE);
                anim.animOpt3 = static_cast<float>(source_itor->motor);
            }
            if (BITMASK_IS_1(source_itor->source, RigDef::Animation::MotorSource::SOURCE_AERO_PITCH)) {
                BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_AEPITCH);
                anim.animOpt3 = static_cast<float>(source_itor->motor);
            }
            if (BITMASK_IS_1(source_itor->source, RigDef::Animation::MotorSource::SOURCE_AERO_STATUS)) {
                BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_AESTATUS);
                anim.animOpt3 = static_cast<float>(source_itor->motor);
            }
        }
        if (anim.animFlags == 0)
        {
            AddMessage(Message::TYPE_ERROR, "Failed to identify animation source");
        }

        /* Anim modes */
        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_ROTATION_X)) {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_ROTA_X);
        }
        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_ROTATION_Y)) {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_ROTA_Y);
        }
        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_ROTATION_Z)) {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_ROTA_Z);
        }
        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_OFFSET_X)) {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_OFFSET_X);
        }
        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_OFFSET_Y)) {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_OFFSET_Y);
        }
        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_OFFSET_Z)) {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_OFFSET_Z);
        }
        if (anim.animMode == 0)
        {
            AddMessage(Message::TYPE_ERROR, "Failed to identify animation mode");
        }

        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_AUTO_ANIMATE)) 
        {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_AUTOANIMATE);

            // Flag whether default lower and/or upper animation limit constraints are effective
            const bool use_default_lower_limit = (anim_def.lower_limit == 0.f);
            const bool use_default_upper_limit = (anim_def.upper_limit == 0.f);

            if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_ROTATION_X)) {
                anim.lower_limit = (use_default_lower_limit) ? (-180.f) : (anim_def.lower_limit + prop.pp_rota.x);
                anim.upper_limit = (use_default_upper_limit) ? ( 180.f) : (anim_def.upper_limit + prop.pp_rota.x);
            }
            if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_ROTATION_Y)) {
                anim.lower_limit = (use_default_lower_limit) ? (-180.f) : (anim_def.lower_limit + prop.pp_rota.y);
                anim.upper_limit = (use_default_upper_limit) ? ( 180.f) : (anim_def.upper_limit + prop.pp_rota.y);
            }
            if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_ROTATION_Z)) {
                anim.lower_limit = (use_default_lower_limit) ? (-180.f) : (anim_def.lower_limit + prop.pp_rota.z);
                anim.upper_limit = (use_default_upper_limit) ? ( 180.f) : (anim_def.upper_limit + prop.pp_rota.z);
            }
            if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_OFFSET_X)) {
                anim.lower_limit = (use_default_lower_limit) ? (-10.f) : (anim_def.lower_limit + prop.pp_offset_orig.x);
                anim.upper_limit = (use_default_upper_limit) ? ( 10.f) : (anim_def.upper_limit + prop.pp_offset_orig.x);
            }
            if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_OFFSET_Y)) {
                anim.lower_limit = (use_default_lower_limit) ? (-10.f) : (anim_def.lower_limit + prop.pp_offset_orig.y);
                anim.upper_limit = (use_default_upper_limit) ? ( 10.f) : (anim_def.upper_limit + prop.pp_offset_orig.y);
            }
            if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_OFFSET_Z)) {
                anim.lower_limit = (use_default_lower_limit) ? (-10.f) : (anim_def.lower_limit + prop.pp_offset_orig.z);
                anim.upper_limit = (use_default_upper_limit) ? ( 10.f) : (anim_def.upper_limit + prop.pp_offset_orig.z);
            }
        }
        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_NO_FLIP)) 
        {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_NOFLIP);
        }
        if (BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_BOUNCE)) 
        {
            BITMASK_SET_1(anim.animMode, PROP_ANIM_MODE_BOUNCE);
            anim.animOpt5 = 1.f;
        }

        // Parameter 'event:'
        if (BITMASK_IS_1(anim_def.source, RigDef::Animation::SOURCE_EVENT) &&
            anim_def.event_name != "")
        {
            int event_id = RoR::App::GetInputEngine()->resolveEventName(anim_def.event_name);
            if (event_id == -1)
            {
                AddMessage(Message::TYPE_ERROR, "Unknown animation event: " + anim_def.event_name);
            }
            else
            {
                PropAnimKeyState state;
                state.eventlock_present = BITMASK_IS_1(anim_def.mode, RigDef::Animation::MODE_EVENT_LOCK);
                state.event_id = static_cast<events>(event_id);
                m_actor->m_prop_anim_key_states.push_back(state);
                BITMASK_SET_1(anim.animFlags, PROP_ANIM_FLAG_EVENT);
            }
        }

        prop.pp_animations.push_back(anim);
    }

    m_actor->m_gfx_actor->m_props.push_back(prop);
}

void ActorSpawner::ProcessFlare3(RigDef::Flare3 & def)
{
    // Do the common processing
    this->ProcessFlare2(def);

    // Now setup the extra inertia feature
    flare_t& f = m_actor->ar_flares.back();
    f.uses_inertia = true;
    this->_ProcessSimpleInertia(*def.inertia_defaults, f.inertia);
}

void ActorSpawner::ProcessFlare2(RigDef::Flare2 & def)
{
    if (m_actor->m_flares_mode == GfxFlaresMode::NONE) { return; }

    int blink_delay = def.blink_delay_milis;
    float size = def.size;

    /* Backwards compatibility */
    if (blink_delay == -2) 
    {
        if (def.type == FlareType::BLINKER_LEFT || def.type == FlareType::BLINKER_RIGHT)
        {
            blink_delay = -1; /* Default blink */
        }
        else
        {
            blink_delay = 0; /* Default no blink */
        }
    }
    
    if (size == -2.f && def.type == FlareType::HEADLIGHT)
    {
        size = 1.f;
    }
    else if ((size == -2.f && def.type != FlareType::HEADLIGHT) || size == -1.f)
    {
        size = 0.5f;
    }

    flare_t flare;
    flare.fl_type              = def.type;
    flare.controlnumber        = -1;
    flare.blinkdelay           = (blink_delay == -1) ? 0.5f : blink_delay / 1000.f;
    flare.blinkdelay_curr      = 0.f;
    flare.blinkdelay_state     = false;
    flare.noderef              = GetNodeIndexOrThrow(def.reference_node);
    flare.nodex                = GetNodeIndexOrThrow(def.node_axis_x);
    flare.nodey                = GetNodeIndexOrThrow(def.node_axis_y);
    flare.offsetx              = def.offset.x;
    flare.offsety              = def.offset.y;
    flare.offsetz              = def.offset.z;
    flare.size                 = size;

    if (def.type == FlareType::USER)
    {
        // control number: convert from 1-10 to 0-9
        if (def.control_number == 12) // Special case - legacy parking brake indicator
        {
            flare.fl_type = FlareType::DASHBOARD;
            flare.dashboard_link = DD_PARKINGBRAKE;
        }
        else if (def.control_number < 1)
        {
            this->AddMessage(Message::TYPE_WARNING,
                fmt::format("Bad flare control num {}, must be 1-{}, using 1.",
                def.control_number, MAX_CLIGHTS));
            flare.controlnumber = 0;
        }
        else if (def.control_number > MAX_CLIGHTS)
        {
            this->AddMessage(Message::TYPE_WARNING,
                fmt::format("Bad flare control num {}, must be 1-{}, using {}.",
                def.control_number, MAX_CLIGHTS, MAX_CLIGHTS));
            flare.controlnumber = MAX_CLIGHTS-1;
        }
        else
        {
            flare.controlnumber = def.control_number - 1;
        }
    }

    if (def.type == FlareType::DASHBOARD)
    {
        flare.dashboard_link = m_actor->ar_dashboard->getLinkIDForName(def.dashboard_link);
        if (flare.dashboard_link == -1)
        {
            this->AddMessage(Message::TYPE_WARNING,
                fmt::format("Skipping 'd' flare, invalid input link '{}'", def.dashboard_link));
            return;
        }
    }

    /* Visuals */
    flare.snode = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
    std::string flare_name = this->ComposeName("Flare", static_cast<int>(m_actor->ar_flares.size()));
    flare.bbs = App::GetGfxScene()->GetSceneManager()->createBillboardSet(flare_name, 1);

    // Backwards compatibility:
    // before 't' (tail light) was introduced in 2022, tail lights were indicated as 'f' (headlight) + custom material.
    bool using_default_material = (def.material_name.length() == 0 || def.material_name == "default");
    if (flare.fl_type == FlareType::HEADLIGHT && !using_default_material)
    {
        flare.fl_type = FlareType::TAIL_LIGHT;
    }

    if (flare.bbs == nullptr)
    {
        AddMessage(Message::TYPE_WARNING, "Failed to create flare: '" + flare_name + "', continuing without it (compatibility)...");
    }
    else
    {
        flare.bbs->createBillboard(0,0,0);
        flare.bbs->setVisibilityFlags(DEPTHMAP_DISABLED);
        std::string material_name = def.material_name;
        if (using_default_material)
        {
            if (flare.fl_type == FlareType::BRAKE_LIGHT)
            {
                material_name = "tracks/brakeflare";
            }
            else if (flare.fl_type == FlareType::BLINKER_LEFT || (flare.fl_type == FlareType::BLINKER_RIGHT))
            {
                material_name = "tracks/blinkflare";
            }
            else if (flare.fl_type == FlareType::DASHBOARD)
            {
                material_name = "tracks/greenflare";
            }
            else if (flare.fl_type == FlareType::TAIL_LIGHT)
            {
                material_name = "tracks/redflare";
            }
            else
            {
                material_name = "tracks/flare";
            }
        }

        Ogre::MaterialPtr material = this->FindOrCreateCustomizedMaterial(material_name);
        if (!material.isNull())
        {
            flare.bbs->setMaterial(material);
            flare.snode->attachObject(flare.bbs);
        }
    }
    flare.intensity = 1.f;
    flare.light = nullptr;

    if ((App::gfx_flares_mode->getEnum<GfxFlaresMode>() >= GfxFlaresMode::CURR_VEHICLE_HEAD_ONLY) && size > 0.001)
    {
        if (flare.fl_type == FlareType::HEADLIGHT)
        {
            flare.light=App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setType(Ogre::Light::LT_SPOTLIGHT);
            flare.light->setDiffuseColour( Ogre::ColourValue(1, 1, 1));
            flare.light->setSpecularColour( Ogre::ColourValue(1, 1, 1));
            flare.light->setAttenuation(200, 0.9, 0, 0);
            flare.light->setSpotlightRange( Ogre::Degree(35), Ogre::Degree(45) );
            flare.light->setCastShadows(false);
        }
        else if (flare.fl_type == FlareType::HIGH_BEAM)
        {
            flare.light = App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setType(Ogre::Light::LT_SPOTLIGHT);
            flare.light->setDiffuseColour(Ogre::ColourValue(1, 1, 1));
            flare.light->setSpecularColour(Ogre::ColourValue(1, 1, 1));
            flare.light->setAttenuation(400, 0.9, 0, 0);
            flare.light->setSpotlightRange(Ogre::Degree(35), Ogre::Degree(45));
            flare.light->setCastShadows(false);
        }
        else if (flare.fl_type == FlareType::FOG_LIGHT)
        {
            flare.light = App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setType(Ogre::Light::LT_SPOTLIGHT);
            flare.light->setDiffuseColour(Ogre::ColourValue(1, 1, 1));
            flare.light->setSpecularColour(Ogre::ColourValue(1, 1, 1));
            flare.light->setAttenuation(400, 0.9, 0, 0);
            flare.light->setSpotlightRange(Ogre::Degree(35), Ogre::Degree(45));
            flare.light->setCastShadows(false);
        }
    }
    if ((App::gfx_flares_mode->getEnum<GfxFlaresMode>() >= GfxFlaresMode::ALL_VEHICLES_ALL_LIGHTS) && size > 0.001)
    {
        if (flare.fl_type == FlareType::TAIL_LIGHT)
        {
            flare.light=App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setDiffuseColour( Ogre::ColourValue(1.0, 0, 0));
            flare.light->setSpecularColour( Ogre::ColourValue(1.0, 0, 0));
            flare.light->setAttenuation(10.0, 1.0, 0, 0);
        }
        else if (flare.fl_type == FlareType::REVERSE_LIGHT)
        {
            flare.light=App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setDiffuseColour(Ogre::ColourValue(1, 1, 1));
            flare.light->setSpecularColour(Ogre::ColourValue(1, 1, 1));
            flare.light->setAttenuation(20.0, 1, 0, 0);
        }
        else if (flare.fl_type == FlareType::BRAKE_LIGHT)
        {
            flare.light=App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setDiffuseColour( Ogre::ColourValue(1.0, 0, 0));
            flare.light->setSpecularColour( Ogre::ColourValue(1.0, 0, 0));
            flare.light->setAttenuation(10.0, 1.0, 0, 0);
        }
        else if (flare.fl_type == FlareType::BLINKER_LEFT || (flare.fl_type == FlareType::BLINKER_RIGHT))
        {
            flare.light=App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setDiffuseColour( Ogre::ColourValue(1, 1, 0));
            flare.light->setSpecularColour( Ogre::ColourValue(1, 1, 0));
            flare.light->setAttenuation(10.0, 1, 1, 0);
        }
        else if (flare.fl_type == FlareType::USER)
        {
            flare.light=App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setDiffuseColour( Ogre::ColourValue(1, 1, 1));
            flare.light->setSpecularColour( Ogre::ColourValue(1, 1, 1));
            flare.light->setAttenuation(1.0, 1.0, 1, 0.2);
        }
        else if (flare.fl_type == FlareType::SIDELIGHT)
        {
            flare.light = App::GetGfxScene()->GetSceneManager()->createLight(flare_name);
            flare.light->setDiffuseColour(Ogre::ColourValue(1, 1, 1));
            flare.light->setSpecularColour(Ogre::ColourValue(1, 1, 1));
            flare.light->setAttenuation(5.0, 1.0, 1, 0.2);
        }
    }

    /* Finalize light */
    if (flare.light != nullptr)
    {
        flare.light->setType(Ogre::Light::LT_SPOTLIGHT);
        flare.light->setSpotlightRange( Ogre::Degree(35), Ogre::Degree(45) );
        flare.light->setCastShadows(false);
    }
    m_actor->ar_flares.push_back(flare);
}

Ogre::MaterialPtr ActorSpawner::InstantiateManagedMaterial(Ogre::String const & source_name, Ogre::String const & clone_name)
{
    Ogre::MaterialPtr src_mat = Ogre::MaterialManager::getSingleton().getByName(source_name);
    if (src_mat.isNull())
    {
        std::stringstream msg;
        msg << "Built-in material '" << source_name << "' missing! Skipping...";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return Ogre::MaterialPtr();
    }

    return src_mat->clone(clone_name, true, m_custom_resource_group);
}

void ActorSpawner::ProcessManagedMaterial(RigDef::ManagedMaterial & def)
{
    if (m_managed_materials.find(def.name) != m_managed_materials.end())
    {
        this->AddMessage(Message::TYPE_ERROR, "Duplicate managed material name: '" + def.name + "'. Ignoring definition...");
        return;
    }

    // Check all textures exist
    Ogre::ResourceGroupManager& rgm = Ogre::ResourceGroupManager::getSingleton();
    if (!rgm.resourceExists(m_custom_resource_group, def.diffuse_map))
    {
        this->AddMessage(Message::TYPE_WARNING, "Skipping managed material, missing texture file: " + def.diffuse_map);
        return;
    }

    if (def.damaged_diffuse_map != "" &&
        !rgm.resourceExists(m_custom_resource_group, def.damaged_diffuse_map))
    {
        this->AddMessage(Message::TYPE_WARNING, "Damage texture not found: " + def.damaged_diffuse_map);
        def.damaged_diffuse_map = "";
    }

    if (def.specular_map != "" &&
        !rgm.resourceExists(m_custom_resource_group, def.specular_map))
    {
        this->AddMessage(Message::TYPE_WARNING, "Specular texture not found: " + def.specular_map);
        def.specular_map = "";
    }

    // Create temporary placeholder
    // This is necessary to load meshes with original material names (= unchanged managed mat names)
    // - if not found, OGRE substitutes them with 'BaseWhite' which breaks subsequent processing.
    if (Ogre::MaterialManager::getSingleton().getByName(def.name, m_custom_resource_group).isNull())
    {
        m_placeholder_managedmat->clone(def.name, /*changeGroup=*/true, m_custom_resource_group);
    }

    std::string custom_name = def.name + ACTOR_ID_TOKEN + TOSTRING(m_actor->ar_instance_id);
    Ogre::MaterialPtr material;
    if (def.type == RigDef::ManagedMaterialType::FLEXMESH_STANDARD || def.type == RigDef::ManagedMaterialType::FLEXMESH_TRANSPARENT)
    {
        std::string mat_name_base
            = (def.type == RigDef::ManagedMaterialType::FLEXMESH_STANDARD)
            ? "managed/flexmesh_standard"
            : "managed/flexmesh_transparent";

        if (def.damaged_diffuse_map != "")
        {
            if (def.specular_map != "")
            {
                /* FLEXMESH, damage, specular */
                if (App::gfx_alt_actor_materials->getBool())
                {
                    material = this->InstantiateManagedMaterial(mat_name_base + "/speculardamage", custom_name);
                }
                else
                {
                    material = this->InstantiateManagedMaterial(mat_name_base + "/speculardamage_nicemetal", custom_name);
                }

                if (material.isNull())
                {
                    return;
                }

                if (App::gfx_alt_actor_materials->getBool())
                {
                    material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);
                    material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Dmg_Diffuse_Map")->setTextureName(def.damaged_diffuse_map);
                    material->getTechnique("BaseTechnique")->getPass("SpecularMapping1")->getTextureUnitState("SpecularMapping1_Tex")->setTextureName(def.specular_map);
                }
                else
                {
                    material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);
                    material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Dmg_Diffuse_Map")->setTextureName(def.damaged_diffuse_map);
                    material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Specular_Map")->setTextureName(def.specular_map);
                    material->getTechnique("BaseTechnique")->getPass("Specular")->getTextureUnitState("Specular_Map")->setTextureName(def.specular_map);
                }
            }
            else
            {
                /* FLEXMESH, damage, no_specular */
                material = this->InstantiateManagedMaterial(mat_name_base + "/damageonly", custom_name);
                if (material.isNull())
                {
                    return;
                }
                material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);
                material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Dmg_Diffuse_Map")->setTextureName(def.damaged_diffuse_map);
            }
        }
        else
        {
            if (def.specular_map != "")
            {
                /* FLEXMESH, no_damage, specular */
                if (App::gfx_alt_actor_materials->getBool())
                {
                    material = this->InstantiateManagedMaterial(mat_name_base + "/specularonly", custom_name);
                }
                else
                {
                    material = this->InstantiateManagedMaterial(mat_name_base + "/specularonly_nicemetal", custom_name);
                }

                if (material.isNull())
                {
                    return;
                }

                if (App::gfx_alt_actor_materials->getBool())
                {
                    material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);
                    material->getTechnique("BaseTechnique")->getPass("SpecularMapping1")->getTextureUnitState("SpecularMapping1_Tex")->setTextureName(def.specular_map);
                }
                else
                {
                    material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);
                    material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Specular_Map")->setTextureName(def.specular_map);
                    material->getTechnique("BaseTechnique")->getPass("Specular")->getTextureUnitState("Specular_Map")->setTextureName(def.specular_map);
                }
            }
            else
            {
                /* FLEXMESH, no_damage, no_specular */
                material = this->InstantiateManagedMaterial(mat_name_base + "/simple", custom_name);
                if (material.isNull())
                {
                    return;
                }
                material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);
            }
        }
    }
    else if (def.type == RigDef::ManagedMaterialType::MESH_STANDARD || def.type == RigDef::ManagedMaterialType::MESH_TRANSPARENT)
    {
        Ogre::String mat_name_base
            = (def.type == RigDef::ManagedMaterialType::MESH_STANDARD)
            ? "managed/mesh_standard"
            : "managed/mesh_transparent";

        if (def.specular_map != "")
        {
            /* MESH, specular */
            if (App::gfx_alt_actor_materials->getBool())
            {
                material = this->InstantiateManagedMaterial(mat_name_base + "/specular", custom_name);
            }
            else
            {
                material = this->InstantiateManagedMaterial(mat_name_base + "/specular_nicemetal", custom_name);
            }

            if (material.isNull())
            {
                return;
            }

            if (App::gfx_alt_actor_materials->getBool())
            {
                material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);
                material->getTechnique("BaseTechnique")->getPass("SpecularMapping1")->getTextureUnitState("SpecularMapping1_Tex")->setTextureName(def.specular_map);
            }
            else
            {
                material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);
                material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Specular_Map")->setTextureName(def.specular_map);
                material->getTechnique("BaseTechnique")->getPass("Specular")->getTextureUnitState("Specular_Map")->setTextureName(def.specular_map);
            }
        }
        else
        {
            /* MESH, no_specular */
            material = this->InstantiateManagedMaterial(mat_name_base + "/simple", custom_name);
            if (material.isNull())
            {
                return;
            }
            material->getTechnique("BaseTechnique")->getPass("BaseRender")->getTextureUnitState("Diffuse_Map")->setTextureName(def.diffuse_map);

        }
    }

    if (def.type != RigDef::ManagedMaterialType::INVALID)
    {
        if (def.options.double_sided)
        {
            material->getTechnique("BaseTechnique")->getPass("BaseRender")->setCullingMode(Ogre::CULL_NONE);
            if (def.specular_map != "")
            {
                if (App::gfx_alt_actor_materials->getBool())
                {
                    material->getTechnique("BaseTechnique")->getPass("SpecularMapping1")->setCullingMode(Ogre::CULL_NONE);
                }
                else
                {
                    material->getTechnique("BaseTechnique")->getPass("Specular")->setCullingMode(Ogre::CULL_NONE);
                }
            }
        }
    }

    /* Finalize */

    material->compile();
    m_managed_materials.insert(std::make_pair(def.name, material));
}

void ActorSpawner::ProcessCollisionBox(RigDef::CollisionBox & def)
{
    int8_t bbox_id = static_cast<int8_t>(m_actor->ar_collision_bounding_boxes.size());
    for (RigDef::Node::Ref& node_ref: def.nodes)
    {
        NodeNum_t node = this->ResolveNodeRef(node_ref);
        if (node == NODENUM_INVALID)
        {
            RoR::LogFormat("[RoR|Spawner] Collision box: skipping invalid node '%s'", node_ref.ToString().c_str());
            continue;
        }
        if (m_actor->ar_nodes[node].nd_coll_bbox_id != BBOXID_INVALID)
        {
            RoR::LogFormat("[RoR|Spawner] Collision box: re-assigning node '%s' from box ID '%d' to '%d'",
                node_ref.ToString().c_str(),
                m_actor->ar_nodes[node].nd_coll_bbox_id,
                bbox_id);
        }
        m_actor->ar_nodes[node].nd_coll_bbox_id = bbox_id;
    }

    m_actor->ar_collision_bounding_boxes.push_back(Ogre::AxisAlignedBox()); // Updated later
    m_actor->ar_predicted_coll_bounding_boxes.push_back(Ogre::AxisAlignedBox());
}

void ActorSpawner::ProcessCollisionRange(RigDef::CollisionRange & def)
{
    if (def.node_collision_range >= 0.f)
        m_actor->ar_collision_range = def.node_collision_range;
    else
        m_actor->ar_collision_range = DEFAULT_COLLISION_RANGE;
}

bool ActorSpawner::AssignWheelToAxle(int & _out_axle_wheel, node_t *axis_node_1, node_t *axis_node_2)
{
    for (int i = 0; i < m_actor->ar_num_wheels; i++)
    {
        wheel_t & wheel = m_actor->ar_wheels[i];
        if	(	(wheel.wh_axis_node0num == axis_node_1->pos && wheel.wh_axis_node1num == axis_node_2->pos)
            ||	(wheel.wh_axis_node0num == axis_node_2->pos && wheel.wh_axis_node1num == axis_node_1->pos)
            )
        {
            _out_axle_wheel = i;
            return true;
        }
    }
    return false;
}

void ActorSpawner::ProcessAxle(RigDef::Axle & def)
{
    if (! CheckAxleLimit(1))
    {
        return;
    }

    node_t *wheel_1_node_1 = GetNodePointerOrThrow(def.wheels[0][0]);
    node_t *wheel_1_node_2 = GetNodePointerOrThrow(def.wheels[0][1]);
    node_t *wheel_2_node_1 = GetNodePointerOrThrow(def.wheels[1][0]);
    node_t *wheel_2_node_2 = GetNodePointerOrThrow(def.wheels[1][1]);

    Differential *diff = new Differential();

    if (! AssignWheelToAxle(diff->di_idx_1, wheel_1_node_1, wheel_1_node_2))
    {
        std::stringstream msg;
        msg << "Couldn't find wheel with axis nodes '" << def.wheels[0][0].ToString()
            << "' and '" << def.wheels[0][1].ToString() << "'";
        AddMessage(Message::TYPE_WARNING, msg.str());
    }

    if (! AssignWheelToAxle(diff->di_idx_2, wheel_2_node_1, wheel_2_node_2))
    {
        std::stringstream msg;
        msg << "Couldn't find wheel with axis nodes '" << def.wheels[1][0].ToString()
            << "' and '" << def.wheels[1][1].ToString() << "'";
        AddMessage(Message::TYPE_WARNING, msg.str());
    }

    if (def.options.size() == 0)
    {
        AddMessage(Message::TYPE_INFO, "No differential defined, defaulting to Open & Locked");
        diff->AddDifferentialType(OPEN_DIFF);
        diff->AddDifferentialType(LOCKED_DIFF);
    }
    else
    {
        auto end = def.options.end();
        for (auto itor = def.options.begin(); itor != end; ++itor)
        {
            switch (*itor)
            {
            case RigDef::DifferentialType::l_LOCKED:
                diff->AddDifferentialType(LOCKED_DIFF);
                break;
            case RigDef::DifferentialType::o_OPEN:
                diff->AddDifferentialType(OPEN_DIFF);
                break;
            case RigDef::DifferentialType::s_SPLIT:
                diff->AddDifferentialType(SPLIT_DIFF);
                break;
            case RigDef::DifferentialType::v_VISCOUS:
                diff->AddDifferentialType(VISCOUS_DIFF);
                break;
            default:
                AddMessage(Message::TYPE_WARNING, fmt::format("Unknown differential type: '{}'", (char)*itor));
                break;
            }
        }
    }

    m_actor->m_wheel_diffs[m_actor->m_num_wheel_diffs] = diff;
    m_actor->m_num_wheel_diffs++;
}

void ActorSpawner::ProcessInterAxle(RigDef::InterAxle & def)
{
    if (def.a1 == def.a2 || std::min(def.a1, def.a2) < 0 || std::max(def.a1 , def.a2) >= m_actor->m_num_wheel_diffs)
    {
        AddMessage(Message::TYPE_ERROR, "Invalid 'interaxle' axle ids, skipping...");
        return;
    }

    if (m_actor->m_transfer_case)
    {
        if ((m_actor->m_transfer_case->tr_ax_1 == def.a1 && m_actor->m_transfer_case->tr_ax_2 == def.a2) ||
            (m_actor->m_transfer_case->tr_ax_1 == def.a2 && m_actor->m_transfer_case->tr_ax_2 == def.a1))
        {
            AddMessage(Message::TYPE_ERROR, "You cannot have both an inter-axle differential and a transfercase between the same two axles, skipping...");
            return;
        }
    }

    Differential *diff = new Differential();

    diff->di_idx_1 = def.a1;
    diff->di_idx_2 = def.a2;

    if (def.options.size() == 0)
    {
        AddMessage(Message::TYPE_INFO, "No differential defined, defaulting to Locked");
        diff->AddDifferentialType(LOCKED_DIFF);
    }
    else
    {
        for (RigDef::DifferentialType val: def.options)
        {
            switch (val)
            {
            case RigDef::DifferentialType::l_LOCKED:
                diff->AddDifferentialType(LOCKED_DIFF);
                break;
            case RigDef::DifferentialType::o_OPEN:
                diff->AddDifferentialType(OPEN_DIFF);
                break;
            case RigDef::DifferentialType::s_SPLIT:
                diff->AddDifferentialType(SPLIT_DIFF);
                break;
            case RigDef::DifferentialType::v_VISCOUS:
                diff->AddDifferentialType(VISCOUS_DIFF);
                break;
            default:
                AddMessage(Message::TYPE_WARNING, fmt::format("Unknown differential type: '{}'", (char)val));
                break;
            }
        }
    }

    m_actor->m_axle_diffs[m_actor->m_num_axle_diffs] = diff;
    m_actor->m_num_axle_diffs++;
}

void ActorSpawner::ProcessTransferCase(RigDef::TransferCase & def)
{
    if (def.a1 == def.a2 || def.a1 < 0 || std::max(def.a1 , def.a2) >= m_actor->m_num_wheel_diffs)
    {
        AddMessage(Message::TYPE_ERROR, "Invalid 'transfercase' axle ids, skipping...");
        return;
    }
    if (def.a2 < 0) // No 4WD mode
    {
        if (!def.has_2wd) // No 2WD mode
        {
            AddMessage(Message::TYPE_ERROR, "Invalid 'transfercase': Define alternate axle or allow 2WD, skipping...");
            return;
        }
        else // Only 2WD
        {
            AddMessage(Message::TYPE_INFO, "No alternate axle defined, defaulting to 2WD only");
        }
    }

    m_actor->m_transfer_case = new TransferCase(def.a1, def.a2, def.has_2wd, def.has_2wd_lo, def.gear_ratios);

    for (int i = 0; i < m_actor->ar_num_wheels; i++)
    {
        m_actor->ar_wheels[i].wh_propulsed = false;
    }
    m_actor->ar_wheels[m_actor->m_wheel_diffs[def.a1]->di_idx_1].wh_propulsed = true;
    m_actor->ar_wheels[m_actor->m_wheel_diffs[def.a1]->di_idx_2].wh_propulsed = true;
    m_actor->m_num_proped_wheels = 2;
    if (!def.has_2wd)
    {
        m_actor->ar_wheels[m_actor->m_wheel_diffs[def.a2]->di_idx_1].wh_propulsed = true;
        m_actor->ar_wheels[m_actor->m_wheel_diffs[def.a2]->di_idx_2].wh_propulsed = true;
        m_actor->m_num_proped_wheels = 4;
        m_actor->m_transfer_case->tr_4wd_mode = true;
    }
}

void ActorSpawner::ProcessCruiseControl(RigDef::CruiseControl & def)
{
    m_actor->cc_target_speed_lower_limit = def.min_speed;
    if (m_actor->cc_target_speed_lower_limit <= 0.f)
    {
        std::stringstream msg;
        msg << "Invalid parameter 'lower_limit' (" << m_actor->cc_target_speed_lower_limit 
            << ") must be positive nonzero number. Using it anyway (compatibility)";
    }
    m_actor->cc_can_brake = def.autobrake != 0;
}

void ActorSpawner::ProcessSpeedLimiter(RigDef::SpeedLimiter& def)
{
    m_actor->sl_enabled = true;
    m_actor->sl_speed_limit = def.max_speed;
}

void ActorSpawner::ProcessTorqueCurve(RigDef::TorqueCurve & def)
{
    if (m_actor->ar_engine == nullptr)
    {
        AddMessage(Message::TYPE_WARNING, "Section 'torquecurve' found but no 'engine' defined, skipping...");
        return;
    }

    TorqueCurve *target_torque_curve = m_actor->ar_engine->getTorqueCurve();

    if (def.predefined_func_name.length() != 0)
    {
        target_torque_curve->setTorqueModel(def.predefined_func_name);
    }
    else
    {
        target_torque_curve->CreateNewCurve(); /* Use default name for custom curve */
        std::vector<RigDef::TorqueCurve::Sample>::iterator itor = def.samples.begin();
        for ( ; itor != def.samples.end(); itor++)
        {
            target_torque_curve->AddCurveSample(itor->power, itor->torque_percent);
        }
    }
}

void ActorSpawner::ProcessParticle(RigDef::Particle & def)
{
    if (App::gfx_particles_mode->getInt() != 1)
    {
        return;
    }

    int particle_index = m_actor->ar_num_custom_particles;
    cparticle_t & particle = m_actor->ar_custom_particles[particle_index];

    particle.emitterNode = GetNodeIndexOrThrow(def.emitter_node);
    particle.directionNode = GetNodeIndexOrThrow(def.reference_node);

    std::string name = this->ComposeName(def.particle_system_name.c_str(), particle_index);
    particle.psys = this->CreateParticleSystem(name, def.particle_system_name);
    if (particle.psys == nullptr)
    {
        std::stringstream msg;
        msg << "Failed to create particle system '" << name << "' (template: '" << def.particle_system_name <<"')";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return;
    }

    particle.snode = m_particles_parent_scenenode->createChildSceneNode();
    particle.snode->attachObject(particle.psys);
    particle.snode->setPosition(m_actor->ar_nodes[particle.emitterNode].AbsPosition);

    /* Shut down the emitters */
    particle.active = false; 
    for (unsigned int i = 0; i < particle.psys->getNumEmitters(); i++)
    {
        particle.psys->getEmitter(i)->setEnabled(false);
    }

    ++m_actor->ar_num_custom_particles;
}

void ActorSpawner::ProcessRopable(RigDef::Ropable & def)
{
    ropable_t ropable;
    ropable.rb_nodenum = this->ResolveNodeRef(def.node);
    ropable.rb_pos = static_cast<RopableID_t>(m_actor->ar_ropables.size());
    ropable.group = def.group;
    ropable.attached_ties = 0;
    ropable.attached_ropes = 0;
    ropable.multilock = def.has_multilock;
    m_actor->ar_ropables.push_back(ropable);
}

void ActorSpawner::ProcessTie(RigDef::Tie & def)
{
    node_t & node_1 = m_actor->ar_nodes[GetNodeIndexOrThrow(def.root_node)];
    node_t & node_2 = m_actor->ar_nodes[( (node_1.pos == 0) ? 1 : 0 )];

    BeamID_t beam_index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = AddBeam(node_1, node_2, def.beam_defaults, def.detacher_group);
    SetBeamStrength(beam, def.beam_defaults->GetScaledBreakingThreshold());
    beam.k = def.beam_defaults->GetScaledSpringiness();
    beam.d = def.beam_defaults->GetScaledDamping();
    beam.bm_type = BEAM_HYDRO;
    beam.L = def.max_reach_length;
    beam.refL = def.max_reach_length;
    beam.bounded = ROPE;
    beam.bm_disabled = true;

    if (BITMASK_IS_0(def.options, RigDef::Tie::OPTION_i_INVISIBLE))
    {
        this->CreateBeamVisuals(beam, beam_index, false, def.beam_defaults);
    }

    /* Register tie */
    tie_t tie;
    tie.ti_group = def.group;
    tie.ti_tying = false;
    tie.ti_tied = false;
    tie.ti_beamid = beam_index;
    tie.ti_locked_actor   = nullptr;
    tie.ti_contract_speed = def.auto_shorten_rate;
    tie.ti_max_stress = def.max_stress;
    tie.ti_min_length = def.min_length;
    tie.ti_no_self_lock = BITMASK_IS_1(def.options, RigDef::Tie::OPTION_s_DISABLE_SELF_LOCK);
    m_actor->ar_ties.push_back(tie);

    m_actor->m_has_command_beams = true;
}

void ActorSpawner::ProcessRope(RigDef::Rope & def)
{
    node_t & root_node = m_actor->ar_nodes[GetNodeIndexOrThrow(def.root_node)];
    node_t & end_node = m_actor->ar_nodes[GetNodeIndexOrThrow(def.end_node)];

    /* Add beam */
    BeamID_t beam_index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = AddBeam(root_node, end_node, def.beam_defaults, def.detacher_group);
    SetBeamStrength(beam, def.beam_defaults->GetScaledBreakingThreshold());
    beam.k = def.beam_defaults->GetScaledSpringiness();
    beam.d = def.beam_defaults->GetScaledDamping();
    beam.bounded = ROPE;
    beam.bm_type = BEAM_HYDRO;
    beam.L = root_node.AbsPosition.distance(end_node.AbsPosition);
    beam.refL = beam.L;

    this->CreateBeamVisuals(beam, beam_index, true, def.beam_defaults, "tracks/beam");

    /* Register rope */
    rope_t rope;
    rope.rp_beam = beam_index;
    rope.rp_locked = UNLOCKED;
    rope.rp_locked_ropable_id = ROPABLEID_INVALID;
    rope.rp_group = 0; // Orig: hardcoded in BTS_ROPES. TODO: To be used.
    m_actor->ar_ropes.push_back(rope);
}

void ActorSpawner::ProcessRailGroup(RigDef::RailGroup & def)
{
    RailGroup* rail_group = this->CreateRail(def.node_list);
    rail_group->rg_id = def.id;
    m_actor->m_railgroups.push_back(rail_group);
}

void ActorSpawner::ProcessSlidenode(RigDef::SlideNode & def)
{
    NodeNum_t node = this->ResolveNodeRef(def.slide_node);
    SlideNode slide_node(m_actor, node, nullptr);

    // Optional args
    if (def._spring_rate_set)      { slide_node.SetSpringRate(def.spring_rate); }
    if (def._break_force_set)      { slide_node.SetBreakForce(def.break_force); }
    if (def._tolerance_set)        { slide_node.SetCorThreshold(def.tolerance); }
    if (def._attachment_rate_set)  { slide_node.SetAttachmentRate(def.attachment_rate); }
    if (def._max_attach_dist_set)  { slide_node.SetAttachmentDistance(def.max_attach_dist); }

    // Constraints
    if (BITMASK_IS_1(def.constraint_flags, RigDef::SlideNode::CONSTRAINT_ATTACH_ALL))
    {
        slide_node.sn_attach_self = true;
        slide_node.sn_attach_foreign = true;
    }
    if (BITMASK_IS_1(def.constraint_flags, RigDef::SlideNode::CONSTRAINT_ATTACH_SELF))
    {
        slide_node.sn_attach_self = true;
        slide_node.sn_attach_foreign = false;
    }
    if (BITMASK_IS_1(def.constraint_flags, RigDef::SlideNode::CONSTRAINT_ATTACH_FOREIGN))
    {
        slide_node.sn_attach_self = false;
        slide_node.sn_attach_foreign = true;
    }
    if (BITMASK_IS_1(def.constraint_flags, RigDef::SlideNode::CONSTRAINT_ATTACH_NONE))
    {
        slide_node.sn_attach_self = false;
        slide_node.sn_attach_foreign = false;
    }

    // RailGroup
    RailGroup *rail_group = nullptr;
    if (def._railgroup_id_set)
    {
        std::vector<RailGroup*>::iterator itor = m_actor->m_railgroups.begin();
        for ( ; itor != m_actor->m_railgroups.end(); itor++)
        {
            if ((*itor)->rg_id == def.railgroup_id)
            {
                rail_group = *itor;
                break;
            }
        }

        if (rail_group == nullptr)
        {
            std::stringstream msg;
            msg << "Specified rail group id '" << def.railgroup_id << "' not found. Ignoring slidenode...";
            AddMessage(Message::TYPE_ERROR, msg.str());
            return;
        }
    }
    else if (def.rail_node_ranges.size() > 0)
    {
        rail_group = this->CreateRail(def.rail_node_ranges);
        if (rail_group != nullptr)
        {
            m_actor->m_railgroups.push_back(rail_group);
        }
    }
    else
    {
        AddMessage(Message::TYPE_ERROR, "No RailGroup available for SlideNode, skipping...");
    }

    slide_node.SetDefaultRail(rail_group);
    m_actor->m_slidenodes.push_back(slide_node);
}

NodeNum_t ActorSpawner::FindNodeIndex(RigDef::Node::Ref & node_ref, bool silent /* = false */)
{
    NodeNum_t node = ResolveNodeRef(node_ref);
    if (node == NODENUM_INVALID && !silent)
    {
        std::stringstream msg;
        msg << "Failed to find node by reference: " << node_ref.ToString();
        AddMessage(Message::TYPE_ERROR, msg.str());
    }
    return node;
}

bool ActorSpawner::CollectNodesFromRanges(
    std::vector<RigDef::Node::Range> & node_ranges,
    std::vector<NodeNum_t> & out_node_indices
    )
{
    std::vector<RigDef::Node::Range>::iterator itor = node_ranges.begin();
    for ( ; itor != node_ranges.end(); itor++)
    {
        if (itor->IsRange())
        {

            NodeNum_t start = FindNodeIndex(itor->start, /* silent= */ false);
            if (start == NODENUM_INVALID)
            {
                AddMessage(Message::TYPE_WARNING, fmt::format("Invalid start node in range: {}", itor->start.ToString()));
                return false;
            }

            NodeNum_t end = FindNodeIndex(itor->end,   /* silent= */ true);

            if (end == NODENUM_INVALID)
            {
                std::stringstream msg;
                msg << "Encountered non-existent node '" << itor->end.ToString() << "' in range [" << itor->start.ToString() << " - " << itor->end.ToString() << "], "
                    << "highest node index is '" << static_cast<int>(m_actor->ar_nodes.size()) - 1 << "'.";

                if (itor->end.Str().empty()) /* If the node is numeric... */
                {
                    msg << " However, this node must be accepted anyway for backwards compatibility."
                        << " Please fix this as soon as possible.";
                    end = itor->end.Num();
                    AddMessage(Message::TYPE_ERROR, msg.str());
                }
                else
                {
                    AddMessage(Message::TYPE_ERROR, msg.str());
                    return false;
                }
            }

            if (end < start)
            {
                NodeNum_t swap = start;
                start = end;
                end = swap;
            }

            for (NodeNum_t i = start; i <= end; i++)
            {
                out_node_indices.push_back(i);
            }
        }
        else
        {
            out_node_indices.push_back(GetNodeIndexOrThrow(itor->start));
        }
    }
    return true;
}

RailGroup *ActorSpawner::CreateRail(std::vector<RigDef::Node::Range> & node_ranges)
{
    // Collect nodes
    std::vector<NodeNum_t> node_indices;
    this->CollectNodesFromRanges(node_ranges, node_indices);

    // Build the rail
    RailGroup* rg = new RailGroup(m_actor);
    for (unsigned int i = 0; i < node_indices.size() - 1; i++)
    {
        beam_t *beam = FindBeamInRig(node_indices[i], node_indices[i + 1]);
        if (beam == nullptr)
        {
            std::stringstream msg;
            msg << "No beam between nodes indexed '" << node_indices[i] << "' and '" << node_indices[i + 1] << "'";
            AddMessage(Message::TYPE_ERROR, msg.str());
            delete rg;
            return nullptr;
        }
        rg->rg_segments.emplace_back(beam->bm_pos);
    }

    rg->rg_segments_loop = (node_indices.front() == node_indices.back());

    return rg; // Transfers memory ownership
}

beam_t *ActorSpawner::FindBeamInRig(NodeNum_t node_a_index, NodeNum_t node_b_index)
{
    for (unsigned int i = 0; i < static_cast<unsigned int>(static_cast<int>(m_actor->ar_beams.size())); i++)
    {
        if	(
                (GetBeam(i).p1num == node_a_index && GetBeam(i).p2num == node_b_index)
            ||	(GetBeam(i).p2num == node_a_index && GetBeam(i).p1num == node_b_index)
            )
        {
            return & GetBeam(i);
        }
    }
    return nullptr;
}

void ActorSpawner::ProcessHook(RigDef::Hook & def)
{
    /* Find the node */
    NodeNum_t nodenum = this->ResolveNodeRef(def.node);
    if (nodenum ==  NODENUM_INVALID)
    {
        return;
    }

    /* Find the hook */
    hook_t *hook = nullptr;
    std::vector <hook_t>::iterator itor = m_actor->ar_hooks.begin();
    for (; itor != m_actor->ar_hooks.end(); itor++)
    {
        if (itor->hk_hook_node == nodenum)
        {
            hook = &*itor;
            break;
        }
    }

    if (hook == nullptr)
    {
        std::stringstream msg;
        msg << "Node '" << def.node.ToString() << "' is not a hook-node (not marked with flag 'h'), ignoring...";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return;
    }

    /* Process options */
    hook->hk_lockrange = def.option_hook_range;
    hook->hk_lockspeed = def.option_speed_coef * HOOK_SPEED_DEFAULT;
    hook->hk_maxforce  = def.option_max_force;
    hook->hk_group     = def.option_hookgroup;
    hook->hk_lockgroup = def.option_lockgroup;
    hook->hk_timer     = 0.f; // Hardcoded in BTS_HOOKS
    hook->hk_timer_preset = def.option_timer;
    hook->hk_min_length = def.option_min_range_meters;
    hook->hk_selflock   = def.flag_self_lock;
    hook->hk_nodisable  = def.flag_no_disable;
    if (def.flag_auto_lock)
    {
        hook->hk_autolock = true;
        if (hook->hk_group == -1)
        {
            hook->hk_group = -2; /* only overwrite hgroup when its still default (-1) */
        }
    }
    if (def.flag_no_rope)
    {
        m_actor->ar_beams[hook->hk_beam].bounded = NOSHOCK;
    }
    if (!def.flag_visible) // NOTE: This flag can only hide a visible beam - it won't show a beam defined with 'invisible' flag.
    {
        // Find beam index
        int beam_index = -1;
        for (int i = 0; i < static_cast<int>(m_actor->ar_beams.size()); ++i)
        {
            if (hook->hk_beam == static_cast<BeamID_t>(i))
            {
                beam_index = i;
                break;
            }
        }

        // Erase beam visuals (only exist if defined without 'invisible' flag - we don't know at this point)
        m_actor->m_gfx_actor->RemoveBeam(beam_index);
    }
}

void ActorSpawner::ProcessLockgroup(RigDef::Lockgroup & lockgroup)
{
    auto itor = lockgroup.nodes.begin();
    auto end  = lockgroup.nodes.end();
    for (; itor != end; ++itor)
    {
        NodeNum_t node = this->GetNodeIndexOrThrow(*itor);
        m_actor->ar_nodes[node].nd_lockgroup = lockgroup.number;
    }
}

void ActorSpawner::ProcessTrigger(RigDef::Trigger & def)
{
    shock_t & shock = this->GetFreeShock();

    // Disable trigger on startup? (default enabled)
    shock.trigger_enabled = BITMASK_IS_0(def.options, RigDef::Trigger::OPTION_x_START_DISABLED);

    m_actor->ar_command_key[def.shortbound_trigger_action].trigger_cmdkeyblock_state = false;
    if (def.longbound_trigger_action != -1)
    {
        m_actor->ar_command_key[def.longbound_trigger_action].trigger_cmdkeyblock_state = false;
    }

    BitMask_t shock_flags = SHOCK_FLAG_NORMAL | SHOCK_FLAG_ISTRIGGER;
    float short_limit = def.contraction_trigger_limit;
    float long_limit = def.expansion_trigger_limit;

    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_B_TRIGGER_BLOCKER))
    {
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_TRG_BLOCKER);
    }
    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_s_CMD_NUM_SWITCH)) // switch that exchanges cmdshort/cmdshort for all triggers with the same commandnumbers, default false
    {
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_TRG_CMD_SWITCH);
    }
    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_c_COMMAND_STYLE)) // trigger is set with commandstyle boundaries instead of shocksytle
    {
        short_limit = fabs(short_limit - 1);
        long_limit = long_limit - 1;
    }
    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_A_INV_TRIGGER_BLOCKER)) // Blocker that enable/disable other triggers, reversed activation method (inverted Blocker style, auto-ON)
    {
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_TRG_BLOCKER_A);
    }
    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_h_UNLOCKS_HOOK_GROUP))
    {
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_TRG_HOOK_UNLOCK);
    }
    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_H_LOCKS_HOOK_GROUP))
    {
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_TRG_HOOK_LOCK);
    }
    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_t_CONTINUOUS))
    {
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_TRG_CONTINUOUS); // this trigger sends values between 0 and 1
    }
    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_E_ENGINE_TRIGGER))
    {
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_TRG_ENGINE);
    }

    // Checks
    if (!def.IsTriggerBlockerAnyType() && !def.IsHookToggleTrigger() && !BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_E_ENGINE_TRIGGER))
    {
        if (def.shortbound_trigger_action < 1 || def.shortbound_trigger_action > MAX_COMMANDS)
        {
            std::stringstream msg;
            msg << "Invalid value of 'shortbound_trigger_action': '" << def.shortbound_trigger_action << "'. Must be between 1 and "<<MAX_COMMANDS<<". Ignoring trigger.";
            AddMessage(Message::TYPE_ERROR, msg.str());
            return;
        }
    }
    else if (!def.IsHookToggleTrigger() && !BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_E_ENGINE_TRIGGER))
    {
        // this is a Trigger-Blocker, make special check
        if (def.shortbound_trigger_action < 0 || def.longbound_trigger_action < 0)
        {
            AddMessage(Message::TYPE_ERROR, "Wrong command-eventnumber (Triggers). Trigger-Blocker deactivated.");
            return;
        }
    }
    else if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_E_ENGINE_TRIGGER))
    {
        if (def.IsTriggerBlockerAnyType() || def.IsHookToggleTrigger() || BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_s_CMD_NUM_SWITCH))
        {
            AddMessage(Message::TYPE_ERROR, "Wrong command-eventnumber (Triggers). Engine trigger deactivated.");
            return;
        }
    }

    const NodeNum_t node_1_index = FindNodeIndex(def.nodes[0]);
    const NodeNum_t node_2_index = FindNodeIndex(def.nodes[1]);
    if (node_1_index == NODENUM_INVALID || node_2_index == NODENUM_INVALID)
    {
        this->AddMessage(Message::TYPE_WARNING, "Skipping trigger, some nodes not found");
        return;
    }

    beam_t & beam = AddBeam(m_actor->ar_nodes[node_1_index], m_actor->ar_nodes[node_2_index], def.beam_defaults, def.detacher_group);
    beam.bm_type = BEAM_HYDRO;
    SetBeamStrength(beam, def.beam_defaults->breaking_threshold);
    SetBeamSpring(beam, 0.f);
    SetBeamDamping(beam, 0.f);
    CalculateBeamLength(beam);
    beam.shortbound = short_limit;
    beam.longbound = long_limit;
    beam.bounded = TRIGGER;
    beam.bm_shockid = shock.sk_pos;

    if (BITMASK_IS_0(def.options, RigDef::Trigger::OPTION_i_INVISIBLE))
    {
        this->CreateBeamVisuals(beam, beam.bm_pos, true, def.beam_defaults);
    }

    if (m_actor->m_trigger_debug_enabled)
    {
        LOG("Trigger added. BeamID " + TOSTRING(beam.bm_pos));
    }

    shock.sk_beamid = beam.bm_pos;
    shock.trigger_switch_state = 0.0f;   // used as bool and countdowntimer, dont touch!

    if (!def.IsTriggerBlockerAnyType())
    {
        shock.trigger_cmdshort = def.shortbound_trigger_action;
        if (def.longbound_trigger_action != -1 || (def.longbound_trigger_action == -1 && def.IsHookToggleTrigger()))
        {
            // this is a trigger or a hook_toggle
            shock.trigger_cmdlong = def.longbound_trigger_action;
        }
        else
        {
            // this is a commandkeyblocker
            shock_flags |= SHOCK_FLAG_TRG_CMD_BLOCKER;
        }
    } 
    else 
    {
        // this is a trigger_blocker
        if (!BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_A_INV_TRIGGER_BLOCKER))
        {
            //normal BLOCKER
            shock_flags |= SHOCK_FLAG_TRG_BLOCKER;
            shock.trigger_cmdshort = def.shortbound_trigger_action;
            shock.trigger_cmdlong  = def.longbound_trigger_action;
        } 
        else
        {
            //inverted BLOCKER
            shock_flags |= SHOCK_FLAG_TRG_BLOCKER_A;
            shock.trigger_cmdshort = def.shortbound_trigger_action;
            shock.trigger_cmdlong  = def.longbound_trigger_action;
        }
    }

    if (BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_b_KEY_BLOCKER) && !BITMASK_IS_1(def.options, RigDef::Trigger::OPTION_B_TRIGGER_BLOCKER))
    {
        m_actor->ar_command_key[def.shortbound_trigger_action].trigger_cmdkeyblock_state = true;
        if (def.longbound_trigger_action != -1)
        {
            m_actor->ar_command_key[def.longbound_trigger_action].trigger_cmdkeyblock_state = true;
        }
    }

    shock.trigger_boundary_t = def.boundary_timer;
    shock.sk_flags           = shock_flags;
    shock.sbd_spring         = def.beam_defaults->springiness;
    shock.sbd_damp           = def.beam_defaults->damping_constant;
    shock.last_debug_state   = 0;
    
}

void ActorSpawner::ProcessContacter(RigDef::Node::Ref & node_ref)
{
    unsigned int node_index = GetNodeIndexOrThrow(node_ref);
    m_actor->ar_nodes[node_index].nd_contacter = true;
};

void ActorSpawner::ProcessRotator(RigDef::Rotator & def)
{
    rotator_t & rotator = m_actor->ar_rotators[m_actor->ar_num_rotators];

    rotator.angle     = 0;
    rotator.rate      = def.rate;
    rotator.axis1     = GetNodeIndexOrThrow(def.axis_nodes[0]);
    rotator.axis2     = GetNodeIndexOrThrow(def.axis_nodes[1]);
    rotator.force     = ROTATOR_FORCE_DEFAULT;
    rotator.tolerance = ROTATOR_TOLERANCE_DEFAULT;
    rotator.engine_coupling = def.engine_coupling;
    rotator.needs_engine = def.needs_engine;
    for (unsigned int i = 0; i < 4; i++)
    {
        rotator.nodes1[i] = GetNodeIndexOrThrow(def.base_plate_nodes[i]);
        rotator.nodes2[i] = GetNodeIndexOrThrow(def.rotating_plate_nodes[i]);
    }

    // Validate the reference structure
    this->ValidateRotator(m_actor->ar_num_rotators + 1, rotator.axis1, rotator.axis2, rotator.nodes1, rotator.nodes2);

    // Rotate left key
    m_actor->ar_command_key[def.spin_left_key].rotators.push_back(- (m_actor->ar_num_rotators + 1));
    m_actor->ar_command_key[def.spin_left_key].description = "Rotate_Left/Right";

    // Rotate right key
    m_actor->ar_command_key[def.spin_right_key].rotators.push_back(m_actor->ar_num_rotators + 1);

    this->_ProcessKeyInertia(def.inertia, *def.inertia_defaults,
                             m_actor->ar_command_key[def.spin_left_key].rotator_inertia,
                             m_actor->ar_command_key[def.spin_right_key].rotator_inertia);

    m_actor->ar_num_rotators++;
    m_actor->m_has_command_beams = true;
}

void ActorSpawner::ProcessRotator2(RigDef::Rotator2 & def)
{
    rotator_t & rotator = m_actor->ar_rotators[m_actor->ar_num_rotators];

    rotator.angle = 0;
    rotator.rate = def.rate;
    rotator.axis1 = GetNodeIndexOrThrow(def.axis_nodes[0]);
    rotator.axis2     = GetNodeIndexOrThrow(def.axis_nodes[1]);
    rotator.force     = def.rotating_force; // Default value is set in constructor
    rotator.tolerance = def.tolerance; // Default value is set in constructor
    rotator.engine_coupling = def.engine_coupling;
    rotator.needs_engine = def.needs_engine;
    for (unsigned int i = 0; i < 4; i++)
    {
        rotator.nodes1[i] = GetNodeIndexOrThrow(def.base_plate_nodes[i]);
        rotator.nodes2[i] = GetNodeIndexOrThrow(def.rotating_plate_nodes[i]);
    }

    // Validate the reference structure
    this->ValidateRotator(m_actor->ar_num_rotators + 1, rotator.axis1, rotator.axis2, rotator.nodes1, rotator.nodes2);

    // Rotate left key
    m_actor->ar_command_key[def.spin_left_key].rotators.push_back(- (m_actor->ar_num_rotators + 1));
    if (! def.description.empty())
    {
        m_actor->ar_command_key[def.spin_left_key].description = def.description;
    }
    else
    {
        m_actor->ar_command_key[def.spin_left_key].description = "Rotate_Left/Right";
    }

    // Rotate right key
    m_actor->ar_command_key[def.spin_right_key].rotators.push_back(m_actor->ar_num_rotators + 1);

    this->_ProcessKeyInertia(def.inertia, *def.inertia_defaults,
                             m_actor->ar_command_key[def.spin_left_key].rotator_inertia,
                             m_actor->ar_command_key[def.spin_right_key].rotator_inertia);

    m_actor->ar_num_rotators++;
    m_actor->m_has_command_beams = true;
}

void ActorSpawner::_ProcessSimpleInertia(RigDef::Inertia & inertia, RoR::SimpleInertia& obj)
{
    // TODO: refactor _ProcessKeyInertia() to use this.

    // Handle placeholders
    std::string start_function;
    std::string stop_function;
    if (inertia.start_function != "" && inertia.start_function != "/" && inertia.start_function != "-")
    {
        start_function = inertia.start_function;
    }
    if (inertia.stop_function != "" && inertia.stop_function != "/" && inertia.stop_function != "-")
    {
        stop_function = inertia.stop_function;
    }

    obj.SetSimpleDelay(
        App::GetGameContext()->GetActorManager()->GetInertiaConfig(),
        inertia.start_delay_factor,
        inertia.stop_delay_factor,
        start_function,
        stop_function
    );
}

void ActorSpawner::_ProcessKeyInertia(
    RigDef::Inertia & inertia,
    RigDef::Inertia & inertia_defaults,
    RoR::CmdKeyInertia& contract_cmd,
    RoR::CmdKeyInertia& extend_cmd
)
{
    /* Handle placeholders */
    Ogre::String start_function;
    Ogre::String stop_function;
    if (! inertia.start_function.empty() && inertia.start_function != "/" && inertia.start_function != "-")
    {
        start_function = inertia.start_function;
    }
    if (! inertia.stop_function.empty() && inertia.stop_function != "/" && inertia.stop_function != "-")
    {
        stop_function = inertia.stop_function;
    }
    if (inertia.start_delay_factor != 0.f && inertia.stop_delay_factor != 0.f)
    {
        contract_cmd.SetCmdKeyDelay(
            App::GetGameContext()->GetActorManager()->GetInertiaConfig(),
            inertia.start_delay_factor,
            inertia.stop_delay_factor,
            start_function,
            stop_function
        );

        extend_cmd.SetCmdKeyDelay(
            App::GetGameContext()->GetActorManager()->GetInertiaConfig(),
            inertia.start_delay_factor,
            inertia.stop_delay_factor,
            start_function,
            stop_function
        );
    }
    else if (inertia_defaults.start_delay_factor > 0 || inertia_defaults.stop_delay_factor > 0)
    {
        contract_cmd.SetCmdKeyDelay(
            App::GetGameContext()->GetActorManager()->GetInertiaConfig(),
            inertia_defaults.start_delay_factor,
            inertia_defaults.stop_delay_factor,
            inertia_defaults.start_function,
            inertia_defaults.stop_function
        );

        extend_cmd.SetCmdKeyDelay(
            App::GetGameContext()->GetActorManager()->GetInertiaConfig(),
            inertia_defaults.start_delay_factor,
            inertia_defaults.stop_delay_factor,
            inertia_defaults.start_function,
            inertia_defaults.stop_function
        );
    }
}

void ActorSpawner::ProcessCommand(RigDef::Command2 & def)
{
    const NodeNum_t beam_index = static_cast<int>(m_actor->ar_beams.size());
    const NodeNum_t node_1_index = FindNodeIndex(def.nodes[0]);
    const NodeNum_t node_2_index = FindNodeIndex(def.nodes[1]);
    if (node_1_index == NODENUM_INVALID || node_2_index == NODENUM_INVALID)
    {
        AddMessage(Message::TYPE_ERROR, "Failed to fetch node");
        return;
    }
    beam_t & beam = AddBeam(m_actor->ar_nodes[node_1_index], m_actor->ar_nodes[node_2_index], def.beam_defaults, def.detacher_group);
    CalculateBeamLength(beam);
    SetBeamStrength(beam, def.beam_defaults->GetScaledBreakingThreshold()); /* Override settings from AddBeam() */
    SetBeamSpring(beam, def.beam_defaults->GetScaledSpringiness());
    SetBeamDamping(beam, def.beam_defaults->GetScaledDamping());
    beam.bm_type = BEAM_HYDRO;

    /* Options */
    if (def.option_r_rope)          { beam.bounded = ROPE; }

    /* set the middle of the command, so its not required to recalculate this everytime ... */
    float center_length = 0.f;
    if (def.max_extension > def.max_contraction)
    {
        center_length = (def.max_extension - def.max_contraction) / 2 + def.max_contraction;
    }
    else
    {
        center_length = (def.max_contraction - def.max_extension) / 2 + def.max_extension;
    }

    /* Add keys */
    command_t* contract_command = &m_actor->ar_command_key[def.contract_key];
    commandbeam_t cmd_beam;
    cmd_beam.cmb_beam_index = static_cast<uint16_t>(beam_index);
    cmd_beam.cmb_is_contraction = true;
    cmd_beam.cmb_speed = def.shorten_rate;
    cmd_beam.cmb_boundary_length = def.max_contraction;
    cmd_beam.cmb_is_force_restricted = def.option_f_not_faster;
    cmd_beam.cmb_is_autocentering = def.option_c_auto_center;
    cmd_beam.cmb_needs_engine = def.needs_engine;
    cmd_beam.cmb_is_1press = def.option_p_1press;      
    cmd_beam.cmb_is_1press_center = def.option_o_1press_center;
    cmd_beam.cmb_plays_sound = def.plays_sound;
    cmd_beam.cmb_engine_coupling = def.affect_engine;
    cmd_beam.cmb_center_length = center_length;
    cmd_beam.cmb_state = std::shared_ptr<commandbeam_state_t>(new commandbeam_state_t);
    contract_command->beams.push_back(cmd_beam);
    if (contract_command->description.empty())
    {
        contract_command->description = def.description;
    }

    command_t* extend_command = &m_actor->ar_command_key[def.extend_key];
    cmd_beam.cmb_is_contraction = false;
    cmd_beam.cmb_speed = def.lengthen_rate;
    cmd_beam.cmb_boundary_length = def.max_extension;
    extend_command->beams.push_back(cmd_beam);
    if (extend_command->description.empty())
    {
        extend_command->description = def.description;
    }

    this->_ProcessKeyInertia(def.inertia, *def.inertia_defaults,
                             contract_command->command_inertia,
                             extend_command->command_inertia);

    if (! def.option_i_invisible)
    {
        this->CreateBeamVisuals(beam, beam_index, true, def.beam_defaults);
    }

    m_actor->m_num_command_beams++;
    m_actor->m_has_command_beams = true;
}

void ActorSpawner::ProcessAnimator(RigDef::Animator & def)
{
    int anim_flags = 0;
    float anim_option = 0;

    /* Options. '{' intentionally misplaced. */

    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_AIRSPEED)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_AIRSPEED);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_VERTICAL_VELOCITY)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_VVI);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_ANGLE_OF_ATTACK)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_AOA);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_FLAP)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_FLAP);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_AIR_BRAKE)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_AIRBRAKE);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_ROLL))	{
        BITMASK_SET_1(anim_flags, ANIM_FLAG_ROLL);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_PITCH)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_PITCH);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_BRAKES)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_BRAKE);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_ACCEL)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_ACCEL);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_CLUTCH)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_CLUTCH);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_SPEEDO)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_SPEEDO);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_TACHO)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_TACHO);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_TURBO)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_TURBO);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_PARKING)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_PBRAKE);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_TORQUE)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_TORQUE);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_BOAT_THROTTLE)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_BTHROTTLE);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_BOAT_RUDDER)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_BRUDDER);
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_SHIFT_LEFT_RIGHT)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_SHIFTER);
        anim_option = 1.f;
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_SHIFT_BACK_FORTH))	{
        BITMASK_SET_1(anim_flags, ANIM_FLAG_SHIFTER);
        anim_option = 2.f;
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_SEQUENTIAL_SHIFT)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_SHIFTER);
        anim_option = 3.f;
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_GEAR_SELECT)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_SHIFTER);
        anim_option = 4.f;
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_ALTIMETER_100K)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_ALTIMETER);
        anim_option = 1.f;
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_ALTIMETER_10K)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_ALTIMETER);
        anim_option = 2.f;
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_ALTIMETER_1K)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_ALTIMETER);
        anim_option = 3.f;
    }
    
    /* Aerial */
    if (BITMASK_IS_1(def.aero_animator.flags, RigDef::AeroAnimator::OPTION_THROTTLE)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_THROTTLE);
        anim_option = static_cast<float>(def.aero_animator.engine_idx);
    }
    if (BITMASK_IS_1(def.aero_animator.flags, RigDef::AeroAnimator::OPTION_RPM)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_RPM);
        anim_option = static_cast<float>(def.aero_animator.engine_idx);
    }
    if (BITMASK_IS_1(def.aero_animator.flags, RigDef::AeroAnimator::OPTION_TORQUE)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_AETORQUE);
        anim_option = static_cast<float>(def.aero_animator.engine_idx);
    }
    if (BITMASK_IS_1(def.aero_animator.flags, RigDef::AeroAnimator::OPTION_PITCH)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_AEPITCH);
        anim_option = static_cast<float>(def.aero_animator.engine_idx);
    }
    if (BITMASK_IS_1(def.aero_animator.flags, RigDef::AeroAnimator::OPTION_STATUS)) {
        BITMASK_SET_1(anim_flags, ANIM_FLAG_AESTATUS);
        anim_option = static_cast<float>(def.aero_animator.engine_idx);
    }

    unsigned int beam_index = static_cast<int>(m_actor->ar_beams.size());
    NodeNum_t n1 = this->GetNodeIndexOrThrow(def.nodes[0]);
    NodeNum_t n2 = this->GetNodeIndexOrThrow(def.nodes[1]);
    beam_t & beam = AddBeam(m_actor->ar_nodes[n1], m_actor->ar_nodes[n2], def.beam_defaults, def.detacher_group);
    /* set the limits to something with sense by default */
    beam.shortbound = 0.99999f;
    beam.longbound = 1000000.0f;
    beam.bm_type = BEAM_HYDRO;
    CalculateBeamLength(beam);
    SetBeamStrength(beam, def.beam_defaults->GetScaledBreakingThreshold());
    SetBeamSpring(beam, def.beam_defaults->GetScaledSpringiness());
    SetBeamDamping(beam, def.beam_defaults->GetScaledDamping());

    if (BITMASK_IS_0(def.flags, RigDef::Animator::OPTION_INVISIBLE))
    {
        this->CreateBeamVisuals(beam, beam_index, true, def.beam_defaults);
    }

    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_SHORT_LIMIT)) 
    {
        beam.shortbound = def.short_limit;
    }
    if (BITMASK_IS_1(def.flags, RigDef::Animator::OPTION_LONG_LIMIT)) 
    {
        beam.longbound = def.long_limit;
    }

    hydrobeam_t hb;
    hb.hb_beam_index = static_cast<uint16_t>(beam_index);
    hb.hb_speed = def.lenghtening_factor;
    hb.hb_ref_length = beam.L;
    hb.hb_flags = 0;
    hb.hb_anim_flags = anim_flags;
    hb.hb_anim_param = anim_option;

    if (def.inertia_defaults->start_delay_factor > 0 && def.inertia_defaults->stop_delay_factor > 0)
    {
        hb.hb_inertia.SetCmdKeyDelay(
            App::GetGameContext()->GetActorManager()->GetInertiaConfig(),
            def.inertia_defaults->start_delay_factor,
            def.inertia_defaults->stop_delay_factor,
            def.inertia_defaults->start_function,
            def.inertia_defaults->stop_function
        );
    }

    m_actor->ar_hydros.push_back(hb);
}

beam_t & ActorSpawner::AddBeam(
    node_t & node_1, 
    node_t & node_2, 
    std::shared_ptr<RigDef::BeamDefaults> & beam_defaults,
    int detacher_group
)
{
    /* Init */
    beam_t& beam = GetFreeBeam();
    InitBeam(beam, &node_1, &node_2);
    beam.detacher_group = detacher_group;
    beam.bm_disabled = false;

    /* Breaking threshold (strength) */
    float strength = beam_defaults->breaking_threshold;
    beam.strength = strength;

    /* Deformation */
    SetBeamDeformationThreshold(beam, beam_defaults);

    float plastic_coef = beam_defaults->plastic_deform_coef;
    beam.plastic_coef = plastic_coef;

    return beam;
}

void ActorSpawner::SetBeamStrength(beam_t & beam, float strength)
{
    beam.strength = strength;
}

void ActorSpawner::ProcessHydro(RigDef::Hydro & def)
{
    bool invisible = false;
    unsigned int hydro_flags = 0;

    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_j_INVISIBLE))
    {
        invisible = true;
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_s_DISABLE_ON_HIGH_SPEED))
    {
        hydro_flags |= HYDRO_FLAG_SPEED;
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_a_INPUT_AILERON))
    {
        hydro_flags |= HYDRO_FLAG_AILERON;
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_r_INPUT_RUDDER))
    {
        hydro_flags |= HYDRO_FLAG_RUDDER;
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_e_INPUT_ELEVATOR))
    {
        hydro_flags |= HYDRO_FLAG_ELEVATOR;
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_u_INPUT_AILERON_ELEVATOR))
    {
        hydro_flags |= (HYDRO_FLAG_AILERON | HYDRO_FLAG_ELEVATOR);
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_v_INPUT_InvAILERON_ELEVATOR))
    {
        hydro_flags |= (HYDRO_FLAG_REV_AILERON | HYDRO_FLAG_ELEVATOR);
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_x_INPUT_AILERON_RUDDER))
    {
        hydro_flags |= (HYDRO_FLAG_AILERON | HYDRO_FLAG_RUDDER);
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_y_INPUT_InvAILERON_RUDDER))
    {
        hydro_flags |= (HYDRO_FLAG_REV_AILERON | HYDRO_FLAG_RUDDER);
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_g_INPUT_ELEVATOR_RUDDER))
    {
        hydro_flags |= (HYDRO_FLAG_ELEVATOR | HYDRO_FLAG_RUDDER);
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_h_INPUT_InvELEVATOR_RUDDER))
    {
        hydro_flags |= (HYDRO_FLAG_REV_ELEVATOR | HYDRO_FLAG_RUDDER);
    }
    if (BITMASK_IS_1(def.options, RigDef::Hydro::OPTION_n_INPUT_NORMAL))
    {
        hydro_flags |= HYDRO_FLAG_DIR;
    }

    node_t & node_1 = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[0])];
    node_t & node_2 = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[1])];

    int beam_index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = AddBeam(node_1, node_2, def.beam_defaults, def.detacher_group);
    SetBeamStrength(beam, def.beam_defaults->GetScaledBreakingThreshold());
    CalculateBeamLength(beam);
    beam.bm_type              = BEAM_HYDRO;
    beam.k                    = def.beam_defaults->GetScaledSpringiness();
    beam.d                    = def.beam_defaults->GetScaledDamping();

    if (!invisible)
    {
        this->CreateBeamVisuals(beam, beam_index, true, def.beam_defaults);
    }

    hydrobeam_t hb;
    hb.hb_flags = hydro_flags;
    hb.hb_speed = def.lenghtening_factor;
    hb.hb_beam_index = static_cast<uint16_t>(beam_index);
    hb.hb_ref_length = beam.L;
    hb.hb_anim_flags = 0;
    hb.hb_anim_param = 0.f;
    this->_ProcessKeyInertia(def.inertia, *def.inertia_defaults, hb.hb_inertia, hb.hb_inertia);

    m_actor->ar_hydros.push_back(hb);
}

void ActorSpawner::ProcessShock3(RigDef::Shock3 & def)
{
    node_t & node_1 = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[0])];
    node_t & node_2 = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[1])];
    float short_bound = def.short_bound;
    float long_bound = def.long_bound;
    BitMask_t shock_flags = SHOCK_FLAG_NORMAL | SHOCK_FLAG_ISSHOCK3;

    if (BITMASK_IS_1(def.options, RigDef::Shock3::OPTION_m_METRIC))
    {
        float beam_length = node_1.AbsPosition.distance(node_2.AbsPosition);
        short_bound /= beam_length;
        long_bound /= beam_length;
    }
    if (BITMASK_IS_1(def.options, RigDef::Shock3::OPTION_M_ABSOLUTE_METRIC))
    {
        float beam_length = node_1.AbsPosition.distance(node_2.AbsPosition);
        short_bound = (beam_length - short_bound) / beam_length;
        long_bound = (long_bound - beam_length) / beam_length;

        if (long_bound < 0.f)
        {
            AddMessage(
                Message::TYPE_WARNING, 
                "Metric shock length calculation failed, 'short_bound' less than beams spawn length. Resetting to beam's spawn length (short_bound = 0)"
            );
            long_bound = 0.f;
        }

        if (short_bound > 1.f)
        {
            AddMessage(
                Message::TYPE_WARNING, 
                "Metric shock length calculation failed, 'short_bound' less than 0 meters. Resetting to 0 meters (short_bound = 1)"
            );
            short_bound = 1.f;
        }
    }
    
    int beam_index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = AddBeam(node_1, node_2, def.beam_defaults, def.detacher_group);
    SetBeamStrength(beam, def.beam_defaults->breaking_threshold * 4.f);
    beam.bm_type              = BEAM_HYDRO;
    beam.bounded              = SHOCK3;
    beam.k                    = def.spring_in;
    beam.d                    = def.damp_in;
    beam.shortbound           = short_bound;
    beam.longbound            = long_bound;

    /* Length + pre-compression */
    CalculateBeamLength(beam);
    beam.L          *= def.precompression;
    beam.refL       *= def.precompression;

    if (BITMASK_IS_0(def.options, RigDef::Shock3::OPTION_i_INVISIBLE))
    {
        this->CreateBeamVisuals(beam, beam_index, true, def.beam_defaults);
    }

    shock_t & shock  = GetFreeShock();
    shock.sk_flags   = shock_flags;
    shock.sbd_spring = def.beam_defaults->springiness;
    shock.sbd_damp   = def.beam_defaults->damping_constant;
    shock.springin   = def.spring_in;
    shock.dampin     = def.damp_in;
    shock.springout  = def.spring_out;
    shock.dampout    = def.damp_out;
    shock.splitin    = def.split_vel_in;
    shock.dslowin    = def.damp_in_slow;
    shock.dfastin    = def.damp_in_fast;
    shock.splitout   = def.split_vel_out;
    shock.dslowout   = def.damp_out_slow;
    shock.dfastout   = def.damp_out_fast;

    beam.bm_shockid = shock.sk_pos;
    shock.sk_beamid = beam.bm_pos;
}

void ActorSpawner::ProcessShock2(RigDef::Shock2 & def)
{
    node_t & node_1 = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[0])];
    node_t & node_2 = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[1])];
    float short_bound = def.short_bound;
    float long_bound = def.long_bound;
    BitMask_t shock_flags = SHOCK_FLAG_NORMAL | SHOCK_FLAG_ISSHOCK2;

    if (BITMASK_IS_1(def.options, RigDef::Shock2::OPTION_s_SOFT_BUMP_BOUNDS))
    {
        BITMASK_SET_0(shock_flags, SHOCK_FLAG_NORMAL); /* Not normal anymore */
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_SOFTBUMP);
    }
    if (BITMASK_IS_1(def.options, RigDef::Shock2::OPTION_m_METRIC))
    {
        float beam_length = node_1.AbsPosition.distance(node_2.AbsPosition);
        short_bound /= beam_length;
        long_bound /= beam_length;
    }
    if (BITMASK_IS_1(def.options, RigDef::Shock2::OPTION_M_ABSOLUTE_METRIC))
    {
        float beam_length = node_1.AbsPosition.distance(node_2.AbsPosition);
        short_bound = (beam_length - short_bound) / beam_length;
        long_bound = (long_bound - beam_length) / beam_length;

        if (long_bound < 0.f)
        {
            AddMessage(
                Message::TYPE_WARNING, 
                "Metric shock length calculation failed, 'short_bound' less than beams spawn length. Resetting to beam's spawn length (short_bound = 0)"
            );
            long_bound = 0.f;
        }

        if (short_bound > 1.f)
        {
            AddMessage(
                Message::TYPE_WARNING, 
                "Metric shock length calculation failed, 'short_bound' less than 0 meters. Resetting to 0 meters (short_bound = 1)"
            );
            short_bound = 1.f;
        }
    }
    
    int beam_index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = AddBeam(node_1, node_2, def.beam_defaults, def.detacher_group);
    SetBeamStrength(beam, def.beam_defaults->breaking_threshold * 4.f);
    beam.bm_type              = BEAM_HYDRO;
    beam.bounded              = SHOCK2;
    beam.k                    = def.spring_in;
    beam.d                    = def.damp_in;
    beam.shortbound           = short_bound;
    beam.longbound            = long_bound;

    /* Length + pre-compression */
    CalculateBeamLength(beam);
    beam.L          *= def.precompression;
    beam.refL       *= def.precompression;

    if (BITMASK_IS_0(def.options, RigDef::Shock2::OPTION_i_INVISIBLE))
    {
        this->CreateBeamVisuals(beam, beam_index, true, def.beam_defaults);
    }

    shock_t & shock  = GetFreeShock();
    shock.sk_flags   = shock_flags;
    shock.sbd_spring = def.beam_defaults->springiness;
    shock.sbd_damp   = def.beam_defaults->damping_constant;
    shock.springin   = def.spring_in;
    shock.dampin     = def.damp_in;
    shock.springout  = def.spring_out;
    shock.dampout    = def.damp_out;
    shock.sprogin    = def.progress_factor_spring_in;
    shock.dprogin    = def.progress_factor_damp_in;
    shock.sprogout   = def.progress_factor_spring_out;
    shock.dprogout   = def.progress_factor_damp_out;

    beam.bm_shockid = shock.sk_pos;
    shock.sk_beamid = beam.bm_pos;
}

void ActorSpawner::ProcessShock(RigDef::Shock & def)
{
    node_t & node_1 = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[0])];
    node_t & node_2 = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[1])];
    float short_bound = def.short_bound;
    float long_bound = def.long_bound;
    BitMask_t shock_flags = SHOCK_FLAG_NORMAL;

    if (BITMASK_IS_1(def.options, RigDef::Shock::OPTION_L_ACTIVE_LEFT))
    {
        BITMASK_SET_0(shock_flags, SHOCK_FLAG_NORMAL); /* Not normal anymore */
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_LACTIVE);
        m_actor->ar_has_active_shocks = true;
    }
    if (BITMASK_IS_1(def.options, RigDef::Shock::OPTION_R_ACTIVE_RIGHT))
    {
        BITMASK_SET_0(shock_flags, SHOCK_FLAG_NORMAL); /* Not normal anymore */
        BITMASK_SET_1(shock_flags, SHOCK_FLAG_RACTIVE);
        m_actor->ar_has_active_shocks = true;
    }
    if (BITMASK_IS_1(def.options, RigDef::Shock::OPTION_m_METRIC))
    {
        float beam_length = node_1.AbsPosition.distance(node_2.AbsPosition);
        short_bound /= beam_length;
        long_bound /= beam_length;
    }
    
    int beam_index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = AddBeam(node_1, node_2, def.beam_defaults, def.detacher_group);
    beam.shortbound = short_bound;
    beam.longbound  = long_bound;
    beam.bounded    = SHOCK1;
    beam.bm_type    = BEAM_HYDRO;
    beam.k          = def.spring_rate;
    beam.d          = def.damping;
    SetBeamStrength(beam, def.beam_defaults->breaking_threshold * 4.f);

    /* Length + pre-compression */
    CalculateBeamLength(beam);
    beam.L          *= def.precompression;
    beam.refL       *= def.precompression;

    shock_t & shock  = GetFreeShock();
    shock.sk_flags   = shock_flags;
    shock.sbd_spring = def.beam_defaults->springiness;
    shock.sbd_damp   = def.beam_defaults->damping_constant;

    if (BITMASK_IS_0(def.options, RigDef::Shock::OPTION_i_INVISIBLE))
    {
        this->CreateBeamVisuals(beam, beam_index, true, def.beam_defaults);
    }

    beam.bm_shockid = shock.sk_pos;
    shock.sk_beamid = beam.bm_pos;
}

void ActorSpawner::ProcessFlexBodyWheel(RigDef::FlexBodyWheel & def)
{
    NodeNum_t base_node_index = static_cast<NodeNum_t>(m_actor->ar_nodes.size());
    wheel_t & wheel = m_actor->ar_wheels[m_actor->ar_num_wheels];

    NodeNum_t axis_node_1 = this->ResolveNodeRef(def.nodes[0]);
    NodeNum_t axis_node_2 = this->ResolveNodeRef(def.nodes[1]);
    // Enforce the "second node must have a larger Z coordinate than the first" constraint
    if (m_actor->ar_nodes[axis_node_1].AbsPosition.z > m_actor->ar_nodes[axis_node_2].AbsPosition.z)
    {
        NodeNum_t swap = axis_node_1;
        axis_node_1 = axis_node_2;
        axis_node_2 = swap;
    }

    // Rigidity node
    NodeNum_t rigidity_node = NODENUM_INVALID;
    NodeNum_t axis_node_closest_to_rigidity_node = NODENUM_INVALID;
    if (def.rigidity_node.IsValidAnyState())
    {
        rigidity_node = this->ResolveNodeRef(def.rigidity_node);
        Ogre::Real distance_1 = (m_actor->ar_nodes[rigidity_node].RelPosition - m_actor->ar_nodes[axis_node_1].RelPosition).length();
        Ogre::Real distance_2 = (m_actor->ar_nodes[rigidity_node].RelPosition - m_actor->ar_nodes[axis_node_2].RelPosition).length();
        axis_node_closest_to_rigidity_node = ((distance_1 < distance_2)) ? axis_node_1 : axis_node_2;
    }

    // Node&beam generation
    Ogre::Vector3 axis_vector = m_actor->ar_nodes[axis_node_2].RelPosition - m_actor->ar_nodes[axis_node_1].RelPosition;
    wheel.wh_width = axis_vector.length(); // wheel_def.width is ignored.
    axis_vector.normalise();
    Ogre::Vector3 rim_ray_vector = axis_vector.perpendicular() * def.rim_radius;
    Ogre::Quaternion rim_ray_rotator = Ogre::Quaternion(Ogre::Degree(-360.f / (def.num_rays * 2)), axis_vector);

    // Rim nodes
    for (unsigned int i = 0; i < def.num_rays; i++)
    {
        float node_mass = def.mass / (4.f * def.num_rays);

        // Outer ring
        Ogre::Vector3 ray_point = m_actor->ar_nodes[axis_node_1].RelPosition + rim_ray_vector;
        rim_ray_vector = rim_ray_rotator * rim_ray_vector;

        {
            node_t& outer_node = AddNode();
            InitNode(outer_node, ray_point, def.node_defaults);

            outer_node.mass = node_mass;
            outer_node.friction_coef = def.node_defaults->friction;
            outer_node.nd_rim_node = true;
            AdjustNodeBuoyancy(outer_node, def.node_defaults);
            m_actor->ar_nodes_aux[outer_node.pos].nda_minimass = m_state.global_minimass;

            wheel.wh_rim_nodes.push_back(outer_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(outer_node.pos));
        }

        // Inner ring
        ray_point = m_actor->ar_nodes[axis_node_2].RelPosition + rim_ray_vector;
        rim_ray_vector = rim_ray_rotator * rim_ray_vector;

        {
            node_t& inner_node = AddNode();
            InitNode(inner_node, ray_point, def.node_defaults);

            inner_node.mass = node_mass;
            inner_node.friction_coef = def.node_defaults->friction;
            inner_node.nd_rim_node = true;
            AdjustNodeBuoyancy(inner_node, def.node_defaults);
            m_actor->ar_nodes_aux[inner_node.pos].nda_minimass = m_state.global_minimass;

            wheel.wh_rim_nodes.push_back(inner_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(inner_node.pos));
        }
        
        
    }

    Ogre::Vector3 tyre_ray_vector = axis_vector.perpendicular() * def.tyre_radius;
    Ogre::Quaternion& tyre_ray_rotator = rim_ray_rotator;
    tyre_ray_vector = tyre_ray_rotator * tyre_ray_vector;

    // Tyre nodes
    for (unsigned int i = 0; i < def.num_rays; i++)
    {
        /* Outer ring */
        float node_mass = def.mass / (4.f * def.num_rays);
        Ogre::Vector3 ray_point = m_actor->ar_nodes[axis_node_1].RelPosition + tyre_ray_vector;
        tyre_ray_vector = tyre_ray_rotator * tyre_ray_vector;

        {
            node_t& outer_node = AddNode();
            InitNode(outer_node, ray_point);
            outer_node.mass = node_mass;
            outer_node.friction_coef = def.node_defaults->friction;
            outer_node.volume_coef = def.node_defaults->volume;
            outer_node.surface_coef = def.node_defaults->surface;
            outer_node.nd_contacter = true;
            outer_node.nd_tyre_node = true;
            AdjustNodeBuoyancy(outer_node, def.node_defaults);

            wheel.wh_tire_nodes.push_back(outer_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(outer_node.pos));
        }

        // Inner ring
        ray_point = m_actor->ar_nodes[axis_node_2].RelPosition + tyre_ray_vector;
        tyre_ray_vector = tyre_ray_rotator * tyre_ray_vector;

        {
            node_t& inner_node = AddNode();
            InitNode(inner_node, ray_point);
            inner_node.mass = node_mass;
            inner_node.friction_coef = def.node_defaults->friction;
            inner_node.volume_coef = def.node_defaults->volume;
            inner_node.surface_coef = def.node_defaults->surface;
            inner_node.nd_contacter = true;
            inner_node.nd_tyre_node = true;
            AdjustNodeBuoyancy(inner_node, def.node_defaults);

            wheel.wh_tire_nodes.push_back(inner_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(inner_node.pos));
        }
    }

    // Beams
    float rim_spring = def.rim_springiness;
    float rim_damp = def.rim_damping;
    float tyre_spring = def.tyre_springiness;
    float tyre_damp = def.tyre_damping;
    float tread_spring = def.beam_defaults->springiness;
    float tread_damp = def.beam_defaults->damping_constant;

    for (unsigned int i = 0; i < def.num_rays; i++)
    {
        // --- Rim --- 

        // Rim axis to rim ring
        NodeNum_t rim_outer_node = base_node_index + (i * 2);
        NodeNum_t rim_inner_node = rim_outer_node + 1;

        AddWheelBeam(axis_node_1, rim_outer_node, rim_spring, rim_damp, def.beam_defaults);
        AddWheelBeam(axis_node_2, rim_inner_node, rim_spring, rim_damp, def.beam_defaults);
        AddWheelBeam(axis_node_2, rim_outer_node, rim_spring, rim_damp, def.beam_defaults);
        AddWheelBeam(axis_node_1, rim_inner_node, rim_spring, rim_damp, def.beam_defaults);

        // Reinforcement rim ring
        NodeNum_t rim_next_outer_node = base_node_index + (((i + 1) % def.num_rays) * 2);
        NodeNum_t rim_next_inner_node = rim_next_outer_node + 1;

        AddWheelBeam(rim_outer_node, rim_inner_node,      rim_spring, rim_damp, def.beam_defaults);
        AddWheelBeam(rim_outer_node, rim_next_outer_node, rim_spring, rim_damp, def.beam_defaults);
        AddWheelBeam(rim_inner_node, rim_next_inner_node, rim_spring, rim_damp, def.beam_defaults);
        AddWheelBeam(rim_inner_node, rim_next_outer_node, rim_spring, rim_damp, def.beam_defaults);
    }

    // Tyre beams
    // Quick&dirty port from original SerializedRig::addWheel3()
    for (unsigned int i = 0; i < def.num_rays; i++)
    {
        NodeNum_t rim_node    = base_node_index + i*2;
        NodeNum_t tyre_node   = base_node_index + i*2 + def.num_rays*2;

        AddWheelBeam(rim_node, tyre_node, tyre_spring/2.f, tyre_damp, def.beam_defaults);

        NodeNum_t tyre_base_index = (i == 0) ? tyre_node + (def.num_rays * 2) : tyre_node;
        AddWheelBeam(rim_node, tyre_base_index - 1, tyre_spring/2.f, tyre_damp, def.beam_defaults);
        AddWheelBeam(rim_node, tyre_base_index - 2, tyre_spring/2.f, tyre_damp, def.beam_defaults);

        NodeNum_t next_rim_node = rim_node + 1;
        AddWheelBeam(next_rim_node, tyre_node,     tyre_spring/2.f, tyre_damp, def.beam_defaults);
        AddWheelBeam(next_rim_node, tyre_node + 1, tyre_spring/2.f, tyre_damp, def.beam_defaults);

        {
            NodeNum_t index = (i == 0) ? tyre_node + (def.num_rays * 2) - 1 : tyre_node - 1;
            AddWheelBeam(next_rim_node, tyre_node, tyre_spring/2.f, tyre_damp, def.beam_defaults);
        }

        //reinforcement (tire tread)
        {
            
            int rays = def.num_rays;

            AddWheelBeam(rim_node+rays*2,              base_node_index+i*2+1+rays*2, tread_spring, tread_damp, def.beam_defaults);
            AddWheelBeam(rim_node+rays*2,              base_node_index+((i+1)%rays)*2+rays*2, tread_spring, tread_damp, def.beam_defaults);
            AddWheelBeam(base_node_index+i*2+1+rays*2, base_node_index+((i+1)%rays)*2+1+rays*2, tread_spring, tread_damp, def.beam_defaults);
            AddWheelBeam(rim_node+1+rays*2,            base_node_index+((i+1)%rays)*2+rays*2, tread_spring, tread_damp, def.beam_defaults);

            if (rigidity_node != NODENUM_INVALID)
            {
                if (axis_node_closest_to_rigidity_node == axis_node_1)
                {
                    axis_node_closest_to_rigidity_node = base_node_index+i*2+rays*2;
                } else
                {
                    axis_node_closest_to_rigidity_node = base_node_index+i*2+1+rays*2;
                };
                BeamID_t beam_index = AddWheelBeam(rigidity_node, axis_node_closest_to_rigidity_node, tyre_spring, tyre_damp, def.beam_defaults);
                GetBeam(beam_index).bm_type = BEAM_VIRTUAL;
            }
        }
    }

    //    Calculate the point where the support beams get stiff and prevent the tire tread nodes
    //    bounce into the rim rimradius / tire radius and add 5%, this is a shortbound calc in % !

    float support_beams_short_bound = 1.0f - ((def.rim_radius / def.tyre_radius) * 0.95f);

    for (uint16_t i=0; i<def.num_rays; i++)
    {
        // tiretread anti collapse reinforcements, using precalced support beams
        NodeNum_t tirenode = base_node_index + i*2 + def.num_rays*2;
        BeamID_t beam_index;

        beam_index = AddWheelBeam(axis_node_1, tirenode,     tyre_spring/2.f, tyre_damp, def.beam_defaults);
        GetBeam(beam_index).shortbound = support_beams_short_bound;
        GetBeam(beam_index).longbound  = 0.f;
        GetBeam(beam_index).bounded = SHOCK1;

        beam_index = AddWheelBeam(axis_node_2, tirenode + 1, tyre_spring/2.f, tyre_damp, def.beam_defaults);
        GetBeam(beam_index).shortbound = support_beams_short_bound;
        GetBeam(beam_index).longbound  = 0.f;
        GetBeam(beam_index).bounded = SHOCK1;
    }

    // Wheel object
    wheel.wh_braking = this->TranslateBrakingDef(def.braking);
    wheel.wh_propulsed = (int)def.propulsion;
    wheel.wh_axis_node0num = m_actor->ar_nodes[axis_node_1].pos;
    wheel.wh_axis_node1num = m_actor->ar_nodes[axis_node_2].pos;
    wheel.wh_radius = def.tyre_radius;
    wheel.wh_rim_radius = def.rim_radius;
    wheel.wh_arm_nodenum = this->ResolveNodeRef(def.reference_arm_node);

    if (def.propulsion != RigDef::WheelPropulsion::NONE)
    {
        // for inter-differential locking
        m_actor->m_proped_wheel_pairs[m_actor->m_num_proped_wheels] = m_actor->ar_num_wheels;
        m_actor->m_num_proped_wheels++;
    }

    // Find near attach
    Ogre::Real length_1 = (m_actor->ar_nodes[axis_node_1].RelPosition - m_actor->ar_nodes[wheel.wh_arm_nodenum].RelPosition).length();
    Ogre::Real length_2 = (m_actor->ar_nodes[axis_node_2].RelPosition - m_actor->ar_nodes[wheel.wh_arm_nodenum].RelPosition).length();
    wheel.wh_near_attach_nodenum = (length_1 < length_2) ? m_actor->ar_nodes[axis_node_1].pos : m_actor->ar_nodes[axis_node_2].pos;

    // Commit the wheel
    int wheel_index = m_actor->ar_num_wheels;
    ++m_actor->ar_num_wheels;

    this->CreateFlexBodyWheelVisuals(wheel_index, base_node_index, m_actor->ar_nodes[axis_node_1].pos, m_actor->ar_nodes[axis_node_2].pos, def); 
}

wheel_t::BrakeCombo ActorSpawner::TranslateBrakingDef(RigDef::WheelBraking def)
{
    switch (def)
    {
    case RigDef::WheelBraking::NONE:                 return wheel_t::BrakeCombo::NONE;
    case RigDef::WheelBraking::FOOT_HAND:            return wheel_t::BrakeCombo::FOOT_HAND;
    case RigDef::WheelBraking::FOOT_HAND_SKID_LEFT:  return wheel_t::BrakeCombo::FOOT_HAND_SKID_LEFT;
    case RigDef::WheelBraking::FOOT_HAND_SKID_RIGHT: return wheel_t::BrakeCombo::FOOT_HAND_SKID_RIGHT;
    case RigDef::WheelBraking::FOOT_ONLY:            return wheel_t::BrakeCombo::FOOT_ONLY;
    default:                                         return wheel_t::BrakeCombo::NONE;
    }
}

void ActorSpawner::ProcessMeshWheel(RigDef::MeshWheel & meshwheel_def)
{
    unsigned int base_node_index = static_cast<int>(m_actor->ar_nodes.size());
    NodeNum_t axis_node_1 = this->ResolveNodeRef(meshwheel_def.nodes[0]);
    NodeNum_t axis_node_2 = this->ResolveNodeRef(meshwheel_def.nodes[1]);

    Ogre::Vector3 pos_1 = m_actor->ar_nodes[axis_node_1].AbsPosition;
    Ogre::Vector3 pos_2 = m_actor->ar_nodes[axis_node_2].AbsPosition;

    /* Enforce the "second node must have a larger Z coordinate than the first" constraint */
    if (pos_1.z > pos_2.z)
    {
        NodeNum_t swap = axis_node_1;
        axis_node_1 = axis_node_2;
        axis_node_2 = swap;
    }

    unsigned int wheel_index = BuildWheelObjectAndNodes(
        meshwheel_def.num_rays,
        axis_node_1,
        axis_node_2,
        this->ResolveNodeRef(meshwheel_def.reference_arm_node),
        meshwheel_def.num_rays * 2,
        meshwheel_def.num_rays * 8,
        meshwheel_def.tyre_radius,
        meshwheel_def.propulsion,
        meshwheel_def.braking,
        meshwheel_def.node_defaults,
        meshwheel_def.mass
    );

    BuildWheelBeams(
        meshwheel_def.num_rays,
        base_node_index,
        axis_node_1,
        axis_node_2,
        meshwheel_def.spring,      /* Tyre */
        meshwheel_def.damping,     /* Tyre */
        meshwheel_def.spring,      /* Rim */
        meshwheel_def.damping,     /* Rim */
        meshwheel_def.beam_defaults,
        meshwheel_def.rigidity_node
    );

    this->BuildMeshWheelVisuals(
        wheel_index,
        base_node_index,
        m_actor->ar_nodes[axis_node_1].pos,
        m_actor->ar_nodes[axis_node_2].pos,
        meshwheel_def.num_rays,
        meshwheel_def.mesh_name,
        meshwheel_def.material_name,
        meshwheel_def.rim_radius,
        /*rim_reverse:*/meshwheel_def.side != RigDef::WheelSide::RIGHT
    );

    CreateWheelSkidmarks(wheel_index);
}

void ActorSpawner::ProcessMeshWheel2(RigDef::MeshWheel2 & def)
{
    unsigned int base_node_index = static_cast<int>(m_actor->ar_nodes.size());
    NodeNum_t axis_node_1 = this->ResolveNodeRef(def.nodes[0]);
    NodeNum_t axis_node_2 = this->ResolveNodeRef(def.nodes[1]);

    if (axis_node_1 == NODENUM_INVALID || axis_node_2 == NODENUM_INVALID)
    {
        this->AddMessage(Message::TYPE_ERROR, "Failed to find axis nodes, skipping meshwheel2...");
        return;
    }

    Ogre::Vector3 pos_1 = m_actor->ar_nodes[axis_node_1].AbsPosition;
    Ogre::Vector3 pos_2 = m_actor->ar_nodes[axis_node_2].AbsPosition;

    /* Enforce the "second node must have a larger Z coordinate than the first" constraint */
    if (pos_1.z > pos_2.z)
    {
        NodeNum_t swap = axis_node_1;
        axis_node_1 = axis_node_2;
        axis_node_2 = swap;
    }	

    unsigned int wheel_index = BuildWheelObjectAndNodes(
        def.num_rays,
        axis_node_1,
        axis_node_2,
        this->ResolveNodeRef(def.reference_arm_node),
        def.num_rays * 2,
        def.num_rays * 8,
        def.tyre_radius,
        def.propulsion,
        def.braking,
        def.node_defaults,
        def.mass
    );

    /* --- Beams --- */
    /* Use data from directive 'set_beam_defaults' for the tiretread beams */
    float tyre_spring = def.spring;
    float tyre_damp = def.damping;
    float rim_spring = def.beam_defaults->springiness;
    float rim_damp = def.beam_defaults->damping_constant;

    BuildWheelBeams(
        def.num_rays,
        base_node_index,
        axis_node_1,
        axis_node_2,
        tyre_spring,
        tyre_damp,
        rim_spring,
        rim_damp,
        def.beam_defaults,
        def.rigidity_node,
        0.15 // max_extension
    );

    this->BuildMeshWheelVisuals(
        wheel_index,
        base_node_index,
        m_actor->ar_nodes[axis_node_1].pos,
        m_actor->ar_nodes[axis_node_2].pos,
        def.num_rays,
        def.mesh_name,
        def.material_name,
        def.rim_radius,
        /*rim_reverse:*/def.side != RigDef::WheelSide::RIGHT
    );

    CreateWheelSkidmarks(wheel_index);
}

void ActorSpawner::BuildMeshWheelVisuals(
    unsigned int wheel_index,
    unsigned int base_node_index,
    unsigned int axis_node_1_index,
    unsigned int axis_node_2_index,
    unsigned int num_rays,
    Ogre::String mesh_name,
    Ogre::String material_name,
    float rim_radius,
    bool rim_reverse
)
{
    m_actor->GetGfxActor()->UpdateSimDataBuffer(); // fill all current nodes - needed to setup flexing meshes

    try
    {
        FlexMeshWheel* flexmesh_wheel = m_flex_factory.CreateFlexMeshWheel(
            wheel_index, 
            axis_node_1_index,
            axis_node_2_index,
            base_node_index,
            num_rays,
            rim_radius,
            rim_reverse,
            mesh_name,
            material_name);
        Ogre::SceneNode* scene_node = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
        scene_node->attachObject(flexmesh_wheel->GetTireEntity());

        WheelGfx visual_wheel;
        visual_wheel.wx_is_meshwheel = false;
        visual_wheel.wx_flex_mesh = flexmesh_wheel;
        visual_wheel.wx_scenenode = scene_node;
        m_actor->m_gfx_actor->m_wheels.push_back(visual_wheel);
    }
    catch (Ogre::Exception& e)
    {
        this->AddMessage(Message::TYPE_ERROR, "Failed to create meshwheel visuals, message: " + e.getFullDescription());
        return;
    }
}

unsigned int ActorSpawner::BuildWheelObjectAndNodes( 
    unsigned int num_rays,
    NodeNum_t axis_node_1,
    NodeNum_t axis_node_2,
    NodeNum_t reference_arm_node,
    unsigned int reserve_nodes,
    unsigned int reserve_beams,
    float wheel_radius,
    RigDef::WheelPropulsion propulsion,
    RigDef::WheelBraking braking,
    std::shared_ptr<RigDef::NodeDefaults> node_defaults,
    float wheel_mass,
    float wheel_width       /* Default: -1.f */
)
{
    wheel_t & wheel = m_actor->ar_wheels[m_actor->ar_num_wheels];

    /* Axis */
    Ogre::Vector3 axis_vector = m_actor->ar_nodes[axis_node_2].RelPosition - m_actor->ar_nodes[axis_node_1].RelPosition;
    float axis_length = axis_vector.length();
    axis_vector.normalise();

    /* Wheel object */
    wheel.wh_braking      = this->TranslateBrakingDef(braking);
    wheel.wh_propulsed    = (int)propulsion;
    wheel.wh_axis_node0num  = axis_node_1;
    wheel.wh_axis_node1num  = axis_node_2;
    wheel.wh_radius       = wheel_radius;
    wheel.wh_width        = (wheel_width < 0) ? axis_length : wheel_width;
    wheel.wh_arm_nodenum     = reference_arm_node;

    /* Find near attach */
    Ogre::Real length_1 = (m_actor->ar_nodes[axis_node_1].RelPosition - m_actor->ar_nodes[wheel.wh_arm_nodenum].RelPosition).length();
    Ogre::Real length_2 = (m_actor->ar_nodes[axis_node_2].RelPosition - m_actor->ar_nodes[wheel.wh_arm_nodenum].RelPosition).length();
    wheel.wh_near_attach_nodenum = (length_1 < length_2) ? axis_node_1 : axis_node_2;

    if (propulsion != RigDef::WheelPropulsion::NONE)
    {
        /* for inter-differential locking */
        m_actor->m_proped_wheel_pairs[m_actor->m_num_proped_wheels] = m_actor->ar_num_wheels;
        m_actor->m_num_proped_wheels++;
    }
    
    /* Nodes */
    Ogre::Vector3 ray_vector = axis_vector.perpendicular() * wheel_radius;
    Ogre::Quaternion ray_rotator = Ogre::Quaternion(Ogre::Degree(-360.0 / (num_rays * 2)), axis_vector);

#ifdef DEBUG_TRUCKPARSER2013
    // TRUCK PARSER 2013 DEBUG
    std::stringstream msg;
    msg << "\nDBG ActorSpawner::BuildWheelObjectAndNodes()\nDBG nodebase:" << static_cast<int>(m_actor->ar_nodes.size()) <<", axis-node-0:"<<m_actor->ar_nodes[axis_node_1].pos <<", axis-node-1:"<<m_actor->ar_nodes[axis_node_2].pos<<"\n";
    msg << "DBG ==== Adding nodes ====";
    // END
#endif

    for (unsigned int i = 0; i < num_rays; i++)
    {
        /* Outer ring */
        Ogre::Vector3 ray_point = m_actor->ar_nodes[axis_node_1].RelPosition + ray_vector;
        ray_vector = ray_rotator * ray_vector;

        {
            node_t& outer_node = AddNode();
            InitNode(outer_node, ray_point, node_defaults);
            outer_node.mass = wheel_mass / (2.f * num_rays);
            outer_node.nd_contacter = true;
            outer_node.nd_tyre_node = true;
            AdjustNodeBuoyancy(outer_node, node_defaults);

            wheel.wh_tire_nodes.push_back(outer_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(outer_node.pos));
        }

        /* Inner ring */
        ray_point = m_actor->ar_nodes[axis_node_2].RelPosition + ray_vector;
        ray_vector = ray_rotator * ray_vector;

        {
            node_t& inner_node = AddNode();
            InitNode(inner_node, ray_point, node_defaults);
            inner_node.mass = wheel_mass / (2.f * num_rays);
            inner_node.nd_contacter = true;
            inner_node.nd_tyre_node = true;
            AdjustNodeBuoyancy(inner_node, node_defaults);

            wheel.wh_tire_nodes.push_back(inner_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(inner_node.pos));
        }        
        

#ifdef DEBUG_TRUCKPARSER2013
        // TRUCK PARSER 2013 DEBUG
        int modifier = 0;
        msg << "\nDBG\tN1: index=" << outer_node.pos + modifier << ", iswheel=" << WHEEL_DEFAULT 
            <<", X=" << outer_node.AbsPosition.x <<", Y=" << outer_node.AbsPosition.y <<", Z=" << outer_node.AbsPosition.z << std::endl
            << "DBG\tN2: index=" << inner_node.pos + modifier << ", iswheel=" << WHEEL_DEFAULT 
            <<", X=" << inner_node.AbsPosition.x <<", Y=" << inner_node.AbsPosition.y <<", Z=" << inner_node.AbsPosition.z;
        // END
#endif
    }

#ifdef DEBUG_TRUCKPARSER2013
    // TRUCK PARSER 2013 DEBUG
    LOG(msg.str());
    // END
#endif

    /* Advance */
    unsigned int wheel_index = m_actor->ar_num_wheels;
    m_actor->ar_num_wheels++;
    return wheel_index;
}

void ActorSpawner::AdjustNodeBuoyancy(node_t & node, RigDef::Node & node_def, std::shared_ptr<RigDef::NodeDefaults> defaults)
{
    unsigned int options = (defaults->options | node_def.options); // Merge flags
    node.buoyancy = BITMASK_IS_1(options, RigDef::Node::OPTION_b_EXTRA_BUOYANCY) ? 10000.f : m_actor->m_dry_mass/15.f;
}

void ActorSpawner::AdjustNodeBuoyancy(node_t & node, std::shared_ptr<RigDef::NodeDefaults> defaults)
{
    node.buoyancy = BITMASK_IS_1(defaults->options, RigDef::Node::OPTION_b_EXTRA_BUOYANCY) ? 10000.f : m_actor->m_dry_mass/15.f;
}

void ActorSpawner::BuildWheelBeams(
    unsigned int num_rays,
    NodeNum_t base_node_index,
    NodeNum_t axis_node_1,
    NodeNum_t axis_node_2,
    float tyre_spring,
    float tyre_damping,
    float rim_spring,
    float rim_damping,
    std::shared_ptr<RigDef::BeamDefaults> beam_defaults,
    RigDef::Node::Ref const & rigidity_node_id,
    float max_extension // = 0.f
)
{
    /* Find out where to connect rigidity node */
    bool rigidity_beam_side_1 = false;
    NodeNum_t rigidity_node = NODENUM_INVALID;
    if (rigidity_node_id.IsValidAnyState())
    {
        rigidity_node = ResolveNodeRef(rigidity_node_id);
        float distance_1 = m_actor->ar_nodes[rigidity_node].RelPosition.distance(m_actor->ar_nodes[axis_node_1].RelPosition);
        float distance_2 = m_actor->ar_nodes[rigidity_node].RelPosition.distance(m_actor->ar_nodes[axis_node_2].RelPosition);
        rigidity_beam_side_1 = distance_1 < distance_2;
    }

    for (unsigned int i = 0; i < num_rays; i++)
    {
        /* Bounded */
        NodeNum_t outer_ring_node = base_node_index + (i * 2);
        NodeNum_t inner_ring_node = outer_ring_node + 1;
        
        AddWheelBeam(axis_node_1, outer_ring_node, tyre_spring, tyre_damping, beam_defaults, 0.66f, max_extension);
        AddWheelBeam(axis_node_2, inner_ring_node, tyre_spring, tyre_damping, beam_defaults, 0.66f, max_extension);
        AddWheelBeam(axis_node_2, outer_ring_node, tyre_spring, tyre_damping, beam_defaults);
        AddWheelBeam(axis_node_1, inner_ring_node, tyre_spring, tyre_damping, beam_defaults);

        /* Reinforcement */
        NodeNum_t next_outer_ring_node = base_node_index + (((i + 1) % num_rays) * 2);
        NodeNum_t next_inner_ring_node = next_outer_ring_node + 1;

        AddWheelBeam(outer_ring_node, inner_ring_node,      rim_spring, rim_damping, beam_defaults);
        AddWheelBeam(outer_ring_node, next_outer_ring_node, rim_spring, rim_damping, beam_defaults);
        AddWheelBeam(inner_ring_node, next_inner_ring_node, rim_spring, rim_damping, beam_defaults);
        AddWheelBeam(inner_ring_node, next_outer_ring_node, rim_spring, rim_damping, beam_defaults);

        /* Rigidity beams */
        if (rigidity_node != NODENUM_INVALID)
        {
            NodeNum_t target_node = (rigidity_beam_side_1) ? outer_ring_node : inner_ring_node;
            unsigned int beam_index = AddWheelBeam(rigidity_node, target_node, tyre_spring, tyre_damping, beam_defaults, -1.f, -1.f, BEAM_VIRTUAL);
            m_actor->ar_beams[beam_index].bm_type = BEAM_VIRTUAL;
        }
    }
}

unsigned int ActorSpawner::AddWheel(RigDef::Wheel & wheel_def)
{
    NodeNum_t base_node_index = static_cast<NodeNum_t>(m_actor->ar_nodes.size());
    NodeNum_t axis_node_1 = this->ResolveNodeRef(wheel_def.nodes[0]);
    NodeNum_t axis_node_2 = this->ResolveNodeRef(wheel_def.nodes[1]);

    if (axis_node_1 == NODENUM_INVALID || axis_node_2 == NODENUM_INVALID)
    {
        std::stringstream msg;
        msg << "Error creating 'wheel': Some axis nodes were not found";
        msg << " (Node1: " << wheel_def.nodes[0].ToString() << " => " << (axis_node_1 == NODENUM_INVALID) ? "NOT FOUND)" : "found)";
        msg << " (Node2: " << wheel_def.nodes[1].ToString() << " => " << (axis_node_2 == NODENUM_INVALID) ? "NOT FOUND)" : "found)";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return -1;
    }

    Ogre::Vector3 pos_1 = m_actor->ar_nodes[axis_node_1].AbsPosition;
    Ogre::Vector3 pos_2 = m_actor->ar_nodes[axis_node_2].AbsPosition;

    /* Enforce the "second node must have a larger Z coordinate than the first" constraint */
    if (pos_1.z > pos_2.z)
    {
        NodeNum_t swap = axis_node_1;
        axis_node_1 = axis_node_2;
        axis_node_2 = swap;
    }	

    unsigned int wheel_index = BuildWheelObjectAndNodes(
        wheel_def.num_rays,
        axis_node_1,
        axis_node_2,
        this->ResolveNodeRef(wheel_def.reference_arm_node),
        wheel_def.num_rays * 2,
        wheel_def.num_rays * 8,
        wheel_def.radius,
        wheel_def.propulsion,
        wheel_def.braking,
        wheel_def.node_defaults,
        wheel_def.mass,
        -1.f // Set width to axis length (width in definition is ignored)
    );

    BuildWheelBeams(
        wheel_def.num_rays,
        base_node_index,
        axis_node_1,
        axis_node_2,
        wheel_def.springiness, /* Tyre */
        wheel_def.damping,     /* Tyre */
        wheel_def.springiness, /* Rim */
        wheel_def.damping,     /* Rim */
        wheel_def.beam_defaults,
        wheel_def.rigidity_node
    );

    this->CreateWheelVisuals(
        wheel_index,
        base_node_index,
        wheel_def.num_rays,
        wheel_def.face_material_name,
        wheel_def.band_material_name,
        /*separate_rim:*/false
        );

    CreateWheelSkidmarks(wheel_index);

    return wheel_index;
}

void ActorSpawner::CreateWheelSkidmarks(unsigned int wheel_index)
{
    // Always create, even if disabled by config
    m_actor->m_skid_trails[wheel_index] = new RoR::Skidmark(m_actor,
        RoR::App::GetGfxScene()->GetSkidmarkConf(), &m_actor->ar_wheels[wheel_index], m_particles_parent_scenenode, 300, 20);
}

unsigned int ActorSpawner::AddWheel2(RigDef::Wheel2 & wheel_2_def)
{
    NodeNum_t base_node_index = static_cast<NodeNum_t>(m_actor->ar_nodes.size());
    wheel_t & wheel = m_actor->ar_wheels[m_actor->ar_num_wheels];
    NodeNum_t axis_node_1 = this->ResolveNodeRef(wheel_2_def.nodes[0]);
    NodeNum_t axis_node_2 = this->ResolveNodeRef(wheel_2_def.nodes[1]);

    if (axis_node_1 == NODENUM_INVALID || axis_node_2 == NODENUM_INVALID)
    {
        std::stringstream msg;
        msg << "Error creating 'wheel2': Some axis nodes were not found";
        msg << " (Node1: " << wheel_2_def.nodes[0].ToString() << " => " << (axis_node_1 == NODENUM_INVALID) ? "NOT FOUND)" : "found)";
        msg << " (Node2: " << wheel_2_def.nodes[1].ToString() << " => " << (axis_node_2 == NODENUM_INVALID) ? "NOT FOUND)" : "found)";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return -1;
    }

    Ogre::Vector3 pos_1 = m_actor->ar_nodes[axis_node_1].AbsPosition;
    Ogre::Vector3 pos_2 = m_actor->ar_nodes[axis_node_2].AbsPosition;

    /* Enforce the "second node must have a larger Z coordinate than the first" constraint */
    if (pos_1.z > pos_2.z)
    {
        NodeNum_t swap = axis_node_1;
        axis_node_1 = axis_node_2;
        axis_node_2 = swap;
    }

    /* Find out where to connect rigidity node */
    bool rigidity_beam_side_1 = false;
    if (wheel_2_def.rigidity_node.IsValidAnyState())
    {
        node_t & rigidity_node = m_actor->ar_nodes[this->GetNodeIndexOrThrow(wheel_2_def.rigidity_node)];
        Ogre::Real distance_1 = (rigidity_node.RelPosition - m_actor->ar_nodes[axis_node_1].RelPosition).length();
        Ogre::Real distance_2 = (rigidity_node.RelPosition - m_actor->ar_nodes[axis_node_2].RelPosition).length();
        rigidity_beam_side_1 = distance_1 < distance_2;
    }


    /* Node&beam generation */
    Ogre::Vector3 axis_vector = m_actor->ar_nodes[axis_node_2].RelPosition - m_actor->ar_nodes[axis_node_1].RelPosition;
    axis_vector.normalise();
    Ogre::Vector3 rim_ray_vector = Ogre::Vector3(0, wheel_2_def.rim_radius, 0);
    Ogre::Quaternion rim_ray_rotator = Ogre::Quaternion(Ogre::Degree(-360.f / wheel_2_def.num_rays), axis_vector);

    /* Width */
    wheel.wh_width = axis_vector.length(); /* wheel_def.width is ignored. */

    /* Rim nodes */
    for (unsigned int i = 0; i < wheel_2_def.num_rays; i++)
    {
        float node_mass = wheel_2_def.mass / (4.f * wheel_2_def.num_rays);

        /* Outer ring */
        Ogre::Vector3 ray_point = m_actor->ar_nodes[axis_node_1].RelPosition + rim_ray_vector;

        {
            node_t& outer_node = AddNode();
            InitNode(outer_node, ray_point, wheel_2_def.node_defaults);
            outer_node.mass = node_mass;
            outer_node.nd_rim_node = true;

            m_actor->ar_nodes_aux[outer_node.pos].nda_minimass = m_state.global_minimass;
            wheel.wh_rim_nodes.push_back(outer_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(outer_node.pos));
        }

        /* Inner ring */
        ray_point = m_actor->ar_nodes[axis_node_2].RelPosition + rim_ray_vector;

        {
            node_t& inner_node = AddNode();
            InitNode(inner_node, ray_point, wheel_2_def.node_defaults);
            inner_node.mass = node_mass;
            inner_node.nd_rim_node = true;

            m_actor->ar_nodes_aux[inner_node.pos].nda_minimass = m_state.global_minimass;
            wheel.wh_rim_nodes.push_back(inner_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(inner_node.pos));
        }        

        rim_ray_vector = rim_ray_rotator * rim_ray_vector;
    }

    Ogre::Vector3 tyre_ray_vector = Ogre::Vector3(0, wheel_2_def.tyre_radius, 0);
    Ogre::Quaternion tyre_ray_rotator = Ogre::Quaternion(Ogre::Degree(-180.f / wheel_2_def.num_rays), axis_vector);
    tyre_ray_vector = tyre_ray_rotator * tyre_ray_vector;

    /* Tyre nodes */
    for (unsigned int i = 0; i < wheel_2_def.num_rays; i++)
    {
        /* Outer ring */
        Ogre::Vector3 ray_point = m_actor->ar_nodes[axis_node_1].RelPosition + tyre_ray_vector;

        {
            node_t& outer_node = AddNode();
            InitNode(outer_node, ray_point);
            outer_node.mass = (0.67f * wheel_2_def.mass) / (2.f * wheel_2_def.num_rays);
            outer_node.friction_coef = wheel.wh_width * WHEEL_FRICTION_COEF;
            outer_node.volume_coef = wheel_2_def.node_defaults->volume;
            outer_node.surface_coef = wheel_2_def.node_defaults->surface;
            outer_node.nd_contacter = true;
            outer_node.nd_tyre_node = true;

            wheel.wh_tire_nodes.push_back(outer_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(outer_node.pos));
        }

        /* Inner ring */
        ray_point = m_actor->ar_nodes[axis_node_2].RelPosition + tyre_ray_vector;

        {
            node_t& inner_node = AddNode();
            InitNode(inner_node, ray_point);
            inner_node.mass = (0.33f * wheel_2_def.mass) / (2.f * wheel_2_def.num_rays);
            inner_node.friction_coef = wheel.wh_width * WHEEL_FRICTION_COEF;
            inner_node.volume_coef = wheel_2_def.node_defaults->volume;
            inner_node.surface_coef = wheel_2_def.node_defaults->surface;
            inner_node.nd_contacter = true;
            inner_node.nd_tyre_node = true;

            wheel.wh_tire_nodes.push_back(inner_node.pos);
            m_actor->m_gfx_actor->m_gfx_nodes.push_back(NodeGfx(inner_node.pos));
        }

        tyre_ray_vector = rim_ray_rotator * tyre_ray_vector;
    }

    /* Beams */
    for (unsigned int i = 0; i < wheel_2_def.num_rays; i++)
    {
        /* --- Rim ---  */

        /* Bounded */
        unsigned int rim_outer_node_index = base_node_index + (i * 2);
        node_t *rim_outer_node = & m_actor->ar_nodes[rim_outer_node_index];
        node_t *rim_inner_node = & m_actor->ar_nodes[rim_outer_node_index + 1];

        unsigned int beam_index;
        beam_index = AddWheelRimBeam(wheel_2_def, &m_actor->ar_nodes[axis_node_1], rim_outer_node);
        GetBeam(beam_index).shortbound = 0.66;
        beam_index = AddWheelRimBeam(wheel_2_def, &m_actor->ar_nodes[axis_node_2], rim_inner_node);
        GetBeam(beam_index).shortbound = 0.66;
        AddWheelRimBeam(wheel_2_def, &m_actor->ar_nodes[axis_node_2], rim_outer_node);
        AddWheelRimBeam(wheel_2_def, &m_actor->ar_nodes[axis_node_1], rim_inner_node);

        /* Reinforcement */
        unsigned int rim_next_outer_node_index = base_node_index + (((i + 1) % wheel_2_def.num_rays) * 2);
        node_t *rim_next_outer_node = & m_actor->ar_nodes[rim_next_outer_node_index];
        node_t *rim_next_inner_node = & m_actor->ar_nodes[rim_next_outer_node_index + 1];

        AddWheelRimBeam(wheel_2_def, &m_actor->ar_nodes[axis_node_1], rim_outer_node);
        AddWheelRimBeam(wheel_2_def, rim_outer_node, rim_inner_node);
        AddWheelRimBeam(wheel_2_def, rim_outer_node, rim_next_outer_node);
        AddWheelRimBeam(wheel_2_def, rim_inner_node, rim_next_inner_node);
        AddWheelRimBeam(wheel_2_def, rim_outer_node, rim_next_inner_node);
        AddWheelRimBeam(wheel_2_def, rim_inner_node, rim_next_outer_node);

        /* -- Rigidity -- */
        if (wheel_2_def.rigidity_node.IsValidAnyState())
        {
            unsigned int rig_beam_index = AddWheelRimBeam(wheel_2_def,
                            GetNodePointer(wheel_2_def.rigidity_node),
                            (rigidity_beam_side_1) ? rim_outer_node : rim_inner_node
            );
            m_actor->ar_beams[rig_beam_index].bm_type = BEAM_VIRTUAL;
        }

        /* --- Tyre --- */

        unsigned int tyre_node_index = rim_outer_node_index + (2 * wheel_2_def.num_rays);
        node_t *tyre_outer_node = & m_actor->ar_nodes[tyre_node_index];
        node_t *tyre_inner_node = & m_actor->ar_nodes[tyre_node_index + 1];
        unsigned int tyre_next_node_index = rim_next_outer_node_index + (2 * wheel_2_def.num_rays);
        node_t *tyre_next_outer_node = & m_actor->ar_nodes[tyre_next_node_index];
        node_t *tyre_next_inner_node = & m_actor->ar_nodes[tyre_next_node_index + 1];

        /* Tyre band */
        AddTyreBeam(wheel_2_def, tyre_outer_node, tyre_next_outer_node);
        AddTyreBeam(wheel_2_def, tyre_outer_node, tyre_next_inner_node);
        AddTyreBeam(wheel_2_def, tyre_inner_node, tyre_next_outer_node);
        AddTyreBeam(wheel_2_def, tyre_inner_node, tyre_next_inner_node);
        /* Tyre sidewalls */
        AddTyreBeam(wheel_2_def, tyre_outer_node, rim_outer_node);
        AddTyreBeam(wheel_2_def, tyre_outer_node, rim_next_outer_node);
        AddTyreBeam(wheel_2_def, tyre_inner_node, rim_inner_node);
        AddTyreBeam(wheel_2_def, tyre_inner_node, rim_next_inner_node);
        /* Reinforcement */
        AddTyreBeam(wheel_2_def, tyre_outer_node, rim_inner_node);
        AddTyreBeam(wheel_2_def, tyre_outer_node, rim_next_inner_node);
        AddTyreBeam(wheel_2_def, tyre_inner_node, rim_outer_node);
        AddTyreBeam(wheel_2_def, tyre_inner_node, rim_next_outer_node);
        /* Backpressure, bounded */
        AddTyreBeam(wheel_2_def, &m_actor->ar_nodes[axis_node_1], tyre_outer_node);
        AddTyreBeam(wheel_2_def, &m_actor->ar_nodes[axis_node_2], tyre_inner_node);
    }

    /* Wheel object */
    wheel.wh_braking       = this->TranslateBrakingDef(wheel_2_def.braking);
    wheel.wh_propulsed     = (int)wheel_2_def.propulsion;
    wheel.wh_axis_node0num = m_actor->ar_nodes[axis_node_1].pos;
    wheel.wh_axis_node1num = m_actor->ar_nodes[axis_node_2].pos;
    wheel.wh_radius        = wheel_2_def.tyre_radius;
    wheel.wh_rim_radius    = wheel_2_def.rim_radius;
    wheel.wh_arm_nodenum   = this->ResolveNodeRef(wheel_2_def.reference_arm_node);

    if (wheel_2_def.propulsion != RigDef::WheelPropulsion::NONE)
    {
        /* for inter-differential locking */
        m_actor->m_proped_wheel_pairs[m_actor->m_num_proped_wheels] = m_actor->ar_num_wheels;
        m_actor->m_num_proped_wheels++;
    }

    /* Find near attach */
    Ogre::Real length_1 = (m_actor->ar_nodes[axis_node_1].RelPosition - m_actor->ar_nodes[wheel.wh_arm_nodenum].RelPosition).length();
    Ogre::Real length_2 = (m_actor->ar_nodes[axis_node_2].RelPosition - m_actor->ar_nodes[wheel.wh_arm_nodenum].RelPosition).length();
    wheel.wh_near_attach_nodenum = (length_1 < length_2) ? m_actor->ar_nodes[axis_node_1].pos : m_actor->ar_nodes[axis_node_2].pos;

    CreateWheelSkidmarks(static_cast<unsigned>(m_actor->ar_num_wheels));

    /* Advance */
    unsigned int wheel_index = m_actor->ar_num_wheels;
    m_actor->ar_num_wheels++;
    return wheel_index;
}

void ActorSpawner::CreateWheelVisuals(
    unsigned int wheel_index, 
    unsigned int node_base_index,
    unsigned int num_rays,
    Ogre::String const & rim_material_name,
    Ogre::String const & band_material_name,
    bool separate_rim,
    float rim_ratio
)
{
    m_actor->GetGfxActor()->UpdateSimDataBuffer(); // fill all current nodes - needed to setup flexing meshes

    wheel_t & wheel = m_actor->ar_wheels[wheel_index];

    try
    {
        WheelGfx visual_wheel;

        const std::string wheel_mesh_name = this->ComposeName("WheelMesh", wheel_index);
        visual_wheel.wx_is_meshwheel = false;
        visual_wheel.wx_flex_mesh = new FlexMesh(
            wheel_mesh_name,
            m_actor->m_gfx_actor.get(),
            wheel.wh_axis_node0num,
            wheel.wh_axis_node1num,
            static_cast<NodeNum_t>(node_base_index), // FIXME - node_base_index should be also NodeNum_t
            num_rays,
            rim_material_name,
            band_material_name,
            separate_rim,
            rim_ratio
        );

        const std::string instance_name = this->ComposeName("WheelEntity", wheel_index);
        Ogre::Entity *ec = App::GetGfxScene()->GetSceneManager()->createEntity(instance_name, wheel_mesh_name);
        this->SetupNewEntity(ec, Ogre::ColourValue(0, 0.5, 0.5));
        visual_wheel.wx_scenenode = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
        m_actor->m_deletion_entities.emplace_back(ec);
        visual_wheel.wx_scenenode->attachObject(ec);
        m_actor->m_gfx_actor->m_wheels.push_back(visual_wheel);
    }
    catch (Ogre::Exception& e)
    {
        AddMessage(Message::TYPE_ERROR, "Failed to create wheel visuals: " +  e.getFullDescription());
    }
}

void ActorSpawner::CreateFlexBodyWheelVisuals(
    unsigned int wheel_index, 
    unsigned int node_base_index,
    NodeNum_t axis_node_1,
    NodeNum_t axis_node_2,
    RigDef::FlexBodyWheel& def)
{
    m_actor->GetGfxActor()->UpdateSimDataBuffer(); // fill all current nodes - needed to setup flexing meshes

    this->BuildMeshWheelVisuals(
        wheel_index,
        node_base_index,
        axis_node_1,
        axis_node_2,
        def.num_rays,
        def.rim_mesh_name,
        "tracks/trans", // Rim material name. Original parser: was hardcoded in BTS_FLEXBODYWHEELS
        def.rim_radius,
        def.side != RigDef::WheelSide::RIGHT
        );

    int num_nodes = def.num_rays * 4;
    std::vector<unsigned int> node_indices;
    node_indices.reserve(num_nodes);
    for (int i = 0; i < num_nodes; ++i)
    {
        node_indices.push_back( node_base_index + i );
    }

    RigDef::Flexbody flexbody_def;
    flexbody_def.mesh_name = def.tyre_mesh_name;
    flexbody_def.offset = Ogre::Vector3(0.5,0,0);

    try
    {
        auto* flexbody = m_flex_factory.CreateFlexBody(
            &flexbody_def,
            axis_node_1,
            axis_node_2,
            static_cast<int>(node_base_index),
            Ogre::Quaternion(Ogre::Degree(90), Ogre::Vector3::UNIT_Y),
            node_indices,
            m_custom_resource_group
            );

        if (flexbody == nullptr)
            return; // Error already logged

        this->CreateWheelSkidmarks(static_cast<unsigned>(wheel_index));

        m_actor->m_gfx_actor->m_flexbodies.push_back(flexbody);
    }
    catch (Ogre::Exception& e)
    {
        this->AddMessage(Message::TYPE_ERROR, 
            "Failed to create flexbodywheel visuals '" + def.tyre_mesh_name + "', reason:" + e.getFullDescription());
    }
}

BeamID_t ActorSpawner::AddWheelBeam(
    NodeNum_t node_1, 
    NodeNum_t node_2, 
    float spring, 
    float damping, 
    std::shared_ptr<RigDef::BeamDefaults> beam_defaults,
    float max_contraction,   /* Default: -1.f */
    float max_extension,     /* Default: -1.f */
    BeamType type            /* Default: BEAM_INVISIBLE */
)
{
    unsigned int index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = AddBeam(m_actor->ar_nodes[node_1], m_actor->ar_nodes[node_2], beam_defaults, DEFAULT_DETACHER_GROUP); 
    beam.bm_type = type;
    beam.k = spring;
    beam.d = damping;
    if (max_contraction > 0.f)
    {
        beam.shortbound = max_contraction;
        beam.longbound = max_extension;
        beam.bounded = SHOCK1;
    }
    CalculateBeamLength(beam);

    return index;
}

BeamID_t ActorSpawner::AddWheelRimBeam(RigDef::Wheel2 & wheel_2_def, node_t *node_1, node_t *node_2)
{
    BeamID_t beam_index = _SectionWheels2AddBeam(wheel_2_def, node_1, node_2);
    beam_t & beam = GetBeam(beam_index);
    beam.k = wheel_2_def.rim_springiness;
    beam.d = wheel_2_def.rim_damping;
    return beam_index;
}

BeamID_t ActorSpawner::AddTyreBeam(RigDef::Wheel2 & wheel_2_def, node_t *node_1, node_t *node_2)
{
    BeamID_t beam_index = _SectionWheels2AddBeam(wheel_2_def, node_1, node_2);
    beam_t & beam = GetBeam(beam_index);
    beam.k = wheel_2_def.tyre_springiness;
    beam.d = wheel_2_def.tyre_damping;

    m_actor->getTyrePressure().AddBeam((int)beam_index);

    return beam_index;
}

BeamID_t ActorSpawner::_SectionWheels2AddBeam(RigDef::Wheel2 & wheel_2_def, node_t *node_1, node_t *node_2)
{
    BeamID_t index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = GetFreeBeam();
    InitBeam(beam, node_1, node_2);
    beam.bm_type = BEAM_NORMAL;
    SetBeamStrength(beam, wheel_2_def.beam_defaults->breaking_threshold);
    SetBeamDeformationThreshold(beam, wheel_2_def.beam_defaults);
    return index;
}

void ActorSpawner::ProcessWheel2(RigDef::Wheel2 & def)
{
    unsigned int node_base_index = static_cast<int>(m_actor->ar_nodes.size());
    unsigned int wheel_index = AddWheel2(def);
    this->CreateWheelVisuals(
        wheel_index,
        node_base_index,
        def.num_rays,
        def.face_material_name,
        def.band_material_name,
        /*separate_rim:*/true,
        /*rim_ratio:*/def.rim_radius / def.tyre_radius
        );
};

void ActorSpawner::ProcessWheel(RigDef::Wheel & def)
{
    AddWheel(def);
};

void ActorSpawner::ProcessWheelDetacher(RigDef::WheelDetacher & def)
{
    if (def.wheel_id > m_actor->ar_num_wheels - 1)
    {
        AddMessage(Message::TYPE_ERROR, std::string("Invalid wheel_id: ") + TOSTRING(def.wheel_id));
        return;
    }

    wheeldetacher_t obj;
    obj.wd_wheel_id = def.wheel_id;
    obj.wd_detacher_group = def.detacher_group;
    m_actor->ar_wheeldetachers.push_back(obj);
};

void ActorSpawner::ProcessTractionControl(RigDef::TractionControl & def)
{
    /* #1: regulating_force */
    float force = def.regulation_force;
    if (force < 1.f || force > 20.f)
    {
        std::stringstream msg;
        msg << "Clamping 'regulating_force' value '" << force << "' to allowed range <1 - 20>";
        AddMessage(Message::TYPE_INFO, msg.str());
        force = (force < 1.f) ? 1.f : 20.f;
    }
    m_actor->tc_ratio = force;

    /* #2: wheelslip */
    // no longer needed

    /* #3: fade_speed */
    // no longer needed

    /* #4: pulse/sec */
    float pulse = def.pulse_per_sec;
    if (pulse <= 1.0f || pulse >= 2000.0f)
    {
        pulse = 2000.0f;
    } 
    m_actor->tc_pulse_time = 1 / pulse;

    /* #4: mode */
    m_actor->tc_mode = def.attr_is_on;
    m_actor->tc_nodash = def.attr_no_dashboard;
    m_actor->tc_notoggle = def.attr_no_toggle;
};

void ActorSpawner::ProcessAntiLockBrakes(RigDef::AntiLockBrakes & def)
{
    /* #1: regulating_force */
    float force = def.regulation_force;
    if (force < 1.f || force > 20.f)
    {
        std::stringstream msg;
        msg << "Clamping 'regulating_force' value '" << force << "' to allowed range <1 - 20>";
        AddMessage(Message::TYPE_INFO, msg.str());
        force = (force < 1.f) ? 1.f : 20.f;
    }
    m_actor->alb_ratio = force;

    /* #2: min_speed */
    /* Wheelspeed adaption: 60 sec * 60 mins / 1000(kilometer) = 3.6 to get meter per sec */
    float min_speed = def.min_speed / 3.6f;
    m_actor->alb_minspeed = std::max(0.5f, min_speed);

    /* #3: pulse_per_sec */
    float pulse = def.pulse_per_sec;
    if (pulse <= 1.0f || pulse >= 2000.0f)
    {
        pulse = 2000.0f;
    } 
    m_actor->alb_pulse_time = 1 / pulse;

    /* #4: mode */
    m_actor->alb_mode = def.attr_is_on;
    m_actor->alb_nodash = def.attr_no_dashboard;
    m_actor->alb_notoggle = def.attr_no_toggle;
}

void ActorSpawner::ProcessBrakes(RigDef::Brakes & def)
{
    m_actor->ar_brake_force = def.default_braking_force;
    m_actor->m_handbrake_force = 2.f * m_actor->ar_brake_force;
    if (def.parking_brake_force != -1.f)
    {
        m_actor->m_handbrake_force = def.parking_brake_force;
    }
};

void ActorSpawner::ProcessEngturbo(RigDef::Engturbo & def)
{
    /* Is this a land vehicle? */
    if (m_actor->ar_engine == nullptr)
    {
        AddMessage(Message::TYPE_WARNING, "Section 'engturbo' found but no engine defined. Skipping ...");
        return;
    }
   
    m_actor->ar_engine->SetTurboOptions(def.version, def.tinertiaFactor, def.nturbos, def.param1, def.param2, def.param3, def.param4, def.param5, def.param6, def.param7, def.param8, def.param9, def.param10, def.param11);
};

void ActorSpawner::ProcessEngoption(RigDef::Engoption & def)
{
    /* Is this a land vehicle? */
    if (m_actor->ar_engine == nullptr)
    {
        AddMessage(Message::TYPE_WARNING, "Section 'engoption' found but no engine defined. Skipping ...");
        return;
    }

    if (def.idle_rpm > 0 && def.stall_rpm > 0 && def.stall_rpm > def.idle_rpm)
    {
        AddMessage(Message::TYPE_WARNING, "Stall RPM is set higher than Idle RPM.");
    }

    /* Process it */
    m_actor->ar_engine->SetEngineOptions(
        def.inertia,
        (char)def.type,
        def.clutch_force,
        def.shift_time,
        def.clutch_time,
        def.post_shift_time,
        def.idle_rpm,
        def.stall_rpm,
        def.max_idle_mixture,
        def.min_idle_mixture,
        def.braking_torque
    );
};

void ActorSpawner::ProcessEngine(RigDef::Engine & def)
{
    /* Process it */
    m_actor->ar_driveable = TRUCK;

    /* Process gear list to EngineSim-compatible format */
    /* TODO: Move this to EngineSim::EngineSim() */
    std::vector<float> gears_compat;
    gears_compat.reserve(2 + def.gear_ratios.size());
    gears_compat.push_back(def.reverse_gear_ratio);
    gears_compat.push_back(def.neutral_gear_ratio);
    std::vector<float>::iterator itor = def.gear_ratios.begin();
    for (; itor < def.gear_ratios.end(); itor++)
    {
        gears_compat.push_back(*itor);
    }

    m_actor->ar_engine = new EngineSim(
        def.shift_down_rpm,
        def.shift_up_rpm,
        def.torque,
        gears_compat,
        def.global_gear_ratio,
        m_actor
    );

    m_actor->ar_engine->SetAutoMode(App::sim_gearbox_mode->getEnum<SimGearboxMode>());
};

void ActorSpawner::ProcessHelp(RigDef::Help & def)
{
    m_help_material_name = def.material;
};

void ActorSpawner::ProcessAuthor(RigDef::Author & def)
{
    authorinfo_t author;
    author.type = def.type;
    author.name = def.name;
    author.email = def.email;
    if (def._has_forum_account)
    {
        author.id = def.forum_account_id;
    }
    m_actor->authors.push_back(author);
};

NodeNum_t ActorSpawner::GetNodeIndexOrThrow(RigDef::Node::Ref const & node_ref)
{
    NodeNum_t node = this->ResolveNodeRef(node_ref);
    if (node == NODENUM_INVALID)
    {
        std::stringstream msg;
        msg << "Failed to retrieve required node: " << node_ref.ToString();
        throw Exception(msg.str());
    }
    return node;
}

void ActorSpawner::ProcessCamera(RigDef::Camera & def)
{
    // Camera at position 0 is main camera and already exists, do not insert another
    if (m_num_cameras_processed > 0)
    {
        m_actor->ar_cameras.push_back(camera_t());
    }
    m_num_cameras_processed++;

    // Set up the camera (note: INVALID nodenums are fixed up in `FinalizeRig()`)
    m_actor->ar_cameras.back().camera_node_pos = this->ResolveNodeRef(def.center_node);
    m_actor->ar_cameras.back().camera_node_dir = this->ResolveNodeRef(def.back_node);
    m_actor->ar_cameras.back().camera_node_roll = this->ResolveNodeRef(def.left_node);    
};

node_t* ActorSpawner::GetBeamNodePointer(RigDef::Node::Ref const & node_ref)
{
    node_t* node = GetNodePointer(node_ref);
    if (node != nullptr)
    {
        return node;
    }
    return nullptr;
}

void ActorSpawner::ProcessBeam(RigDef::Beam & def)
{
    // Nodes
    node_t* ar_nodes[] = {nullptr, nullptr};
    ar_nodes[0] = GetBeamNodePointer(def.nodes[0]);
    if (ar_nodes[0] == nullptr)
    {
        AddMessage(Message::TYPE_WARNING, std::string("Ignoring beam, could not find node: ") + def.nodes[0].ToString());
        return;
    }
    ar_nodes[1] = GetBeamNodePointer(def.nodes[1]);
    if (ar_nodes[1] == nullptr)
    {
        AddMessage(Message::TYPE_WARNING, std::string("Ignoring beam, could not find node: ") + def.nodes[1].ToString());
        return;
    }

    // Beam
    int beam_index = static_cast<int>(m_actor->ar_beams.size());
    beam_t & beam = AddBeam(*ar_nodes[0], *ar_nodes[1], def.defaults, def.detacher_group);
    beam.bm_type = BEAM_NORMAL;
    beam.k = def.defaults->GetScaledSpringiness();
    beam.d = def.defaults->GetScaledDamping();
    beam.bounded = NOSHOCK; // Orig: if (shortbound) ... hardcoded in BTS_BEAMS

    /* Calculate length */
    // orig = precompression hardcoded to 1
    CalculateBeamLength(beam);

    /* Strength */
    float beam_strength = def.defaults->GetScaledBreakingThreshold();
    beam.strength  = beam_strength;

    /* Options */
    if (BITMASK_IS_1(def.options, RigDef::Beam::OPTION_r_ROPE))
    {
        beam.bounded = ROPE;
    }
    if (BITMASK_IS_1(def.options, RigDef::Beam::OPTION_s_SUPPORT))
    {
        beam.bounded = SUPPORTBEAM;
        beam.longbound = def.extension_break_limit;
    }

    if (BITMASK_IS_0(def.options, RigDef::Beam::OPTION_i_INVISIBLE))
    {
        this->CreateBeamVisuals(beam, beam_index, true, def.defaults);
    }
}

void ActorSpawner::SetBeamDeformationThreshold(beam_t & beam, std::shared_ptr<RigDef::BeamDefaults> beam_defaults)
{
    /*
    ---------------------------------------------------------------------------
        Old parser logic
    ---------------------------------------------------------------------------

    VAR default_deform              = BEAM_DEFORM (400,000)
    VAR default_deform_scale        = 1
    VAR beam_creak                  = BEAM_CREAK_DEFAULT (100,000)
    VAR enable_advanced_deformation = false


    add_beam()
        IF default_deform < beam_creak
            default_deform = beam_creak
        END IF

        VAR beam;
        beam.default_deform = default_deform * default_deform_scale
    END

    
    enable_advanced_deformation:
        READ enable_advanced_deformation


    set_beam_defaults:
        READ default_deform
        VAR  default_deform_user_defined
        READ default_deform_scale
        VAR  plastic_coef_user_defined

        IF (!enable_advanced_deformation && default_deform < BEAM_DEFORM)
           default_deform = BEAM_DEFORM;
        END IF

        IF (plastic_coef_user_defined)
            beam_creak = 0
        END IF
  
    ---------------------------------------------------------------------------
        TruckParser2013
    ---------------------------------------------------------------------------    

    VAR beam_defaults
    {
        default_deform                = BEAM_DEFORM
        scale.default_deform          = 1
        _enable_advanced_deformation  = false
        _user_defined                 = false
        _default_deform_set           = false
        _plastic_coef_user_defined    = false
    }


    set_beam_defaults:
        READ beam_defaults


    add_beam:

        // Init

        VAR default_deform = BEAM_DEFORM;
        VAR beam_creak = BEAM_CREAK_DEFAULT;

        // Old 'set_beam_defaults'

        IF (beam_defaults._is_user_defined)

            default_deform = beam_defaults.default_deform
            IF (!beam_defaults._enable_advanced_deformation && default_deform < BEAM_DEFORM)
               default_deform = BEAM_DEFORM;
            END IF

            IF (beam_defaults._plastic_coef_user_defined && beam_defaults.plastic_coef >= 0)
                beam_creak = 0
            END IF

        END IF

        // Old 'add_beam'

        IF default_deform < beam_creak
            default_deform = beam_creak
        END IF

        VAR beam;
        beam.default_deform = default_deform * beam_defaults.scale.default_deform
    
    ---------------------------------------------------------------------------
    */

    // Old init
    float default_deform = BEAM_DEFORM; 
    float beam_creak = BEAM_CREAK_DEFAULT;

    // Old 'set_beam_defaults'
    if (beam_defaults->_is_user_defined)
    {
        default_deform = beam_defaults->deformation_threshold;
        if (!beam_defaults->_enable_advanced_deformation && default_deform < BEAM_DEFORM)
        {
            default_deform = BEAM_DEFORM;
        }

        if (beam_defaults->_is_plastic_deform_coef_user_defined && beam_defaults->plastic_deform_coef >= 0.f)
        {
            beam_creak = 0.f;
        }
    }

    // Old 'add_beam'
    if (default_deform < beam_creak)
    {
        default_deform = beam_creak;
    }

    float deformation_threshold = default_deform * beam_defaults->scale.deformation_threshold_constant;

    beam.minmaxposnegstress = deformation_threshold;
    beam.maxposstress       = deformation_threshold;
    beam.maxnegstress       = -(deformation_threshold);
}

void ActorSpawner::CreateBeamVisuals(beam_t const & beam, int beam_index, bool visible, std::shared_ptr<RigDef::BeamDefaults> const& beam_defaults, std::string material_override)
{
    std::string material_name = material_override;
    if (material_name.empty())
    {
        if (beam.bm_type == BEAM_HYDRO)
        {
            material_name = "tracks/Chrome";
        }
        else
        {
            material_name = beam_defaults->beam_material_name;
            // Check for existing substitute
            auto it = m_managed_materials.find(material_name);
            if (it != m_managed_materials.end())
            {
                auto material = it->second;
                if (!material.isNull())
                {
                    material_name = material->getName();
                }
            }
        }
    }

    if (m_actor->m_gfx_actor->m_gfx_beams_parent_scenenode == nullptr)
    {
        m_actor->m_gfx_actor->m_gfx_beams_parent_scenenode
            = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
    }

    try
    {
        Ogre::Entity* entity = App::GetGfxScene()->GetSceneManager()->createEntity(this->ComposeName("beam", beam_index), "beam.mesh");
        entity->setMaterialName(material_name);

        BeamGfx beamx;
        beamx.rod_diameter_mm = uint16_t(beam_defaults->visual_beam_diameter * 1000.f);
        beamx.rod_beam_index = static_cast<uint16_t>(beam_index);
        beamx.rod_node1 = beam.p1num;
        beamx.rod_node2 = beam.p2num;
        beamx.rod_target_actor = m_actor;
        beamx.rod_is_visible = false;

        beamx.rod_scenenode = m_actor->m_gfx_actor->m_gfx_beams_parent_scenenode->createChildSceneNode();
        beamx.rod_scenenode->attachObject(entity);
        beamx.rod_scenenode->setVisible(visible, /*cascade:*/ false);
        beamx.rod_scenenode->setScale(beam_defaults->visual_beam_diameter, -1, beam_defaults->visual_beam_diameter);

        m_actor->m_gfx_actor->m_gfx_beams.push_back(beamx);
    }
    catch (Ogre::Exception& e)
    {
        this->AddMessage(Message::TYPE_WARNING, fmt::format("Could not create beam visuals: {}", e.getFullDescription()));
    }
}

void ActorSpawner::CalculateBeamLength(beam_t & beam)
{
    float beam_length = (m_actor->ar_nodes[beam.p1num].RelPosition - m_actor->ar_nodes[beam.p2num].RelPosition).length();
    beam.L = beam_length;
    beam.refL = beam_length;
}

void ActorSpawner::InitBeam(beam_t & beam, node_t *node_1, node_t *node_2)
{
    beam.p1num = node_1->pos;
    beam.p2num = node_2->pos;

    /* Length */
    CalculateBeamLength(beam);
}

void ActorSpawner::AddMessage(ActorSpawner::Message type,	Ogre::String const & text)
{
    Str<4000> txt;
    if (m_file)
    {
        txt << m_file->name;
    }
    if (m_current_keyword != RigDef::Keyword::INVALID)
    {
        txt << " (" << RigDef::KeywordToString(m_current_keyword) << ")";
    }
    txt << ": " << text;
    RoR::Console::MessageType cm_type;
    switch (type)
    {
    case Message::TYPE_ERROR:
        cm_type = RoR::Console::MessageType::CONSOLE_SYSTEM_ERROR;
        break;

    case Message::TYPE_WARNING:
        cm_type = RoR::Console::MessageType::CONSOLE_SYSTEM_WARNING;
        break;

    default:
        cm_type = RoR::Console::MessageType::CONSOLE_SYSTEM_NOTICE;
        break;
    }

    RoR::App::GetConsole()->putMessage(RoR::Console::CONSOLE_MSGTYPE_ACTOR, cm_type, txt.ToCStr());
}

NodeNum_t ActorSpawner::ResolveNodeRef(RigDef::Node::Ref const & node_ref)
{
    if (!node_ref.IsValidAnyState())
    {
        AddMessage(Message::TYPE_ERROR, std::string("Attempt to resolve invalid node reference: ") + node_ref.ToString());
        return NODENUM_INVALID;
    }
    bool is_imported = node_ref.GetImportState_IsValid();
    bool is_named = (is_imported ? node_ref.GetImportState_IsResolvedNamed() : node_ref.GetRegularState_IsNamed());
    if (is_named)
    {
        auto result = m_named_nodes.find(node_ref.Str());
        if (result != m_named_nodes.end())
        {
            return (NodeNum_t)result->second;
        }

        std::stringstream msg;
        msg << "Failed to resolve node-ref (node not found):" << node_ref.ToString();
        AddMessage(Message::TYPE_ERROR, msg.str());

        return NODENUM_INVALID;
    }
    else
    {
        // Imported nodes pass without check
        if (!is_imported && (node_ref.Num() >= static_cast<unsigned int>(static_cast<int>(m_actor->ar_nodes.size()))))
        {

            std::stringstream msg;
            msg << "Failed to resolve node-ref (node index too big, node count is: "<<static_cast<int>(m_actor->ar_nodes.size())<<"): " << node_ref.ToString();
            AddMessage(Message::TYPE_ERROR, msg.str());

            return NODENUM_INVALID;
        }
        return (NodeNum_t)node_ref.Num();
    }
}

node_t* ActorSpawner::GetNodePointer(RigDef::Node::Ref const & node_ref)
{
    NodeNum_t node = ResolveNodeRef(node_ref);
    if (node != NODENUM_INVALID)
    {
        return & m_actor->ar_nodes[node];
    }
    else
    {
        return nullptr;
    }
}

node_t* ActorSpawner::GetNodePointerOrThrow(RigDef::Node::Ref const & node_ref)
{
    node_t *node = GetNodePointer(node_ref);
    if (node == nullptr)
    {
        std::stringstream msg;
        msg << "Required node not found: " << node_ref.ToString();
        throw Exception(msg.str());
    }
    return node;
}

NodeNum_t ActorSpawner::AddNode(RigDef::Node::Id & id)
{
    if (!id.IsValid())
    {
        std::stringstream msg;
        msg << "Attempt to add node with 'INVALID' flag: " << id.ToString() << " (number of nodes at this point: " << static_cast<int>(m_actor->ar_nodes.size()) << ")";
        this->AddMessage(Message::TYPE_ERROR, msg.str());
        return NODENUM_INVALID;
    }

    if (id.IsTypeNamed())
    {
        NodeNum_t new_index = static_cast<NodeNum_t>(m_actor->ar_nodes.size());
        auto insert_result = m_named_nodes.insert(std::make_pair(id.Str(), new_index));
        if (! insert_result.second)
        {
            std::stringstream msg;
            msg << "Ignoring named node! Duplicate name: " << id.Str() << " (number of nodes at this point: " << static_cast<int>(m_actor->ar_nodes.size()) << ")";
            this->AddMessage(Message::TYPE_ERROR, msg.str());
            return NODENUM_INVALID;
        }
        node_t& node = AddNode();
        m_actor->ar_nodes_aux[node.pos].nda_source_name = id.Str();
        m_actor->ar_nodes_name_top_length = std::max(m_actor->ar_nodes_name_top_length, (int)id.Str().length());
        
        return new_index;
    }
    if (id.IsTypeNumbered())
    {
        if (id.Num() < static_cast<unsigned int>(static_cast<int>(m_actor->ar_nodes.size())))
        {
            std::stringstream msg;
            msg << "Duplicate node number, previous definition will be overriden! - " << id.ToString() << " (number of nodes at this point: " << static_cast<int>(m_actor->ar_nodes.size()) << ")";
            this->AddMessage(Message::TYPE_WARNING, msg.str());
        }
        node_t& node = AddNode();        
        return node.pos;
    }
    // Invalid node ID without type flag!
    throw Exception("Invalid Node::Id without type flags!");
}

void ActorSpawner::ProcessNode(RigDef::Node & def)
{
    NodeNum_t inserted_node = AddNode(def.id);
    if (inserted_node == NODENUM_INVALID)
    {
        return;
    }

    node_t & node = m_actor->ar_nodes[inserted_node];
    node.pos = inserted_node; /* Node index */

    /* Positioning */
    Ogre::Vector3 node_position = m_spawn_position + def.position;
    node.AbsPosition = node_position; 
    node.RelPosition = node_position - m_actor->ar_origin;

    node.friction_coef = def.node_defaults->friction;
    node.volume_coef = def.node_defaults->volume;
    node.surface_coef = def.node_defaults->surface;

    /* Mass */
    if (def.default_minimass)
    {
        m_actor->ar_nodes_aux[inserted_node].nda_minimass = def.default_minimass->min_mass_Kg;
    }
    else
    {
        m_actor->ar_nodes_aux[inserted_node].nda_minimass = m_state.global_minimass;
    }

    if (def.node_defaults->load_weight >= 0.f) // The >= operator is in orig.
    {
        // orig = further override of hardcoded default.
        node.mass = def.node_defaults->load_weight; 
        m_actor->ar_nodes_aux[inserted_node].nda_override_mass = true;
        m_actor->ar_nodes_aux[inserted_node].nda_loaded_mass = true;
    }
    else
    {
        node.mass = 10; // Hardcoded in original (bts_nodes, call to init_node())
        m_actor->ar_nodes_aux[inserted_node].nda_loaded_mass = false;
    }

    /* Lockgroup */
    node.nd_lockgroup = (m_file->lockgroup_default_nolock) ? RigDef::Lockgroup::LOCKGROUP_NOLOCK : RigDef::Lockgroup::LOCKGROUP_DEFAULT;

    /* Options */
    unsigned int options = def.options | def.node_defaults->options; /* Merge bit flags */
    if (BITMASK_IS_1(options, RigDef::Node::OPTION_l_LOAD_WEIGHT))
    {
        m_actor->ar_nodes_aux[inserted_node].nda_loaded_mass = true;
        if (def._has_load_weight_override)
        {
            m_actor->ar_nodes_aux[inserted_node].nda_override_mass = true;
            node.mass = def.load_weight_override;
        }
        else
        {
            m_actor->m_masscount++;
        }
    }
    if (BITMASK_IS_1(options, RigDef::Node::OPTION_h_HOOK_POINT))
    {
        /* Link [current-node] -> [node-0] */
        /* If current node is 0, link [node-0] -> [node-1] */
        node_t & node_2 = m_actor->ar_nodes[((node.pos == 0) ? 1 : 0)];
        BeamID_t beam_index = static_cast<int>(m_actor->ar_beams.size());

        beam_t & beam = AddBeam(node, node_2, def.beam_defaults, def.detacher_group);
        SetBeamStrength(beam, def.beam_defaults->GetScaledBreakingThreshold() * 100.f);
        beam.bm_type           = BEAM_HYDRO;
        beam.d                 = def.beam_defaults->GetScaledDamping() * 0.1f;
        beam.k                 = def.beam_defaults->GetScaledSpringiness();
        beam.bounded           = ROPE;
        beam.bm_disabled       = true;
        beam.L                 = HOOK_RANGE_DEFAULT;
        beam.refL              = HOOK_RANGE_DEFAULT;
        SetBeamDeformationThreshold(beam, def.beam_defaults);
        CreateBeamVisuals(beam, beam_index, false, def.beam_defaults);
            
        // Logic cloned from SerializedRig.cpp, section BTS_NODES
        hook_t hook;
        hook.hk_hook_node         = node.pos;
        hook.hk_group             = -1;
        hook.hk_locked            = UNLOCKED;
        hook.hk_lockgroup         = -1;
        hook.hk_beam              = beam_index;
        hook.hk_maxforce          = HOOK_FORCE_DEFAULT;
        hook.hk_lockrange         = HOOK_RANGE_DEFAULT;
        hook.hk_lockspeed         = HOOK_SPEED_DEFAULT;
        hook.hk_selflock          = false;
        hook.hk_nodisable         = false;
        hook.hk_timer             = 0.0f;
        hook.hk_timer_preset      = HOOK_LOCK_TIMER_DEFAULT;
        hook.hk_autolock          = false;
        hook.hk_min_length        = 0.f;
        m_actor->ar_hooks.push_back(hook);
    }
    AdjustNodeBuoyancy(node, def, def.node_defaults);
    node.nd_no_ground_contact = BITMASK_IS_1(options, RigDef::Node::OPTION_c_NO_GROUND_CONTACT);
    node.nd_no_mouse_grab  = BITMASK_IS_1(options, RigDef::Node::OPTION_m_NO_MOUSE_GRAB);

    m_actor->ar_exhaust_dir_node        = BITMASK_IS_1(options, RigDef::Node::OPTION_y_EXHAUST_DIRECTION) ? node.pos : 0;
    m_actor->ar_exhaust_pos_node         = BITMASK_IS_1(options, RigDef::Node::OPTION_x_EXHAUST_POINT) ? node.pos : 0;

    // Update "fusedrag" autocalc y & z span
    if (def.position.z < m_fuse_z_min) { m_fuse_z_min = def.position.z; }
    if (def.position.z > m_fuse_z_max) { m_fuse_z_max = def.position.z; }
    if (def.position.y < m_fuse_y_min) { m_fuse_y_min = def.position.y; }
    if (def.position.y > m_fuse_y_max) { m_fuse_y_max = def.position.y; }

    // GFX
    NodeGfx nfx(node.pos);
    nfx.nx_no_particles = BITMASK_IS_1(options, RigDef::Node::OPTION_p_NO_PARTICLES);
    nfx.nx_may_get_wet  = BITMASK_IS_0(options, RigDef::Node::OPTION_c_NO_GROUND_CONTACT);
    nfx.nx_no_particles = BITMASK_IS_1(options, RigDef::Node::OPTION_p_NO_PARTICLES);
    nfx.nx_no_sparks    = BITMASK_IS_1(options, RigDef::Node::OPTION_f_NO_SPARKS);
    m_actor->m_gfx_actor->m_gfx_nodes.push_back(nfx);
}

void ActorSpawner::AddExhaust(
        NodeNum_t emitter_node_idx,
        NodeNum_t direction_node_idx
    )
{
    exhaust_t exhaust;
    exhaust.emitterNode = emitter_node_idx;
    exhaust.directionNode = direction_node_idx;

    exhaust.smoker = App::GetGfxScene()->GetSceneManager()->createParticleSystem(
        this->ComposeName("Exhaust", static_cast<int>(m_actor->exhausts.size())),
        /*quota=*/500, // Default value
        m_custom_resource_group);

    if (exhaust.smoker == nullptr)
    {
        AddMessage(Message::TYPE_ERROR, "Failed to create exhaust");
        return;
    }
    exhaust.smoker->setVisibilityFlags(DEPTHMAP_DISABLED); // Disable particles in depthmap

    Ogre::MaterialPtr mat = this->FindOrCreateCustomizedMaterial("tracks/Smoke");
    exhaust.smoker->setMaterialName(mat->getName(), mat->getGroup());

    exhaust.smokeNode = m_particles_parent_scenenode->createChildSceneNode();
    exhaust.smokeNode->attachObject(exhaust.smoker);
    exhaust.smokeNode->setPosition(m_actor->ar_nodes[exhaust.emitterNode].AbsPosition);

    m_actor->m_gfx_actor->SetNodeHot(exhaust.emitterNode, true);
    m_actor->m_gfx_actor->SetNodeHot(exhaust.directionNode, true);

    m_actor->exhausts.push_back(exhaust);
}

void ActorSpawner::ProcessCinecam(RigDef::Cinecam & def)
{
    // Node
    Ogre::Vector3 node_pos = m_spawn_position + def.position;
    node_t& camera_node = AddNode();
    InitNode(camera_node, node_pos);
    camera_node.nd_no_ground_contact = true; // Orig: hardcoded in BTS_CINECAM
    camera_node.friction_coef = NODE_FRICTION_COEF_DEFAULT; // Node defaults are ignored here.
    AdjustNodeBuoyancy(camera_node, def.node_defaults);
    camera_node.volume_coef   = def.node_defaults->volume;
    camera_node.surface_coef  = def.node_defaults->surface;
    // NOTE: Not applying the 'node_mass' value here for backwards compatibility - this node must go through initial `Actor::RecalculateNodeMasses()` pass with default weight.

    m_actor->ar_nodes_aux[camera_node.pos].nda_minimass = m_state.global_minimass;

    m_actor->ar_cinecam_node[m_actor->ar_num_cinecams] = camera_node.pos;
    m_actor->ar_num_cinecams++;

    // Beams
    for (unsigned int i = 0; i < 8; i++)
    {
        int beam_index = static_cast<int>(m_actor->ar_beams.size());
        node_t& node = m_actor->ar_nodes[this->GetNodeIndexOrThrow(def.nodes[i])];
        beam_t & beam = AddBeam(camera_node, node, def.beam_defaults, DEFAULT_DETACHER_GROUP);
        beam.bm_type = BEAM_NORMAL;
        CalculateBeamLength(beam);
        beam.k = def.spring;
        beam.d = def.damping;
    }
};

void ActorSpawner::InitNode(node_t & node, Ogre::Vector3 const & position)
{
    /* Position */
    node.AbsPosition = position;
    node.RelPosition = position - m_actor->ar_origin;
}

void ActorSpawner::InitNode(
    node_t & node, 
    Ogre::Vector3 const & position,
    std::shared_ptr<RigDef::NodeDefaults> node_defaults
)
{
    InitNode(node, position);
    node.friction_coef = node_defaults->friction;
    node.volume_coef = node_defaults->volume;
    node.surface_coef = node_defaults->surface;
}

void ActorSpawner::ProcessGlobals(RigDef::Globals & def)
{
    m_actor->m_dry_mass = def.dry_mass;
    m_actor->m_load_mass = def.cargo_mass;

    // NOTE: Don't do any material pre-processing here; it'll be done on actual entities (via `SetupNewEntity()`).
    if (! def.material_name.empty())
    {
        Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(def.material_name); // Check if exists (compatibility)
        if (!mat.isNull())
        {
            m_cab_material_name = def.material_name;
        }
        else
        {
            std::stringstream msg;
            msg << "Material '" << def.material_name << "' defined in section 'globals' not found. Trying material 'tracks/transred'";
            this->AddMessage(Message::TYPE_ERROR, msg.str());

            m_cab_material_name = "tracks/transred";
        }
    }
}

/* -------------------------------------------------------------------------- */
// Limits.
/* -------------------------------------------------------------------------- */

bool ActorSpawner::CheckParticleLimit(unsigned int count)
{
    if ((m_actor->ar_num_custom_particles + count) > MAX_CPARTICLES)
    {
        std::stringstream msg;
        msg << "Particle limit (" << MAX_CPARTICLES << ") exceeded";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return false;
    }
    return true;
}

bool ActorSpawner::CheckAxleLimit(unsigned int count)
{
    if ((m_actor->m_num_wheel_diffs + count) > MAX_WHEELS/2)
    {
        std::stringstream msg;
        msg << "Axle limit (" << MAX_WHEELS/2 << ") exceeded";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return false;
    }
    return true;
}

bool ActorSpawner::CheckSubmeshLimit(unsigned int count)
{
    if ((m_oldstyle_cab_submeshes.size() + count) > MAX_SUBMESHES)
    {
        std::stringstream msg;
        msg << "Submesh limit (" << MAX_SUBMESHES << ") exceeded";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return false;
    }
    return true;
}

bool ActorSpawner::CheckTexcoordLimit(unsigned int count)
{
    if ((m_oldstyle_cab_texcoords.size() + count) > MAX_TEXCOORDS)
    {
        std::stringstream msg;
        msg << "Texcoord limit (" << MAX_TEXCOORDS << ") exceeded";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return false;
    }
    return true;
}

bool ActorSpawner::CheckCabLimit(unsigned int count)
{
    if ((m_actor->ar_num_cabs + count) > MAX_CABS)
    {
        std::stringstream msg;
        msg << "Cab limit (" << MAX_CABS << ") exceeded";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return false;
    }
    return true;
}

bool ActorSpawner::CheckCameraRailLimit(unsigned int count)
{
    if ((m_actor->ar_num_camera_rails + count) > MAX_CAMERARAIL)
    {
        std::stringstream msg;
        msg << "CameraRail limit (" << MAX_CAMERARAIL << ") exceeded";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return false;
    }
    return true;
}

bool ActorSpawner::CheckAeroEngineLimit(unsigned int count)
{
    if ((m_actor->ar_num_aeroengines + count) > MAX_AEROENGINES)
    {
        std::stringstream msg;
        msg << "AeroEngine limit (" << MAX_AEROENGINES << ") exceeded";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return false;
    }
    return true;
}

bool ActorSpawner::CheckScrewpropLimit(unsigned int count)
{
    if ((m_actor->ar_num_screwprops + count) > MAX_SCREWPROPS)
    {
        std::stringstream msg;
        msg << "Screwprop limit (" << MAX_SCREWPROPS << ") exceeded";
        AddMessage(Message::TYPE_ERROR, msg.str());
        return false;
    }
    return true;
}

void ActorSpawner::InitNode(unsigned int node_index, Ogre::Vector3 const & position)
{
    InitNode(m_actor->ar_nodes[node_index], position);
}

beam_t & ActorSpawner::GetBeam(unsigned int index)
{
    return m_actor->ar_beams[index];
}

node_t & ActorSpawner::AddNode()
{
    NodeNum_t pos = static_cast<NodeNum_t>(m_actor->ar_nodes.size());
    m_actor->ar_nodes.push_back(node_t(pos));
    m_actor->ar_nodes_aux.push_back(node_aux_t()); // Initialize with empty values (assume a generated node)

    return m_actor->ar_nodes.back();
}

beam_t & ActorSpawner::GetFreeBeam()
{
    m_actor->ar_beams.push_back(beam_t(static_cast<BeamID_t>(m_actor->ar_beams.size())));
    m_actor->ar_beams_aux.push_back(beam_aux_t());
    return m_actor->ar_beams.back();
}

shock_t & ActorSpawner::GetFreeShock()
{
    m_actor->ar_shocks.push_back(shock_t(static_cast<ShockID_t>(m_actor->ar_shocks.size())));
    return m_actor->ar_shocks.back();
}

void ActorSpawner::SetBeamSpring(beam_t & beam, float spring)
{
    beam.k = spring;
}

void ActorSpawner::SetBeamDamping(beam_t & beam, float damping)
{
    beam.d = damping;
}

void ActorSpawner::SetupDefaultSoundSources(ActorPtr const& vehicle)
{
    int trucknum = vehicle->ar_instance_id;
    int ar_exhaust_pos_node = vehicle->ar_exhaust_pos_node;

#ifdef USE_OPENAL
    if (App::GetSoundScriptManager()->isDisabled()) 
    {
        return;
    }

    //engine
    if (vehicle->ar_engine != nullptr) /* Land vehicle */
    {
        if (vehicle->ar_engine->m_engine_type == 't')
        {
            AddSoundSourceInstance(vehicle, "tracks/default_diesel", ar_exhaust_pos_node);
            AddSoundSourceInstance(vehicle, "tracks/default_force", ar_exhaust_pos_node);
            AddSoundSourceInstance(vehicle, "tracks/default_brakes", 0);
            AddSoundSourceInstance(vehicle, "tracks/default_parkbrakes", 0);
            AddSoundSourceInstance(vehicle, "tracks/default_reverse_beep", 0);
        }
        if (vehicle->ar_engine->m_engine_type == 'c')
            AddSoundSourceInstance(vehicle, "tracks/default_car", ar_exhaust_pos_node);
        if (vehicle->ar_engine->HasTurbo())
        {
            if (vehicle->ar_engine->m_turbo_inertia_factor >= 3)
                AddSoundSourceInstance(vehicle, "tracks/default_turbo_big", ar_exhaust_pos_node);
            else if (vehicle->ar_engine->m_turbo_inertia_factor <= 0.5)
                AddSoundSourceInstance(vehicle, "tracks/default_turbo_small", ar_exhaust_pos_node);
            else
                AddSoundSourceInstance(vehicle, "tracks/default_turbo_mid", ar_exhaust_pos_node);

            AddSoundSourceInstance(vehicle, "tracks/default_turbo_bov", ar_exhaust_pos_node);
            AddSoundSourceInstance(vehicle, "tracks/default_wastegate_flutter", ar_exhaust_pos_node);
        }

        if (vehicle->ar_engine->m_engine_has_air)
            AddSoundSourceInstance(vehicle, "tracks/default_air_purge", 0);
        //starter
        AddSoundSourceInstance(vehicle, "tracks/default_starter", 0);
        // turn signals
        AddSoundSourceInstance(vehicle, "tracks/default_turn_signal", 0);
    }
    if (vehicle->ar_driveable==TRUCK)
    {
        //horn
        if (vehicle->ar_is_police)
            AddSoundSourceInstance(vehicle, "tracks/default_police", 0);
        else
            AddSoundSourceInstance(vehicle, "tracks/default_horn", 0);
        //shift
            AddSoundSourceInstance(vehicle, "tracks/default_shift", 0);
    }
    //pump
    if (vehicle->m_has_command_beams)
    {
        AddSoundSourceInstance(vehicle, "tracks/default_pump", 0);
    }
    //antilock brake
    if (vehicle->alb_mode || !vehicle->alb_notoggle)
    {
        AddSoundSourceInstance(vehicle, "tracks/default_antilock", 0);
    }
    //tractioncontrol
    if (vehicle->tc_mode || !vehicle->tc_notoggle)
    {
        AddSoundSourceInstance(vehicle, "tracks/default_tractioncontrol", 0);
    }
    //screetch
    if ((vehicle->ar_driveable==TRUCK || vehicle->ar_driveable==AIRPLANE) && vehicle->ar_num_wheels != 0)
    {
        AddSoundSourceInstance(vehicle, "tracks/default_screetch", 0);
    }
    //break & creak
    AddSoundSourceInstance(vehicle, "tracks/default_break", 0);
    AddSoundSourceInstance(vehicle, "tracks/default_creak", 0);
    //boat engine
    if (vehicle->ar_driveable==BOAT)
    {
        if (vehicle->m_total_mass>50000.0)
            AddSoundSourceInstance(vehicle, "tracks/default_marine_large", ar_exhaust_pos_node);
        else
            AddSoundSourceInstance(vehicle, "tracks/default_marine_small", ar_exhaust_pos_node);
        //no start/stop engine for boats, so set sound always on!
        SOUND_START(trucknum, SS_TRIG_ENGINE);
        SOUND_MODULATE(trucknum, SS_MOD_ENGINE, 0.5);
    }
    //airplane warnings
    if (vehicle->ar_driveable==AIRPLANE)
    {
        AddSoundSourceInstance(vehicle, "tracks/default_gpws_10", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_gpws_20", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_gpws_30", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_gpws_40", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_gpws_50", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_gpws_100", 0);

        AddSoundSourceInstance(vehicle, "tracks/default_gpws_pullup", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_gpws_minimums", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_gpws_apdisconnect", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aoa_warning", 0);

        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat01", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat02", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat03", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat04", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat05", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat06", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat07", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat08", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat09", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat10", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat11", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat12", 0);
        AddSoundSourceInstance(vehicle, "tracks/default_aivionic_chat13", 0);
    }
    //airplane engines
    for (int i=0; i<vehicle->ar_num_aeroengines && i<8; i++)
    {
        int turbojet_node = vehicle->ar_aeroengines[i]->getNoderef();
        Ogre::String index_str = TOSTRING(i+1);

        if (vehicle->ar_aeroengines[i]->getType() == AeroEngineType::AE_TURBOJET)
        {
            AddSoundSourceInstance(vehicle, "tracks/default_turbojet_start" + index_str, turbojet_node);
            AddSoundSourceInstance(vehicle, "tracks/default_turbojet_lopower" + index_str, turbojet_node);
            AddSoundSourceInstance(vehicle, "tracks/default_turbojet_hipower" + index_str, turbojet_node);
            if (((Turbojet*)(vehicle->ar_aeroengines[i]))->tjet_afterburnable)
            {
                AddSoundSourceInstance(vehicle, "tracks/default_turbojet_afterburner" + index_str, turbojet_node);
            }
        }
        else if (vehicle->ar_aeroengines[i]->getType() == AeroEngineType::AE_XPROP)
        {
            if (((Turboprop*)vehicle->ar_aeroengines[i])->is_piston)
            {
                AddSoundSourceInstance(vehicle, "tracks/default_pistonprop_start" + index_str, turbojet_node);
                AddSoundSourceInstance(vehicle, "tracks/default_pistonprop_lopower" + index_str, turbojet_node);
                AddSoundSourceInstance(vehicle, "tracks/default_pistonprop_hipower" + index_str, turbojet_node);
            }
            else
            {
                AddSoundSourceInstance(vehicle, "tracks/default_turboprop_start" + index_str, turbojet_node);
                AddSoundSourceInstance(vehicle, "tracks/default_turboprop_lopower" + index_str, turbojet_node);
                AddSoundSourceInstance(vehicle, "tracks/default_turboprop_hipower" + index_str, turbojet_node);
            }
        }
    }

    // linked sounds
    for (int i=0; i<vehicle->m_num_command_beams; i++)
    {
        AddSoundSource(vehicle, App::GetSoundScriptManager()->createInstance(Ogre::String("tracks/linked/default_command/extend"), trucknum, SL_COMMAND, i), 0);
        AddSoundSource(vehicle, App::GetSoundScriptManager()->createInstance(Ogre::String("tracks/linked/default_command/retract"), trucknum, SL_COMMAND, -i), 0);
    }

#endif //OPENAL
}

void ActorSpawner::UpdateCollcabContacterNodes()
{
    for (int i=0; i<m_actor->ar_num_collcabs; i++)
    {
        int tmpv = m_actor->ar_collcabs[i] * 3;
        m_actor->ar_nodes[m_actor->ar_cabs[tmpv]].nd_cab_node = true;
        m_actor->ar_nodes[m_actor->ar_cabs[tmpv+1]].nd_cab_node = true;
        m_actor->ar_nodes[m_actor->ar_cabs[tmpv+2]].nd_cab_node = true;
    }
    for (int i = 0; i < static_cast<int>(m_actor->ar_nodes.size()); i++)
    {
        if (m_actor->ar_nodes[i].nd_contacter)
        {
            m_actor->ar_num_contactable_nodes++;
            m_actor->ar_num_contacters++;
        }
        else if (!m_actor->ar_nodes[i].nd_no_ground_contact &&
                 (m_actor->ar_nodes[i].nd_cab_node || m_actor->ar_nodes[i].nd_rim_node || m_actor->ar_num_collcabs == 0))
        {
            m_actor->ar_nodes[i].nd_contactable = true;
            m_actor->ar_num_contactable_nodes++;
        }
    }
}

RigDef::MaterialFlareBinding* ActorSpawner::FindFlareBindingForMaterial(std::string const & material_name)
{
    for (auto& module: m_selected_modules)
    {
        for (auto& def: module->materialflarebindings)
        {
            if (def.material_name == material_name)
            {
                return &def;
            }
        }
    }
    return nullptr;
}

RigDef::VideoCamera* ActorSpawner::FindVideoCameraByMaterial(std::string const & material_name)
{
    for (auto& module: m_selected_modules)
    {
        for (auto& def: module->videocameras)
        {
            if (def.material_name == material_name)
            {
                return &def;
            }
        }
    }

    return nullptr;
}

Ogre::MaterialPtr ActorSpawner::FindOrCreateCustomizedMaterial(std::string mat_lookup_name)
{
    try
    {
        // Check for existing substitute
        auto lookup_res = m_material_substitutions.find(mat_lookup_name);
        if (lookup_res != m_material_substitutions.end())
        {
            return lookup_res->second.material;
        }

        CustomMaterial lookup_entry;

        // Query old-style mirrors (=special props, hardcoded material name 'mirror')
        if (mat_lookup_name == "mirror")
        {
            lookup_entry.mirror_prop_type = m_curr_mirror_prop_type;
            lookup_entry.mirror_prop_scenenode = m_curr_mirror_prop_scenenode;
            lookup_entry.material_flare_def = nullptr;
            static int mirror_counter = 0;
            const std::string new_mat_name = this->ComposeName("RenderMaterial", mirror_counter);
            ++mirror_counter;
            lookup_entry.material = Ogre::MaterialManager::getSingleton().getByName("mirror")->clone(new_mat_name, true, m_custom_resource_group);
            // Special case - register under generated name. This is because all mirrors use the same material 'mirror'
            m_material_substitutions.insert(std::make_pair(new_mat_name, lookup_entry));
            return lookup_entry.material; // Done!
        }

        // Query 'videocameras'
        RigDef::VideoCamera* videocam_def = this->FindVideoCameraByMaterial(mat_lookup_name);
        if (videocam_def != nullptr)
        {
            Ogre::MaterialPtr video_mat_shared;
            auto found_managedmat = m_managed_materials.find(mat_lookup_name);
            if (found_managedmat != m_managed_materials.end())
            {
                video_mat_shared = found_managedmat->second;
            }
            else
            {
                video_mat_shared = Ogre::MaterialManager::getSingleton().getByName(mat_lookup_name);
            }

            if (!video_mat_shared.isNull())
            {
                lookup_entry.video_camera_def = videocam_def;
                const std::string video_mat_name = this->ComposeName(videocam_def->material_name.c_str(), 0);
                lookup_entry.material = video_mat_shared->clone(video_mat_name, true, m_custom_resource_group);
                m_material_substitutions.insert(std::make_pair(mat_lookup_name, lookup_entry));
                return lookup_entry.material; // Done!
            }
            else
            {
                std::stringstream msg;
                msg << "VideoCamera material '" << mat_lookup_name << "' not found! Ignoring videocamera.";
                this->AddMessage(Message::TYPE_WARNING, msg.str());
            }
        }

        // Resolve 'materialflarebindings'.
        RigDef::MaterialFlareBinding* mat_flare_def = this->FindFlareBindingForMaterial(mat_lookup_name);
        if (mat_flare_def != nullptr)
        {
            lookup_entry.material_flare_def = mat_flare_def;
        }

        // Query .skin material replacements
        if (m_actor->m_used_skin_entry != nullptr)
        {
            std::shared_ptr<RoR::SkinDef>& skin_def = m_actor->m_used_skin_entry->skin_def;

            auto skin_res = skin_def->replace_materials.find(mat_lookup_name);
            if (skin_res != skin_def->replace_materials.end())
            {
                Ogre::MaterialPtr skin_mat = Ogre::MaterialManager::getSingleton().getByName(
                    skin_res->second, m_actor->m_used_skin_entry->resource_group);
                if (!skin_mat.isNull())
                {
                    std::stringstream name_buf;
                    name_buf << skin_mat->getName() << ACTOR_ID_TOKEN << m_actor->ar_instance_id;
                    lookup_entry.material = skin_mat->clone(name_buf.str(), /*changeGroup=*/true, m_custom_resource_group);
                    m_material_substitutions.insert(std::make_pair(mat_lookup_name, lookup_entry));
                    return lookup_entry.material;
                }
                else
                {
                    std::stringstream buf;
                    buf << "Material '" << skin_res->second << "' from skin '" << m_actor->m_used_skin_entry->dname
                        << "' not found (filename: '" << m_actor->m_used_skin_entry->fname 
                        << "', resource group: '"<< m_actor->m_used_skin_entry->resource_group
                        <<"')! Ignoring it...";
                    this->AddMessage(Message::TYPE_ERROR, buf.str());
                }
            }
        }

        // Acquire substitute - either use managedmaterial or generate new by cloning.
        auto mmat_res = m_managed_materials.find(mat_lookup_name);
        if (mmat_res != m_managed_materials.end())
        {
            // Use managedmaterial as substitute
            lookup_entry.material = mmat_res->second;
        }
        else
        {
            // Generate new substitute
            Ogre::MaterialPtr orig_mat = Ogre::MaterialManager::getSingleton().getByName(mat_lookup_name, m_custom_resource_group);
            if (orig_mat.isNull())
            {
                std::stringstream buf;
                buf << "Material doesn't exist:" << mat_lookup_name;
                this->AddMessage(Message::TYPE_ERROR, buf.str());
                return Ogre::MaterialPtr(); // NULL
            }

            std::stringstream name_buf;
            name_buf << orig_mat->getName() << ACTOR_ID_TOKEN << m_actor->ar_instance_id;
            lookup_entry.material = orig_mat->clone(name_buf.str(), true, m_custom_resource_group);
        }

        // Finally, query texture replacements - .skin and builtins
        for (auto& technique: lookup_entry.material->getTechniques())
        {
            for (auto& pass: technique->getPasses())
            {
                for (auto& tex_unit: pass->getTextureUnitStates())
                {
                    // Built-ins
                    if (tex_unit->getTextureName() == "dashtexture")
                    {
                        if (!m_oldstyle_renderdash)
                        {
                            // This is technically a bug, but does it matter at all? Let's watch ~ only_a_ptr, 05/2019
                            std::stringstream msg;
                            msg << "Warning: '" << mat_lookup_name
                                << "' references 'dashtexture', but Renderdash isn't created yet! Texture will be blank.";
                            this->AddMessage(Message::TYPE_WARNING, msg.str());
                        }
                        else
                        {
                            tex_unit->setTexture(m_oldstyle_renderdash->getTexture());
                        }
                    }
                    // .skin
                    else if (m_actor->m_used_skin_entry != nullptr)
                    {
                        const size_t num_frames = tex_unit->getNumFrames();
                        for (size_t i = 0; i < num_frames; ++i)
                        {
                            const auto end = m_actor->m_used_skin_entry->skin_def->replace_textures.end();
                            const auto query = m_actor->m_used_skin_entry->skin_def->replace_textures.find(tex_unit->getFrameTextureName((unsigned int)i));
                            if (query != end)
                            {
                                // Skin has replacement for this texture
                                if (m_actor->m_used_skin_entry->resource_group != m_custom_resource_group) // The skin comes from a SkinZip bundle (different resource group)
                                {
                                    Ogre::TexturePtr tex = Ogre::TextureManager::getSingleton().getByName(
                                        query->second, m_actor->m_used_skin_entry->resource_group);
                                    if (tex.isNull())
                                    {
                                        // `Ogre::TextureManager` doesn't automatically register all images in resource groups,
                                        // it waits for `Ogre::Resource`s to be created explicitly.
                                        // Normally this is done by `Ogre::MaterialManager` when loading a material.
                                        // In this case we must do it manually
                                        tex = Ogre::TextureManager::getSingleton().create(
                                            query->second, m_actor->m_used_skin_entry->resource_group);
                                    }
                                    tex_unit->_setTexturePtr(tex, i);
                                }
                                else // The skin lives in the vehicle bundle (same resource group)
                                {
                                    tex_unit->setFrameTextureName(query->second, (unsigned int)i);
                                }
                            }
                        }
                    }
                } // texture unit states
            } // passes
        } // techniques

        m_material_substitutions.insert(std::make_pair(mat_lookup_name, lookup_entry)); // Register the substitute
        return lookup_entry.material;
    }
    catch (Ogre::Exception& e)
    {
        std::stringstream msg;
        msg << "Exception while customizing material \"" << mat_lookup_name << "\", message: " << e.getFullDescription();
        this->AddMessage(Message::TYPE_ERROR, msg.str());
    }
    return Ogre::MaterialPtr(); // NULL
}

Ogre::MaterialPtr ActorSpawner::CreateSimpleMaterial(Ogre::ColourValue color)
{
    ROR_ASSERT(!m_simple_material_base.isNull());

    static unsigned int simple_mat_counter = 0;
    char name_buf[300];
    snprintf(name_buf, 300, "SimpleMaterial-%u%s%d", simple_mat_counter, ACTOR_ID_TOKEN, m_actor->ar_instance_id);
    Ogre::MaterialPtr newmat = m_simple_material_base->clone(name_buf);
    ++simple_mat_counter;
    newmat->getTechnique(0)->getPass(0)->setAmbient(color);

    return newmat;
}

void ActorSpawner::SetupNewEntity(Ogre::Entity* ent, Ogre::ColourValue simple_color)
{
    // RULE: Each actor must have it's own material instances (a lookup table is kept for OrigName->CustomName)
    //
    // Setup routine:
    //
    //   1. If "SimpleMaterials" (plain color surfaces denoting component type) are enabled in config file, 
    //          material is generated (not saved to lookup table) and processing ends.
    //   2. If the material name is 'mirror', it's a special prop - rear view mirror.
    //          material is generated, added to lookup table under generated name (special case) and processing ends.
    //   3. If the material is a 'videocamera' of any subtype, material is created, added to lookup table and processing ends.
    //   4  'materialflarebindngs' are resolved -> binding is persisted in lookup table.
    //   5  SkinZIP _material replacements_ are queried. If match is found, it's added to lookup table and processing ends.
    //   6. ManagedMaterials are queried. If match is found, it's added to lookup table and processing ends.
    //   7. Orig. material is cloned to create substitute.
    //   8. SkinZIP _texture replacements_ are queried. If match is found, substitute material is updated.
    //   9. Material is added to lookup table, processing ends.
    // ==========================================================

    if (ent == nullptr)
    {
        // Dirty but I don't see any alternative ... ~ ulteq, 10/2018
        AddMessage(Message::TYPE_WARNING, "Failed to create entity: continuing without it ...");
        return;
    }

    // Use simple materials if applicable
    if (m_apply_simple_materials)
    {
        Ogre::MaterialPtr mat = this->CreateSimpleMaterial(simple_color);

        size_t num_sub_entities = ent->getNumSubEntities();
        for (size_t i = 0; i < num_sub_entities; i++)
        {
            Ogre::SubEntity* subent = ent->getSubEntity(i);
            subent->setMaterial(mat);
        }

        return; // Done!
    }

    // Create unique sub-entity (=instance of submesh) materials
    size_t subent_max = ent->getNumSubEntities();
    for (size_t i = 0; i < subent_max; ++i)
    {
        Ogre::SubEntity* subent = ent->getSubEntity(i);

        if (!subent->getMaterial().isNull())
        {
            Ogre::MaterialPtr own_mat = this->FindOrCreateCustomizedMaterial(subent->getMaterialName());
            if (!own_mat.isNull())
            {
                subent->setMaterial(own_mat);
            }
        }
    }
}

void ActorSpawner::FinalizeGfxSetup()
{
    // Check and warn if there are unclaimed managed materials
    // TODO &*&*

    // Process special materials
    for (auto& entry: m_material_substitutions)
    {
        if (entry.second.material_flare_def != nullptr) // 'materialflarebindings'
        {
            this->CreateMaterialFlare(
                entry.second.material_flare_def->flare_number, entry.second.material);
        }
        else if (entry.second.mirror_prop_type != CustomMaterial::MirrorPropType::MPROP_NONE) // special 'prop' - rear view mirror
        {
            this->CreateMirrorPropVideoCam(
                entry.second.material, entry.second.mirror_prop_type, entry.second.mirror_prop_scenenode);
        }
        else if (entry.second.video_camera_def != nullptr) // 'videocameras'
        {
            this->SetCurrentKeyword(RigDef::Keyword::VIDEOCAMERA); // Logging
            this->CreateVideoCamera(entry.second.video_camera_def);
            this->SetCurrentKeyword(RigDef::Keyword::INVALID); // Logging
        }
    }

    if (!App::gfx_enable_videocams->getBool())
    {
        m_actor->m_gfx_actor->SetVideoCamState(VideoCamState::VCSTATE_DISABLED);
    }

    // Load dashboard layouts
    for (auto& module: m_selected_modules)
    {
        for (auto& gs: module->guisettings)
        {
            if (gs.key == "dashboard")
            {
                m_actor->ar_dashboard->loadDashBoard(gs.value, false);
            }
            else if (gs.key == "texturedashboard")
            {
                m_actor->ar_dashboard->loadDashBoard(gs.value, true);
            }
        }
    }

    // If none specified, load default dashboard layouts
    if (!m_actor->ar_dashboard->WasDashboardLoaded())
    {
        if (m_actor->ar_driveable == TRUCK) // load default for a truck
        {
            if (App::gfx_speedo_digital->getBool())
            {
                if (App::gfx_speedo_imperial->getBool())
                {
                    if (m_actor->ar_engine->getMaxRPM() > 3500)
                    {
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard7000_mph.layout", false); //7000 rpm tachometer thanks to Klink
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard7000_mph.layout", true);
                    }
                    else
                    {
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard3500_mph.layout", false);
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard3500_mph.layout", true);
                    }
                }
                else
                {
                    if (m_actor->ar_engine->getMaxRPM() > 3500)
                    {
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard7000.layout", false); //7000 rpm tachometer thanks to Klink
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard7000.layout", true);
                    }
                    else
                    {
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard3500.layout", false);
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard3500.layout", true);
                    }
                }
            }
            else // Analog speedometer
            {
                if (App::gfx_speedo_imperial->getBool())
                {
                    if (m_actor->ar_engine->getMaxRPM() > 3500)
                    {
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard7000_analog_mph.layout", false); //7000 rpm tachometer thanks to Klink
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard7000_analog_mph.layout", true);
                    }
                    else
                    {
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard3500_analog_mph.layout", false);
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard3500_analog_mph.layout", true);
                    }
                }
                else
                {
                    if (m_actor->ar_engine->getMaxRPM() > 3500)
                    {
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard7000_analog.layout", false); //7000 rpm tachometer thanks to Klink
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard7000_analog.layout", true);
                    }
                    else
                    {
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard3500_analog.layout", false);
                        m_actor->ar_dashboard->loadDashBoard("default_dashboard3500_analog.layout", true);
                    }
                }
            }
        }
        else if (m_actor->ar_driveable == BOAT)
        {
            m_actor->ar_dashboard->loadDashBoard("default_dashboard_boat.layout", false);
            m_actor->ar_dashboard->loadDashBoard("default_dashboard_boat.layout", true);
        }
    }

    m_actor->ar_dashboard->setVisible(false);

    if (!m_help_material_name.empty())
    {
        try
        {
            Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(m_help_material_name, m_custom_resource_group);
            m_actor->m_gfx_actor->m_help_mat = mat;
            if (mat &&
                mat->getNumTechniques() > 0 &&
                mat->getTechnique(0)->getNumPasses() > 0 &&
                mat->getTechnique(0)->getPass(0)->getNumTextureUnitStates() > 0 &&
                mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getNumFrames() > 0)
            {
                m_actor->m_gfx_actor->m_help_tex =
                    Ogre::TextureManager::getSingleton().getByName(
                        mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getFrameTextureName(0), m_custom_resource_group);
            }
        }
        catch (Ogre::Exception& e)
        {
            this->AddMessage(Message::TYPE_ERROR,
                "Failed to load `help` material '" + m_help_material_name + "', message:" + e.getFullDescription());
        }
    }
}

void ActorSpawner::ValidateRotator(int id, int axis1, int axis2, NodeNum_t *nodes1, NodeNum_t *nodes2)
{
    const float eps = 0.001f;
    const Ogre::Vector3 ax1 = m_actor->ar_nodes[axis1].AbsPosition;
    const Ogre::Vector3 ax2 = m_actor->ar_nodes[axis2].AbsPosition;
    Ogre::Plane pl = Ogre::Plane((ax1 - ax2).normalisedCopy(), 0);

    Ogre::Vector3 a1 = pl.projectVector(ax1 - m_actor->ar_nodes[nodes1[0]].AbsPosition);
    Ogre::Vector3 a2 = pl.projectVector(ax1 - m_actor->ar_nodes[nodes1[1]].AbsPosition);
    Ogre::Vector3 a3 = pl.projectVector(ax1 - m_actor->ar_nodes[nodes1[2]].AbsPosition);
    Ogre::Vector3 a4 = pl.projectVector(ax1 - m_actor->ar_nodes[nodes1[3]].AbsPosition);
    float a1len = a1.normalise();
    float a2len = a2.normalise();
    float a3len = a3.normalise();
    float a4len = a4.normalise();
    if ((std::max(a1len, a3len) / std::min(a1len, a3len) > 1.f + eps) ||
        (std::max(a2len, a4len) / std::min(a2len, a4len) > 1.f + eps))
    {
        Ogre::String msg = Ogre::StringUtil::format("Off-centered axis on base plate of rotator %d", id);
        AddMessage(Message::TYPE_WARNING, msg);	
    }

    Ogre::Vector3 b1 = pl.projectVector(ax2 - m_actor->ar_nodes[nodes2[0]].AbsPosition);
    Ogre::Vector3 b2 = pl.projectVector(ax2 - m_actor->ar_nodes[nodes2[1]].AbsPosition);
    Ogre::Vector3 b3 = pl.projectVector(ax2 - m_actor->ar_nodes[nodes2[2]].AbsPosition);
    Ogre::Vector3 b4 = pl.projectVector(ax2 - m_actor->ar_nodes[nodes2[3]].AbsPosition);
    float b1len = b1.normalise();
    float b2len = b2.normalise();
    float b3len = b3.normalise();
    float b4len = b4.normalise();
    if ((std::max(b1len, b3len) / std::min(b1len, b3len) > 1.f + eps) ||
        (std::max(b2len, b4len) / std::min(b2len, b4len) > 1.f + eps))
    {
        Ogre::String msg = Ogre::StringUtil::format("Off-centered axis on rotating plate of rotator %d", id);
        AddMessage(Message::TYPE_WARNING, msg);	
    }

    float rot1 = a1.dotProduct(b1);
    float rot2 = a2.dotProduct(b2);
    float rot3 = a3.dotProduct(b3);
    float rot4 = a4.dotProduct(b4);
    if ((std::max(rot1, rot2) / std::min(rot1, rot2) > 1.f + eps) ||
        (std::max(rot2, rot3) / std::min(rot2, rot3) > 1.f + eps) ||
        (std::max(rot3, rot4) / std::min(rot3, rot4) > 1.f + eps) ||
        (std::max(rot4, rot1) / std::min(rot4, rot1) > 1.f + eps))
    {
        Ogre::String msg = Ogre::StringUtil::format("Misaligned plates on rotator %d", id);
        AddMessage(Message::TYPE_WARNING, msg);	
    }
}

Ogre::ManualObject* CreateVideocameraDebugMesh()
{
    // Create material
    static size_t counter = 0;
    Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().create(
        "VideoCamDebugMat-" + TOSTRING(counter), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    ++counter;
    mat->getTechnique(0)->getPass(0)->createTextureUnitState();
    mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureFiltering(Ogre::TFO_ANISOTROPIC);
    mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureAnisotropy(3);
    mat->setLightingEnabled(false);
    mat->setReceiveShadows(false);
    // Create mesh
    Ogre::ManualObject* mo = App::GetGfxScene()->GetSceneManager()->createManualObject(); // TODO: Eliminate gEnv
    mo->begin(mat->getName(), Ogre::RenderOperation::OT_LINE_LIST);
    Ogre::ColourValue pos_mark_col(1.f, 0.82f, 0.26f);
    Ogre::ColourValue dir_mark_col(0.f, 1.f, 1.f); // TODO: This comes out green in simulation - why? ~ only_a_ptr, 05/2017
    const float pos_mark_len = 0.8f;
    const float dir_mark_len = 4.f;
    // X
    mo->position(pos_mark_len,0,0);
    mo->colour(pos_mark_col);
    mo->position(-pos_mark_len,0,0);
    mo->colour(pos_mark_col);
    // Y
    mo->position(0,pos_mark_len,0);
    mo->colour(pos_mark_col);
    mo->position(0,-pos_mark_len,0);
    mo->colour(pos_mark_col);
    // +Z
    mo->position(0,0,pos_mark_len);
    mo->colour(pos_mark_col);
    mo->position(0,0,0);
    mo->colour(pos_mark_col);
    // -Z = the direction
    mo->position(0,0,-dir_mark_len);
    mo->colour(dir_mark_col);
    mo->position(0,0,0);
    mo->colour(dir_mark_col);
    mo->end(); // Don't forget this!

    return mo;
}

void ActorSpawner::CreateVideoCamera(RigDef::VideoCamera* def)
{
    try
    {
        RoR::VideoCamera vcam;

        vcam.vcam_material = this->FindOrCreateCustomizedMaterial(def->material_name);
        if (vcam.vcam_material.isNull())
        {
            this->AddMessage(Message::TYPE_ERROR, "Failed to create VideoCamera with material: " + def->material_name);
            return;
        }

        switch (def->camera_role)
        {
        case -1: vcam.vcam_type = VCTYPE_VIDEOCAM;          break;
        case  0: vcam.vcam_type = VCTYPE_TRACKING_VIDEOCAM; break;
        case  1: vcam.vcam_type = VCTYPE_MIRROR;            break;
        default:
            this->AddMessage(Message::TYPE_ERROR, "VideoCamera (mat: " + def->material_name + ") has invalid 'role': " + TOSTRING(def->camera_role));
            return;
        }

        vcam.vcam_node_center = this->GetNodeIndexOrThrow(def->reference_node);
        vcam.vcam_node_dir_y  = this->GetNodeIndexOrThrow(def->bottom_node);
        vcam.vcam_node_dir_z  = this->GetNodeIndexOrThrow(def->left_node);
        vcam.vcam_pos_offset  = def->offset;

        //rotate camera picture 180 degrees, skip for mirrors
        float rotation_z = (def->camera_role != 1) ? def->rotation.z + 180 : def->rotation.z;
        vcam.vcam_rotation
            = Ogre::Quaternion(Ogre::Degree(rotation_z), Ogre::Vector3::UNIT_Z)
            * Ogre::Quaternion(Ogre::Degree(def->rotation.y), Ogre::Vector3::UNIT_Y)
            * Ogre::Quaternion(Ogre::Degree(def->rotation.x), Ogre::Vector3::UNIT_X);

        // set alternative camposition (optional)
        if (def->alt_reference_node.IsValidAnyState())
        {
            vcam.vcam_node_alt_pos = this->GetNodeIndexOrThrow(def->alt_reference_node);
        }
        else
        {
            vcam.vcam_node_alt_pos = vcam.vcam_node_center;
        }

        // set alternative lookat position (optional)
        if (def->alt_orientation_node.IsValidAnyState())
        {
            // This is a tracker camera
            vcam.vcam_type = VCTYPE_TRACKING_VIDEOCAM;
            vcam.vcam_node_lookat = this->GetNodeIndexOrThrow(def->alt_orientation_node);
        }

        // TODO: Eliminate gEnv
        vcam.vcam_ogre_camera = App::GetGfxScene()->GetSceneManager()->createCamera(vcam.vcam_material->getName() + "_camera");

        if (!App::gfx_window_videocams->getBool())
        {
            vcam.vcam_render_tex = Ogre::TextureManager::getSingleton().createManual(
                vcam.vcam_material->getName() + "_texture",
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                Ogre::TEX_TYPE_2D,
                def->texture_width,
                def->texture_height,
                0, // no mip maps
                Ogre::PF_R8G8B8,
                Ogre::TU_RENDERTARGET);
            vcam.vcam_render_target = vcam.vcam_render_tex->getBuffer()->getRenderTarget();
            vcam.vcam_render_target->setAutoUpdated(false);
        }
        else
        {
            const std::string window_name = (!def->camera_name.empty()) ? def->camera_name : def->material_name;
            vcam.vcam_render_window = App::GetAppContext()->CreateCustomRenderWindow(window_name, def->texture_width, def->texture_height);
            vcam.vcam_render_window->setAutoUpdated(false);
            vcam.vcam_render_window->setDeactivateOnFocusChange(false);

            // TODO: disable texture mirrors
        }

        vcam.vcam_ogre_camera->setNearClipDistance(def->min_clip_distance);
        vcam.vcam_ogre_camera->setFarClipDistance(def->max_clip_distance);
        vcam.vcam_ogre_camera->setFOVy(Ogre::Degree(def->field_of_view));
        const float aspect_ratio = static_cast<float>(def->texture_width) / static_cast<float>(def->texture_height);
        vcam.vcam_ogre_camera->setAspectRatio(aspect_ratio);
        vcam.vcam_material->getTechnique(0)->getPass(0)->setLightingEnabled(false);
        vcam.vcam_off_tex_name = "Chrome.dds"; // Built-in gray texture

        if (vcam.vcam_render_target)
        {
            Ogre::Viewport* vp = vcam.vcam_render_target->addViewport(vcam.vcam_ogre_camera);
            vp->setClearEveryFrame(true);
            vp->setBackgroundColour(App::GetCameraManager()->GetCamera()->getViewport()->getBackgroundColour());
            vp->setVisibilityMask(~HIDE_MIRROR);
            vp->setVisibilityMask(~DEPTHMAP_DISABLED);
            vp->setOverlaysEnabled(false);

            vcam.vcam_material->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(vcam.vcam_render_tex->getName());

            // this is a mirror, flip the image left<>right to have a mirror and not a cameraimage
            if (def->camera_role == 1)
                vcam.vcam_material->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureUScale(-1);
        }

        if (vcam.vcam_render_window)
        {
            Ogre::Viewport* vp = vcam.vcam_render_window->addViewport(vcam.vcam_ogre_camera);
            vp->setClearEveryFrame(true);
            vp->setBackgroundColour(App::GetCameraManager()->GetCamera()->getViewport()->getBackgroundColour());
            vp->setVisibilityMask(~HIDE_MIRROR);
            vp->setVisibilityMask(~DEPTHMAP_DISABLED);
            vp->setOverlaysEnabled(false);
            vcam.vcam_material->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(vcam.vcam_off_tex_name);
        }

        if (App::diag_videocameras->getBool())
        {
            Ogre::ManualObject* mo = CreateVideocameraDebugMesh(); // local helper function
            vcam.vcam_debug_node = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
            vcam.vcam_debug_node->attachObject(mo);
        }

        m_actor->m_gfx_actor->m_videocameras.push_back(vcam);
    }
    catch (std::exception & ex)
    {
        this->AddMessage(Message::TYPE_ERROR, ex.what());
    }
    catch (...)
    {
        this->AddMessage(Message::TYPE_ERROR, "An unknown exception has occured");
    }
}

void ActorSpawner::CreateMirrorPropVideoCam(
    Ogre::MaterialPtr custom_mat, CustomMaterial::MirrorPropType type, Ogre::SceneNode* prop_scenenode)
{
    static size_t mprop_counter = 0;
    try
    {
        // Prepare videocamera entry
        RoR::VideoCamera vcam;
        vcam.vcam_off_tex_name = "mirror.dds";
        vcam.vcam_prop_scenenode = prop_scenenode;
        switch (type)
        {
        case CustomMaterial::MirrorPropType::MPROP_LEFT:
            vcam.vcam_type = VCTYPE_MIRROR_PROP_LEFT;
            break;

        case CustomMaterial::MirrorPropType::MPROP_RIGHT:
            vcam.vcam_type = VCTYPE_MIRROR_PROP_RIGHT;
            break;

        default:
            this->AddMessage(Message::TYPE_ERROR, "Cannot create mirror prop of type 'MPROP_NONE'");
            return;
        }

        // Create rendering texture
        const std::string mirror_tex_name = this->ComposeName("MirrorPropTexture-", static_cast<int>(mprop_counter));
        vcam.vcam_render_tex = Ogre::TextureManager::getSingleton().createManual(mirror_tex_name
            , m_custom_resource_group
            , Ogre::TEX_TYPE_2D
            , 128
            , 256
            , 0
            , Ogre::PF_R8G8B8
            , Ogre::TU_RENDERTARGET);

        // Create OGRE camera
        vcam.vcam_ogre_camera = App::GetGfxScene()->GetSceneManager()->createCamera(this->ComposeName("MirrorPropCamera-", static_cast<int>(mprop_counter)));
        vcam.vcam_ogre_camera->setNearClipDistance(0.2f);
        vcam.vcam_ogre_camera->setFarClipDistance(App::GetCameraManager()->GetCamera()->getFarClipDistance());
        vcam.vcam_ogre_camera->setFOVy(Ogre::Degree(50));
        vcam.vcam_ogre_camera->setAspectRatio(
            (App::GetCameraManager()->GetCamera()->getViewport()->getActualWidth() / App::GetCameraManager()->GetCamera()->getViewport()->getActualHeight()) / 2.0f);

        // Setup rendering
        vcam.vcam_render_target = vcam.vcam_render_tex->getBuffer()->getRenderTarget();
        vcam.vcam_render_target->setActive(true);
        Ogre::Viewport* v = vcam.vcam_render_target->addViewport(vcam.vcam_ogre_camera);
        v->setClearEveryFrame(true);
        v->setBackgroundColour(App::GetCameraManager()->GetCamera()->getViewport()->getBackgroundColour());
        v->setOverlaysEnabled(false);

        // Setup material
        vcam.vcam_material = custom_mat;
        vcam.vcam_material->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(vcam.vcam_render_tex->getName());
        vcam.vcam_material->getTechnique(0)->getPass(0)->setLightingEnabled(false);

        // Submit the videocamera
        m_actor->m_gfx_actor->m_videocameras.push_back(vcam);
    }
    catch (std::exception & ex)
    {
        this->AddMessage(Message::TYPE_ERROR, ex.what());
    }
    catch (...)
    {
        this->AddMessage(Message::TYPE_ERROR, "An unknown exception has occured");
    }
    ++mprop_counter;
}

void ActorSpawner::HandleException()
{
    try { throw; } // Rethrow

    catch (Ogre::Exception& ogre_e)
    {
        // Add the message silently, OGRE already printed it to RoR.log
        RoR::Str<2000> txt;
        txt << "(Keyword: " << RigDef::KeywordToString(m_current_keyword)
            << ") " << ogre_e.getFullDescription();
        RoR::App::GetConsole()->putMessage(
            RoR::Console::CONSOLE_MSGTYPE_ACTOR, RoR::Console::CONSOLE_SYSTEM_ERROR, txt.ToCStr());
    }
    catch (std::exception& std_e)
    {
        this->AddMessage(Message::TYPE_ERROR, std_e.what());
    }
    catch (...)
    {
        this->AddMessage(Message::TYPE_ERROR, "An unknown exception has occurred");
    }
}

Ogre::ParticleSystem* ActorSpawner::CreateParticleSystem(std::string const & name, std::string const & template_name)
{
    // None of `Ogre::SceneManager::createParticleSystem()` overloads
    // lets us specify both resource group and template name.

    Ogre::NameValuePairList params;
    params["resourceGroup"] = m_custom_resource_group;
    params["templateName"] = template_name;

    Ogre::MovableObject* obj = App::GetGfxScene()->GetSceneManager()->createMovableObject(
       name, Ogre::ParticleSystemFactory::FACTORY_TYPE_NAME, &params);
    Ogre::ParticleSystem* psys = static_cast<Ogre::ParticleSystem*>(obj);
    psys->setVisibilityFlags(DEPTHMAP_DISABLED); // disable particles in depthmap
    return psys;
}

void ActorSpawner::CreateCabVisual()
{
    ROR_ASSERT(m_oldstyle_cab_texcoords.size() > 0);
    ROR_ASSERT(m_actor->ar_num_cabs > 0);

    //the cab materials are as follow:
    //texname: base texture with emissive(2 pass) or without emissive if none available(1 pass), alpha cutting
    //texname-trans: transparency texture (1 pass)
    //texname-back: backface texture: black+alpha cutting (1 pass)
    //texname-noem: base texture without emissive (1 pass), alpha cutting

    //material passes must be:
    //0: normal texture
    //1: transparent (windows)
    //2: emissive

    Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(m_cab_material_name);
    if (mat.isNull())
    {
        Ogre::String msg = "Material '"+m_cab_material_name+"' missing!";
        AddMessage(Message::TYPE_ERROR, msg);
        return;
    }

    //-trans
    char transmatname[256];
    static int trans_counter = 0;
    sprintf(transmatname, "%s-trans-%d", m_cab_material_name.c_str(), trans_counter++);
    Ogre::MaterialPtr transmat=mat->clone(transmatname);
    if (mat->getTechnique(0)->getNumPasses()>1) // If there's the "emissive pass", remove it from the 'transmat'
    {
        transmat->getTechnique(0)->removePass(1);
    }
    transmat->getTechnique(0)->getPass(0)->setAlphaRejectSettings(Ogre::CMPF_LESS_EQUAL, 128);
    transmat->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
    if (transmat->getTechnique(0)->getPass(0)->getNumTextureUnitStates()>0)
    {
        transmat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureFiltering(Ogre::TFO_NONE);
    }
    transmat->compile();
    m_cab_trans_material = transmat;

    //-back
    char backmatname[256];
    static int back_counter = 0;
    sprintf(backmatname, "%s-back-%d", m_cab_material_name.c_str(), back_counter++);
    Ogre::MaterialPtr backmat=mat->clone(backmatname);
    if (mat->getTechnique(0)->getNumPasses()>1)// If there's the "emissive pass", remove it from the 'transmat'
    {
        backmat->getTechnique(0)->removePass(1);
    }
    if (transmat->getTechnique(0)->getPass(0)->getNumTextureUnitStates()>0)
    {
        backmat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setColourOperationEx(
            Ogre::LBX_SOURCE1, 
            Ogre::LBS_MANUAL, 
            Ogre::LBS_MANUAL, 
            Ogre::ColourValue(0,0,0),
            Ogre::ColourValue(0,0,0)
        );
    }
    if (App::gfx_reduce_shadows->getBool())
    {
        backmat->setReceiveShadows(false);
    }
    backmat->compile();

    m_actor->GetGfxActor()->UpdateSimDataBuffer(); // fill all current nodes - needed to setup flexing meshes

    char cab_material_name_cstr[1000] = {};
    strncpy(cab_material_name_cstr, m_cab_material_name.c_str(), 999);
    std::string mesh_name = this->ComposeName("VehicleCabMesh", 0);
    FlexObj* cab_mesh =new FlexObj(
        m_actor,
        m_oldstyle_cab_texcoords,
        m_actor->ar_num_cabs,
        m_actor->ar_cabs,
        m_oldstyle_cab_submeshes,
        cab_material_name_cstr,
        mesh_name.c_str(),
        backmatname,
        transmatname
    );

    Ogre::SceneNode* cab_scene_node = App::GetGfxScene()->GetSceneManager()->getRootSceneNode()->createChildSceneNode();
    Ogre::Entity *ec = nullptr;
    try
    {
        ec = App::GetGfxScene()->GetSceneManager()->createEntity(this->ComposeName("VehicleCabEntity", 0), mesh_name);
        this->SetupNewEntity(ec, Ogre::ColourValue(0.5, 1, 0.5));
        if (ec)
        {
            cab_scene_node->attachObject(ec);
        }

        // Process "emissive cab" materials
        auto search_itor = m_material_substitutions.find(m_cab_material_name);
        m_actor->m_gfx_actor->RegisterCabMaterial(search_itor->second.material, m_cab_trans_material);
        m_actor->m_gfx_actor->SetCabLightsActive(false); // Reset emissive lights to "off" state

        m_actor->GetGfxActor()->RegisterCabMesh(ec, cab_scene_node, cab_mesh);
    }
    catch (Ogre::Exception& e)
    {
        this->AddMessage(Message::TYPE_ERROR, "error creating cab mesh: "+e.getFullDescription());
        if (ec)
        {
            App::GetGfxScene()->GetSceneManager()->destroyEntity(ec);
        }
    }
}

void ActorSpawner::CreateMaterialFlare(int flareid, Ogre::MaterialPtr m)
{
    RoR::FlareMaterial binding;
    binding.flare_index = flareid;
    binding.mat_instance = m;

    if (m.isNull())
        return;
    Ogre::Technique* tech = m->getTechnique(0);
    if (!tech)
        return;
    Ogre::Pass* p = tech->getPass(0);
    if (!p)
        return;
    // save emissive colour and then set to zero (light disabled by default)
    binding.emissive_color = p->getSelfIllumination();
    p->setSelfIllumination(Ogre::ColourValue::ZERO);

    m_actor->m_gfx_actor->m_flare_materials.push_back(binding);
}
