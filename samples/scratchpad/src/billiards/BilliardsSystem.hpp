/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/ecs/System.hpp>
#include <crogine/graphics/MeshData.hpp>

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <memory>
#include <vector>

struct CollisionID final
{
    enum
    {
        Table, Cushion,
        Ball, Pocket,

        Count
    };
};

struct BilliardBall final : public btMotionState
{
    void getWorldTransform(btTransform& worldTrans) const override;
    void setWorldTransform(const btTransform& worldTrans) override;

private:
    cro::Entity m_parent;
    btRigidBody* m_physicsBody = nullptr;

    friend class BilliardsSystem;
};

class BulletDebug;
class BilliardsSystem final : public cro::System
{
public:
    BilliardsSystem(cro::MessageBus&, BulletDebug&);
    ~BilliardsSystem();

    BilliardsSystem(const BilliardsSystem&) = delete;
    BilliardsSystem(BilliardsSystem&&) = delete;

    BilliardsSystem& operator = (const BilliardsSystem&) = delete;
    BilliardsSystem& operator = (BilliardsSystem&&) = delete;

    void process(float) override;
    void initTable(const cro::Mesh::Data&);

    void applyImpulse(/*vec3 dir, vec3 offset*/);

private:
    BulletDebug& m_debugDrawer;

    std::unique_ptr<btCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_constraintSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_collisionWorld;

    //we have to keep a local copy of the table verts as the
    //collision world only maintains pointers to it
    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;

    //these are what do the pointing.
    std::vector<std::unique_ptr<btRigidBody>> m_tableObjects;
    std::vector<std::unique_ptr<btTriangleIndexVertexArray>> m_tableVertices;
    std::vector<std::unique_ptr<btBvhTriangleMeshShape>> m_tableShapes;


    //tracks ball objects
    std::vector<std::unique_ptr<btRigidBody>> m_ballObjects;
    std::unique_ptr<btSphereShape> m_ballShape; //balls can all share this.

    //pockets
    std::unique_ptr<btBoxShape> m_pocketShape;

    btRigidBody::btRigidBodyConstructionInfo createBodyDef(std::int32_t, float, btCollisionShape*, btMotionState* = nullptr);

    void onEntityAdded(cro::Entity) override;
    void onEntityRemoved(cro::Entity) override;
};