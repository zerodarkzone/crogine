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

#include "TextChat.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "../GolfGame.hpp"

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/util/Random.hpp>

#include <cstring>

namespace
{
    const cro::Time ResendTime = cro::seconds(1.5f);

    const std::array<std::string, 12u> ApplaudStrings =
    {
        "/me applauds",
        "/me nods in appreciation",
        "/me claps",
        "/me congratulates",
        "/me collapses in awe",
        "/me politely golf-claps",
        "/me does a double-take",
        "/me pats your back",
        "/me feels a bit star-struck",
        "/me ovates (which means applauding.)",
        "/me raises a glass of Fizz",
        "/me now knows what a true hero looks like"
    };

    const std::array<std::string, 16u> HappyStrings =
    {
        "/me is ecstatic",
        "/me grins from ear to ear",
        "/me couldn't be happier",
        "/me is incredibly pleased",
        "/me skips away in delight",
        "/me moonwalks",
        "/me twerks awkwardly",
        "/me dabs",
        "/me sploots with joy",
        "/me is over the moon",
        "/me cartwheels to the clubhouse and back",
        "/me 's rubbing intensifies",
        "/me high fives their caddy",
        "/me is on top of the world",
        "/me pirouettes jubilantly",
        "/me feels like a dog with two tails",

    };

    const std::array<std::string, 11u> LaughStrings =
    {
        "/me laughs like a clogged drain",
        "/me giggles into their sleeve",
        "/me laughs out loud",
        "/me rolls on the floor with laughter",
        "/me can't contain their glee",
        "/me belly laughs",
        "/me 's sides have split from laughing",
        "/me can't contain themselves",
        "/me howls with laughter",
        "/me goes into hysterics",
        "/me cachinnates (which means laughing loudly)"
    };

    const std::array<std::string, 14u> AngerStrings =
    {
        "/me cow-fudged that one up",
        "/me rages",
        "/me throws their club into the lake",
        "/me stamps their foot",
        "/me throws a tantrum",
        "/me accidentally swears on a live public feed",
        "/me throws a hissy-fit",
        "/me is appalled",
        "/me clenches their teeth",
        "/me wails despondently",
        "/me shakes a fist",
        "/me cries like a baby",
        "/me 's blood boils",
        "/me blows a fuse"
    };
}

TextChat::TextChat(cro::Scene& s, SharedStateData& sd)
    : m_scene           (s),
    m_sharedData        (sd),
    m_visible           (false),
    m_scrollToEnd       (false),
    m_focusInput        (false),
    m_screenChatIndex   (0)
{
    registerWindow([&]() 
        {
            if (m_visible)
            {
                ImGui::SetNextWindowSize({ 600.f, 280.f });
                if (ImGui::Begin("Chat Window", &m_visible))
                {
                    const float reserveHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
                    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -reserveHeight), false);

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
                    for (const auto& [str, colour] : m_displayBuffer)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, colour);
                        ImGui::TextWrapped("%s", str.toUtf8().c_str());
                        ImGui::PopStyleColor();
                    }

                    if (m_scrollToEnd ||
                        ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    {
                        ImGui::SetScrollHereY(1.0f);
                        m_scrollToEnd = false;
                    }
                    ImGui::PopStyleVar();
                    ImGui::EndChild();
                    ImGui::Separator();

                    if (ImGui::InputText("##ip", &m_inputBuffer, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        sendTextChat();
                        m_focusInput = true;
                    }
                    ImGui::SetItemDefaultFocus();
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Send"))
                    {
                        sendTextChat();
                        m_focusInput = true;
                    }
                    if (m_focusInput)
                    {
                        ImGui::SetKeyboardFocusHere(-1);
                    }
                    m_focusInput = false;
                }
                ImGui::End();
            }
        });
}

