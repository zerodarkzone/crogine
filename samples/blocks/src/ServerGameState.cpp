/*-----------------------------------------------------------------------

Matt Marchant 2020
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
#include "ServerMessages.hpp"
#include "TerrainGen.hpp"

#include <crogine/core/Log.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>


using namespace Sv;

namespace
{

}

GameState::GameState(SharedData& sd)
    : m_returnValue (StateID::Game),
    m_sharedData    (sd),
    m_scene         (sd.messageBus)
{
    initScene();
    buildWorld();
    LOG("Entered Server Game State", cro::Logger::Type::Info);
}

void GameState::handleMessage(const cro::Message& msg)
{
    if (msg.id == Sv::MessageID::ConnectionMessage)
    {
        const auto& data = msg.getData<ConnectionEvent>();
        if (data.type == ConnectionEvent::Disconnected)
        {
            auto entityID = m_playerEntities[data.playerID].getIndex();
            m_scene.destroyEntity(m_playerEntities[data.playerID]);

            m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, entityID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
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
        if (m_sharedData.clients[i].connected
            && m_playerEntities[i].isValid())
        {
            const auto& player = m_playerEntities[i].getComponent<Player>();

            PlayerUpdate update;
            update.position = m_playerEntities[i].getComponent<cro::Transform>().getPosition();
            update.rotation = cro::Util::Net::compressQuat(m_playerEntities[i].getComponent<cro::Transform>().getRotation());
            update.pitch = cro::Util::Net::compressFloat(player.cameraPitch, 16);
            update.yaw = cro::Util::Net::compressFloat(player.cameraYaw, 16);
            update.timestamp = player.inputStack[player.lastUpdatedInput].timeStamp;

            m_sharedData.host.sendPacket(m_sharedData.clients[i].peer, PacketID::PlayerUpdate, update, cro::NetFlag::Unreliable);
        }
    }

    //broadcast other actor transforms
    auto timestamp = m_serverTime.elapsed().asMilliseconds();
    const auto& actors = m_scene.getSystem<ActorSystem>()->getEntities();
    for (auto e : actors)
    {
        const auto& actor = e.getComponent<Actor>();
        const auto& tx = e.getComponent<cro::Transform>();

        ActorUpdate update;
        update.actorID = actor.id;
        update.serverID = actor.serverEntityId;
        update.position = tx.getPosition();
        update.rotation = cro::Util::Net::compressQuat(tx.getRotation());
        update.timestamp = timestamp;
        m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, update, cro::NetFlag::Unreliable);
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
            PlayerInfo info;
            info.playerID = i;
            info.spawnPosition = m_playerEntities[i].getComponent<cro::Transform>().getPosition();
            info.rotation = cro::Util::Net::compressQuat(m_playerEntities[i].getComponent<cro::Transform>().getRotation());
            info.serverID = m_playerEntities[i].getIndex();
            info.timestamp = m_serverTime.elapsed().asMilliseconds();

            m_sharedData.host.sendPacket(m_sharedData.clients[playerID].peer, PacketID::PlayerSpawn, info, cro::NetFlag::Reliable);
        }
    }


    //send map data to start building the world
    //sort the chunk positions first so we start building on the
    //client with the chunks nearest the player
    auto chunkPositions = m_world.chunks.getChunkPositions();
    auto spawnPos = m_terrainGenerator.getSpawnPoints()[playerID];
    std::sort(chunkPositions.begin(), chunkPositions.end(),
        [spawnPos](glm::ivec3 a, glm::ivec3 b)
        {
            return glm::length2(spawnPos - glm::vec3(a)) < glm::length2(spawnPos - glm::vec3(b));
        });
    
    for (auto p : chunkPositions)
    {
        sendChunk(playerID, p);
    }


    //client said it was ready, so mark as ready
    m_sharedData.clients[playerID].ready = true;
}

void GameState::handlePlayerInput(const cro::NetEvent::Packet& packet)
{
    auto input = packet.as<InputUpdate>();
    CRO_ASSERT(m_playerEntities[input.playerID].isValid(), "Not a valid player!");
    auto& player = m_playerEntities[input.playerID].getComponent<Player>();

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

    auto data = evt.packet.as<ServerCommand>();
    if (data.target < m_sharedData.clients.size()
        && m_sharedData.clients[data.target].connected)
    {
        switch (data.commandID)
        {
        default: break;
        case CommandPacket::SetModeFly:
            if (m_playerEntities[data.target].isValid())
            {
                m_playerEntities[data.target].getComponent<Player>().flyMode = true;
                m_sharedData.host.sendPacket(m_sharedData.clients[data.target].peer, PacketID::ServerCommand, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            break;
        case CommandPacket::SetModeWalk:
            if (m_playerEntities[data.target].isValid())
            {
                m_playerEntities[data.target].getComponent<Player>().flyMode = false;
                m_sharedData.host.sendPacket(m_sharedData.clients[data.target].peer, PacketID::ServerCommand, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            break;
        }
    }

}

void GameState::initScene()
{
    auto& mb = m_sharedData.messageBus;

    m_scene.addSystem<ActorSystem>(mb);
    m_scene.addSystem<PlayerSystem>(mb, m_world.chunks, m_voxelData);
}

void GameState::buildWorld()
{
    //TODO parse a config file or similar with voxel data
    //and add the types to m_voxelData

    //std::hash<std::string> hash;
    //hm this performs some truncation on the value... does it matter?
    //auto seed = static_cast<std::uint32_t>(/*hash("buns")*/std::time(nullptr));
    std::int32_t seed = 1234567;

    LOG("Seed: " + std::to_string(seed), cro::Logger::Type::Info);

    //count per side
    auto chunkCount = WorldConst::ChunksPerSide;

    LOG("Generating...", cro::Logger::Type::Info);
    for (auto z = 0; z < chunkCount; ++z)
    {
        for (auto x = 0; x < chunkCount; ++x)
        {
            m_terrainGenerator.generateTerrain(m_world.chunks, x, z, m_voxelData, seed, chunkCount);
        }
    }
    m_terrainGenerator.createSpawnPoints(m_world.chunks, m_voxelData, chunkCount);

    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].connected)
        {
            //insert a player in this slot
            m_playerEntities[i] = m_scene.createEntity();
            m_playerEntities[i].addComponent<cro::Transform>().setPosition(m_terrainGenerator.getSpawnPoints()[i]);
            m_playerEntities[i].getComponent<cro::Transform>().setRotation( //look at centre of the world
                glm::inverse(glm::lookAt(m_terrainGenerator.getSpawnPoints()[i], glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f))));
            m_playerEntities[i].addComponent<Player>().id = i;
            m_playerEntities[i].getComponent<Player>().spawnPosition = m_terrainGenerator.getSpawnPoints()[i];
            m_playerEntities[i].getComponent<Player>().cameraYaw = glm::eulerAngles(m_playerEntities[i].getComponent<cro::Transform>().getRotation()).y;
            m_playerEntities[i].addComponent<Actor>().id = i;
            m_playerEntities[i].getComponent<Actor>().serverEntityId = m_playerEntities[i].getIndex();
        }
    }
}

