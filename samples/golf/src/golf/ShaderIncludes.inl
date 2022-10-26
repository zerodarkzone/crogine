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

#include <string>
#include <unordered_map>

static inline const std::string WindBuffer = R"(
    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };)";

static inline const std::string ResolutionBuffer = R"(
    layout (std140) uniform ScaledResolution
    {
        vec2 u_scaledResolution;
        float u_nearFadeDistance;
    };)";

static inline const std::string ScaleBuffer = R"(
    layout (std140) uniform PixelScale
    {
        float u_pixelScale;
    };)";


static inline const std::unordered_map<std::string, const char*> IncludeMappings =
{
    std::make_pair("WIND_BUFFER", WindBuffer.c_str()),
    std::make_pair("RESOLUTION_BUFFER", ResolutionBuffer.c_str()),
    std::make_pair("SCALE_BUFFER", ScaleBuffer.c_str()),
};