/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "GameConsts.hpp"
#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/graphics/CubemapTexture.hpp>

struct SharedStateData;

class TrophyState final : public cro::State, public cro::GuiClient
{
public:
    TrophyState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Trophy; }

private:

    cro::Scene m_scene;
    cro::Scene m_trophyScene;
    SharedStateData& m_sharedData;

    std::array<std::function<void()>, 2u> m_pageFuncs = {};

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    std::size_t m_trophyIndex;
    cro::RenderTexture m_trophyTexture;
    cro::CubemapTexture m_reflectionMap;
    std::vector<cro::Entity> m_trophyEnts;

    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;
    void buildScene();
    void buildTrophyScene();

    void quitState();
};