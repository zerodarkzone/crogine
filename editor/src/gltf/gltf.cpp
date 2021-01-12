/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "../ModelState.hpp" //includes tinygltf

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/gui/Gui.hpp>

#include <string>
#include <cstring>

namespace
{

}

void ModelState::showGLTFBrowser()
{
    std::int32_t ID = 2000;
    const std::function<void(std::int32_t, const std::vector<tf::Node>&)> drawNode
        = [&](std::int32_t idx, const std::vector<tf::Node>& nodes)
    {
        const auto& node = nodes[idx];
        if (node.mesh > -1)
        {
            std::string label = node.name + "##";
            std::string IDString = std::to_string(ID++);
            label += IDString;

            if (ImGui::TreeNode(label.c_str()))
            {
                static bool importAnim = false;
                if (node.skin > -1)
                {
                    auto boxLabel = "Import Animation##" + IDString;
                    ImGui::Checkbox(boxLabel.c_str(), &importAnim);
                }
                auto buttonLabel = "Import##" + IDString;
                if (ImGui::Button(buttonLabel.c_str()))
                {
                    //import the node
                    parseGLTFNode(idx, importAnim);
                    m_GLTFLoader.reset();
                    m_browseGLTF = false;
                    ImGui::CloseCurrentPopup();
                }

                for (const auto& child : node.children)
                {
                    drawNode(child, nodes);
                }
                ImGui::TreePop();
            }
        }
    };

    //ImGui::ShowDemoWindow();
    ImGui::OpenPopup("Scene Browser");
    if (ImGui::BeginPopupModal("Scene Browser", &m_browseGLTF, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const auto& nodes = m_GLTFScene.nodes;
        for (auto i = 0u; i < nodes.size(); ++i)
        {
            drawNode(static_cast<std::int32_t>(i), nodes);
        }

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_GLTFLoader.reset();
            m_browseGLTF = false;
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::EndPopup();
    }
}

void ModelState::parseGLTFNode(std::int32_t idx, bool loadAnims)
{
    importGLTF(m_GLTFScene.nodes[idx].mesh, loadAnims);

    //load animations if selected
    if (loadAnims)
    {
        parseGLTFAnimations(idx, m_entities[EntityID::ActiveModel].addComponent<cro::Skeleton>());
    }
}

void ModelState::parseGLTFAnimations(std::int32_t idx, cro::Skeleton& dest)
{
    //TODO model might have multple skins (ie for multiple meshes)
    //so we need to traverse the tree and parse all of them.

    //fetch the skin details
    const auto& node = m_GLTFScene.nodes[idx];
    CRO_ASSERT(node.skin > -1 && node.skin < m_GLTFScene.skins.size(), "");
    const auto& skin = m_GLTFScene.skins[node.skin];

    std::vector<glm::mat4> inverseBindPose(skin.joints.size());
    dest.frameSize = skin.joints.size();
    dest.jointIndices.resize(skin.joints.size()); //not used for this format, but still needs to be correct size.
    std::fill(dest.jointIndices.begin(), dest.jointIndices.end(), -1);

    const auto& accessor = m_GLTFScene.accessors[skin.inverseBindMatrices];
    const auto& bufferView = m_GLTFScene.bufferViews[accessor.bufferView];
    const auto& buffer = m_GLTFScene.buffers[bufferView.buffer];
    std::size_t index = accessor.byteOffset + bufferView.byteOffset;
    dest.frames.resize(dest.frameSize);

    //create a vector of parent IDs as nodes only track children
    std::vector<std::int32_t> parents(m_GLTFScene.nodes.size());
    std::fill(parents.begin(), parents.end(), -1);
    for (auto i = 0u; i < m_GLTFScene.nodes.size(); ++i)
    {
        const auto& node = m_GLTFScene.nodes[i];
        for (auto j = 0u; j < node.children.size(); ++j)
        {
            parents[node.children[j]] = static_cast<std::int32_t>(i);
        }
    }

    //read the inv bindpose
    for (auto i = 0u; i < dest.frameSize; ++i)
    {
        std::memcpy(&inverseBindPose[i], &buffer.data[index], sizeof(glm::mat4));
        index += sizeof(glm::mat4);

        //this is pre-calculated when the matrices are
        //built so not needed at run-time
        //dest.jointIndices[i] = parents[skin.joints[i]];
    }



    //copy the nodes as we'll modify the transforms for each key frame
    auto sceneNodes = m_GLTFScene.nodes;

    //functions to process a frame
    auto getLocalMatrix = [](const tf::Node& node)
    {
        glm::mat4 jointMat = glm::mat4(1.f);
        glm::vec3 translation(0.f);
        glm::quat rotation(1.f, 0.f, 0.f, 0.f);
        glm::vec3 scale(1.f);

        if (node.translation.size() == 3)
        {
            translation.x = static_cast<float>(node.translation[0]);
            translation.y = static_cast<float>(node.translation[1]);
            translation.z = static_cast<float>(node.translation[2]);
        }
        if (node.rotation.size() == 4)
        {
            rotation.w = static_cast<float>(node.rotation[3]);
            rotation.x = static_cast<float>(node.rotation[0]);
            rotation.y = static_cast<float>(node.rotation[1]);
            rotation.z = static_cast<float>(node.rotation[2]);
        }
        if (node.scale.size() == 3)
        {
            scale.x = static_cast<float>(node.scale[0]);
            scale.y = static_cast<float>(node.scale[1]);
            scale.z = static_cast<float>(node.scale[2]);
        }
        jointMat = glm::translate(jointMat, translation);
        jointMat *= glm::toMat4(rotation);
        jointMat = glm::scale(jointMat, scale);
        return jointMat;
    };
    
    auto getMatrix = [&](std::int32_t i)
    {
        const auto& joint = sceneNodes[i];
        auto jointMat = getLocalMatrix(joint);

        std::int32_t currentParent = parents[i];
        while (currentParent > -1)
        {
            jointMat = getLocalMatrix(sceneNodes[currentParent]) * jointMat;
            currentParent = parents[currentParent];
        }
        return jointMat;
    };

    std::function<void(std::int32_t)> updateJoints =
        [&](std::int32_t nodeIdx)
    {
        //applying this undoes the scale/rot applied to the armature with
        //blender and, in general, makes the model huge and lying on its back..
        //auto inverseTx = glm::inverse(getMatrix(nodeIdx));
        for (auto i = 0u; i < dest.frameSize; ++i)
        {
            dest.frames[i] = /*inverseTx **/ getMatrix(skin.joints[i]) * inverseBindPose[i];
        }
    };

    updateJoints(idx);

    
    //update the output so we have something to look at.
    for (auto i = 0u; i < dest.frameSize; ++i)
    {
        dest.currentFrame.push_back(dest.frames[i]);
    }

    dest.animations.emplace_back(); //TODO remove this when actually adding animations
    dest.animations.back().looped = true;
    dest.play(0);
}

void ModelState::importGLTF(std::int32_t meshIndex, bool loadAnims)
{
    //start with mesh. This contains a Primitive for each sub-mesh
    //which in turn contains the indices into the accessor array.

    //an accessor contains info about a buffer view, such as its index
    //in the bufferview array, and which data type the buffer contains
    //ie, vec2, vec3 etc. componentType matches the GL_ENUM for GL_FLOAT,
    //GL_UINT etc. It may contain an additional offset which should be
    //added to the beginning of the view offset, and subtracted from
    //the view's length

    //a buffer view contains the buffer index, the offset into that
    //buffer at which data starts, the length of the data, and the stride.

    std::uint16_t attribFlags = 0u;
    std::vector<float> vertices;
    std::vector<std::vector<std::uint32_t>> indices;
    std::vector<std::int32_t> primitiveTypes;
    std::size_t indexOffset = 0;

    CRO_ASSERT(meshIndex < m_GLTFScene.meshes.size(), "");
    const auto& mesh = m_GLTFScene.meshes[meshIndex];
    for (const auto& prim : mesh.primitives)
    {
        std::uint16_t flags = 0;
        std::array<std::int32_t, cro::Mesh::Attribute::Total> attribIndices = {};
        std::fill(attribIndices.begin(), attribIndices.end(), -1);

        for (const auto& [name, idx] : prim.attributes)
        {
            if (name == "POSITION")
            {
                flags |= cro::VertexProperty::Position;
                attribIndices[cro::Mesh::Attribute::Position] = idx;
            }
            else if (name == "NORMAL")
            {
                flags |= cro::VertexProperty::Normal;
                attribIndices[cro::Mesh::Attribute::Normal] = idx;
            }
            else if (name == "TANGENT")
            {
                flags |= cro::VertexProperty::Tangent | cro::VertexProperty::Bitangent;
                attribIndices[cro::Mesh::Tangent] = idx;
            }
            else if (name == "TEXCOORD_0")
            {
                flags |= cro::VertexProperty::UV0;
                attribIndices[cro::Mesh::UV0] = idx;
            }
            else if (name == "TEXCOORD_1")
            {
                flags |= cro::VertexProperty::UV1;
                attribIndices[cro::Mesh::UV1] = idx;
            }
            else if (name == "COLOR_0")
            {
                flags |= cro::VertexProperty::Colour;
                attribIndices[cro::Mesh::Colour] = idx;
            }
            else if (name == "JOINTS_0"
                && loadAnims)
            {
                flags |= cro::VertexProperty::BlendIndices;
                attribIndices[cro::Mesh::BlendIndices] = idx;
            }
            else if (name == "WEIGHTS_0"
                && loadAnims)
            {
                flags |= cro::VertexProperty::BlendWeights;
                attribIndices[cro::Mesh::BlendWeights] = idx;
            }
        }

        if (attribFlags != 0
            && attribFlags != flags)
        {
            //TODO ideally mesh format should account for these cases
            //after all each submesh has its own shader/material aaaanyway
            //but it also means creating multiple VBOs per VAO and... meh
            LogW << "submesh with differing vertex attribs found - skipping..." << std::endl;
            continue;
        }
        attribFlags = flags;

        //interleave and concat vertex data
        //TODO this might actually already be interleaved - and so would
        //be faster if we know this and do a straight copy...
        std::array<std::pair<std::vector<float>, std::size_t>, cro::Mesh::Attribute::Total> tempData; //float components, component size
        auto getComponentCount = [](int type)
        {
            switch (type)
            {
            default: return 0;
            case TINYGLTF_TYPE_SCALAR:
                return 1;
            case TINYGLTF_TYPE_VEC2:
                return 2;
            case TINYGLTF_TYPE_VEC3:
                return 3;
            case TINYGLTF_TYPE_VEC4:
                return 4;
            case TINYGLTF_TYPE_MAT2:
                return 4;
            case TINYGLTF_TYPE_MAT3:
                return 9;
            case TINYGLTF_TYPE_MAT4:
                return 16;
            }
        };
        for (auto i = 0u; i < tempData.size(); ++i)
        {
            if (attribIndices[i] > -1)
            {
                const auto& accessor = m_GLTFScene.accessors[attribIndices[i]];

                switch (accessor.componentType)
                {
                case GL_FLOAT:
                {
                    std::size_t componentCount = getComponentCount(accessor.type);
                    tempData[i] = std::make_pair(std::vector<float>(), componentCount);

                    CRO_ASSERT(accessor.bufferView < m_GLTFScene.bufferViews.size(), "");
                    const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
                    CRO_ASSERT(view.buffer < m_GLTFScene.buffers.size(), "");
                    const auto& buffer = m_GLTFScene.buffers[view.buffer];

                    for (auto j = 0u; j < accessor.count; ++j)
                    {
                        std::size_t index = view.byteOffset + accessor.byteOffset + (std::max(componentCount, view.byteStride) * j * sizeof(float));
                        for (auto k = 0u; k < componentCount; ++k)
                        {
                            float f = 0.f;
                            std::memcpy(&f, &buffer.data[index], sizeof(float));
                            tempData[i].first.push_back(f);
                            index += sizeof(float);
                        }
                    }
                }
                    break;
                case GL_UNSIGNED_BYTE:
                {
                    //TODO in the case of blend indices this is actually
                    //wrong to convert to float, as well as it bloating
                    //the vertex data with unnecessary bytes...
                    //but some bright spark decided long ago that the
                    //vertex data should always be interleaved...
                    std::size_t componentCount = getComponentCount(accessor.type);
                    tempData[i] = std::make_pair(std::vector<float>(), componentCount);

                    CRO_ASSERT(accessor.bufferView < m_GLTFScene.bufferViews.size(), "");
                    const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
                    CRO_ASSERT(view.buffer < m_GLTFScene.buffers.size(), "");
                    const auto& buffer = m_GLTFScene.buffers[view.buffer];

                    for (auto j = 0u; j < accessor.count; ++j)
                    {
                        std::size_t index = view.byteOffset + accessor.byteOffset + (std::max(componentCount, view.byteStride) * j * sizeof(std::uint8_t));

                        for (auto k = 0u; k < componentCount; ++k)
                        {
                            std::uint8_t v = 0;
                            std::memcpy(&v, &buffer.data[index + k], sizeof(std::uint8_t));
                            if (i == cro::Mesh::Attribute::BlendIndices)
                            {
                                tempData[i].first.push_back(static_cast<float>(v));
                            }
                            else
                            {
                                //normalise (probably blendweight, could be UV though...)
                                float f = static_cast<float>(v) / std::numeric_limits<std::uint8_t>::max();
                                tempData[i].first.push_back(f);
                            }
                        }
                    }
                    int buns = 0;
                }
                    break;
                case GL_UNSIGNED_SHORT:
                {
                    std::size_t componentCount = getComponentCount(accessor.type);
                    tempData[i] = std::make_pair(std::vector<float>(), componentCount);

                    CRO_ASSERT(accessor.bufferView < m_GLTFScene.bufferViews.size(), "");
                    const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
                    CRO_ASSERT(view.buffer < m_GLTFScene.buffers.size(), "");
                    const auto& buffer = m_GLTFScene.buffers[view.buffer];

                    for (auto j = 0u; j < accessor.count; ++j)
                    {
                        std::size_t index = view.byteOffset + accessor.byteOffset + (std::max(componentCount, view.byteStride) * j * sizeof(std::uint16_t));
                        for (auto k = 0u; k < componentCount; ++k)
                        {
                            std::uint16_t v = 0;
                            std::memcpy(&v, &buffer.data[index], sizeof(std::uint16_t));
                            if (i == cro::Mesh::Attribute::BlendIndices)
                            {
                                tempData[i].first.push_back(static_cast<float>(v));
                            }
                            else
                            {
                                //normalise (probably blendweight, could be UV though...)
                                float f = static_cast<float>(v) / std::numeric_limits<std::uint16_t>::max();
                                tempData[i].first.push_back(f);
                            }
                            index += sizeof(std::uint16_t);
                        }
                    }
                }
                    break;
                case GL_UNSIGNED_INT:
                {
                    //TODO UVs may not be normalised and will
                    //have to be converted here. In the case of UVs
                    //in pixels we'll need to have loaded the texture
                    //data so we have an image size to normalise with
                }
                    break;
                default: break;
                }
            }
        }

        CRO_ASSERT(!tempData[0].first.empty(), "Position data missing");
        auto vertexCount = tempData[0].first.size() / tempData[0].second;
        for (auto i = 0u; i < vertexCount; ++i)
        {
            for (auto j = 0u; j < tempData.size(); ++j)
            {
                const auto& [data, size] = tempData[j];

                if (data.empty())
                {
                    continue;
                }

                if (j == cro::Mesh::Tangent)
                {
                    if (!data.empty())
                    {
                        //calc bitan and correct for sign
                        //NOTE we flip the sign because we also flip the UV
                        auto idx = i * size;
                        const auto& normalData = tempData[cro::Mesh::Normal].first;

                        glm::vec3 normal(normalData[idx], normalData[idx + 1], normalData[idx + 2]);
                        glm::vec3 tan(data[idx], data[idx + 1], data[idx + 2]);
                        float sign = data[idx + 3];

                        glm::vec3 bitan = glm::cross(normal, tan) * -sign;

                        vertices.push_back(tan.x);
                        vertices.push_back(tan.y);
                        vertices.push_back(tan.z);

                        vertices.push_back(bitan.x);
                        vertices.push_back(bitan.y);
                        vertices.push_back(bitan.z);
                    }
                }
                else if (j == cro::Mesh::UV0
                    || j == cro::Mesh::UV1)
                {
                    //flip the V
                    auto idx = i * size;
                    glm::vec2 uv(data[idx], data[idx + 1]);
                    uv.t = 1.f - uv.t;
                    vertices.push_back(uv.s);
                    vertices.push_back(uv.t);
                }
                else
                {
                    if (!data.empty())
                    {
                        for (auto k = i * size; k < (i * size) + size; ++k)
                        {
                            vertices.push_back(data[k]);
                        }
                    }
                }
            }
        }

        //copy the index data
        CRO_ASSERT(prim.indices < m_GLTFScene.accessors.size(), "");
        const auto& accessor = m_GLTFScene.accessors[prim.indices];
        CRO_ASSERT(accessor.bufferView < m_GLTFScene.bufferViews.size(), "");
        const auto& view = m_GLTFScene.bufferViews[accessor.bufferView];
        CRO_ASSERT(view.buffer < m_GLTFScene.buffers.size(), "");
        const auto& buffer = m_GLTFScene.buffers[view.buffer];

        indices.emplace_back();

        for (auto j = 0u; j < accessor.count; ++j)
        {
            switch (accessor.componentType)
            {
            default: 
                LogW << "Unknown index type " << accessor.componentType << std::endl;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            {
                std::uint8_t v = 0;
                std::size_t index = view.byteOffset + accessor.byteOffset + j;
                std::memcpy(&v, &buffer.data[index], sizeof(std::uint8_t));
                indices.back().push_back(v + static_cast<std::uint8_t>(indexOffset & 0xff));
            }
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            {
                std::uint16_t v = 0;
                std::size_t index = view.byteOffset + accessor.byteOffset + (j * sizeof(std::uint16_t));
                std::memcpy(&v, &buffer.data[index], sizeof(std::uint16_t));
                indices.back().push_back(v + static_cast<std::uint16_t>(indexOffset & 0xffff));
            }
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            {
                std::uint32_t v = 0;
                std::size_t index = view.byteOffset + accessor.byteOffset + (j * sizeof(std::uint32_t));
                std::memcpy(&v, &buffer.data[index], sizeof(std::uint32_t));
                indices.back().push_back(v + static_cast<std::uint32_t>(indexOffset & 0xffffffff));
            }
                break;
            }
        }
            
        primitiveTypes.push_back(prim.mode); //GL_TRIANGLES etc.

        //offset indices for submesh as we're concatting the vertex data
        indexOffset += vertexCount;

        prim.material; //TODO parse this later
    }

    CMFHeader header;
    header.flags = attribFlags;
    header.animated = loadAnims;
    //TODO make sure we don't have more than MaxMaterials sub meshes
    updateImportNode(header, vertices, indices);
}