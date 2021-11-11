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

/// @file
/// @brief  Vehicle spawning logic.
/// @author Petr Ohlidal
/// @date   12/2013


#pragma once

#include "Application.h"
#include "RigDef_Parser.h"
#include "SimData.h"
#include "FlexFactory.h"
#include "FlexObj.h"
#include "GfxActor.h"

#include <OgreString.h>
#include <string>

namespace RoR {

class ActorSpawner
{
    friend class RoR::FlexFactory; // Needs to use `ComposeName()` and `SetupNewEntity()`

public:

    ActorSpawner() {}

    struct ActorMemoryRequirements
    {
        ActorMemoryRequirements() { memset(this,0, sizeof(ActorMemoryRequirements)); }

        size_t num_nodes;
        size_t num_beams;
        size_t num_shocks;
        size_t num_rotators;
        size_t num_wings;
        size_t num_airbrakes;
        size_t num_fixes;
        // ... more to come ...
    };

    struct Message  // TODO: remove, use console API directly
    {
        enum Type
        {
            TYPE_INFO,
            TYPE_WARNING,
            TYPE_ERROR,
            TYPE_INTERNAL_ERROR,

            TYPE_INVALID = 0xFFFFFFFF
        };
    };

    class Exception: public std::runtime_error
    {
    public:

        Exception(Ogre::String const & message):
            runtime_error(message)
        {}

    };

    void Setup(
        Actor *actor,
        std::shared_ptr<RigDef::File> file,
        Ogre::SceneNode *parent,
        Ogre::Vector3 const & spawn_position
        );

    Actor *SpawnActor();

    /**
    * Defines which configuration (sectionconfig) to use.
    */
    void SetConfig(std::string const & config) { m_selected_config = config; }

    Actor *GetActor()
    {
        return m_actor;
    }

    ActorMemoryRequirements const& GetMemoryRequirements()
    {
        return m_memory_requirements;
    }

    /**
    * Finds and clones given material. Reports errors.
    * @return NULL Ogre::MaterialPtr on error.
    */
    Ogre::MaterialPtr InstantiateManagedMaterial(Ogre::String const & source_name, Ogre::String const & clone_name);

    /**
    * Finds existing node by NodeRef_t; throws an exception if the node doesn't exist.
    * @return Index of existing node
    * @throws Exception If the node isn't found.
    */
    NodeNum_t GetNodeIndexOrThrow(RigDef::NodeRef_t const & id);

    std::string GetSubmeshGroundmodelName() { return m_state.submesh_groundmodel; }

    static void SetupDefaultSoundSources(Actor *vehicle);

    static void ComposeName(RoR::Str<100>& str, const char* type, int number, int actor_id);

private:

    struct CustomMaterial
    {
        enum class MirrorPropType
        {
            MPROP_NONE,
            MPROP_LEFT,
            MPROP_RIGHT,
        };

        CustomMaterial(){}

        CustomMaterial(Ogre::MaterialPtr& mat):
            material(mat)
        {}

        Ogre::MaterialPtr              material;
        RigDef::DataPos_t              material_flare_pos = RigDef::DATAPOS_INVALID;
        RigDef::DataPos_t              video_camera_pos = RigDef::DATAPOS_INVALID;
        MirrorPropType                 mirror_prop_type = MirrorPropType::MPROP_NONE;
        Ogre::SceneNode*               mirror_prop_scenenode = nullptr;
    };

    struct BeamVisualsTicket //!< Visuals are queued for processing using this struct
    {
        BeamVisualsTicket(int idx, float diam, const char* mtr=nullptr, bool vis=true):
            beam_index(idx), diameter(diam), material_name(mtr), visible(vis)
        {}

        int beam_index;
        std::string material_name; // TODO: how does std::string behave when parent struct is re-allocated within std::vector?  ;)
        float diameter;
        bool visible; // Some beams are spawned as hidden (ties, hooks) and displayed only when activated
    };