//public
void TextChat::handlePacket(const net::NetEvent::Packet& pkt)
{
    const auto msg = pkt.as<TextMessage>();
    //only one person can type on a connected computer anyway
    //so we'll always assume it's player 0
    auto outStr = m_sharedData.connectionData[msg.client].playerData[0].name;

    auto end = std::find(msg.messageData.begin(), msg.messageData.end(), char(0));
    auto msgText = cro::String::fromUtf8(msg.messageData.begin(), end);

    cro::Colour chatColour = TextNormalColour;

    //process any emotes such as /me and choose colour
    if (auto p = msgText.find("/me"); p == 0
        && msgText.size() > 4)
    {
        chatColour = TextGoldColour;
        outStr += " " + msgText.substr(p + 4);
    }
    else
    {
        outStr += ": " + msgText;
    }
    m_displayBuffer.emplace_back(outStr, ImVec4(chatColour));

    if (m_displayBuffer.size() > MaxLines)
    {
        m_displayBuffer.pop_front();
    }

    
    //create an entity to temporarily show the message on screen
    if (m_screenChatBuffer[m_screenChatIndex].isValid())
    {
        m_scene.destroyEntity(m_screenChatBuffer[m_screenChatIndex]);
    }


    auto uiSize = glm::vec2(GolfGame::getActiveTarget()->getSize());
    uiSize /= getViewScale(uiSize);

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Label);
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, std::floor(uiSize.y - 36.f), 1.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(outStr);
    entity.getComponent<cro::Text>().setFillColour(chatColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(10.f);
    
    auto bounds = cro::Text::getLocalBounds(entity);
    bounds.left -= 2.f;
    bounds.bottom -= 2.f;
    bounds.width += 5.f;
    bounds.height += 4.f;

    static constexpr float BgAlpha = 0.2f;
    const cro::Colour c(0.f, 0.f, 0.f, BgAlpha);
    auto bgEnt = m_scene.createEntity();
    bgEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.05f });
    bgEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(bounds.left, bounds.bottom + bounds.height), c),
            cro::Vertex2D(glm::vec2(bounds.left, bounds.bottom), c),
            cro::Vertex2D(glm::vec2(bounds.left + bounds.width, bounds.bottom + bounds.height), c),
            cro::Vertex2D(glm::vec2(bounds.left + bounds.width, bounds.bottom), c)
        }
    );
    bgEnt.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_STRIP);
    entity.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());
    
    entity.getComponent<cro::Callback>().function =
        [&, bgEnt](cro::Entity e, float dt) mutable
    {
        float& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;
        float alpha = std::clamp(currTime, 0.f, 1.f);
        auto c = e.getComponent<cro::Text>().getFillColour();
        c.setAlpha(alpha);
        e.getComponent<cro::Text>().setFillColour(c);

        c = e.getComponent<cro::Text>().getShadowColour();
        c.setAlpha(alpha);
        e.getComponent<cro::Text>().setShadowColour(c);

        c = cro::Colour(0.f, 0.f, 0.f, BgAlpha * alpha);
        auto& verts = bgEnt.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour = c;
        }

        if (currTime < 0)
        {
            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    for (auto e : m_screenChatBuffer)
    {
        if (e.isValid())
        {
            e.getComponent<cro::Transform>().move({ 0.f, 16.f });
        }
    }
    m_screenChatBuffer[m_screenChatIndex] = entity;
    m_screenChatIndex = (m_screenChatIndex + 1) % m_screenChatBuffer.size();
}

void TextChat::toggleWindow()
{
    m_visible = (!m_visible && !Social::isSteamdeck());
    m_focusInput = m_visible;
}

void TextChat::quickEmote(std::int32_t emote)
{
    //preserve any partially complete input
    auto oldStr = m_inputBuffer;

    switch (emote)
    {
    default: return;
    case Angry:
        m_inputBuffer = AngerStrings[cro::Util::Random::value(0u, AngerStrings.size() - 1)];
        break;
    case Happy:
        m_inputBuffer = HappyStrings[cro::Util::Random::value(0u, HappyStrings.size() - 1)];
        break;
    case Laughing:
        m_inputBuffer = LaughStrings[cro::Util::Random::value(0u, LaughStrings.size() - 1)];
        break;
    case Applaud:
        m_inputBuffer = ApplaudStrings[cro::Util::Random::value(0u, ApplaudStrings.size() - 1)];
        break;
    }

    sendTextChat();

    m_inputBuffer = oldStr;
}


//private
void TextChat::sendTextChat()
{
    if (!m_inputBuffer.empty()
        && m_limitClock.elapsed() > ResendTime)
    {
        TextMessage msg;
        msg.client = m_sharedData.clientConnection.connectionID;

        auto len = std::min(m_inputBuffer.size(), TextMessage::MaxBytes);
        std::memcpy(msg.messageData.data(), m_inputBuffer.data(), len);

        m_inputBuffer.clear();

        m_sharedData.clientConnection.netClient.sendPacket(PacketID::ChatMessage, msg, net::NetFlag::Reliable, ConstVal::NetChannelStrings);

        m_limitClock.restart();
        m_scrollToEnd = true;

        m_visible = false;
    }
}