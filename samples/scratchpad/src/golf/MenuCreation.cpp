/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "GolfMenuState.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "Utility.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/UISystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Easings.hpp>

#include <cstring>

namespace
{
    bool activated(const cro::ButtonEvent& evt)
    {
        switch (evt.type)
        {
        default: return false;
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
            return evt.button.button == SDL_BUTTON_LEFT;
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERBUTTONDOWN:
            return evt.cbutton.button == SDL_CONTROLLER_BUTTON_A;
        case SDL_FINGERUP:
        case SDL_FINGERDOWN:
            return true;
        case SDL_KEYUP:
        case SDL_KEYDOWN:
            return (evt.key.keysym.sym == SDLK_SPACE || evt.key.keysym.sym == SDLK_RETURN);
        }
    }
}

const std::array<glm::vec2, GolfMenuState::MenuID::Count> GolfMenuState::m_menuPositions =
{
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, cro::DefaultSceneSize.y),
    glm::vec2(-static_cast<float>(cro::DefaultSceneSize.x), cro::DefaultSceneSize.y),
    glm::vec2(-static_cast<float>(cro::DefaultSceneSize.x), 0.f),
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, 0.f)
};

void GolfMenuState::createMainMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Golf!");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //host
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 540.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Host");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.hosting = true;
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Avatar]);
                    m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Avatar);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 480.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Join");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.hosting = false;
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Avatar]);
                    m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Avatar);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 420.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Options");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    cro::Console::show();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //quit
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 360.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Quit");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    cro::App::quit();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void GolfMenuState::createAvatarMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());
    m_menuEntities[MenuID::Avatar] = menuEntity;

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition({ 0.f, -static_cast<float>(cro::DefaultSceneSize.y) });

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Player Details");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback(
            [&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                    m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Main);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());



    //add player button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f / 2.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Add Player");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().move({ -360.f, 0.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback(
            [&, mouseEnter, mouseExit](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    if (m_sharedData.localConnectionData.playerCount < m_sharedData.localConnectionData.MaxPlayers)
                    {
                        auto index = m_sharedData.localConnectionData.playerCount;
                        
                        m_sharedData.localConnectionData.playerData[index].name = "Player " + std::to_string(index + 1);
                        m_sharedData.localConnectionData.playerCount++;

                        updateLocalAvatars(mouseEnter, mouseExit);
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //remove player button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f / 2.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Remove Player");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().move({ 10.f, 0.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback(
            [&, mouseEnter, mouseExit](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    
                    if (m_sharedData.localConnectionData.playerCount > 1)
                    {
                        m_sharedData.localConnectionData.playerCount--;
                        updateLocalAvatars(mouseEnter, mouseExit);
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());



    //continue
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Continue");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    //entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback(
            [&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    if (m_sharedData.hosting)
                    {
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.serverInstance.launch();

                            //small delay for server to get ready
                            cro::Clock clock;
                            while (clock.elapsed().asMilliseconds() < 500) {}

                            m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("255.255.255.255", ConstVal::GamePort);

                            if (!m_sharedData.clientConnection.connected)
                            {
                                m_sharedData.serverInstance.stop();
                                m_sharedData.errorMessage = "Failed to connect to local server.";
                                requestStackPush(States::Golf::Error);
                            }
                            else
                            {
                                cro::Command cmd;
                                cmd.targetFlags = MenuCommandID::ReadyButton;
                                cmd.action = [](cro::Entity e, float)
                                {
                                    e.getComponent<cro::Text>().setString("Start");
                                };
                                m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                                cmd.targetFlags = MenuCommandID::ServerInfo;
                                cmd.action = [](cro::Entity e, float)
                                {
                                    e.getComponent<cro::Text>().setString("Hosting on: localhost:" + std::to_string(ConstVal::GamePort));
                                };
                                m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                                //send the selected map/course
                                //TODO this should be sent from lobby menu so host can change the selected course
                                auto data = serialiseString(m_sharedData.mapDirectory);
                                m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), cro::NetFlag::Reliable, ConstVal::NetChannelStrings);
                            }
                        }
                    }
                    else
                    {
                        parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Join]);
                        m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Join);
                    }
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    updateLocalAvatars(mouseEnter, mouseExit);
}

