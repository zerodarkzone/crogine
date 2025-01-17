/*-----------------------------------------------------------------------

Matt Marchant 2024
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


Based on articles: http://www.extentofthejam.com/pseudo/
                   https://codeincomplete.com/articles/javascript-racer/

-----------------------------------------------------------------------*/

#pragma once

#include "TrackSprite.hpp"

#include <crogine/graphics/Colour.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <vector>

struct TrackSegment final
{
    glm::vec3 position = glm::vec3(0.f);
    float length = 200.f;
    float width = 2000.f;
    float curve = 0.f;

    cro::Colour roadColour;
    cro::Colour rumbleColour;
    cro::Colour grassColour;
    bool roadMarking = false;
    float fogAmount = 0.f;

    std::vector<TrackSprite> sprites;
    float clipHeight = 0.f;

    //contains the last known screen projection
    struct Projection final
    {
        glm::vec2 position = glm::vec2(0.f);
        float width = 0.f;
        float scale = 1.f;
    }projection;

    glm::vec2 uv = glm::vec2(0.f);

    TrackSegment() = default;
    TrackSegment(glm::vec3 p, float l, float w, float c)
        : position(p), length(l), width(w), curve(c) {}
};