    struct WheelVisualsTicket //!< Wheel visuals are queued for processing using this struct
    {
        int                    wheel_index = -1;
        RigDef::Keyword        wheel_type = RigDef::KEYWORD_INVALID;
        RigDef::DataPos_t      wheel_datapos = RigDef::DATAPOS_INVALID;
        NodeNum_t              base_node_index = NODENUM_INVALID;
        NodeNum_t              axis_node_1 = NODENUM_INVALID;
        NodeNum_t              axis_node_2 = NODENUM_INVALID;
    };

    struct FlexbodyTicket
    {
        // offsets to RigDef::File data arrays
        RigDef::DataPos_t flexbodies_data_pos           = RigDef::DATAPOS_INVALID;
        RigDef::DataPos_t forset_data_pos               = RigDef::DATAPOS_INVALID;
        RigDef::DataPos_t flexbody_camera_mode_data_pos = RigDef::DATAPOS_INVALID;
    };

    // --------------------------------------
    // Processing functions (alphabetically).

    void ProcessAddAnimation(RigDef::DataPos_t pos);
    void ProcessAirbrake(RigDef::DataPos_t pos);
    void ProcessAnimator(RigDef::DataPos_t pos);
    void ProcessAntiLockBrakes(RigDef::DataPos_t pos);
    void ProcessAuthors(RigDef::DataPos_t pos);
    void ProcessAxle(RigDef::DataPos_t pos);
    void ProcessBeam(RigDef::DataPos_t pos);
    void ProcessBrakes(RigDef::DataPos_t pos);
    void ProcessCameraRail(RigDef::DataPos_t pos);
    void ProcessCamera(RigDef::DataPos_t pos);
    void ProcessCinecam(RigDef::DataPos_t pos);
    void ProcessCollisionBox(RigDef::DataPos_t pos);
    void ProcessCommand(RigDef::DataPos_t pos);
    void ProcessCommand2(RigDef::DataPos_t pos);
    void ProcessContacter(RigDef::DataPos_t pos);
    void ProcessCruiseControl(RigDef::DataPos_t pos);
    void ProcessEngine(RigDef::DataPos_t pos);
    void ProcessEngoption(RigDef::DataPos_t pos);
    void ProcessEngturbo(RigDef::DataPos_t pos);
    void ProcessExhaust(RigDef::DataPos_t pos);
    void ProcessExtCamera(RigDef::DataPos_t pos);
    void ProcessFixedNode(RigDef::DataPos_t pos);
    void ProcessFlare(RigDef::DataPos_t pos);
    void ProcessFlare2(RigDef::DataPos_t pos);
    void ProcessFlexbody(RigDef::DataPos_t pos);
    void ProcessFlexbodyCameraMode(RigDef::DataPos_t pos);
    void ProcessFlexBodyWheel(RigDef::DataPos_t pos);
    void ProcessForset(RigDef::DataPos_t pos);
    void ProcessFusedrag(RigDef::DataPos_t pos);
    void ProcessGlobals(RigDef::DataPos_t pos);
    void ProcessGuiSettings(RigDef::DataPos_t pos);
    void ProcessHelp(RigDef::DataPos_t pos);
    void ProcessHook(RigDef::DataPos_t pos);
    void ProcessHydro(RigDef::DataPos_t pos);
    void ProcessInterAxle(RigDef::DataPos_t pos);
    void ProcessLockgroup(RigDef::DataPos_t pos);
    void ProcessLockgroupDefaultNolock();
    void ProcessManagedMaterial(RigDef::DataPos_t pos);
    void ProcessMeshWheel(RigDef::DataPos_t pos);
    void ProcessMeshWheel2(RigDef::DataPos_t pos);
    void ProcessMinimass(RigDef::DataPos_t pos);
    void ProcessNode(RigDef::DataPos_t pos);
    void ProcessNode2(RigDef::DataPos_t pos);
    void ProcessParticle(RigDef::DataPos_t pos);
    void ProcessPistonprop(RigDef::DataPos_t pos);
    void ProcessProp(RigDef::DataPos_t pos);
    void ProcessRailGroup(RigDef::DataPos_t pos);
    void ProcessRopable(RigDef::DataPos_t pos);
    void ProcessRope(RigDef::DataPos_t pos);
    void ProcessRotator(RigDef::DataPos_t pos);
    void ProcessRotator2(RigDef::DataPos_t pos);
    void ProcessScrewprop(RigDef::DataPos_t pos);
    void ProcessShock(RigDef::DataPos_t pos);
    void ProcessShock2(RigDef::DataPos_t pos);
    void ProcessShock3(RigDef::DataPos_t pos);
    void ProcessSlidenode(RigDef::DataPos_t pos);
    void ProcessSlidenodeConnectInstantly();
    void ProcessSoundSource(RigDef::DataPos_t pos);
    void ProcessSoundSource2(RigDef::DataPos_t pos);
    void ProcessSpeedLimiter(RigDef::DataPos_t pos);
    void ProcessSubmesh();
    void ProcessSubmeshGroundModel(RigDef::DataPos_t pos);
    void ProcessTexcoord(RigDef::DataPos_t pos);
    void ProcessTie(RigDef::DataPos_t pos);
    void ProcessTorqueCurve(RigDef::DataPos_t pos);
    void ProcessTractionControl(RigDef::DataPos_t pos);
    void ProcessTransferCase(RigDef::DataPos_t pos);
    void ProcessTrigger(RigDef::DataPos_t pos);
    void ProcessTurbojet(RigDef::DataPos_t pos);
    void ProcessTurboprop(RigDef::DataPos_t pos);
    void ProcessTurboprop2(RigDef::DataPos_t pos);
    void ProcessWheelDetacher(RigDef::DataPos_t pos);
    void ProcessWheel(RigDef::DataPos_t pos);
    void ProcessWheel2(RigDef::DataPos_t pos);
    void ProcessWing(RigDef::DataPos_t pos);
    void ProcessNodeDefaults(RigDef::DataPos_t pos);
    void ProcessInertiaDefaults(RigDef::DataPos_t pos);
    void ProcessBeamDefaults(RigDef::DataPos_t pos);
    void ProcessBeamDefaultsScale(RigDef::DataPos_t pos);
    void ProcessCollisionRange(RigDef::DataPos_t pos);
    void ProcessManagedMatOptions(RigDef::DataPos_t pos);
    void ProcessSkeletonSettings(RigDef::DataPos_t pos);

