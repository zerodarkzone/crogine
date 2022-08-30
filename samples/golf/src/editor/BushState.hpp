/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "../StateIDs.hpp"
#include "../golf/GameConsts.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <array>

namespace cro
{
    struct Camera;
}

struct SharedStateData;
class BushState final : public cro::State, public cro::GuiClient
{
public:
    BushState(cro::StateStack&, cro::State::Context, const SharedStateData&);
    ~BushState() = default;

    cro::StateID getStateID() const override { return StateID::Bush; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    const SharedStateData& m_sharedData;
    cro::SimpleQuad m_backgroundQuad;
    cro::Scene m_gameScene;
    cro::Scene m_skyScene;

    cro::ResourceCollection m_resources;
    cro::UniformBuffer<WindData> m_windBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<float> m_scaleBuffer;

    struct ModelID final
    {
        enum
        {
            Default, Single, Instanced,

            Count
        };
    };
    cro::Entity m_root;
    std::array<cro::Entity, ModelID::Count> m_models;
    std::vector<glm::mat4> m_instanceTransforms;

    struct MaterialSlot final
    {
        std::array<cro::Material::Data, 2u> materials = {};
        std::int32_t activeMaterial = 0;
    };
    std::vector<MaterialSlot> m_materials;


    cro::RenderTexture m_billboardTexture;
    cro::Entity m_billboardCamera;

    cro::RenderTexture m_thumbnailTexture;
    cro::Entity m_thumbnailCamera;

    bool m_editSkybox;

    void addSystems();
    void loadAssets();
    void createScene();

    //assigned to camera resize callback
    void updateView(cro::Camera&);

    void drawUI();

    void loadModel(const std::string&);
    void loadPreset(const std::string&);
    void savePreset(const std::string&);
    void loadSkyboxFile();
    void saveSkyboxFile();
    void addSkyboxModel();

    void createThumbnails();
};