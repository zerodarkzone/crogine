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

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

namespace cro
{
    struct Camera;
}

class FrustumState final : public cro::State, public cro::GuiClient
{
public:
    FrustumState(cro::StateStack&, cro::State::Context);
    ~FrustumState() = default;

    cro::StateID getStateID() const override { return States::ScratchPad::Frustum; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_cameraPreview;
    cro::SimpleQuad m_depthQuad;

        struct EntityID final
    {
        enum
        {
            Cube,
            Camera,
            CameraViz01,
            CameraViz02,
            CameraViz03,

            Light,

            LightViz01,
            LightViz02,
            LightViz03,
            LightDummy01,
            LightDummy02,
            LightDummy03,

            Count
        };
    };
    std::array<cro::Entity, EntityID::Count> m_entities;


    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void calcCameraFrusta();
    void calcLightFrusta();
    void updateFrustumVis(glm::mat4, const std::array<glm::vec4, 8u>&, cro::Mesh::Data&, glm::vec4 c = glm::vec4(1.f));
    bool rotateEntity(cro::Entity, std::int32_t keyset, float);
};