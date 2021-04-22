/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "ServerGameState.hpp"
#include "PacketIDs.hpp"
#include "CommonConsts.hpp"
#include "ServerPacketData.hpp"
#include "ClientPacketData.hpp"
#include "PlayerSystem.hpp"
#include "ActorSystem.hpp"
#include "Messages.hpp"
#include "GameConsts.hpp"
#include "WeatherDirector.hpp"
#include "MapData.hpp"
#include "ServerLog.hpp"
#include "ServerState.hpp"
#include "Collision.hpp"
#include "CrateSystem.hpp"

#include <crogine/core/Log.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/detail/glm/vec3.hpp>

using namespace sv;

namespace
{

}

GameState::GameState(SharedData& sd)
    : m_returnValue (StateID::Game),
    m_sharedData    (sd),
    m_scene         (sd.messageBus),
    m_playerSpawns  (PlayerSpawns)
{
    initScene();
    buildWorld();
    LOG("Entered Server Game State", cro::Logger::Type::Info);
}

void GameState::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::ConnectionMessage)
    {
        const auto& data = msg.getData<ConnectionEvent>();
        if (data.type == ConnectionEvent::Disconnected)
        {
            for (auto i = 0u; i < ConstVal::MaxClients; ++i)
            {
                if (m_playerEntities[data.playerID][i].isValid())
                {
                    auto entityID = m_playerEntities[data.playerID][i].getIndex();
                    m_scene.destroyEntity(m_playerEntities[data.playerID][i]);

                    m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, entityID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
            }
        }
    }
    else if (msg.id == MessageID::CrateMessage)
    {
        const auto& data = msg.getData<CrateEvent>();
        if (data.type == CrateEvent::StateChanged)
        {
            if (data.crate.isValid())
            {
                CrateState state;
                state.state = data.crate.getComponent<Crate>().state;
                state.owner = data.crate.getComponent<Crate>().owner;
                state.collisionLayer = data.crate.getComponent<Crate>().collisionLayer;
                state.serverEntityID = static_cast<std::uint16_t>(data.crate.getIndex());

                m_sharedData.host.broadcastPacket(PacketID::CrateUpdate, state, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
        }
    }
    else if (msg.id == MessageID::GameMessage)
    {
        const auto& data = msg.getData<GameEvent>();
        switch (data.type)
        {
        default: break;
        case GameEvent::GameBegin:
            startGame();
            break;
        }
    }
    else if (msg.id == cro::Message::SceneMessage)
    {
        const auto& data = msg.getData<cro::Message::SceneEvent>();
        if (data.event == cro::Message::SceneEvent::EntityDestroyed)
        {
            m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, data.entityID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
    else if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        PlayerStateChange psc;
        psc.playerID = data.player.getComponent<Player>().avatar.getComponent<Actor>().id;
        psc.playerState = data.type;
        psc.serverEntityID = data.player.getIndex();

        m_sharedData.host.broadcastPacket(PacketID::PlayerState, psc, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }

    m_scene.forwardMessage(msg);
}

void GameState::netEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::RequestData:
        {
            std::uint16_t data = evt.packet.as<std::uint16_t>();
            std::uint8_t connectionID = (data & 0xff00) >> 8;
            std::uint8_t flags = (data & 0x00ff);

            switch (flags)
            {
            default: break;

            }
        }
            break;
        case PacketID::ClientReady:
            if (!m_sharedData.clients[evt.packet.as<std::uint8_t>()].ready)
            {
                sendInitialGameState(evt.packet.as<std::uint8_t>());
            }
            break;
        case PacketID::InputUpdate:
            handlePlayerInput(evt.packet);
            break;
        case PacketID::ServerCommand:
            doServerCommand(evt);
            break;
        }
    }
}

void GameState::netBroadcast()
{
    //send reconciliation for each player
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].ready)
        {
            for (auto j = 0u; j < m_sharedData.clients[i].playerCount; ++j)
            {
                if (m_playerEntities[i][j].isValid())
                {
                    const auto& player = m_playerEntities[i][j].getComponent<Player>();

                    PlayerUpdate update;
                    update.position = cro::Util::Net::compressVec3(m_playerEntities[i][j].getComponent<cro::Transform>().getPosition());
                    update.rotation = cro::Util::Net::compressQuat(m_playerEntities[i][j].getComponent<cro::Transform>().getRotation());
                    update.pack(player);

                    m_sharedData.host.sendPacket(m_sharedData.clients[i].peer, PacketID::PlayerUpdate, update, cro::NetFlag::Unreliable);
                }
            }
        }
    }

    //broadcast other actor transforms
    //remember this includes the avatar entities attached to players
    //don't send these until clients are all ready, a slow loading client
    //will get backed up messages from this which pops the message buffer :(
    auto timestamp = m_serverTime.elapsed().asMilliseconds();
    const auto& actors = m_scene.getSystem<ActorSystem>().getEntities1();
    for (auto e : actors)
    {
        const auto& actor = e.getComponent<Actor>();
        const auto& tx = e.getComponent<cro::Transform>();

        ActorUpdate update;
        update.actorID = actor.id;
        update.serverID = actor.serverEntityId;
        update.position = cro::Util::Net::compressVec3(tx.getWorldPosition());
        update.rotation = cro::Util::Net::compressQuat(tx.getRotation());
        update.timestamp = timestamp;

        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
        {
            if (m_sharedData.clients[i].ready)
            {
                m_sharedData.host.sendPacket(m_sharedData.clients[i].peer, PacketID::ActorUpdate, update, cro::NetFlag::Unreliable);
            }
        }
    }
}