    // --------------------------------------
    // Builder utility functions.

    // Aeroengines
    void BuildAeroEngine(
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
    );

    // Flexbodies
    void BuildFlexbody(FlexbodyTicket const& ticket);

    // Nodes
    void AddNode(RigDef::NodesCommon& def, std::string const& node_name, NodeNum_t node_number);
    void InitNode(node_t & node, Ogre::Vector3 const & position);
    void InitNode(unsigned int node_index, Ogre::Vector3 const & position);

    // Beams
    beam_t & AddBeam(node_t & node_1, node_t & node_2);
    beam_t & AddBeam(RigDef::NodeRef_t n1, RigDef::NodeRef_t n2);
    void InitBeam(beam_t & beam, node_t *node_1, node_t *node_2); //!< Sets up nodes & length of a beam.
    void CalculateBeamLength(beam_t & beam);
    void SetBeamStrength(beam_t & beam, float strength);
    void SetBeamSpring(beam_t & beam, float spring);
    void SetBeamDamping(beam_t & beam, float damping);
    void CreateBeamVisuals(beam_t const& beam, int beam_index, bool visible, std::string material_override="");

    // Wheels
    unsigned int AddWheel(RigDef::DataPos_t pos); //!< @return wheel index in rig_t::wheels array.

    // Wheels2
    unsigned int AddWheel2(RigDef::DataPos_t pos); //!< @return wheel index in rig_t::wheels array.
    unsigned int AddWheels2RimBeam(RigDef::DataPos_t pos, node_t *node_1, node_t *node_2);
    unsigned int AddWheels2TyreBeam(RigDef::DataPos_t pos, node_t *node_1, node_t *node_2);
    unsigned int AddWheels2Beam(RigDef::DataPos_t pos, node_t *node_1, node_t *node_2);

    // Flares
    void ProcessFlareCommon(RigDef::FlaresCommon& def);

    void AddCommand(RigDef::DataPos_t pos, float shorten_rate, float lenghten_rate);

    
    void ProcessBackmesh();

