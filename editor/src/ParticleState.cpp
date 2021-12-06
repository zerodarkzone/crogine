/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "ParticleState.hpp"
#include "Messages.hpp"
#include "UIConsts.hpp"
#include "SharedStateData.hpp"

#include "ModelViewerConsts.inl" //TODO this should be .hpp, no?

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>

#include <crogine/util/Matrix.hpp>

#include<array>

namespace
{
    const std::string prefPath = cro::FileSystem::getConfigDirectory("crogine_editor") + "particle_editor.cfg";

    const float CamSpeed = 10.f;
}

ParticleState::ParticleState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_viewportRatio     (1.f),
    m_fov               (DefaultFOV),
    m_gizmoMode         (ImGuizmo::TRANSLATE),
    m_particleSettings  (nullptr),
    m_selectedBlendMode (0),
    m_showPreferences   (false),
    m_showRenderer      (false),
    m_renderFOV         (0.5f),
    m_renderFrameRate   (6.f)
{
    ctx.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        setupScene();
        initUI();
        });

    ctx.appInstance.resetFrameTime();
    ctx.mainWindow.setTitle("Crogine Particle Editor");
}

ParticleState::~ParticleState()
{
    savePrefs();
}

//public
bool ParticleState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    //if (evt.type == SDL_KEYUP)
    switch(evt.type)
    {
    default: break;
    case SDL_KEYDOWN:
        if (evt.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
        {
            switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_g:
                m_gizmoMode = ImGuizmo::OPERATION::TRANSLATE;
                break;
            case SDLK_r:
                m_gizmoMode = ImGuizmo::OPERATION::ROTATE;
                break;
            case SDLK_s:
                m_gizmoMode = ImGuizmo::OPERATION::SCALE;
                break;
            }
        }
        break;
    case SDL_MOUSEWHEEL:
    {
        m_fov = std::min(MaxFOV, std::max(MinFOV, m_fov - (evt.wheel.y * 0.1f)));
        m_viewportRatio = updateView(m_scene.getActiveCamera(), DefaultFarPlane, m_fov);
    }
    break;
    case SDL_MOUSEMOTION:
        updateMouseInput(evt);
        break;
    }

    m_scene.forwardEvent(evt);
    return false;
}

void ParticleState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateLayout(data.data0, data.data1);
            m_viewportRatio = updateView(m_scene.getActiveCamera(), DefaultFarPlane, m_fov);
        }
    }
    /*else if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            if (data.id == States::ModelViewer)
            {
                initUI();
            }
        }
    }*/
    else if (msg.id == cro::Message::ConsoleMessage)
    {
        const auto& data = msg.getData<cro::Message::ConsoleEvent>();
        if (data.type == cro::Message::ConsoleEvent::LinePrinted)
        {
            switch (data.level)
            {
            default:
            case cro::Message::ConsoleEvent::Info:
                m_messageColour = { 0.f,1.f,0.f,1.f };
                break;
            case cro::Message::ConsoleEvent::Warning:
                m_messageColour = { 1.f,0.5f,0.f,1.f };
                break;
            case cro::Message::ConsoleEvent::Error:
                m_messageColour = { 1.f,0.f,0.f,1.f };
                break;
            }
        }
    }

    m_scene.forwardMessage(msg);
}

bool ParticleState::simulate(float dt)
{
    //updates message colour
    const float colourIncrease = dt;// *0.5f;
    m_messageColour.w = std::min(1.f, m_messageColour.w + colourIncrease);
    m_messageColour.x = std::min(1.f, m_messageColour.x + colourIncrease);
    m_messageColour.y = std::min(1.f, m_messageColour.y + colourIncrease);
    m_messageColour.z = std::min(1.f, m_messageColour.z + colourIncrease);


    //handle camera input
    glm::vec3 motion(0.f);
    auto tx = m_scene.getActiveCamera().getComponent<cro::Transform>().getWorldTransform();
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_W))
    {
        motion += cro::Util::Matrix::getForwardVector(tx) * CamSpeed;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_S))
    {
        motion -= cro::Util::Matrix::getForwardVector(tx) * CamSpeed;
    }

    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_D))
    {
        motion += cro::Util::Matrix::getRightVector(tx) * CamSpeed;
    }
    if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_A))
    {
        motion -= cro::Util::Matrix::getRightVector(tx) * CamSpeed;
    }


    if (glm::length2(motion) > 0)
    {
        m_entities[EntityID::ArcBall].getComponent<cro::Transform>().move(motion * dt);
    }



    m_scene.simulate(dt);
    return false;
}

