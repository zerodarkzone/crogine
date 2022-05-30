/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "ServerState.hpp"

#include <atomic>
#include <memory>
#include <thread>

class Server final
{
public:
    enum class GameMode
    {
        Golf, Billiards, None
    };

    Server();
    ~Server();

    Server(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator = (const Server&) = delete;
    Server& operator = (Server&&) = delete;

    void launch(std::size_t, GameMode);
    bool running() const { return m_running; }
    void stop();

    void setHostID(std::uint32_t id);

    //note this is not atomic!
    void setPreferredIP(const std::string& ip) { m_preferredIP = ip; }
    const std::string& getPreferredIP() const { return m_preferredIP; }


private:
    std::size_t m_maxConnections;
    std::string m_preferredIP;
    std::atomic_bool m_running;
    std::unique_ptr<std::thread> m_thread;

    std::unique_ptr<sv::State> m_currentState;
    GameMode m_gameMode;

    sv::SharedData m_sharedData;

    struct PendingConnection final
    {
        cro::NetPeer peer;
        cro::Clock connectionTime;
        static constexpr float Timeout = 15.f;
    };
    std::vector<PendingConnection> m_pendingConnections;

    void run();

    void checkPending();
    void validatePeer(cro::NetPeer&);

    //returns slot index, or >= MaxClients if full
    std::uint8_t addClient(const cro::NetPeer&);
    void removeClient(const cro::NetEvent&);
};
