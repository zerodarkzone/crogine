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

#include <reactphysics3d/mathematics/Vector3.h>
#include <reactphysics3d/mathematics/Quaternion.h>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

constexpr glm::vec3 BallSpawnPosition = glm::vec3({ 16.f, 30.f, -38.f });
constexpr glm::vec3 Gravity = glm::vec3(0.f, -0.98f, 0.f);

static inline glm::vec3 toGLM(reactphysics3d::Vector3 v)
{
    return { v.x, v.y, v.z };
}

static inline glm::quat toGLM(reactphysics3d::Quaternion q)
{
    return { q.w, q.x, q.y, q.z };
}

static inline reactphysics3d::Vector3 toR3D(glm::vec3 v)
{
    return { v.x, v.y, v.z };
}

static inline reactphysics3d::Quaternion toR3D(glm::quat q)
{
    return { q.x, q.y, q.z, q.w };
}