void GameState::sendChunk(std::uint8_t playerID, glm::ivec3 chunkPos)
{
    const Chunk* chunk = nullptr;
    if (m_world.chunks.hasChunk(chunkPos))
    {
        chunk = &m_world.chunks.getChunk(chunkPos);
    }
    else
    {
        chunk = &m_world.chunks.addChunk(chunkPos);
    }

    CRO_ASSERT(chunk, "Something went wrong");

    auto compressedData = compressVoxels(chunk->getVoxels());

    ChunkData cd;
    cd.x = static_cast<std::int16_t>(chunkPos.x);
    cd.y = static_cast<std::int16_t>(chunkPos.y);
    cd.z = static_cast<std::int16_t>(chunkPos.z);
    cd.dataSize = static_cast<std::uint32_t>(compressedData.size());
    cd.highestPoint = chunk->getHighestPoint();

    std::vector<std::uint8_t> data(sizeof(cd) + (cd.dataSize * sizeof(RLEPair)));
    std::memcpy(data.data(), &cd, sizeof(cd));
    std::memcpy(data.data() + sizeof(cd), compressedData.data(), compressedData.size() * sizeof(RLEPair));

    m_sharedData.host.sendPacket(m_sharedData.clients[playerID].peer, PacketID::ChunkData, data.data(), data.size(), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
}