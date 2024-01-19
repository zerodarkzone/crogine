/*-----------------------------------------------------------------------

Matt Marchant 2024
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

Based on articles: http://www.extentofthejam.com/pseudo/
                   https://codeincomplete.com/articles/javascript-racer/

-----------------------------------------------------------------------*/

#pragma once

#include "../StateIDs.hpp"
#include "TrackCamera.hpp"
#include "TrackSprite.hpp"
#include "Track.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Vertex2D.hpp>

#include <array>

struct SharedStateData;
class EndlessDrivingState final : public cro::State, public cro::GuiClient, public cro::ConsoleClient
{
public:
    EndlessDrivingState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return StateID::EndlessRunner; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    SharedStateData& m_sharedData;

    //vehicle is a 3D model in its own scene
    //rendered to a buffer to use as a sprite
    cro::Scene m_playerScene;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    struct BackgroundLayer final
    {
        cro::Entity entity;
        cro::FloatRect textureRect;
        float speed = 0.f;
        float verticalSpeed = 0.f;
        enum
        {
            Sky, Hills, Trees,
            Count
        };
    };
    std::array<BackgroundLayer, BackgroundLayer::Count> m_background = {};

    std::array<TrackSprite, TrackSprite::Count> m_trackSprites = {};
    cro::Entity m_trackSpriteEntity;


    //buffer for player sprite
    cro::RenderTexture m_playerTexture;
    cro::Entity m_playerEntity; //3D model
    cro::Entity m_playerSprite;
    cro::Entity m_roadEntity;

    cro::Entity m_debugEntity;

    //texture is fixed size and game is rendered to this
    //the ui scene then scales this to the current output
    cro::RenderTexture m_gameTexture;
    cro::Entity m_gameEntity;

    //logic ents
    TrackCamera m_trackCamera;
    Track m_road;

    struct Player final
    {
        //float x = 0.f; //+/- 1 from X centre
        //float y = 0.f;
        //float z = 0.f; //rel distance from camera
        glm::vec3 position = glm::vec3(0.f);
        float speed = 0.f;

        static inline constexpr float MaxSpeed = SegmentLength * 120.f; //60 is our frame time
        static inline constexpr float Acceleration = MaxSpeed / 3.f;
        static inline constexpr float Braking = -MaxSpeed;
        static inline constexpr float Deceleration = -Acceleration;
        static inline constexpr float OffroadDeceleration = -MaxSpeed / 2.f;
        static inline constexpr float OffroadMaxSpeed = MaxSpeed / 4.f;
        static inline constexpr float Centrifuge = 530.f; // "pull" on cornering

        struct Model final
        {
            float targetRotationX = 0.f;
            float rotationX = 0.f;
            float rotationY = 0.f;

            static constexpr float MaxX = 0.1f;
            static constexpr float MaxY = 0.2f;
        }model;

        struct State final
        {
            enum
            {
                Normal, Reset
            };
        };
        std::int32_t state = 0;
    }m_player;

    struct InputFlags final
    {
        enum
        {
            Up    = 0x1,
            Down  = 0x2,
            Left  = 0x4,
            Right = 0x8
        };
        std::uint16_t flags = 0;
        std::uint16_t prevFlags = 0;

        float steerMultiplier = 1.f;
        float accelerateMultiplier = 1.f;
        float brakeMultiplier = 1.f;

        std::int32_t keyCount = 0; //tracks how many keys are pressed and only allows controller to override when 0
    }m_inputFlags;


    struct GameRules final
    {
        float remainingTime = 30.f;
        float totalTime = 0.f;

        struct State final
        {
            enum
            {
                CountIn, Running
            };
        };
        std::int32_t state = State::CountIn;
    }m_gameRules;


    std::array<std::vector<float>, TrackSprite::Animation::Count> m_wavetables = {};

    void addSystems();
    void loadAssets();
    void createPlayer();
    void createScene();
    void createRoad();
    void createUI();

    void updateControllerInput();
    void updatePlayer(float dt);
    void updateRoad(float dt);
    void addRoadQuad(const TrackSegment& s1, const TrackSegment& s2, float widthMultiplier, cro::Colour, std::vector<cro::Vertex2D>&);
    void addRoadSprite(TrackSprite&, const TrackSegment&, std::vector<cro::Vertex2D>&);

    std::pair<glm::vec2, glm::vec2> getScreenCoords(TrackSprite&, const TrackSegment&, bool animate);
};