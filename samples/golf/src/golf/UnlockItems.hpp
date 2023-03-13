/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#pragma once

#include <cstdint>
#include <string>
#include <array>

struct UnlockData final
{
    std::int32_t modelID = -1; //preview model
    std::string description;
    std::string name;

    UnlockData(std::int32_t i, const std::string& d, const std::string& n)
        : modelID(i), description(d), name(n) {}
};

namespace ul
{
    struct ModelID final
    {
        enum
        {
            Wood,
            Iron,

            BronzeCup,
            SilverCup,
            GoldCup,
            PlatinumCup,
            DiamondCup,

            LevelBadge01,
            LevelBadge10,
            LevelBadge20,
            LevelBadge30,
            LevelBadge40,
            LevelBadge50,

            Padlock,

            Count
        };
    };

    static const std::array<std::string, ModelID::Count> ModelPaths = 
    {
        std::string("assets/golf/models/club_wood.cmt"),
        std::string("assets/golf/models/club_iron.cmt"),
        std::string("assets/golf/models/balls/ball_bronze.cmt"),
        std::string("assets/golf/models/balls/ball_silver.cmt"),
        std::string("assets/golf/models/balls/ball_gold.cmt"),
        std::string("assets/golf/models/balls/ball_platinum.cmt"),
        std::string("assets/golf/models/balls/ball_diamond.cmt"),
        std::string("assets/golf/models/trophies/level01.cmt"),
        std::string("assets/golf/models/trophies/level10.cmt"),
        std::string("assets/golf/models/trophies/level20.cmt"),
        std::string("assets/golf/models/trophies/level30.cmt"),
        std::string("assets/golf/models/trophies/level40.cmt"),
        std::string("assets/golf/models/trophies/level50.cmt"),
        std::string("assets/golf/models/trophies/unlock.cmt"),
    };

    //these MUST be in the correct order for unlocking
    struct UnlockID final
    {
        enum
        {
            FiveWood,
            FourIron,
            SixIron,
            SevenIron,
            NineIron,

            BronzeBall,
            SilverBall,
            GoldBall,
            PlatinumBall,
            DiamondBall,

            Level1,
            Level10,
            Level20,
            Level30,
            Level40,
            Level50,

            RangeExtend01,
            RangeExtend02,
            Clubhouse,
            CourseEditor,

            Count
        };
    };

    static const std::array<UnlockData, UnlockID::Count> Items =
    {
        UnlockData(ModelID::Wood, "5 Wood", "New Golf Club!"),
        { ModelID::Iron, "4 Iron", "New Golf Club!" },
        { ModelID::Iron, "6 Iron", "New Golf Club!" },
        { ModelID::Iron, "7 Iron", "New Golf Club!" },
        { ModelID::Iron, "9 Iron", "New Golf Club!" },

        { ModelID::BronzeCup,   "Bronze Ball",   "New Golf Ball!" },
        { ModelID::SilverCup,   "Silver Ball",   "New Golf Ball!" },
        { ModelID::GoldCup,     "Gold Ball",     "New Golf Ball!" },
        { ModelID::PlatinumCup, "Platinum Ball", "New Golf Ball!" },
        { ModelID::DiamondCup,  "Diamond Ball",  "New Golf Ball!" },

        { ModelID::LevelBadge01,  "Level One",     "New Level Badge!" },
        { ModelID::LevelBadge10,  "Level Ten",     "New Level Badge!" },
        { ModelID::LevelBadge20,  "Level Twenty",  "New Level Badge!" },
        { ModelID::LevelBadge30,  "Level Thirty",  "New Level Badge!" },
        { ModelID::LevelBadge40,  "Level Forty",   "New Level Badge!" },
        { ModelID::LevelBadge50,  "Level Fifty",   "New Level Badge!" },

        { ModelID::Padlock,  "Skill Up!",   "Club Range Extended" },
        { ModelID::Padlock,  "Skill Up!",   "Club Range Extended" },
        { ModelID::Padlock,  "Socialiser",  "Clubhouse Unlocked" },
        { ModelID::Padlock,  "Designer",    "Course Editor Unlocked" },
    };
}