    RailGroup *CreateRail(std::vector<RigDef::NodeRangeCommon> & node_ranges);

    static void AddSoundSource(Actor *vehicle, SoundScriptInstance *sound_script, NodeNum_t node_index, int type = -2);

    static void AddSoundSourceInstance(Actor *vehicle, Ogre::String const & sound_script_name, int node_index, int type = -2);

    // LIMITS - to be elmiminated

    bool CheckParticleLimit(unsigned int count);
    bool CheckAxleLimit(unsigned int count);
    bool CheckCabLimit(unsigned int count);
    bool CheckCameraRailLimit(unsigned int count);
    static bool CheckSoundScriptLimit(Actor *vehicle, unsigned int count);
    bool CheckAeroEngineLimit(unsigned int count);
    bool CheckScrewpropLimit(unsigned int count);

    // **** TO BE SORTED ****

    /**
    * Seeks node.
    * @return Pointer to node, or nullptr if not found.
    */
    node_t* GetBeamNodePointer(RigDef::NodeRef_t const & node_ref);

    /**
    * Seeks node in both RigDef::File definition and rig_t generated rig.
    * @return Node index or -1 if the node was not found.
    */
    NodeNum_t FindNodeIndex(RigDef::NodeRef_t & node_ref, bool silent = false);

    /**
    * Finds wheel with given axle nodes and returns it's index.
    * @param _out_axle_wheel Index of the found wheel.
    * @return True if wheel was found, false if not.
    */
    bool AssignWheelToAxle(int & _out_axle_wheel, node_t *axis_node_1, node_t *axis_node_2);

    float ComputeWingArea(
        Ogre::Vector3 const & ref, 
        Ogre::Vector3 const & x, 
        Ogre::Vector3 const & y, 
        Ogre::Vector3 const & aref
    );

    /**
    * Adds a message to internal log.
    */
    void AddMessage(Message::Type type, Ogre::String const & text);

    void AddExhaust(
        unsigned int emitter_node_idx,
        unsigned int direction_node_idx
    );

    NodeNum_t ResolveNodeRef(RigDef::NodeRef_t const & node_ref);

    void ResolveNodeRanges(
        std::vector<NodeNum_t> & out_nodes,
        std::vector<RigDef::NodeRangeCommon> & in_ranges
    );

    /**
    * Finds existing node by NodeRef_t
    * @return First: Index of existing node; Second: true if node was found.
    */
    std::pair<NodeNum_t, bool> GetNodeIndex(RigDef::NodeRef_t const & node_ref, bool quiet = false);

    /**
    * Finds existing node by NodeRef_t
    * @return Pointer to node or nullptr if not found.
    */
    node_t* GetNodePointer(RigDef::NodeRef_t const & node_ref);

    /**
    * Finds existing node by NodeRef_t
    * @return Pointer to node
    * @throws Exception If the node isn't found.
    */
    node_t* GetNodePointerOrThrow(RigDef::NodeRef_t const & node_ref);

    /**
    * Finds existing node by NodeRef_t; throws an exception if the node doesn't exist.
    * @return Reference to existing node.
    * @throws Exception If the node isn't found.
    */
    node_t & GetNodeOrThrow(RigDef::NodeRef_t const & node_ref);

    /**
    * Finds existing pointer by Node::Id
    * @return Ref. to node.
    */
    node_t & GetNode(RigDef::NodeRef_t & node_ref)
    {
        node_t * node = GetNodePointer(node_ref);
        if (node == nullptr)
        {
            throw Exception(std::string("Failed to retrieve node from reference: ") + node_ref);
        }
        return * node;
    }

    /**
    * Finds existing node by index.
    * @return Pointer to node or nullptr if not found.
    */
    node_t & GetNode(NodeNum_t node_index);




    /**
    * Setter.
    */
    void SetCurrentKeyword(RigDef::Keyword keyword)
    {
        m_current_keyword = keyword;
    }

    beam_t & GetBeam(unsigned int index);



    /**
    * Gets a free node slot; checks limits, sets it's array position and updates 'free_node' index.
    * @return A reference to node slot.
    */
    node_t & GetFreeNode();

