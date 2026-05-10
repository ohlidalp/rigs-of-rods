/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2016      Fabian Killus

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

#include "DynamicCollisions.h"

#include "Application.h"
#include "Actor.h"
#include "SimData.h"
#include "CartesianToTriangleTransform.h"
#include "Collisions.h"
#include "PointColDetector.h"
#include "Triangle.h"

using namespace Ogre;
using namespace RoR;

/// Determine on which side of a triangle an occuring collision takes place.
/**
 * The frontface of the triangle is the side towards which the surface normal is pointing.
 * The backface is the opposite side.
 *
 * \note This implementation is a heuristic, not an accurate calculation.
 *
 * @param distance      Signed shortest distance from collision point to triangle plane.
 * @param normal        Surface normal of the triangle.
 * @param surface_point Arbitrary point within the triangle plane.
 * @param neighbour_node_ids Indices of neighbouring nodes connected to the colliding node.
 * @param nodes         
 */
static bool BackfaceCollisionTest(
        // The local actor:
        const Actor* surface_actor,
        const NodeNum_t surface_point,
        // The foreign actor:
        const Actor* hit_actor,
        const NodeNum_t hit_node,
        // Parameters:
        const float distance,
        const Vector3 normal
        )
{
    auto sign = [](float x){ return (x >= 0) ? 1 : -1; };

    // Summarize over the collision node and its connected neighbour nodes to infer on which side
    // of the collision plane most of these nodes are located.
    // Nodes in front contribute positively, nodes on the backface contribute negatively.

    // the contribution of the collision node itself has triple weight in this heuristic
    const int weight = 3;
    int face_indicator = weight * sign(distance);

    // calculate the contribution of neighbouring nodes (if it can still change the final outcome)
    if (hit_actor->ar_node_to_node_connections[hit_node].size() > weight)
    {
        for (NodeNum_t id : hit_actor->ar_node_to_node_connections[hit_node])
        {
            const auto neighbour_distance = normal.dotProduct(hit_actor->ar_nodes[id].AbsPosition - surface_actor->ar_nodes[surface_point].AbsPosition);
            face_indicator += sign(neighbour_distance);
        }
    }

    // a negative sum indicates that the collision is occuring on the backface
    return (face_indicator < 0);
}


/// Test if a point given in triangle local coordinates lies within the triangle itself.
/**
 * A point (within in the triangle plane) is located inside the triangle if its barycentric coordinates
 * \f$\alpha, \beta, \gamma\f$ are all positive. The margin additionally defines the maximum distance
 * from the triangle plane within which a three-dimensional point is still considered to be close
 * enough to be inside the triangle.
 *
 * @param local Point in triangle local coordinates.
 * @param margin Range within which a point is considered to be close enough to the triangle plane.
 */
static bool InsideTriangleTest(const CartesianToTriangleTransform::TriangleCoord &local, const float margin)
{
    const auto coord    = local.barycentric;
    const auto distance = local.distance;
    return (coord.alpha >= 0) && (coord.beta >= 0) && (coord.gamma >= 0) && (std::abs(distance) <= margin);
}