std::int32_t GameState::process(float dt)
{
    m_scene.simulate(dt);
    return m_returnValue;
}

//private
void GameState::sendInitialGameState(std::uint8_t playerID)
{
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].connected)
        {
            //TODO name/skin info is sent on lobby join

            for (auto j = 0u; j < m_sharedData.clients[i].playerCount; ++j)
            {
                PlayerInfo info;
                info.playerID = j;
                info.spawnPosition = m_playerEntities[i][j].getComponent<cro::Transform>().getPosition();
                info.rotation = cro::Util::Net::compressQuat(m_playerEntities[i][j].getComponent<cro::Transform>().getRotation());
                info.serverID = m_playerEntities[i][j].getIndex();
                info.timestamp = m_serverTime.elapsed().asMilliseconds();
                info.connectionID = i;

                m_sharedData.host.sendPacket(m_sharedData.clients[playerID].peer, PacketID::PlayerSpawn, info, cro::NetFlag::Reliable);
            }
        }
    }

    //client said it was ready, so mark as ready
    m_sharedData.clients[playerID].ready = true;


    //check all clients are ready and begin the game
    bool ready = true;
    for (const auto& client : m_sharedData.clients)
    {
        if (client.connected && !client.ready)
        {
            ready = false;
        }
    }

    if (ready)
    {
        auto* msg = m_sharedData.messageBus.post<GameEvent>(MessageID::GameMessage);
        msg->type = GameEvent::GameBegin;
    }
}

void GameState::handlePlayerInput(const cro::NetEvent::Packet& packet)
{
    auto input = packet.as<InputUpdate>();
    CRO_ASSERT(m_playerEntities[input.connectionID][input.playerID].isValid(), "Not a valid player!");
    auto& player = m_playerEntities[input.connectionID][input.playerID].getComponent<Player>();

    //only add new inputs
    auto lastIndex = (player.nextFreeInput + (Player::HistorySize - 1) ) % Player::HistorySize;
    if (input.input.timeStamp > (player.inputStack[lastIndex].timeStamp))
    {
        player.inputStack[player.nextFreeInput] = input.input;
        player.nextFreeInput = (player.nextFreeInput + 1) % Player::HistorySize;
    }
}

void GameState::doServerCommand(const cro::NetEvent& evt)
{
    //TODO validate this sender has permission to request commands
    //by checking evt peer against client data
    //TODO packet data needs to include connection ID

    //auto data = evt.packet.as<ServerCommand>();
    //if (data.target < m_sharedData.clients.size()
    //    && m_sharedData.clients[data.target].connected)
    //{
    //    switch (data.commandID)
    //    {
    //    default: break;
    //    case CommandPacket::SetModeFly:
    //        if (m_playerEntities[data.target].isValid())
    //        {
    //            m_playerEntities[data.target].getComponent<Player>().flyMode = true;
    //            m_sharedData.host.sendPacket(m_sharedData.clients[data.target].peer, PacketID::ServerCommand, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    //        }
    //        break;
    //    case CommandPacket::SetModeWalk:
    //        if (m_playerEntities[data.target].isValid())
    //        {
    //            m_playerEntities[data.target].getComponent<Player>().flyMode = false;
    //            m_sharedData.host.sendPacket(m_sharedData.clients[data.target].peer, PacketID::ServerCommand, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    //        }
    //        break;
    //    }
    //}

}

