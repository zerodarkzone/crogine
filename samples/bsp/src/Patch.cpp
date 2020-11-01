/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "Patch.hpp"

#include <array>

namespace
{
    const std::int32_t TesselationLevel = 10;
    const std::int32_t pointCountX = 3;
    const std::int32_t pointCountY = 3;
}

Patch::Patch(const Q3::Face& face, const std::vector<Q3::Vertex>& mapVerts, std::vector<float>& verts)
{
    std::int32_t patchXCount = (face.size[0] - 1) >> 1;
    std::int32_t patchYCount = (face.size[1] - 1) >> 1;

    for (auto y = 0; y < patchYCount; ++y)
    {
        for (auto x = 0; x < patchXCount; ++x)
        {
            std::array<Q3::Vertex, 9> quad = {};
            for (auto col = 0; col < pointCountY; ++col)
            {
                for (auto row = 0; row < pointCountX; ++row)
                {
                    quad[row * pointCountX + col] = mapVerts[face.firstVertIndex +
                                                    (y * 2 * face.size[0] + x * 2) +
                                                    row * face.size[0] + col];
                }
            }

            tesselate(quad, verts);
        }
    }

    //remove the final degen tri
    m_indices.pop_back();
}

//private
void Patch::tesselate(const std::array<Q3::Vertex, 9u>& quad, std::vector<float>& verts)
{
    std::vector<Q3::Vertex> vertTemp((TesselationLevel + 1) * (TesselationLevel + 1));

    for (auto i = 0; i <= TesselationLevel; ++i)
    {
        float a = static_cast<float>(i) / TesselationLevel;
        float b = 1.f - a;

        vertTemp[i] = quad[0] * (b * b) +
                    quad[3] * (2 * b * a) +
                    quad[6] * (a * a);
    }

    for (int i = 1; i <= TesselationLevel; ++i)
    {
        float a = (float)i / TesselationLevel;
        float b = 1.f - a;

        std::array<Q3::Vertex, 3> temp;

        for (auto j = 0, k = 0; j < 3; ++j, k = 3 * j)
        {
            temp[j] = quad[k + 0] * (b * b) +
                    quad[k + 1] * (2 * b * a) +
                    quad[k + 2] * (a * a);
        }

        for (auto j = 0; j <= TesselationLevel; ++j)
        {
            float a = static_cast<float>(j) / TesselationLevel;
            float b = 1.f - a;

            vertTemp[i * (TesselationLevel + 1) + j] = temp[0] * (b * b) +
                                                    temp[1] * (2 * b * a) +
                                                    temp[2] * (a * a);
        }
    }

    //push temp verts to vert output
    const std::size_t vertSize = 12; //maaaaagic number!!! Actually the number of floats in a vert
    std::uint32_t indexOffset = static_cast<std::uint32_t>(verts.size() / vertSize);
    for (const auto& vertex : vertTemp)
    {
        verts.push_back(vertex.position.x);
        verts.push_back(vertex.position.z);
        verts.push_back(-vertex.position.y);

        verts.push_back(static_cast<float>(vertex.colour[0]) / 255.f);
        verts.push_back(static_cast<float>(vertex.colour[1]) / 255.f);
        verts.push_back(static_cast<float>(vertex.colour[2]) / 255.f);
        verts.push_back(static_cast<float>(vertex.colour[3]) / 255.f);

        verts.push_back(vertex.normal.x);
        verts.push_back(vertex.normal.z);
        verts.push_back(-vertex.normal.y);

        verts.push_back(vertex.uv1.x);
        verts.push_back(vertex.uv1.y);
    }


    std::vector<std::uint32_t> indexTemp(TesselationLevel * (TesselationLevel + 1) * 2);
    for (auto row = 0; row < TesselationLevel; ++row)
    {
        for (auto col = 0; col <= TesselationLevel; ++col)
        {
            indexTemp[(row * (TesselationLevel + 1) + col) * 2 + 1] = row * (TesselationLevel + 1) + col;
            indexTemp[(row * (TesselationLevel + 1) + col) * 2] = (row + 1) * (TesselationLevel + 1) + col;
        }
    }

    //arrange indices in correct order and add offset into the vertex array
    const auto colCount = 2 * (TesselationLevel + 1);
    std::vector<std::uint32_t> indices;
    for (auto row = 0; row < TesselationLevel; ++row)
    {
        if (row > 0)
        {
            //create the first degen
            indices.push_back(indexTemp[row * 2 * (TesselationLevel + 1)] + indexOffset);
        }

        //create the strip
        for (auto col = 0; col < colCount; ++col)
        {
            indices.push_back(indexTemp[row * 2 * (TesselationLevel + 1) + col] + indexOffset);
        }

        //add the final degen
        indices.push_back(indices.back());
    }

    //if this is not the first quad then prepend a degen tri
    if (!m_indices.empty())
    {
        m_indices.push_back(indices[0]);
    }

    m_indices.insert(m_indices.end(), indices.begin(), indices.end());
}