void ParticleState::render()
{
    if (m_showRenderer)
    {
        auto oldCam = m_scene.getActiveCamera();
        m_scene.setActiveCamera(m_renderCamera);
        m_renderTexture.clear(m_renderClearColour);
        m_scene.render(m_renderTexture);
        m_renderTexture.display();

        m_scene.setActiveCamera(oldCam);
    }

    auto& rw = getContext().mainWindow;
    m_scene.render(rw);
}

//private
void ParticleState::loadAssets()
{
    if (m_sharedData.skymapTexture.empty()
        || !m_environmentMap.loadFromFile(m_sharedData.skymapTexture))
    {
        m_environmentMap.loadFromFile("assets/images/brooklyn_bridge.hdr");
    }

    m_renderTexture.create(256, 256);
    m_renderTexture.clear(cro::Colour::Black);
    m_renderTexture.display();

    m_renderCamera = m_scene.createEntity();
    m_renderCamera.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 5.f });
    //m_renderCamera.addComponent<cro::Camera>().setOrthographic(-5.f, 5.f, -5.f, 5.f, 1.f, 18.f);
    m_renderCamera.addComponent<cro::Camera>().setPerspective(m_renderFOV, 1.f, RenderNear, RenderFar);
}

void ParticleState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
    m_scene.addSystem<cro::ParticleSystem>(mb);
}

void ParticleState::setupScene()
{
    //create the camera - using a custom camera prevents the scene updating the projection on window resize
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(DefaultCameraPosition);
    entity.addComponent<cro::Camera>().shadowMapBuffer.create(4096, 4096);
    m_viewportRatio = updateView(entity, DefaultFarPlane, DefaultFOV);
    m_scene.setActiveCamera(entity);

    m_entities[EntityID::ArcBall] = m_scene.createEntity();
    m_entities[EntityID::ArcBall].addComponent<cro::Transform>().setPosition(DefaultArcballPosition);
    m_entities[EntityID::ArcBall].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    m_entities[EntityID::Emitter] = m_scene.createEntity();
    m_entities[EntityID::Emitter].addComponent<cro::Transform>();
    m_entities[EntityID::Emitter].addComponent<cro::ParticleEmitter>().start();
    m_particleSettings = &m_entities[EntityID::Emitter].getComponent<cro::ParticleEmitter>().settings;
    m_selectedEntity = m_entities[EntityID::Emitter];

    //sunlight node
    /*cro::ModelDefinition def;
    def.loadFromFile("assets/models/arrow.cmt", m_resources);
    def.createModel(m_scene.getSunlight(), m_resources);
    m_scene.getSunlight().setLabel("Sunlight");*/
}

void ParticleState::loadPrefs()
{
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(prefPath))
    {
        const auto& props = cfg.getProperties();
        for (const auto& prop : props)
        {
            const auto& name = prop.getName();
            if (name == "sky_colour")
            {
                getContext().appInstance.setClearColour(prop.getValue<cro::Colour>());
            }
            else if (name == "render_colour")
            {
                m_renderClearColour = prop.getValue<cro::Colour>();
            }
        }
    }
}

void ParticleState::savePrefs()
{
    cro::ConfigFile cfg;
    cfg.addProperty("sky_colour").setValue(getContext().appInstance.getClearColour());
    cfg.addProperty("render_colour").setValue(m_renderClearColour);


    cfg.save(prefPath);
}