    /**
    * Gets a free beam slot; checks limits, sets it's array position and updates 'free_beam' index.
    * @return A reference to beam slot.
    */
    beam_t & GetFreeBeam();

    /**
    * Gets a free beam slot; Sets up defaults & position of a node.
    * @return A reference to node slot.
    */
    node_t & GetAndInitFreeNode(Ogre::Vector3 const & position);

    /**
    * Gets a free beam slot; checks limits, sets it's array position and updates 'rig_t::free_beam' index.
    * @return A reference to beam slot.
    */
    beam_t & GetAndInitFreeBeam(node_t & node_1, node_t & node_2);

    shock_t & GetFreeShock();



    beam_t *FindBeamInRig(NodeNum_t node_a, NodeNum_t node_b);

    void UpdateCollcabContacterNodes();

    wheel_t::BrakeCombo TranslateBrakingDef(RigDef::WheelBraking def);




    /**
    * Builds complete wheel visuals (sections 'wheels', 'wheels2').
    * @param rim_ratio Percentual size of the rim.
    */
    void CreateWheelVisuals(
        unsigned int wheel_index, 
        unsigned int node_base_index,
        unsigned int def_num_rays,
        Ogre::String const & rim_material_name,
        Ogre::String const & band_material_name,
        bool separate_rim,
        float rim_ratio = 1.f
    );

    void CreateWheelSkidmarks(unsigned int wheel_index);

    /**
    * Performs full material setup for a new entity.
    * RULE: Each actor must have it's own material instances (a lookup table is kept for OrigName->CustomName)
    *
    * Setup routine:
    *
    *   1. If "SimpleMaterials" (plain color surfaces denoting component type) are enabled in config file, 
    *          material is generated (not saved to lookup table) and processing ends.
    *   2. If the material name is 'mirror', it's a special prop - rear view mirror.
    *          material is generated, added to lookup table under generated name (special case) and processing ends.
    *   3. If the material is a 'videocamera' of any subtype, material is created, added to lookup table and processing ends.
    *   4  'materialflarebindngs' are resolved -> binding is persisted in lookup table.
    *   5  SkinZIP _material replacements_ are queried. If match is found, it's added to lookup table and processing ends.
    *   6. ManagedMaterials are queried. If match is found, it's added to lookup table and processing ends.
    *   7. Orig. material is cloned to create substitute.
    *   8. SkinZIP _texture replacements_ are queried. If match is found, substitute material is updated.
    *   9. Material is added to lookup table, processing ends.
    */
    void SetupNewEntity(Ogre::Entity* e, Ogre::ColourValue simple_color);

    /**
    * Factory of GfxActor; invoke after all gfx setup was done.
    */
    void FinalizeGfxSetup();

    /**
    * Validator for the rotator reference structure
    */
    void ValidateRotator(int id, int axis1, int axis2, NodeNum_t *nodes1, NodeNum_t *nodes2);

    /**
    * Helper for 'SetupNewEntity()' - see it's doc.
    */
    Ogre::MaterialPtr FindOrCreateCustomizedMaterial(std::string orig_name);

    Ogre::MaterialPtr CreateSimpleMaterial(Ogre::ColourValue color);

    Ogre::ParticleSystem* CreateParticleSystem(std::string const & name, std::string const & template_name);

    RigDef::DataPos_t FindFlareBindingForMaterial(std::string const & material_name); //!< Returns DATAPOS_INVALID if none found

    RigDef::DataPos_t FindVideoCameraByMaterial(std::string const & material_name); //!< Returns DATAPOS_INVALID if none found

    void CreateVideoCamera(RigDef::DataPos_t pos);
    void CreateMirrorPropVideoCam(Ogre::MaterialPtr custom_mat, CustomMaterial::MirrorPropType type, Ogre::SceneNode* prop_scenenode);

    /**
    * Creates name containing actor ID token, i.e. "Object_1@Actor_2"
    */
    std::string ComposeName(const char* base, int number);