/// Calculate collision forces and apply them to the collision node and the three vertex nodes of the collision triangle.
void ResolveCollisionForces(
        // Local actor:
        Actor* const local_actor,
        const NodeNum_t na,
        const NodeNum_t nb,
        const NodeNum_t no,
        // Foreign actor:
        Actor* const hit_actor,
        const NodeNum_t hit_node,
        // Params:
        const float penetration_depth,
        const float alpha, const float beta, const float gamma,
        const Vector3 &normal,
        const float dt,
        const bool remote,
        ground_model_t &submesh_ground_model)
{
    //OLD: const auto velocity = hitnode.Velocity - (na.Velocity * alpha + nb.Velocity * beta + no.Velocity * gamma);
    const Ogre::Vector3 velocity
        = hit_actor->ar_nodes[hit_node].Velocity - (
            local_actor->ar_nodes[na].Velocity * alpha,
            local_actor->ar_nodes[nb].Velocity * beta,
            local_actor->ar_nodes[no].Velocity * gamma);

    //OLD: const float tr_mass = na.mass * alpha + nb.mass * beta + no.mass * gamma;
    const float tr_mass
        = local_actor->ar_nodes[na].mass * alpha
        + local_actor->ar_nodes[nb].mass * beta
        + local_actor->ar_nodes[no].mass * gamma;

    const float hitnode_mass = hit_actor->ar_nodes[hit_node].mass;
    const float mass = remote ? hitnode_mass : (hitnode_mass * tr_mass) / (hitnode_mass + tr_mass);

    const Ogre::Vector3 forcevec = primitiveCollision(hit_actor, hit_node, velocity, mass, normal, dt, &submesh_ground_model, penetration_depth);

    hit_actor->ar_nodes[hit_node].Forces += forcevec;
    local_actor->ar_nodes[na].Forces -= forcevec * alpha;
    local_actor->ar_nodes[nb].Forces -= forcevec * beta;
    local_actor->ar_nodes[no].Forces -= forcevec * gamma;
}


void RoR::ResolveInterActorCollisions(Actor* const actor, PointColDetector &interPointCD,
        const int free_collcab, int collcabs[], int cabs[],
        collcab_rate_t inter_collcabrate[],
        const float collrange,
        ground_model_t &submesh_ground_model)
{
    for (int i=0; i<free_collcab; i++)
    {
        if (inter_collcabrate[i].rate > 0)
        {
            inter_collcabrate[i].distance++;
            inter_collcabrate[i].rate--;
            continue;
        }
        inter_collcabrate[i].rate = std::min(inter_collcabrate[i].distance, 12);
        inter_collcabrate[i].distance = 0;

        int tmpv = collcabs[i]*3;
        const NodeNum_t no = static_cast<NodeNum_t>(cabs[tmpv]);
        const NodeNum_t na = static_cast<NodeNum_t>(cabs[tmpv+1]);
        const NodeNum_t nb = static_cast<NodeNum_t>(cabs[tmpv+2]);
        const Ogre::Vector3 no_AbsPosition = actor->ar_nodes[no].AbsPosition;
        const Ogre::Vector3 na_AbsPosition = actor->ar_nodes[na].AbsPosition;
        const Ogre::Vector3 nb_AbsPosition = actor->ar_nodes[nb].AbsPosition;

        interPointCD.query(
                no_AbsPosition,
                na_AbsPosition,
                nb_AbsPosition, collrange);

        if (!interPointCD.hit_list.empty())
        {
            // setup transformation of points to triangle local coordinates
            const Triangle triangle(na_AbsPosition, nb_AbsPosition, no_AbsPosition);
            const CartesianToTriangleTransform transform(triangle);

            for (auto h : interPointCD.hit_list)
            {
                Actor* hit_actor = h->actor;
                const NodeNum_t hit_node = h->node_id;

                // transform point to triangle local coordinates
                const auto local_point = transform(hit_actor->ar_nodes[hit_node].AbsPosition);

                // collision test
                const bool is_colliding = InsideTriangleTest(local_point, collrange);
                if (is_colliding)
                {
                    inter_collcabrate[i].rate = 0;

                    const auto coord = local_point.barycentric;
                    auto distance   = local_point.distance;
                    auto normal     = triangle.normal();

                    // adapt in case the collision is occuring on the backface of the triangle
                    const auto& neighbour_node_ids = hit_actor->ar_node_to_node_connections[h->node_id];
                    const bool is_backface = BackfaceCollisionTest(
                        actor, no, // Local actor
                        hit_actor, hit_node, // Foreign actor
                        distance, normal); // Params

                    if (is_backface)
                    {
                        // flip surface normal and distance to triangle plane
                        normal   = -normal;
                        distance = -distance;
                    }

                    const auto penetration_depth = collrange - distance;

                    const bool remote = (hit_actor->ar_state == ActorState::NETWORKED_OK);

                    ResolveCollisionForces(
                        actor, na, nb, no,  // Local actor - cab triangle
                        hit_actor, hit_node, // Foreign actor - colliding node
                        penetration_depth,
                        coord.alpha, coord.beta, coord.gamma,
                        normal, PHYSICS_DT, remote, submesh_ground_model);

                    hit_actor->ar_nodes[hit_node].nd_last_collision_gm = &submesh_ground_model;
                    hit_actor->ar_nodes[hit_node].nd_has_mesh_contact = true;

                    actor->ar_nodes[na].nd_has_mesh_contact = true;
                    actor->ar_nodes[nb].nd_has_mesh_contact = true;
                    actor->ar_nodes[no].nd_has_mesh_contact = true;
                    
                }
            }
        }
        else
        {
            inter_collcabrate[i].rate++;
        }
    }
}


