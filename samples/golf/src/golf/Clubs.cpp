/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "Clubs.hpp"
#include <Social.hpp>

namespace
{
    struct Stat final
    {
        constexpr Stat() = default;
        constexpr Stat(float p, float t) : power(p), target(t) {}
        float power = 0.f; //impulse strength
        float target = 0.f; //distance target
    };

    struct ClubStat final
    {
        constexpr ClubStat(Stat a, Stat b, Stat c)
            : stats() { stats = { a, b, c }; }
        std::array<Stat, 3> stats = {};
    };

    constexpr std::array<ClubStat, ClubID::Count> ClubStats =
    {
        ClubStat({44.f,   220.f},{46.88f, 250.f},{50.11f, 280.f}), //123
        ClubStat({39.6f,  180.f},{42.89f, 210.f},{43.93f, 230.f}), //123
        ClubStat({36.3f,  150.f},{38.67f, 170.f},{40.76f, 190.f}), //123

        ClubStat({35.44f, 140.f},{36.49f, 150.f},{37.69f, 160.f}), //123
        ClubStat({34.16f, 130.f},{35.44f, 140.f},{36.49f, 150.f}), //123
        ClubStat({32.85f, 120.f},{34.16f, 130.f},{35.44f, 140.f}), //123
        ClubStat({31.33f, 110.f},{32.85f, 120.f},{34.16f, 130.f}), //123
        ClubStat({29.91f, 100.f},{31.33f, 110.f},{32.85f, 120.f}), //123
        ClubStat({28.4f,  90.f}, {29.91f, 100.f},{31.33f, 110.f}), //123

        //these don't increase in range
        ClubStat({25.2f, 70.f}, {25.2f, 70.f}, {25.2f, 70.f}),
        ClubStat({17.4f, 30.f}, {17.4f, 30.f}, {17.4f, 30.f}),
        ClubStat({10.3f, 10.f}, {10.3f, 10.f}, {10.3f, 10.f}),
        ClubStat({9.11f, 10.f}, {9.11f, 10.f}, {9.11f, 10.f}) //except this which is dynamic
    };
}

Club::Club(std::int32_t id, const std::string& name, float angle, float sidespin, float topspin)
    : m_id      (id), 
    m_name      (name), 
    m_angle     (angle * cro::Util::Const::degToRad),
    m_sidespin  (sidespin),
    m_topspin   (topspin)
{

}

//public
std::string Club::getName(bool imperial, float distanceToPin) const
{
    auto t = getTarget(distanceToPin);
    
    if (imperial)
    {
        static constexpr float ToYards = 1.094f;
        static constexpr float ToFeet = 3.281f;
        //static constexpr float ToInches = 12.f;

        if (getPower(distanceToPin) > 10.f)
        {
            auto dist = static_cast<std::int32_t>(t * ToYards);
            return m_name + std::to_string(dist) + "yds";
        }
        else
        {
            auto dist = static_cast<std::int32_t>(t * ToFeet);
            return m_name + std::to_string(dist) + "ft";
        }
    }
    else
    {
        if (t < 1.f)
        {
            t *= 100.f;
            auto dist = static_cast<std::int32_t>(t);
            return m_name + std::to_string(dist) + "cm";
        }
        auto dist = static_cast<std::int32_t>(t);
        return m_name + std::to_string(dist) + "m";
    }
}

float Club::getPower(float distanceToPin) const
{
    if (m_id == ClubID::Putter)
    {
        return getScaledValue(ClubStats[m_id].stats[0].target, distanceToPin);
    }

    //check player level and return further distance
    auto level = Social::getLevel();
    if (level > 29)
    {
        return ClubStats[m_id].stats[2].power;
    }

    if (level > 14)
    {
        return ClubStats[m_id].stats[1].power;
    }
    return ClubStats[m_id].stats[0].power;
}

float Club::getTarget(float distanceToPin) const
{
    if (m_id == ClubID::Putter)
    {
        return getScaledValue(ClubStats[m_id].stats[0].target, distanceToPin);
    }

    return getBaseTarget();
}

float Club::getBaseTarget() const
{
    //check player level and return increased distance
    auto level = Social::getLevel();
    if (level > 29)
    {
        return ClubStats[m_id].stats[2].target;
    }

    if (level > 14)
    {
        return ClubStats[m_id].stats[1].target;
    }
    return ClubStats[m_id].stats[0].target;
}

//private
float Club::getScaledValue(float value, float distanceToPin) const
{
    auto target = getBaseTarget();
    if (distanceToPin < target * ShortRangeThreshold)
    {
        if (distanceToPin < target * TinyRangeThreshold)
        {
            return value * TinyRange;
        }

        return value * ShortRange;
    }
    return value;
}