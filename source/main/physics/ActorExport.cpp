/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2024 Petr Ohlidal

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

#include "Actor.h"
#include "Application.h"
#include "CacheSystem.h"
#include "GameContext.h"
#include "RigDef_File.h"

#include <Ogre.h>

using namespace RoR;
using namespace RigDef;

static RigDef::Node::Ref BuildNodeRef(Actor* actor, NodeNum_t n)
{
    if (n == NODENUM_INVALID)
    {
        return RigDef::Node::Ref();
    }

    const BitMask_t nodeflags = RigDef::Node::Ref::REGULAR_STATE_IS_VALID;
    if (actor->ar_nodes_name[n] != "")
    {
        return RigDef::Node::Ref(actor->ar_nodes_name[n], 0, nodeflags, 0);
    }
    else
    {
        return RigDef::Node::Ref(fmt::format("{}", actor->ar_nodes_id[n]), actor->ar_nodes_id[n], nodeflags, 0);
    }
}

static void UpdateSetBeamDefaults(std::shared_ptr<BeamDefaults>& beam_defaults, Actor* actor, int i)
{
    float b_spring = actor->ar_beams[i].k;
    float b_damp = actor->ar_beams[i].d;
    float b_deform = actor->ar_beams[i].default_beam_deform;
    float b_break = actor->ar_beams[i].strength;
    if (beam_defaults->springiness != b_spring
        || beam_defaults->damping_constant != b_damp
        || beam_defaults->deformation_threshold != b_deform
        || beam_defaults->breaking_threshold != b_break)
    {
        beam_defaults = std::shared_ptr<BeamDefaults>(new BeamDefaults);
        beam_defaults->springiness = b_spring;
        beam_defaults->damping_constant = b_damp;
        beam_defaults->deformation_threshold = b_deform;
        beam_defaults->breaking_threshold = b_break;
    }
}

static void UpdateSetNodeDefaults(std::shared_ptr<NodeDefaults>& node_defaults, Actor* actor, NodeNum_t i)
{
    float n_loadweight = actor->ar_nodes_default_loadweights[i];
    float n_friction = actor->ar_nodes[i].friction_coef;
    float n_volume = actor->ar_nodes[i].volume_coef;
    float n_surface = actor->ar_nodes[i].surface_coef;
    if (node_defaults->load_weight != n_loadweight
        || node_defaults->friction != n_friction
        || node_defaults->surface != n_surface
        || node_defaults->volume != n_volume)
    {
        node_defaults = std::shared_ptr<NodeDefaults>(new NodeDefaults);
        node_defaults->load_weight = n_loadweight;
        node_defaults->friction = n_friction;
        node_defaults->surface = n_surface;
        node_defaults->volume = n_volume;
    }
}

