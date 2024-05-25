/*
    This source file is part of Rigs of Rods
    Copyright 2009 Lefteris Stamatogiannakis

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

#pragma once

#include "Application.h"

namespace RoR {

/// @addtogroup Physics
/// @{

/// @addtogroup Collisions
/// @{

/// Detects collisions between points (nodes) and triangles (collision cabs).
/// It has two modes of operation: IntraPoint (actor's self-collision) and InterPoint (collision between actors).
/// How it operates:
///  1. Buffering of points: all contactable nodes from all collision partners are buffered into `contactable_point_pool`.
///  1. Finding collision partners (actors whose nodes will colide with `m_actor`)
///     Done by `update_structures_for_contacters()`, invoked from either `UpdateIntraPoint()` or `UpdateInterPoint()`.
///  2. Broadphase collision detection: collcabs of partners are checked against contactable nodes. 
///     Done by `query()`, invoked from either `RoR::ResolveInterActorCollisions()` or `RoR::ResolveIntraActorCollisions()`, see file DynamicCollisions.cpp
///  3. Narrowphase collision detection: for each hit, the exact point-triangle collision is checked.
///     Done by `RoR::InsideTriangleTest()`, invoked from either `RoR::ResolveInterActorCollisions()` or `RoR::ResolveIntraActorCollisions()`, see file DynamicCollisions.cpp
///  4. Collision force calculation: for each triangle collision, resulting forces for the colliding node and cab-triangle nodes are calculated.
///     Done by 'RoR::ResolveCollisionForces()`, invoked from either `RoR::ResolveInterActorCollisions()` or `RoR::ResolveIntraActorCollisions()`, see file DynamicCollisions.cpp
class PointColDetector
{
public:

    /// Buffered contactable node from a collision partner; use ColPointID_t for indexing
    struct ColPoint
    {
        NodeNum_t nodenum = NODENUM_INVALID;
        Ogre::Vector3 nodepos = Ogre::Vector3::ZERO; // Cached node AbsPosition
    };

    struct ColActor
    {
        ColActor(Actor* _actor): actor(_actor) {};
        bool operator==(const ColActor& other) const { return actor == other.actor; }
        Actor* actor = nullptr;
        int point_pool_start = -1;
        int point_pool_count = -1;
    };

    std::vector<NodeNum_t> hit_list;
    std::vector<ColActor>    collision_partners; //!< Actors whose contactable nodes will collide; IntraPoint: always just owning actor; InterPoint: all colliding actors
    std::vector<ColPoint> contactable_point_pool;
    PointColDetector(ActorPtr actor): m_actor(actor) {};
    bool QueryCollisionsWithSinglePartner(const Ogre::AxisAlignedBox collcab_aabb, const ColActor& partner); //!< Fills `hit_list`; returns true if there are any collisions
    bool QueryCollisionsWithAllPartners(const Ogre::Vector3& vec1, const Ogre::Vector3& vec2, const Ogre::Vector3& vec3, const float enlargeBB); //!< Only returns true if there are any collisions, doesn't record hits.

protected:

    ActorPtr               m_actor; //!< The actor whose collcabs will collide
    int                    m_object_list_size = -1;

    void update_structures_for_contacters(bool ignoreinternal);
    void refresh_node_positions();
};

class IntraPointColDetector : public PointColDetector
{
public:
    IntraPointColDetector(ActorPtr actor): PointColDetector(actor) {};
    void UpdateIntraPoint(bool contactables = false);
};

class InterPointColDetector : public PointColDetector
{
public:
    InterPointColDetector(ActorPtr actor): PointColDetector(actor) {};
    void UpdateInterPoint(bool ignorestate = false);
};

/// @} // addtogroup Collisions
/// @} // addtogroup Physics

} // namespace RoR
