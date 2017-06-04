/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "NPCSystem.hpp"
#include "Messages.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

#include <glm/gtx/norm.hpp>

namespace
{
    const float fixedUpdate = 1.f / 60.f;
    const float zDepth = -9.3f;
    const glm::vec3 gravity(0.f, -9.f, 0.f);
}

NpcSystem::NpcSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(NpcSystem)),
    m_accumulator(0.f)
{
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
    requireComponent<Npc>();

    auto poissonPos = cro::Util::Random::poissonDiscDistribution({ 0.f, -2.f, 4.f, 4.f }, 1.f, 5);
    for (auto p : poissonPos)
    {
        m_elitePositions.emplace_back(p, zDepth);
    }

    poissonPos = cro::Util::Random::poissonDiscDistribution({ -0.8f, -0.8f, 1.6f, 1.6f }, 0.6f, 6);
    for (auto p : poissonPos)
    {
        m_eliteIdlePositions.emplace_back(p, 0.f);
    }

    m_choppaTable = cro::Util::Wavetable::sine(1.f, 0.4f);
}

//public
void NpcSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        switch (data.type)
        {
        default: break;
        case PlayerEvent::Moved:
            m_playerPosition = data.position;
            break;
        }
    }
}

void NpcSystem::process(cro::Time dt)
{
    DPRINT("Player Position", std::to_string(m_playerPosition.x) + ", " + std::to_string(m_playerPosition.y));
    
    auto& entities = getEntities();
    m_accumulator += std::min(dt.asSeconds(), 1.f);
       
    while (m_accumulator > fixedUpdate)
    {
        m_accumulator -= fixedUpdate;

        for (auto& entity : entities) 
        {
            //check if entity has moved off-screen and
            //reset it if it has
            auto& status = entity.getComponent<Npc>();
            auto& tx = entity.getComponent<cro::Transform>();
            if (status.active)
            {               
                //process logic based on type
                switch (status.type)
                {
                default: break;
                case Npc::Elite:
                    processElite(entity);
                    break;
                case Npc::Choppa:
                    processChoppa(entity);
                    break;
                case Npc::Turret:
                    processTurret(entity);
                    continue; //turrets are parented to terrain entities - so don't need following update
                }  

                bool visible = entity.getComponent<cro::Model>().isVisible();
                if (!visible && status.wantsReset)
                {
                    tx.setPosition({ 7.f, cro::Util::Random::value(-2.f, 2.f), zDepth });
                    status.wantsReset = false;
                    status.active = false;
                }
                else if (visible)
                {
                    //we've entered the visible area so will eventually want resetting
                    status.wantsReset = true;
                }
            }
        }
    }
}

//private
void NpcSystem::processElite(cro::Entity entity)
{
    auto& tx = entity.getComponent<cro::Transform>();
    auto& status = entity.getComponent<Npc>();
    
    //move toward target
    auto movement = status.elite.destination - tx.getWorldPosition();
      
    if (status.elite.active) //big movements
    {
        movement *= 4.f;
        if (glm::length2(movement) < 1.f)
        {
            status.elite.active = false;
            status.elite.pauseTime = cro::Util::Random::value(1.4f, 2.1f);
            status.elite.movementCount--;

            status.elite.destination = m_eliteIdlePositions[status.elite.idleIndex] + tx.getWorldPosition();
        }
    }
    else
    {
        //do some floaty bobbins               
        /*if (glm::length2(movement) < 0.2f)
        {
            status.elite.idleIndex = (status.elite.idleIndex + 1) % m_eliteIdlePositions.size();
            status.elite.destination = m_eliteIdlePositions[status.elite.idleIndex] + tx.getWorldPosition();
        }*/

        //count down to next movement
        status.elite.pauseTime -= fixedUpdate;
        if (status.elite.pauseTime < 0)
        {
            status.elite.active = true;
            status.elite.destination = (status.elite.movementCount)
                ? m_elitePositions[cro::Util::Random::value(0, m_elitePositions.size() - 1)]
                : glm::vec3(-7.f, tx.getWorldPosition().y, zDepth);
        }
    }
    tx.move(movement * fixedUpdate);
}

void NpcSystem::processChoppa(cro::Entity entity)
{
    auto& tx = entity.getComponent<cro::Transform>();
    auto& status = entity.getComponent<Npc>();

    if (status.choppa.inCombat)
    {
        //fly / shoot
        glm::vec3 movement(status.choppa.moveSpeed, m_choppaTable[status.choppa.tableIndex], 0.f);
        movement.y += (m_playerPosition.y - tx.getWorldPosition().y) * 0.17f;
        tx.move(movement * fixedUpdate);
        status.choppa.tableIndex = (status.choppa.tableIndex + 1) % m_choppaTable.size();
    }
    else
    {
        //diiieeeee
        tx.rotate({ 0.f, 1.f, 0.f }, 3.f + fixedUpdate);
        tx.move(status.choppa.deathVelocity * fixedUpdate);
        status.choppa.deathVelocity += gravity * fixedUpdate;
    }
}

void NpcSystem::processTurret(cro::Entity entity)
{
    auto& tx = entity.getComponent<cro::Transform>();
    
    glm::vec3 target = m_playerPosition - tx.getWorldPosition();

    float rotation = atan2(target.x, target.y);
    tx.setRotation({ 0.f, -rotation, 0.f });
}

void NpcSystem::onEntityAdded(cro::Entity entity)
{

    auto& status = entity.getComponent<Npc>();
    switch (status.type)
    {
    default:break;
    case Npc::Elite:
        status.elite.active = true;
        status.elite.destination = m_elitePositions[0];
        status.elite.movementCount = cro::Util::Random::value(4, 8);
        status.elite.pauseTime = cro::Util::Random::value(1.2f, 2.2f);
        status.elite.idleIndex = cro::Util::Random::value(0, m_eliteIdlePositions.size());
        break;
    case Npc::Choppa:
        status.choppa.moveSpeed = cro::Util::Random::value(-1.3f, -0.8f);
        status.choppa.deathVelocity.x = status.choppa.moveSpeed;
        status.choppa.tableIndex = cro::Util::Random::value(0, m_choppaTable.size() - 1);
        break;
    case Npc::Turret:
        status.active = true;
        break;
    }
}