void GolfMenuState::createJoinMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition({ cro::DefaultSceneSize.x, -static_cast<float>(cro::DefaultSceneSize.y) });

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Join Game");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //ip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ glm::vec2(cro::DefaultSceneSize) / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString(m_sharedData.targetIP);
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -bounds.height / 2.f });
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //add a cursor to the end of the string when active
        cro::String str = m_sharedData.targetIP + "_";
        e.getComponent<cro::Text>().setString(str);
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    //box background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y / 2.f, -0.01f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>().setTexture(m_textureResource.get("assets/golf/images/textbox.png"));
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto& callback = textEnt.getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        textEnt.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                        m_textEdit.string = &m_sharedData.targetIP;
                        m_textEdit.entity = textEnt;
                        SDL_StartTextInput();
                    }
                    else
                    {
                        applyTextEdit();
                    }
                }
            });

    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                    m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Main);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Join");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>().addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit(); //finish any pending changes
                    if (!m_sharedData.targetIP.empty() &&
                        !m_sharedData.clientConnection.connected)
                    {
                        m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect(m_sharedData.targetIP.toAnsiString(), ConstVal::GamePort);
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.errorMessage = "Could not connect to server";
                            requestStackPush(States::Golf::Error);
                        }

                        cro::Command cmd;
                        cmd.targetFlags = MenuCommandID::ReadyButton;
                        cmd.action = [](cro::Entity e, float)
                        {
                            e.getComponent<cro::Text>().setString("Ready");
                        };
                        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                        cmd.targetFlags = MenuCommandID::ServerInfo;
                        cmd.action = [&](cro::Entity e, float)
                        {
                            e.getComponent<cro::Text>().setString("Connected to: " + m_sharedData.targetIP + ":" + std::to_string(ConstVal::GamePort));
                        };
                        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                    }
                }
            });
    
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void GolfMenuState::createLobbyMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition({ cro::DefaultSceneSize.x, 0.f });

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Lobby");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //display lobby members
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 400.f, 700.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("No Players...");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::LobbyList;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    for (auto i = 0u; i < m_readyState.size(); ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 360.f, 668.f + (i * -58.f) });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, i](cro::Entity e, float)
        {
            cro::Colour c =
                m_sharedData.connectionData[i].playerCount == 0 ? cro::Colour::Transparent :
                m_readyState[i] ? cro::Colour::Green : cro::Colour::Red;

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            verts =
            {
                cro::Vertex2D(glm::vec2(0.f), c),
                cro::Vertex2D(glm::vec2(20.f, 0.f), c),
                cro::Vertex2D(glm::vec2(0.f, 20.f), c),
                cro::Vertex2D(glm::vec2(20.f), c)
            };
            e.getComponent<cro::Drawable2D>().updateLocalBounds();
        };

        menuTransform.addChild(entity.getComponent<cro::Transform>());
    }

    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.clientConnection.connected = false;
                    m_sharedData.clientConnection.netClient.disconnect();

                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                    m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Main);
                    if (m_sharedData.hosting)
                    {
                        m_sharedData.serverInstance.stop();
                        m_sharedData.hosting = false;
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //start
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::ReadyButton;
    entity.addComponent<cro::Text>(m_font).setString("Start");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>().addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.hosting)
                    {
                        //check all members ready
                        bool ready = true;
                        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
                        {
                            if (m_sharedData.connectionData[i].playerCount != 0
                                && !m_readyState[i])
                            {
                                ready = false;
                                break;
                            }
                        }

                        if (ready && m_sharedData.clientConnection.connected
                            && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                        {
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(0), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        }
                    }
                    else
                    {
                        //toggle readyness
                        std::uint8_t ready = m_readyState[m_sharedData.clientConnection.connectionID] ? 0 : 1;
                        m_sharedData.clientConnection.netClient.sendPacket(PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | ready),
                            cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //server info message
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 20.f, 1080.f - 20.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::ServerInfo;
    entity.addComponent<cro::Text>(m_font).setString("Connected to");
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void GolfMenuState::createOptionsMenu(cro::Entity parent, std::uint32_t, std::uint32_t)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
}

void GolfMenuState::createPlayerConfigMenu()
{
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/player_selection.spt", m_textureResource);

    auto bgNode = m_scene.createEntity();
    bgNode.addComponent<cro::Transform>();
    bgNode.addComponent<cro::Drawable2D>();
    bgNode.addComponent<cro::CommandTarget>().ID = MenuCommandID::PlayerConfig;
    bgNode.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = bgNode.getComponent<cro::Sprite>().getTextureBounds();
    bgNode.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    bgNode.addComponent<cro::Callback>().active = true;
    bgNode.getComponent<cro::Callback>().setUserData<std::pair<float, float>>(0.f, 0.f);
    bgNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [current, target] = e.getComponent<cro::Callback>().getUserData<std::pair<float, float >>();
        float scale = current;

        if (current < target)
        {
            current = std::min(current + dt * 2.f, target);
            scale = cro::Util::Easing::easeOutBounce(current);
        }
        else if (current > target)
        {
            current = std::max(current - dt * 3.f, target);
            scale = cro::Util::Easing::easeOutQuint(current);
        }

        //TODO this *is* correct but won't appear so
        //until the camera ortho is set 1:1 when
        //the other menu graphics are done
        //auto size = glm::vec2(cro::App::getWindow().getSize());
        glm::vec2 size(1920.f, 1080.f);
        e.getComponent<cro::Transform>().setPosition(size / 2.f);
        e.getComponent<cro::Transform>().setScale(m_viewScale * scale);
    };

    auto selected = m_scene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e) 
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });
    auto unselected = m_scene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    static constexpr float ButtonDepth = 0.1f;
    auto createButton = [&](glm::vec2 pos, const std::string& sprite)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, ButtonDepth));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite(sprite);
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::PlayerConfig);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

    //I need to finish the layout editor :3
    auto entity = createButton({ 82.f, 159.f }, "name_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //c1 left
    entity = createButton({ 19.f, 127.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c1 right
    entity = createButton({ 52.f, 127.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c2 left
    entity = createButton({ 19.f, 102.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c2 right
    entity = createButton({ 52.f, 102.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c3 left
    entity = createButton({ 19.f, 77.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c3 right
    entity = createButton({ 52.f, 77.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c4 left
    entity = createButton({ 19.f, 52.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c4 right
    entity = createButton({ 52.f, 52.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //skin left
    entity = createButton({ 90.f, 92.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //skin right
    entity = createButton({ 172.f, 92.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //done
    entity = createButton({ 76.f, 15.f }, "button_highlight");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //applyTextEdit();
                    showPlayerConfig(false, 0);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //player preview
    spriteSheet.loadFromFile("assets/golf/sprites/player.spt", m_textureResource);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 113.f, 68.f, ButtonDepth });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("female");
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void GolfMenuState::updateLocalAvatars(std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    static constexpr glm::vec2 BasePosition({ 600.f, 700.f });
    static constexpr glm::vec2 EditButtonOffset({ -360.f, 0.f });
    static constexpr float LineHeight = 56.f;

    for (auto e : m_avatarListEntities)
    {
        m_scene.destroyEntity(e);
    }
    m_avatarListEntities.clear();

    auto pos = BasePosition;
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(pos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(m_font).setString(m_sharedData.localConnectionData.playerData[i].name);
        entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);

        m_menuEntities[MenuID::Avatar].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        //TODO add avatar preview


        //add edit button
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(pos + EditButtonOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(m_font).setString("EDIT");
        entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        auto bounds = cro::Text::getLocalBounds(entity);
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(GroupID::Avatar);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_scene.getSystem<cro::UISystem>().addCallback(
                [](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        //TODO show the edit window for this index
                    }
                });
        m_menuEntities[MenuID::Avatar].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        pos.y -= LineHeight;
    }
}

void GolfMenuState::updateLobbyData(const cro::NetEvent& evt)
{
    ConnectionData cd;
    if (cd.deserialise(evt.packet))
    {
        m_sharedData.connectionData[cd.connectionID] = cd;
    }

    updateLobbyAvatars();
}

void GolfMenuState::updateLobbyAvatars()
{
    //TODO detect only the avatars which changed
    //so we don't needlessly update textures

    cro::Command cmd;
    cmd.targetFlags = MenuCommandID::LobbyList;
    cmd.action = [&](cro::Entity e, float)
    {
        cro::String str;
        for (const auto& c : m_sharedData.connectionData)
        {
            if (c.playerCount > 0)
            {
                str += c.playerData[0].name;
                for (auto i = 1u; i < c.playerCount; ++i)
                {
                    str += ", " + c.playerData[i].name;
                }
                str += "\n";
            }
        }

        if (!str.empty())
        {
            e.getComponent<cro::Text>().setString(str);
        }
        else
        {
            e.getComponent<cro::Text>().setString("No Players...");
        }
    };
    m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void GolfMenuState::showPlayerConfig(bool visible, std::uint8_t playerIndex)
{
    cro::Command cmd;
    cmd.targetFlags = MenuCommandID::PlayerConfig;
    cmd.action = [&,visible](cro::Entity e, float)
    {
        float target = visible ? 1.f : 0.f;
        std::size_t menu = visible ? MenuID::PlayerConfig : MenuID::Avatar;

        e.getComponent<cro::Callback>().getUserData<std::pair<float, float>>().second = target;
        m_scene.getSystem<cro::UISystem>().setActiveGroup(menu);
    };
    m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}