    /**
    * Sets up wheel and builds nodes for sections 'wheels', 'meshwheels' and 'meshwheels2'.
    * @param wheel_width Width of the wheel (used in section 'wheels'). Use negative value to calculate width from axis beam.
    * @return Wheel index.
    */
    unsigned int BuildWheelObjectAndNodes(
        unsigned int num_rays,
        node_t *axis_node_1,
        node_t *axis_node_2,
        node_t *reference_arm_node,
        unsigned int reserve_nodes,
        unsigned int reserve_beams,
        float wheel_radius,
        RigDef::WheelPropulsion propulsion,
        RigDef::WheelBraking braking,
        float wheel_mass,
        float wheel_width = -1.f
    );

    /**
    * Adds beams to wheels from 'wheels', 'meshwheels'
    */
    void BuildWheelBeams(
        unsigned int num_rays,
        unsigned int base_node_index,
        node_t *axis_node_1,
        node_t *axis_node_2,
        float tyre_spring,
        float tyre_damping,
        float rim_spring,
        float rim_damping,
        RigDef::NodeRef_t const & rigidity_node_id,
        float max_extension = 0.f
    );

    /**
    * Creates beam for wheels 'wheels', 'meshwheels', 'meshwheels2'
    */
    unsigned int AddWheelBeam(
        node_t *node_1,
        node_t *node_2,
        float spring,
        float damping,
        float max_contraction = -1.f,
        float max_extension = -1.f,
        BeamType type = BEAM_NORMAL
    );

    /**
    * Builds wheel visuals (sections 'meshwheels', 'meshwheels2').
    */
    void BuildMeshWheelVisuals(
        unsigned int wheel_index,
        unsigned int base_node_index,
        unsigned int axis_node_1_index,
        unsigned int axis_node_2_index,
        unsigned int num_rays,
        Ogre::String mesh_name,
        Ogre::String material_name,
        float rim_radius,
        bool rim_reverse	
    );

    /**
    * From SerializedRig::wash_calculator()
    */
    void WashCalculator();

    void FetchAxisNodes(
        node_t* & axis_node_1, 
        node_t* & axis_node_2, 
        RigDef::NodeRef_t const & axis_node_1_id,
        RigDef::NodeRef_t const & axis_node_2_id
    );

    void _ProcessKeyInertia(
        RigDef::InertiaCommon & inertia, 
        RoR::CmdKeyInertia& contract_key, 
        RoR::CmdKeyInertia& extend_key
    );

    /** 
    * For specified nodes
    */
    void AdjustNodeBuoyancy(node_t & node, RigDef::NodesCommon & node_def);

    /** 
    * For generated nodes
    */
    void AdjustNodeBuoyancy(node_t & node);

    /**
    * Ported from SerializedRig::loadTruck() [v0.4.0.7]
    */
    void FinalizeRig();

    /**
    * Ported from SerializedRig::SerializedRig() [v0.4.0.7]
    */
    void InitializeRig();

    void CalcMemoryRequirements(ActorMemoryRequirements& req);

    void HandleException();

    // Input
    std::shared_ptr<RigDef::File>    m_document; //!< The parsed input file.
    Ogre::Vector3                    m_spawn_position;
    std::string                      m_selected_config;

    // Context
    struct ActorSpawnState
    {
        // GlobalsLine
        float       truckmass=0;   //!< Keyword 'globals' - dry mass
        float       loadmass=0;
        std::string texname;       //!< Keyword 'globals' - submeshes texture

        bool        wheel_contact_requested = false;
        bool        rescuer = false;
        bool        disable_default_sounds=false;
        int         detacher_group_state=DEFAULT_DETACHER_GROUP;
        bool        slope_brake=false;
        float       beam_creak=BEAM_CREAK_DEFAULT;
        int         externalcameramode=0;
        int         externalcameranode=-1;
        bool        slidenodes_connect_instantly=false;

        float       default_spring=DEFAULT_SPRING;
        float       default_spring_scale=1;
        float       default_damp=DEFAULT_DAMP;
        float       default_damp_scale=1;
        float       default_deform=BEAM_DEFORM;
        float       default_deform_scale=1;
        float       default_break=BEAM_BREAK;
        float       default_break_scale=1;