void GameState::initScene()
{
    auto& mb = m_sharedData.messageBus;

    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::DynamicTreeSystem>(mb);
    m_scene.addSystem<ActorSystem>(mb);
    m_scene.addSystem<PlayerSystem>(mb);
    m_scene.addSystem<CrateSystem>(mb);

    m_scene.addDirector<WeatherDirector>(m_sharedData.host);
}

void GameState::buildWorld()
{
    MapData mapData;
    if (mapData.loadFromFile("assets/maps/" + m_sharedData.mapName))
    {
        //set spawn points
        for (auto i = 0u; i < mapData.getSpawnPositions().size(); ++i)
        {
            auto spawn = mapData.getSpawnPositions()[i];
            m_playerSpawns[i].x = spawn.x;
            m_playerSpawns[i].y = spawn.y;
        }

        std::size_t playerCount = 0;

        //create a network game as usual
        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
        {
            if (m_sharedData.clients[i].connected
                && playerCount < ConstVal::MaxClients)
            {
                //insert requested players in this slot
                for (auto j = 0u; j < m_sharedData.clients[i].playerCount && playerCount < ConstVal::MaxClients; ++j)
                {
                    auto position = m_playerSpawns[playerCount];

                    m_playerEntities[i][j] = m_scene.createEntity();
                    m_playerEntities[i][j].addComponent<cro::Transform>().setPosition(position);
                    m_playerEntities[i][j].addComponent<Player>().id = i + j;
                    m_playerEntities[i][j].getComponent<Player>().collisionLayer = playerCount / 2;
                    m_playerEntities[i][j].getComponent<Player>().spawnPosition = position;
                    m_playerEntities[i][j].getComponent<Player>().connectionID = i;
                    m_playerEntities[i][j].getComponent<Player>().direction = position.x > 0 ? Player::Left : Player::Right;

                    playerCount++;

                    //this controls the server side appearance
                    //such as rotation and animation which needs
                    //to be sync'd with clients.
                    auto avatar = m_scene.createEntity();
                    avatar.addComponent<cro::Transform>();
                    avatar.addComponent<Actor>().id = i + j;
                    avatar.getComponent<Actor>().serverEntityId = static_cast<std::uint16_t>(m_playerEntities[i][j].getIndex());
                    avatar.addComponent<PlayerAvatar>();

                    m_playerEntities[i][j].getComponent<Player>().avatar = avatar;
                    m_playerEntities[i][j].getComponent<cro::Transform>().addChild(avatar.getComponent<cro::Transform>());

                    m_playerEntities[i][j].addComponent<cro::DynamicTreeComponent>().setArea(PlayerBounds);
                    m_playerEntities[i][j].getComponent<cro::DynamicTreeComponent>().setFilterFlags((playerCount / 2) + 1);

                    m_playerEntities[i][j].addComponent<CollisionComponent>().rectCount = 3;
                    m_playerEntities[i][j].getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Body;
                    m_playerEntities[i][j].getComponent<CollisionComponent>().rects[0].bounds = { -PlayerSize.x / 2.f, 0.f, PlayerSize.x, PlayerSize.y };
                    m_playerEntities[i][j].getComponent<CollisionComponent>().rects[1].material = CollisionMaterial::Foot;
                    m_playerEntities[i][j].getComponent<CollisionComponent>().rects[1].bounds = FootBounds;
                    m_playerEntities[i][j].getComponent<CollisionComponent>().rects[2].material = CollisionMaterial::Sensor;
                    auto crateArea = Util::expand(CrateArea, 0.1f);
                    crateArea.left += CrateCarryOffset.x;
                    crateArea.bottom += CrateCarryOffset.y;
                    m_playerEntities[i][j].getComponent<CollisionComponent>().rects[2].bounds = crateArea;
                    m_playerEntities[i][j].getComponent<CollisionComponent>().calcSum();

                    //adjust the sum so it includes the crate whichever side it's on
                    auto& sumRect = m_playerEntities[i][j].getComponent<CollisionComponent>().sumRect;
                    auto diff = ((CrateCarryOffset.x + (CrateArea.width / 2.f)) + (PlayerBounds[0].x / 2.f)) + 0.2f;
                    sumRect.left -= diff;
                    sumRect.width += diff;
                }
            }
        }


        //add collision data to scene via filtered AABB tree
        for (auto i = 0; i < 2; ++i)
        {
            float layerDepth = LayerDepth * Util::direction(i);

            const auto& rects = mapData.getCollisionRects(i);
            for (const auto& rect : rects)
            {
                auto collisionEnt = m_scene.createEntity();
                collisionEnt.addComponent<cro::Transform>().setPosition({ rect.left, rect.bottom, layerDepth });
                collisionEnt.addComponent<cro::DynamicTreeComponent>().setArea({ glm::vec3(0.f, 0.f, LayerThickness), glm::vec3(rect.width, rect.height, -LayerThickness) });
                collisionEnt.getComponent<cro::DynamicTreeComponent>().setFilterFlags(i + 1);
                
                collisionEnt.addComponent<CollisionComponent>().rectCount = 1;
                collisionEnt.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Solid;
                collisionEnt.getComponent<CollisionComponent>().rects[0].bounds = { 0.f, 0.f, rect.width, rect.height };
                collisionEnt.getComponent<CollisionComponent>().calcSum();
            }

            const auto& teleports = mapData.getTeleportRects(i);
            for (const auto& rect : teleports)
            {
                auto entity = m_scene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ rect.left, rect.bottom, layerDepth });
                entity.addComponent<cro::DynamicTreeComponent>().setArea({ glm::vec3(0.f, 0.f, LayerThickness), glm::vec3(rect.width, rect.height, -LayerThickness) });
                entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags(i + 1);

                entity.addComponent<CollisionComponent>().rectCount = 1;
                entity.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Teleport;
                entity.getComponent<CollisionComponent>().rects[0].bounds = { 0.f, 0.f, rect.width, rect.height };
                entity.getComponent<CollisionComponent>().calcSum();
            }

            const auto& crateSpawns = mapData.getCratePositions(i);
            for (auto spawn : crateSpawns)
            {
                m_crateSpawns.push_back(glm::vec3(spawn, layerDepth));
            }
        }
    }
    else
    {
        m_sharedData.host.broadcastPacket(PacketID::LogMessage, sv::LogID::MapLoadFailure, cro::NetFlag::Reliable);

        endGame();
    }
}

