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

#ifndef TL_BUDDY_SYSTEM_HPP_
#define TL_BUDDY_SYSTEM_HPP_

#include <crogine/ecs/System.hpp>

struct Buddy final
{
    enum
    {
        Inactive, Initialising, Active
    }state = Inactive;
    float fireTime = 0.f;
    float lifespan = 0.f;
};

class BuddySystem final : public cro::System
{
public:
    BuddySystem(cro::MessageBus&);

    void handleMessage(const cro::Message&) override;
    void process(cro::Time) override;

private:

    bool m_requestSpawn;
    bool m_requestDespawn;

    void processInit(float, cro::Entity);
    void processActive(float, cro::Entity);
};

#endif //TL_BUDDY_SYSTEM_HPP_