        float       default_beam_diameter=DEFAULT_BEAM_DIAMETER;
        float       default_plastic_coef=0;
        float       skeleton_beam_diameter=BEAM_SKELETON_DIAMETER;
        std::string default_beam_material = "tracks/beam";
        float       default_node_friction=NODE_FRICTION_COEF_DEFAULT;
        float       default_node_volume=NODE_VOLUME_COEF_DEFAULT;
        float       default_node_surface=NODE_SURFACE_COEF_DEFAULT;
        float       default_node_loadweight=NODE_LOADWEIGHT_DEFAULT;
        std::string default_node_options;

        bool        managedmaterials_doublesided=false;
        float       inertia_startDelay=-1;
        float       inertia_stopDelay=-1;
        std::string inertia_default_startFunction;
        std::string inertia_default_stopFunction;

        bool        enable_advanced_deformation = false;
        int         lockgroup_default = NODE_LOCKGROUP_DEFAULT;

        // Minimass
        float       global_minimass=DEFAULT_MINIMASS;   //!< 'minimass' - does not change default minimass (only updates global fallback value)!
        float       default_minimass=-1;                //!< 'set_default_minimass' - does not change global minimass!
        bool        minimass_skip_loaded = false;       //!< The 'l' flag on 'minimass'.

        // Submeshes
        std::vector<CabTexcoord>         texcoords;       //!< 'texcoords'
        int                              free_sub = 0;    //!< Counter of 'submesh' (+1) or 'backmesh' (+2)
        std::vector<CabBackmeshType>     subisback;       //!< 'backmesh'
        std::vector<int>                 subtexcoords;    //!< maps 'texcoords' to 'submesh'
        std::vector<int>                 subcabs;         //!< maps 'cab' to 'submesh'
        std::string                      submesh_groundmodel;

        // 'guisettings':
        std::string helpmat;
        std::string tachomat;
        std::string speedomat;

    };
    std::map<std::string, NodeNum_t> m_node_names;
    RigDef::DataPos_t                m_pending_flexbody = RigDef::DATAPOS_INVALID; //!< set by 'flexbody', reset by 'forset'
    FlexBody*                        m_last_flexbody = nullptr;
    std::vector<RoR::Prop>           m_props;              //!< 'props', 'prop_camera_mode'

    ActorSpawnState                  m_state;
    RigDef::Keyword                  m_current_keyword; //!< For error reports

    // Output
    Actor*             m_actor; //!< The output actor.
    
    // ***** TO BE SORTED ******

    bool                           m_apply_simple_materials;
    std::string        m_cab_material_name; //!< Original name defined in truckfile/globals.
    std::string                    m_custom_resource_group;
    float              m_wing_area;
    int                m_airplane_left_light;
    int                m_airplane_right_light;
    RoR::FlexFactory   m_flex_factory;
    Ogre::MaterialPtr  m_placeholder_managedmat;
    Ogre::SceneNode*   m_particles_parent_scenenode;
    Ogre::MaterialPtr  m_cab_trans_material;
    Ogre::MaterialPtr  m_simple_material_base;
    RoR::Renderdash*   m_oldstyle_renderdash;
    float              m_fuse_z_min;
    float              m_fuse_z_max;
    float              m_fuse_y_min;
    float              m_fuse_y_max;
    bool               m_generate_wing_position_lights;
    int                m_first_wing_index;
    Ogre::SceneNode*   m_curr_mirror_prop_scenenode;
    
    int                       m_driverseat_prop_index;
    ActorMemoryRequirements   m_memory_requirements;
    std::vector<RoR::NodeGfx> m_gfx_nodes;
    CustomMaterial::MirrorPropType         m_curr_mirror_prop_type;
    
    std::map<std::string, CustomMaterial>  m_material_substitutions; //!< Maps original material names (shared) to their actor-specific substitutes; There's 1 substitute per 1 material, regardless of user count.
    std::vector<BeamVisualsTicket>         m_beam_visuals_queue; //!< We want to spawn visuals asynchronously in the future
    std::vector<WheelVisualsTicket>        m_wheel_visuals_queue; //!< We want to spawn visuals asynchronously in the future
    std::vector<FlexbodyTicket>            m_flexbody_queue; //!< We want to process this asynchronously in the future
    std::map<std::string, Ogre::MaterialPtr>  m_managed_materials;
};

} // namespace RoR