cro::Entity GameState::spawnActor(std::int32_t actorID, glm::vec3 position)
{
    if (actorID <= ActorID::PlayerFour)
    {
        return {};
    }

    //add to our scene
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<Actor>().id = actorID;
    entity.getComponent<Actor>().serverEntityId = static_cast<std::uint16_t>(entity.getIndex());

    switch (actorID)
    {
    default: break;
    case ActorID::Crate:
        entity.addComponent<Crate>().collisionLayer = position.z > 0 ? 0 : 1;

        entity.addComponent<cro::DynamicTreeComponent>().setArea(CrateBounds);
        entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags((position.z > 0 ? 1 : 2) | CollisionID::Crate);

        entity.addComponent<CollisionComponent>().rectCount = 2;
        entity.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Crate;
        entity.getComponent<CollisionComponent>().rects[0].bounds = CrateArea;
        entity.getComponent<CollisionComponent>().rects[1].material = CollisionMaterial::Foot;
        entity.getComponent<CollisionComponent>().rects[1].bounds = CrateFoot;
        entity.getComponent<CollisionComponent>().calcSum();
        break;
    }

    //notify clients
    ActorSpawn as;
    as.id = actorID;
    as.position = cro::Util::Net::compressVec3(position);
    as.serverEntityId = static_cast<std::uint16_t>(entity.getIndex());
    as.timestamp = m_serverTime.elapsed().asMilliseconds();

    m_sharedData.host.broadcastPacket(PacketID::ActorSpawn, as, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

    return entity;
}

void GameState::removeEntity(std::uint32_t entityIndex)
{
    m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, entityIndex, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void GameState::startGame()
{
    //send all the crates / other actors
    for (auto spawn : m_crateSpawns)
    {
        spawnActor(ActorID::Crate, spawn);
    }

    //send packet to tell clients to begin the game
    m_sharedData.host.broadcastPacket(PacketID::GameMessage, std::uint8_t(GameEvent::GameBegin), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void GameState::endGame()
{
    m_sharedData.host.broadcastPacket(PacketID::StateChange, std::uint8_t(sv::StateID::Lobby), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    m_returnValue = sv::StateID::Lobby;
}