void RoR::ResolveIntraActorCollisions(Actor* const actor, PointColDetector &intraPointCD,
        const int free_collcab, int collcabs[], int cabs[],
        collcab_rate_t intra_collcabrate[],
        const float collrange,
        ground_model_t &submesh_ground_model)
{
    for (int i=0; i<free_collcab; i++)
    {
        if (intra_collcabrate[i].rate > 0)
        {
            intra_collcabrate[i].distance++;
            intra_collcabrate[i].rate--;
            continue;
        }
        if (intra_collcabrate[i].distance > 0)
        {
            intra_collcabrate[i].rate = std::min(intra_collcabrate[i].distance, 12);
            intra_collcabrate[i].distance = 0;
        }

        int tmpv = collcabs[i]*3;
        const NodeNum_t no = static_cast<NodeNum_t>(cabs[tmpv]);
        const NodeNum_t na = static_cast<NodeNum_t>(cabs[tmpv+1]);
        const NodeNum_t nb = static_cast<NodeNum_t>(cabs[tmpv+2]);
        const Ogre::Vector3 no_AbsPosition = actor->ar_nodes[no].AbsPosition;
        const Ogre::Vector3 na_AbsPosition = actor->ar_nodes[na].AbsPosition;
        const Ogre::Vector3 nb_AbsPosition = actor->ar_nodes[nb].AbsPosition;

        intraPointCD.query(
                no_AbsPosition,
                na_AbsPosition,
                nb_AbsPosition, collrange);

        bool collision = false;

        if (!intraPointCD.hit_list.empty())
        {
            // setup transformation of points to triangle local coordinates
            const Triangle triangle(na_AbsPosition, nb_AbsPosition, no_AbsPosition);
            const CartesianToTriangleTransform transform(triangle);

            for (auto h : intraPointCD.hit_list)
            {
                NodeNum_t hitnode_num = h->node_id;

                //ignore wheel/chassis self contact
                if (actor->ar_nodes[hitnode_num].nd_tyre_node) continue;
                if (no == hitnode_num || na == hitnode_num || nb == hitnode_num) continue;

                // transform point to triangle local coordinates
                const auto local_point = transform(actor->ar_nodes[hitnode_num].AbsPosition);

                // collision test
                const bool is_colliding = InsideTriangleTest(local_point, collrange);
                if (is_colliding)
                {
                    collision = true;

                    const auto coord = local_point.barycentric;
                    auto distance = local_point.distance;
                    auto normal   = triangle.normal();

                    // adapt in case the collision is occuring on the backface of the triangle
                    if (distance < 0) 
                    {
                        // flip surface normal and distance to triangle plane
                        normal   = -normal;
                        distance = -distance;
                    }

                    const auto penetration_depth = collrange - distance;

                    ResolveCollisionForces(
                        actor, na, nb, no,  // The cab triangle
                        h->actor, hitnode_num, // The colliding node
                        penetration_depth,
                        coord.alpha, coord.beta, coord.gamma,
                        normal, PHYSICS_DT, false, submesh_ground_model);
                }
            }
        }

        if (collision)
        {
            intra_collcabrate[i].rate = -20000;
        }
        else
        {
            intra_collcabrate[i].rate++;
        }
    }
}