void Actor::propagateNodeBeamChangesToDef()
{
    // PROOF OF CONCEPT:
    // * assumes 'section/end_section' is not used (=only root module exists)
    // * uses dummy 'set_beam_defaults' and 'detacher_group' for the hookbeams (nodes with 'h' option)
    // * doesn't separate out 'set_beam_defaults_scale' from 'set_beam_defaults'
    // * uses only 'set_default_minimass', ignores global 'minimass'
    // * ignores 'detacher_group' for the hookbeams (nodes with 'h' option)
    // -------------------------------------------------------------------------

    

    // Purge old data
    m_used_actor_entry->actor_def->root_module->nodes.clear();
    m_used_actor_entry->actor_def->root_module->beams.clear();
    m_used_actor_entry->actor_def->root_module->cinecam.clear();
    m_used_actor_entry->actor_def->root_module->wheels.clear();
    m_used_actor_entry->actor_def->root_module->wheels2.clear();
    m_used_actor_entry->actor_def->root_module->meshwheels.clear();
    m_used_actor_entry->actor_def->root_module->meshwheels2.clear();
    m_used_actor_entry->actor_def->root_module->flexbodywheels.clear();

    // Prepare 'set_node_defaults' with builtin values.
    auto node_defaults = std::shared_ptr<NodeDefaults>(new NodeDefaults); // comes pre-filled
    
    // Prepare 'set_default_minimass' with builtin values.
    auto default_minimass = std::shared_ptr<DefaultMinimass>(new DefaultMinimass());
    default_minimass->min_mass_Kg = DEFAULT_MINIMASS;

    // Prepare 'set_beam_defaults' with builtin values.
    auto beam_defaults = std::shared_ptr<BeamDefaults>(new BeamDefaults);
    beam_defaults->springiness           = DEFAULT_SPRING;
    beam_defaults->damping_constant      = DEFAULT_DAMP;
    beam_defaults->deformation_threshold = BEAM_DEFORM;
    beam_defaults->breaking_threshold    = BEAM_BREAK;
    beam_defaults->visual_beam_diameter  = DEFAULT_BEAM_DIAMETER;

    // Prepare 'detacher_group' with builtin values.
    int detacher_group = DEFAULT_DETACHER_GROUP;

    // When converting node from calculated-mass to override-mass, reduce dry mass accordingly.
    float dry_mass_reduction = 0.0f;

    // ~~~ Nodes ~~~

    Ogre::Vector3 pivot = this->getRotationCenter();
    pivot.y = App::GetGameContext()->GetTerrain()->GetHeightAt(pivot.x, pivot.z);
    for (NodeNum_t i = 0; i < ar_num_nodes; i++)
    {
        if (ar_nodes[i].nd_rim_node || ar_nodes[i].nd_tyre_node || ar_nodes[i].nd_cinecam_node)
        {
            // Skip wheel nodes and cinecam nodes
            continue;
        }

        UpdateSetNodeDefaults(node_defaults, this, i);

        // Check if 'set_default_minimass' needs update.
        if (default_minimass->min_mass_Kg != ar_minimass[i])
        {
            default_minimass = std::shared_ptr<DefaultMinimass>(new DefaultMinimass());
            default_minimass->min_mass_Kg = ar_minimass[i];
        }

        // Build the node
        RigDef::Node node;
        node.node_defaults = node_defaults;
        node.default_minimass = default_minimass;
        node.beam_defaults = beam_defaults; // Needed for hookbeams (nodes with 'h' option)
        node.detacher_group = detacher_group; // Needed for hookbeams (nodes with 'h' option)
        if (ar_nodes_name[i] != "")
        {
            node.id = RigDef::Node::Id(ar_nodes_name[i]);
        }
        else
        {
            node.id = RigDef::Node::Id(ar_nodes_id[i]);
        }
        node.position = ar_initial_node_positions[i] - pivot;
        node.load_weight_override = ar_nodes_override_loadweights[i];
        node._has_load_weight_override = (node.load_weight_override >= 0.0f);
        node.options = ar_nodes_options[i];

        if (ar_nb_export_override_all_node_masses)
        {
            // When converting node from calculated-mass to override-mass, reduce dry mass accordingly.
            if (!node._has_load_weight_override)
            {
                dry_mass_reduction += ar_nodes[i].mass;
            }

            // Enforce the 'total mass' slider value by overriding masses of all nodes
            node.load_weight_override = ar_nodes[i].mass;
            node._has_load_weight_override = true;
            node.options |= RigDef::Node::OPTION_l_LOAD_WEIGHT;
        }

        // Submit the node
        m_used_actor_entry->actor_def->root_module->nodes.push_back(node);
    }

    // ~~~ Beams ~~~

    for (int i = 0; i < ar_num_beams; i++)
    {
        if (!ar_beams_user_defined[i])
        {
            // Skip everything not from 'beams' (wheels/cinecam/hooknode/wings/rotators etc...)
            continue;
        }

        UpdateSetBeamDefaults(beam_defaults, this, i);

        // Check if 'detacher_group' needs update.
        if (detacher_group != ar_beams[i].detacher_group)
        {
            detacher_group = ar_beams[i].detacher_group;
        }

        // Build the beam
        RigDef::Beam beam;
        beam.defaults = beam_defaults;
        beam.detacher_group = detacher_group;
        beam.extension_break_limit = ar_beams[i].longbound;
        
        if (ar_beams[i].bounded == SUPPORTBEAM)
        {
            beam._has_extension_break_limit = true;
            beam.options |= RigDef::Beam::OPTION_s_SUPPORT;
        }
        else if (ar_beams[i].bounded == ROPE)
        {
            beam.options |= RigDef::Beam::OPTION_r_ROPE;
        }
        
        if (ar_beams_invisible[i])
        {
            beam.options |= RigDef::Beam::OPTION_i_INVISIBLE;
        }

        // Build refs to the nodes
        beam.nodes[0] = BuildNodeRef(this, ar_beams[i].p1->pos);
        beam.nodes[1] = BuildNodeRef(this, ar_beams[i].p2->pos);

        // Submit the beam
        m_used_actor_entry->actor_def->root_module->beams.push_back(beam);
    }

    // ~~~ Cinecam ~~~

    for (NodeNum_t i = 0; i < ar_num_nodes; i++)
    {
        if (!ar_nodes[i].nd_cinecam_node)
        {
            // Skip everything but cinecam nodes
            continue;
        }

        UpdateSetNodeDefaults(node_defaults, this, i);

        // Find all beams that connect to the cinecam node
        std::array<int, 8> cinecam_nodes = { -1, -1, -1, -1, -1, -1, -1, -1 };
        int num_cinecam_nodes = 0;
        int cinecam_beamid = -1;
        for (int j = 0; j < ar_num_beams; j++)
        {
            if (ar_beams[j].p1->pos == i)
            {
                cinecam_beamid = j;
                cinecam_nodes[num_cinecam_nodes++] = ar_beams[j].p2->pos;
            }
            else if (ar_beams[j].p2->pos == i)
            {
                cinecam_beamid = j;
                cinecam_nodes[num_cinecam_nodes++] = ar_beams[j].p1->pos;
            }
        }
        ROR_ASSERT(num_cinecam_nodes == 8);
        ROR_ASSERT(cinecam_beamid != -1);

        UpdateSetBeamDefaults(beam_defaults, this, cinecam_beamid);

        // Build the cinecam (NOTE: no minimass/detacher_group)
        RigDef::Cinecam cinecam;
        cinecam.node_defaults = node_defaults;
        cinecam.beam_defaults = beam_defaults;
        cinecam.position = ar_initial_node_positions[i] - pivot;
        for (int n = 0; n < 8; n++)
        {
            cinecam.nodes[n] = BuildNodeRef(this, cinecam_nodes[n]);
        }
        cinecam.node_mass = ar_nodes[i].mass;
        cinecam.spring = ar_beams[cinecam_beamid].k;
        cinecam.damping = ar_beams[cinecam_beamid].d;

        // Submit the cinecam
        m_used_actor_entry->actor_def->root_module->cinecam.push_back(cinecam);
    }

    // ~~~ Wheels ~~~
    for (int i = 0; i < ar_num_wheels; i++)
    {
        // PLEASE maintain the same order of arguments as the docs: https://docs.rigsofrods.org/vehicle-creation/fileformat-truck/#vehicle-specific

        switch (ar_wheels[i].wh_arg_keyword)
        {
        case Keyword::WHEELS:
        {
            RigDef::Wheel wheel;
            wheel.beam_defaults = beam_defaults;
            wheel.node_defaults = node_defaults;
            // radius
            wheel.radius = ar_wheels[i].wh_radius;
            // rays
            wheel.num_rays = ar_wheels[i].wh_arg_num_rays;
            // nodes
            wheel.nodes[0] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_0->pos);
            wheel.nodes[1] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_1->pos);
            wheel.rigidity_node = BuildNodeRef(this, ar_wheels[i].wh_arg_rigidity_node);
            // braking, propulsion
            wheel.braking = ar_wheels[i].wh_braking;
            wheel.propulsion = ar_wheels[i].wh_propulsed;
            // arm node
            wheel.reference_arm_node = BuildNodeRef(this, (ar_wheels[i].wh_arm_node ? ar_wheels[i].wh_arm_node->pos : NODENUM_INVALID));
            // mass
            wheel.mass = ar_wheels[i].wh_mass;
            // springiness, damping
            wheel.springiness = ar_wheels[i].wh_arg_simple_spring;
            wheel.damping = ar_wheels[i].wh_arg_simple_damping;
            // materials
            wheel.face_material_name = ar_wheels[i].wh_arg_media1;
            wheel.band_material_name = ar_wheels[i].wh_arg_media2;

            m_used_actor_entry->actor_def->root_module->wheels.push_back(wheel);
            break;
        }
        case Keyword::WHEELS2:
        {
            RigDef::Wheel2 wheel;
            wheel.beam_defaults = beam_defaults;
            wheel.node_defaults = node_defaults;
            // radius
            wheel.rim_radius = ar_wheels[i].wh_rim_radius;
            wheel.tyre_radius = ar_wheels[i].wh_radius;
            // rays
            wheel.num_rays = ar_wheels[i].wh_arg_num_rays;
            // nodes
            wheel.nodes[0] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_0->pos);
            wheel.nodes[1] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_1->pos);
            wheel.rigidity_node = BuildNodeRef(this, ar_wheels[i].wh_arg_rigidity_node);
            // braking, propulsion
            wheel.braking = ar_wheels[i].wh_braking;
            wheel.propulsion = ar_wheels[i].wh_propulsed;
            // arm node
            wheel.reference_arm_node = BuildNodeRef(this, (ar_wheels[i].wh_arm_node ? ar_wheels[i].wh_arm_node->pos : NODENUM_INVALID));
            // mass
            wheel.mass = ar_wheels[i].wh_mass;
            // springiness, damping
            wheel.rim_springiness = ar_wheels[i].wh_arg_rim_spring;
            wheel.rim_damping = ar_wheels[i].wh_arg_rim_damping;
            wheel.tyre_springiness = ar_wheels[i].wh_arg_simple_spring;
            wheel.tyre_damping = ar_wheels[i].wh_arg_simple_damping;
            // materials
            wheel.face_material_name = ar_wheels[i].wh_arg_media1;
            wheel.band_material_name = ar_wheels[i].wh_arg_media2;

            m_used_actor_entry->actor_def->root_module->wheels2.push_back(wheel);
            break;
        }
        case Keyword::MESHWHEELS:
        {
            RigDef::MeshWheel wheel;
            wheel.beam_defaults = beam_defaults;
            wheel.node_defaults = node_defaults;
            // radius
            wheel.rim_radius = ar_wheels[i].wh_rim_radius;
            wheel.tyre_radius = ar_wheels[i].wh_radius;
            // rays
            wheel.num_rays = ar_wheels[i].wh_arg_num_rays;
            // nodes
            wheel.nodes[0] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_0->pos);
            wheel.nodes[1] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_1->pos);
            wheel.rigidity_node = BuildNodeRef(this, ar_wheels[i].wh_arg_rigidity_node);
            // braking, propulsion
            wheel.braking = ar_wheels[i].wh_braking;
            wheel.propulsion = ar_wheels[i].wh_propulsed;
            // arm node
            wheel.reference_arm_node = BuildNodeRef(this, (ar_wheels[i].wh_arm_node ? ar_wheels[i].wh_arm_node->pos : NODENUM_INVALID));
            // mass
            wheel.mass = ar_wheels[i].wh_mass;
            // springiness, damping
            wheel.spring = ar_wheels[i].wh_arg_simple_spring;
            wheel.damping = ar_wheels[i].wh_arg_simple_damping;
            // media
            wheel.side = ar_wheels[i].wh_arg_side;
            wheel.mesh_name = ar_wheels[i].wh_arg_media1;
            wheel.material_name = ar_wheels[i].wh_arg_media2;

            m_used_actor_entry->actor_def->root_module->meshwheels.push_back(wheel);
            break;
        }
        case Keyword::MESHWHEELS2:
        {
            // Update 'set_beam_defaults' (these define rim params)
            beam_defaults = std::shared_ptr<BeamDefaults>(new BeamDefaults);
            beam_defaults->springiness = ar_wheels[i].wh_arg_rim_spring;
            beam_defaults->damping_constant = ar_wheels[i].wh_arg_rim_damping;
            beam_defaults->deformation_threshold = -1;
            beam_defaults->breaking_threshold = -1;

            RigDef::MeshWheel2 wheel;
            wheel.beam_defaults = beam_defaults;
            wheel.node_defaults = node_defaults;
            // radius
            wheel.rim_radius = ar_wheels[i].wh_rim_radius;
            wheel.tyre_radius = ar_wheels[i].wh_radius;
            // rays
            wheel.num_rays = ar_wheels[i].wh_arg_num_rays;
            // nodes
            wheel.nodes[0] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_0->pos);
            wheel.nodes[1] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_1->pos);
            wheel.rigidity_node = BuildNodeRef(this, ar_wheels[i].wh_arg_rigidity_node);
            // braking, propulsion
            wheel.braking = ar_wheels[i].wh_braking;
            wheel.propulsion = ar_wheels[i].wh_propulsed;
            // arm node
            wheel.reference_arm_node = BuildNodeRef(this, (ar_wheels[i].wh_arm_node ? ar_wheels[i].wh_arm_node->pos : NODENUM_INVALID));
            // mass
            wheel.mass = ar_wheels[i].wh_mass;
            // springiness, damping
            wheel.spring = ar_wheels[i].wh_arg_simple_spring;
            wheel.damping = ar_wheels[i].wh_arg_simple_damping;
            // media
            wheel.side = ar_wheels[i].wh_arg_side;
            wheel.mesh_name = ar_wheels[i].wh_arg_media1;
            wheel.material_name = ar_wheels[i].wh_arg_media2;

            m_used_actor_entry->actor_def->root_module->meshwheels2.push_back(wheel);
            break;
        }
        case Keyword::FLEXBODYWHEELS:
        {
            // Update 'set_beam_defaults' (these define rim params)
            beam_defaults = std::shared_ptr<BeamDefaults>(new BeamDefaults);
            beam_defaults->springiness = ar_wheels[i].wh_arg_rim_spring;
            beam_defaults->damping_constant = ar_wheels[i].wh_arg_rim_damping;
            beam_defaults->deformation_threshold = -1;
            beam_defaults->breaking_threshold = -1;

            RigDef::FlexBodyWheel wheel;
            wheel.beam_defaults = beam_defaults;
            wheel.node_defaults = node_defaults;
            // radius
            wheel.rim_radius = ar_wheels[i].wh_rim_radius;
            wheel.tyre_radius = ar_wheels[i].wh_radius;
            // rays
            wheel.num_rays = ar_wheels[i].wh_arg_num_rays;
            // nodes
            wheel.nodes[0] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_0->pos);
            wheel.nodes[1] = BuildNodeRef(this, ar_wheels[i].wh_axis_node_1->pos);
            wheel.rigidity_node = BuildNodeRef(this, ar_wheels[i].wh_arg_rigidity_node);
            // braking, propulsion
            wheel.braking = ar_wheels[i].wh_braking;
            wheel.propulsion = ar_wheels[i].wh_propulsed;
            // arm node
            wheel.reference_arm_node = BuildNodeRef(this, (ar_wheels[i].wh_arm_node ? ar_wheels[i].wh_arm_node->pos : NODENUM_INVALID));
            // mass
            wheel.mass = ar_wheels[i].wh_mass;
            // springiness, damping
            wheel.rim_springiness = ar_wheels[i].wh_arg_simple_spring;
            wheel.rim_damping = ar_wheels[i].wh_arg_simple_damping;
            // media
            wheel.side = ar_wheels[i].wh_arg_side;
            wheel.rim_mesh_name = ar_wheels[i].wh_arg_media1;
            wheel.tyre_mesh_name = ar_wheels[i].wh_arg_media2;

            m_used_actor_entry->actor_def->root_module->flexbodywheels.push_back(wheel);
            break;
        }
        default:
            ROR_ASSERT(false);
            break;
        }
    }

    // ~~~ Globals (update in-place) ~~~

    m_used_actor_entry->actor_def->root_module->globals[0].dry_mass -= dry_mass_reduction;
}