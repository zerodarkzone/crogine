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

#include "ModelState.hpp"
#include "SharedStateData.hpp"
#include "GLCheck.hpp"
#include "ModelViewerConsts.inl"

#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace ai = Assimp;

namespace
{

}

void ModelState::openModel()
{
    auto path = cro::FileSystem::openFileDialogue(m_preferences.lastModelDirectory, "cmt");
    if (!path.empty()
        && cro::FileSystem::getFileExtension(path) == ".cmt")
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.find(m_sharedData.workingDirectory) == std::string::npos)
        {
            cro::FileSystem::showMessageBox("Warning", "This model was not opened from the current working directory.");
        }

        openModelAtPath(path);

        m_preferences.lastModelDirectory = cro::FileSystem::getFilePath(path);
    }
    else
    {
        cro::Logger::log(path + ": invalid file path", cro::Logger::Type::Error);
    }
}

void ModelState::openModelAtPath(const std::string& path)
{
    if (m_entities[EntityID::ActiveModel].isValid())
    {
        showSaveMessage();
    }

    closeModel();

    cro::ModelDefinition def(m_sharedData.workingDirectory);
    if (def.loadFromFile(path, m_resources, &m_environmentMap))
    {
        m_currentFilePath = path;

        m_entities[EntityID::ActiveModel] = m_scene.createEntity();
        m_entities[EntityID::RootNode].getComponent<cro::Transform>().addChild(m_entities[EntityID::ActiveModel].addComponent<cro::Transform>());

        def.createModel(m_entities[EntityID::ActiveModel], m_resources);

        //always have the option to cast shadows
        if (!m_entities[EntityID::ActiveModel].hasComponent<cro::ShadowCaster>())
        {
            m_entities[EntityID::ActiveModel].addComponent<cro::ShadowCaster>().active = false;

            const cro::Material::Data* matData = nullptr;
            if (m_entities[EntityID::ActiveModel].hasComponent<cro::Skeleton>())
            {
                matData = &m_resources.materials.get(m_materialIDs[MaterialID::DefaultShadowSkinned]);
            }
            else
            {
                matData = &m_resources.materials.get(m_materialIDs[MaterialID::DefaultShadow]);
            }

            for (auto i = 0u; i < m_entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData().submeshCount; ++i)
            {
                m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setShadowMaterial(i, *matData);
            }
        }

        m_currentModelConfig.loadFromFile(path);

        const auto& meshData = m_entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();

        //read all the model properties
        m_modelProperties.name = m_currentModelConfig.getId();
        if (m_modelProperties.name.empty())
        {
            m_modelProperties.name = "Untitled";
        }
        const auto& properties = m_currentModelConfig.getProperties();
        for (const auto& prop : properties)
        {
            const auto& name = prop.getName();
            if (name == "mesh")
            {
                auto val = prop.getValue<std::string>();
                auto ext = cro::FileSystem::getFileExtension(val);
                if (ext == ".cmf")
                {
                    m_modelProperties.type = ModelProperties::Static;
                }
                else if (ext == ".iqm")
                {
                    m_modelProperties.type = ModelProperties::Skinned;
                }
                else
                {
                    if (val == "quad")
                    {
                        m_modelProperties.type = ModelProperties::Quad;
                    }
                    else if (val == "sphere")
                    {
                        m_modelProperties.type = ModelProperties::Sphere;
                    }
                    else if (val == "billboard")
                    {
                        m_modelProperties.type = ModelProperties::Billboard;

                        m_entities[EntityID::ActiveModel].getComponent<cro::BillboardCollection>().addBillboard({});
                    }
                    else if (val == "cube")
                    {
                        m_modelProperties.type = ModelProperties::Cube;
                    }
                }
            }
            else if (name == "radius")
            {
                m_modelProperties.radius = prop.getValue<float>();
            }
            else if (name == "size")
            {
                m_modelProperties.size = prop.getValue<glm::vec3>();
            }
            else if (name == "uv")
            {
                m_modelProperties.uv = prop.getValue<glm::vec4>();
            }
            else if (name == "lock_rotation")
            {
                m_modelProperties.lockRotation = prop.getValue<bool>();
            }
            else if (name == "lock_scale")
            {
                m_modelProperties.lockScale = prop.getValue<bool>();
            }
            else if (name == "cast_shadows")
            {
                m_modelProperties.castShadows = prop.getValue<bool>();
                m_entities[EntityID::ActiveModel].getComponent<cro::ShadowCaster>().active = m_modelProperties.castShadows;
            }
        }

        //parse all of the material properties into the material/texture browser
        const auto& objects = m_currentModelConfig.getObjects();
        std::uint32_t submeshID = 0;
        for (const auto& obj : objects)
        {
            if (obj.getName() == "material")
            {
                MaterialDefinition matDef;
                readMaterialDefinition(matDef, obj);

                matDef.submeshIDs.push_back(submeshID++);
                m_activeMaterials.push_back(static_cast<std::int32_t>(m_materialDefs.size()));
                addMaterialToBrowser(std::move(matDef));

                //don't add more materials than we can use
                if (m_activeMaterials.size() == meshData.submeshCount)
                {
                    break;
                }
            }
        }

        if (m_activeMaterials.size() < meshData.submeshCount)
        {
            //pad with default material
            auto defMat = m_resources.materials.get(m_materialIDs[MaterialID::Default]);
            for (auto i = m_activeMaterials.size(); i < meshData.submeshCount; ++i)
            {
                m_activeMaterials.push_back(-1);
                m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(i, defMat);
            }
        }

        //make sure to update the bounding display if needed
        std::optional<cro::Sphere> sphere;
        if (m_showSphere) sphere = meshData.boundingSphere;

        std::optional<cro::Box> box;
        if (m_showAABB) box = meshData.boundingBox;
        updateGridMesh(m_entities[EntityID::GridMesh].getComponent<cro::Model>().getMeshData(), sphere, box);


        //read back the buffer data into the imported buffer so we can do things like normal vis and lightmap baking
        m_modelProperties.vertexData.clear();
        m_modelProperties.vertexData.resize(meshData.vertexCount * (meshData.vertexSize / sizeof(float)));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
        glCheck(glGetBufferSubData(GL_ARRAY_BUFFER, 0, meshData.vertexCount * meshData.vertexSize, m_modelProperties.vertexData.data()));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        m_modelProperties.indexData.clear();
        m_modelProperties.indexData.resize(meshData.submeshCount);

        for (auto i = 0u; i < meshData.submeshCount; ++i)
        {
            m_modelProperties.indexData[i].resize(meshData.indexData[i].indexCount);
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].ibo));

            //fudgy kludge for different index types
            switch (meshData.indexData[i].format)
            {
            default: break;
            case GL_UNSIGNED_BYTE:
            {
                std::vector<std::uint8_t> temp(meshData.indexData[i].indexCount);
                glCheck(glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshData.indexData[i].indexCount, temp.data()));
                for (auto j = 0u; j < meshData.indexData[i].indexCount; ++j)
                {
                    m_modelProperties.indexData[i][j] = temp[j];
                }
            }
            break;
            case GL_UNSIGNED_SHORT:
            {
                std::vector<std::uint16_t> temp(meshData.indexData[i].indexCount);
                glCheck(glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshData.indexData[i].indexCount * sizeof(std::uint16_t), temp.data()));
                for (auto j = 0u; j < meshData.indexData[i].indexCount; ++j)
                {
                    m_modelProperties.indexData[i][j] = temp[j];
                }
            }
            break;
            case GL_UNSIGNED_INT:
                glCheck(glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshData.indexData[i].indexCount * sizeof(std::uint32_t), m_modelProperties.indexData[i].data()));
                break;
            }
        }
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        updateNormalVis();
    }
    else
    {
        cro::Logger::log("Check current working directory (Options)?", cro::Logger::Type::Error);
    }
}

void ModelState::saveModel(const std::string& path)
{
    cro::ConfigFile newCfg("model", m_modelProperties.name);
    switch (m_modelProperties.type)
    {
    default:
        //is a static or skinned type
    {
        const auto& properties = m_currentModelConfig.getProperties();
        for (const auto& prop : properties)
        {
            if (prop.getName() == "mesh")
            {
                newCfg.addProperty(prop);
            }
        }
    }
    break;
    case ModelProperties::Billboard:
        newCfg.addProperty("mesh", "billboard");
        newCfg.addProperty("lock_rotation").setValue(m_modelProperties.lockRotation);
        newCfg.addProperty("lock_scale").setValue(m_modelProperties.lockScale);
        break;
    case ModelProperties::Cube:
        newCfg.addProperty("mesh", "cube");
        newCfg.addProperty("size").setValue(m_modelProperties.size);
        break;
    case ModelProperties::Quad:
        newCfg.addProperty("mesh", "quad");
        newCfg.addProperty("size").setValue(glm::vec2(m_modelProperties.size.x, m_modelProperties.size.y));
        newCfg.addProperty("uv").setValue(m_modelProperties.uv);
        break;
    case ModelProperties::Sphere:
        newCfg.addProperty("mesh", "sphere");
        newCfg.addProperty("radius").setValue(m_modelProperties.radius);
        break;
    }
    newCfg.addProperty("cast_shadows").setValue(m_modelProperties.castShadows);

    auto textureName = [&](std::uint32_t id)
    {
        const auto& t = m_materialTextures.at(id);
        return t.relPath + t.name;
    };

    //add all the active materials
    for (auto i : m_activeMaterials)
    {
        auto* obj = newCfg.addObject("material");

        const auto& mat = m_materialDefs[i];
        switch (mat.type)
        {
        case MaterialDefinition::PBR:
            obj->setId("PBR");
            break;
        default:
        case MaterialDefinition::VertexLit:
            obj->setId("VertexLit");
            break;
        case MaterialDefinition::Unlit:

            obj->setId("Unlit");

            break;
        }

        if (mat.type != MaterialDefinition::Unlit)
        {
            if (mat.textureIDs[MaterialDefinition::Mask] != 0)
            {
                obj->addProperty("mask").setValue(textureName(mat.textureIDs[MaterialDefinition::Mask]));
            }
            else
            {
                obj->addProperty("mask_colour").setValue(mat.maskColour);
            }

            if (mat.textureIDs[MaterialDefinition::Normal] != 0)
            {
                obj->addProperty("normal").setValue(textureName(mat.textureIDs[MaterialDefinition::Normal]));
            }
        }

        obj->addProperty("name").setValue(mat.name);
        obj->addProperty("colour").setValue(mat.colour);
        obj->addProperty("smooth").setValue(mat.smoothTexture);
        obj->addProperty("repeat").setValue(mat.repeatTexture);

        if (mat.useRimlighing)
        {
            obj->addProperty("rim").setValue(mat.rimlightColour);
            obj->addProperty("rim_falloff").setValue(mat.rimlightFalloff);
        }

        if (mat.useSubrect)
        {
            obj->addProperty("subrect").setValue(mat.subrect);
        }

        if (mat.vertexColoured)
        {
            obj->addProperty("vertex_coloured").setValue(true);
        }

        if (mat.receiveShadows)
        {
            obj->addProperty("rx_shadows").setValue(true);
        }

        switch (mat.blendMode)
        {
        default:
        case cro::Material::BlendMode::None:
            obj->addProperty("blendmode", "none");
            break;
        case cro::Material::BlendMode::Additive:
            obj->addProperty("blendmode", "add");
            break;
        case cro::Material::BlendMode::Alpha:
            obj->addProperty("blendmode", "alpha");
            break;
        case cro::Material::BlendMode::Multiply:
            obj->addProperty("blendmode", "multiply");
            break;
        }

        if (mat.textureIDs[MaterialDefinition::Diffuse] != 0)
        {
            obj->addProperty("diffuse").setValue(textureName(mat.textureIDs[MaterialDefinition::Diffuse]));

            if (mat.alphaClip > 0)
            {
                obj->addProperty("alpha_clip").setValue(mat.alphaClip);
            }
        }

        if (mat.textureIDs[MaterialDefinition::Lightmap] != 0)
        {
            obj->addProperty("lightmap").setValue(textureName(mat.textureIDs[MaterialDefinition::Lightmap]));
        }

        if (m_entities[EntityID::ActiveModel].hasComponent<cro::Skeleton>())
        {
            obj->addProperty("skinned").setValue(true);
        }
    }

    if (newCfg.save(path))
    {
        m_currentFilePath = path;
        m_currentModelConfig = newCfg;
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed writing model to " + path);
    }
}

void ModelState::closeModel()
{
    if (m_entities[EntityID::ActiveModel].isValid())
    {
        m_scene.destroyEntity(m_entities[EntityID::ActiveModel]);
        m_entities[EntityID::ActiveModel] = {};

        m_importedIndexArrays.clear();
        m_importedVBO.clear();

        if (m_entities[EntityID::ActiveSkeleton].isValid())
        {
            m_scene.destroyEntity(m_entities[EntityID::ActiveSkeleton]);
            m_entities[EntityID::ActiveSkeleton] = {};
        }
    }

    if (m_entities[EntityID::NormalVis].isValid())
    {
        m_scene.destroyEntity(m_entities[EntityID::NormalVis]);
        m_entities[EntityID::NormalVis] = {};
    }

    m_currentModelConfig = {};
    m_modelProperties = {};

    //remove any IDs from active materials
    //for (auto i : m_activeMaterials)
    //{
    //    if (i > -1)
    //    {
    //        m_materialDefs[i].submeshIDs.clear();
    //    }
    //}
    m_materialDefs.clear();
    m_activeMaterials.clear();

    m_currentFilePath.clear();
}

void ModelState::importModel()
{
    auto calcFlags = [](const aiMesh* mesh, std::uint32_t& vertexSize)->std::uint8_t
    {
        std::uint8_t retVal = cro::VertexProperty::Position | cro::VertexProperty::Normal;
        vertexSize = 6;
        if (mesh->HasVertexColors(0))
        {
            retVal |= cro::VertexProperty::Colour;
            vertexSize += 3;
        }
        if (!mesh->HasNormals())
        {
            cro::Logger::log("Missing normals for mesh data", cro::Logger::Type::Warning);
            vertexSize = 0;
            return 0;
        }
        if (mesh->HasTangentsAndBitangents())
        {
            retVal |= cro::VertexProperty::Bitangent | cro::VertexProperty::Tangent;
            vertexSize += 6;
        }
        if (!mesh->HasTextureCoords(0))
        {
            cro::Logger::log("UV coordinates were missing in (sub)mesh", cro::Logger::Type::Warning);
        }
        else
        {
            retVal |= cro::VertexProperty::UV0;
            vertexSize += 2;
        }
        if (mesh->HasTextureCoords(1))
        {
            retVal |= cro::VertexProperty::UV1;
            vertexSize += 2;
        }
        return retVal;
    };

    auto path = cro::FileSystem::openFileDialogue(m_preferences.lastImportDirectory + "/untitled", "obj,dae,fbx,glb");
    if (!path.empty())
    {
        if (cro::FileSystem::getFileExtension(path) == ".glb")
        {
            std::string warning;
            std::string error;

            m_GLTFLoader = std::make_unique<tf::TinyGLTF>();
            m_GLTFScene = {};
            m_browseGLTF = m_GLTFLoader->LoadBinaryFromFile(&m_GLTFScene, &error, &warning, path);

            if (!warning.empty())
            {
                LogW << warning << std::endl;
            }

            if (!error.empty())
            {
                LogE << error << std::endl;
            }

            if (m_browseGLTF)
            {
                m_preferences.lastImportDirectory = cro::FileSystem::getFilePath(path);
                savePrefs();
            }
        }
        else
        {
            ai::Importer importer;
            auto* scene = importer.ReadFile(path,
                aiProcess_CalcTangentSpace

                //| aiProcess_PopulateArmatureData

                | aiProcess_GenSmoothNormals
                | aiProcess_Triangulate
                | aiProcess_JoinIdenticalVertices
                | aiProcess_OptimizeMeshes
                | aiProcess_GenBoundingBoxes);

            if (scene && scene->HasMeshes())
            {
                //sort the meshes by material ready for concatenation
                std::vector<aiMesh*> meshes;
                for (auto i = 0u; i < scene->mNumMeshes; ++i)
                {
                    meshes.push_back(scene->mMeshes[i]);
                }
                std::sort(meshes.begin(), meshes.end(),
                    [](const aiMesh* a, const aiMesh* b)
                    {
                        return a->mMaterialIndex > b->mMaterialIndex;
                    });

                //prep the header
                std::uint32_t vertexSize = 0;
                CMFHeader header;
                header.flags = calcFlags(meshes[0], vertexSize);

                if (header.flags == 0)
                {
                    cro::Logger::log("Invalid vertex data...", cro::Logger::Type::Error);
                    return;
                }

                std::vector<std::vector<std::uint32_t>> importedIndexArrays;
                std::vector<float> importedVBO;

                //concat sub meshes assuming they meet the same flags
                for (const auto* mesh : meshes)
                {
                    if (calcFlags(mesh, vertexSize) != header.flags)
                    {
                        cro::Logger::log("sub mesh vertex data differs - skipping...", cro::Logger::Type::Warning);
                        continue;
                    }

                    if (header.arrayCount < 32)
                    {
                        std::uint32_t offset = static_cast<std::uint32_t>(importedVBO.size()) / vertexSize;
                        for (auto i = 0u; i < mesh->mNumVertices; ++i)
                        {
                            auto pos = mesh->mVertices[i];
                            importedVBO.push_back(pos.x);
                            importedVBO.push_back(pos.y);
                            importedVBO.push_back(pos.z);

                            if (mesh->HasVertexColors(0))
                            {
                                auto colour = mesh->mColors[0][i];
                                importedVBO.push_back(colour.r);
                                importedVBO.push_back(colour.g);
                                importedVBO.push_back(colour.b);
                            }

                            auto normal = mesh->mNormals[i];
                            importedVBO.push_back(normal.x);
                            importedVBO.push_back(normal.y);
                            importedVBO.push_back(normal.z);

                            if (mesh->HasTangentsAndBitangents())
                            {
                                auto tan = mesh->mTangents[i];
                                importedVBO.push_back(tan.x);
                                importedVBO.push_back(tan.y);
                                importedVBO.push_back(tan.z);

                                auto bitan = mesh->mBitangents[i];
                                importedVBO.push_back(bitan.x);
                                importedVBO.push_back(bitan.y);
                                importedVBO.push_back(bitan.z);
                            }

                            if (mesh->HasTextureCoords(0))
                            {
                                auto coord = mesh->mTextureCoords[0][i];
                                importedVBO.push_back(coord.x);
                                importedVBO.push_back(coord.y);
                            }

                            if (mesh->HasTextureCoords(1))
                            {
                                auto coord = mesh->mTextureCoords[1][i];
                                importedVBO.push_back(coord.x);
                                importedVBO.push_back(coord.y);
                            }
                        }

                        auto idx = mesh->mMaterialIndex;

                        while (idx >= importedIndexArrays.size())
                        {
                            //create an index array
                            importedIndexArrays.emplace_back();
                        }

                        for (auto i = 0u; i < mesh->mNumFaces; ++i)
                        {
                            importedIndexArrays[idx].push_back(mesh->mFaces[i].mIndices[0] + offset);
                            importedIndexArrays[idx].push_back(mesh->mFaces[i].mIndices[1] + offset);
                            importedIndexArrays[idx].push_back(mesh->mFaces[i].mIndices[2] + offset);
                        }
                    }
                    else
                    {
                        cro::Logger::log("Max materials have been reached - model may be incomplete", cro::Logger::Type::Warning);
                    }
                }

                //remove any empty arrays
                importedIndexArrays.erase(std::remove_if(importedIndexArrays.begin(), importedIndexArrays.end(),
                    [](const std::vector<std::uint32_t>& arr)
                    {
                        return arr.empty();
                    }), importedIndexArrays.end());

                //updates header with array sizes
                updateImportNode(header, importedVBO, importedIndexArrays);
            }

            m_preferences.lastImportDirectory = cro::FileSystem::getFilePath(path);
            savePrefs();
        }
        updateGridMesh(m_entities[EntityID::GridMesh].getComponent<cro::Model>().getMeshData(), {}, {});
    }
}

void ModelState::updateImportNode(CMFHeader header, std::vector<float>& importedVBO, std::vector<std::vector<std::uint32_t>>& importedIndexArrays)
{
    for (const auto& indices : importedIndexArrays)
    {
        header.arraySizes.push_back(static_cast<std::int32_t>(indices.size() * sizeof(std::uint32_t)));
    }
    header.arrayCount = static_cast<std::uint8_t>(importedIndexArrays.size());

    auto indexOffset = sizeof(header.flags) + sizeof(header.arrayCount) + sizeof(header.arrayOffset) + (header.arraySizes.size() * sizeof(std::int32_t));
    indexOffset += sizeof(float) * importedVBO.size();
    header.arrayOffset = static_cast<std::int32_t>(indexOffset);

    if (header.arrayCount > 0 && !importedVBO.empty() && !importedIndexArrays.empty())
    {
        closeModel();

        m_importedHeader = header;
        m_importedIndexArrays.swap(importedIndexArrays);
        m_importedVBO.swap(importedVBO);

        //create a new VBO if it doesn't exist, else recycle it
        if (m_importedMeshes.count(header.flags) == 0)
        {
            m_importedMeshes.insert(std::make_pair(header.flags, m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(header.flags, MaxSubMeshes, GL_TRIANGLES))));
            LOG("Created new import mesh", cro::Logger::Type::Info);
        }
        else
        {
            LOG("Recycled imported mesh VBO", cro::Logger::Type::Info);
        }
        auto meshID = m_importedMeshes.at(header.flags);
        auto meshData = m_resources.meshes.getMesh(meshID);

        //make sure we draw the correct number of sub-meshes
        meshData.vertexCount = m_importedVBO.size() / (meshData.vertexSize / sizeof(float));
        meshData.submeshCount = std::min(MaxSubMeshes, header.arrayCount);

        //update the buffers
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, meshData.vertexCount * meshData.vertexSize, m_importedVBO.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        for (auto i = 0u; i < meshData.submeshCount; ++i)
        {
            meshData.indexData[i].indexCount = static_cast<std::uint32_t>(m_importedIndexArrays[i].size());
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].indexCount * sizeof(std::uint32_t), m_importedIndexArrays[i].data(), GL_STATIC_DRAW));
        }
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        //TODO we ought to correctly calc the bounding geom
        //but as it's only needed to tell the renderer this model is visible
        //(imported models don't have the bounds display option)
        //this will work long enough to convert the model and calc the correct bounds
        meshData.boundingBox[0] = glm::vec3(-5.5f);
        meshData.boundingBox[1] = glm::vec3(5.5f);
        meshData.boundingSphere.radius = 5.5f;

        m_entities[EntityID::ActiveModel] = m_scene.createEntity();
        m_entities[EntityID::RootNode].getComponent<cro::Transform>().addChild(m_entities[EntityID::ActiveModel].addComponent<cro::Transform>());
        m_entities[EntityID::ActiveModel].addComponent<cro::Model>(meshData, m_resources.materials.get(m_materialIDs[(header.flags & cro::VertexProperty::BlendIndices) ? MaterialID::DefaultSkinned : MaterialID::Default]));

        for (auto i = 0; i < header.arrayCount; ++i)
        {
            m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setShadowMaterial(i, m_resources.materials.get(m_materialIDs[(header.flags & cro::VertexProperty::BlendIndices) ? MaterialID::DefaultShadowSkinned : MaterialID::DefaultShadow]));
        }
        m_entities[EntityID::ActiveModel].addComponent<cro::ShadowCaster>();

        m_importedTransform = {};
    }
    else
    {
        cro::Logger::log("Failed importing model", cro::Logger::Type::Error);
        cro::Logger::log("Missing index or vertex data", cro::Logger::Type::Error);
    }
}

void ModelState::buildSkeleton()
{
    return;

    if (m_entities[EntityID::ActiveModel].isValid()
        && m_entities[EntityID::ActiveModel].hasComponent<cro::Skeleton>())
    {
        if (!m_entities[EntityID::ActiveSkeleton].isValid())
        {
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>();

            auto material = m_resources.materials.get(m_materialIDs[MaterialID::SkeletonDraw]);
            material.enableDepthTest = false;
            entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_skeletonMeshID), material);
            entity.addComponent<cro::Skeleton>();

            m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_entities[EntityID::ActiveSkeleton] = entity;
        }

        const auto& skeleton = m_entities[EntityID::ActiveModel].getComponent<cro::Skeleton>();
        m_entities[EntityID::ActiveSkeleton].getComponent<cro::Skeleton>() = skeleton;

        auto getTransform = [](const cro::Joint& j)
        {
            glm::mat4 ret = glm::translate(glm::mat4(1.f), j.translation);
            ret *= glm::toMat4(j.rotation);
            ret = glm::scale(ret, j.scale);
            return ret;
        };

        std::vector<float> verts;
        for(auto i = 0u; i < skeleton.bindPose.size(); ++i)
        {
            auto tx = getTransform(skeleton.bindPose[i]);
            glm::vec4 position = tx * glm::vec4(0.f, 0.f, 0.f, 1.f);

            verts.push_back(position.x);
            verts.push_back(position.y);
            verts.push_back(position.z);

            verts.push_back(1.f);
            verts.push_back(0.f);
            verts.push_back(1.f);
            verts.push_back(1.f);

            verts.push_back(static_cast<float>(i));
            verts.push_back(0.f);
            verts.push_back(0.f);
            verts.push_back(0.f);

            verts.push_back(1.f);
            verts.push_back(0.f);
            verts.push_back(0.f);
            verts.push_back(0.f);
        }

        auto vertStride = 15u;

        auto& meshData = m_entities[EntityID::ActiveSkeleton].getComponent<cro::Model>().getMeshData();
        meshData.boundingBox = { glm::vec3(-2.f), glm::vec3(2.f) };
        meshData.boundingSphere.radius = 2.f;
        meshData.boundingSphere.centre = { 0.f, 2.f, 0.f };
        meshData.vertexSize = vertStride * sizeof(float);
        meshData.vertexCount = verts.size() / vertStride;

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, verts.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        std::vector<std::uint32_t> indices;
        for (auto i = 0u; i < skeleton.bindPose.size(); ++i)
        {
            if (skeleton.bindPose[i].parent > -1)
            {
                indices.push_back(i);
                indices.push_back(skeleton.bindPose[i].parent);
            }
        }

        auto& submesh = meshData.indexData[0];
        submesh.indexCount = static_cast<std::uint32_t>(indices.size());
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        //m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setHidden(true);
        m_entities[EntityID::ActiveSkeleton].getComponent<cro::Skeleton>().play(0);
        //TODO apply source skel anims to entity
    }
}

void ModelState::exportStaticModel(bool modelOnly, bool openOnSave)
{
    CRO_ASSERT(m_importedHeader.flags, "");
    CRO_ASSERT(m_importedHeader.arrayCount, "");
    CRO_ASSERT(m_importedHeader.arrayOffset, "");
    CRO_ASSERT(!m_importedHeader.animated, ""); //animated meshes will have too much vertex data...
    CRO_ASSERT(!m_importedVBO.empty(), "");
    CRO_ASSERT(!m_importedIndexArrays.empty(), "");

    if (!modelOnly
        && !cro::FileSystem::showMessageBox("Confirm", "This will overwrite any existing model definition with the default material. Are you sure?", cro::FileSystem::YesNo, cro::FileSystem::Warning))
    {
        return;
    }

    auto path = cro::FileSystem::saveFileDialogue(m_preferences.lastExportDirectory + "/untitled", "cmf");
    std::replace(path.begin(), path.end(), '\\', '/');
    if (!path.empty())
    {
        if (cro::FileSystem::getFileExtension(path) != ".cmf")
        {
            path += ".cmf";
        }

        //write binary file
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");
        if (file.file)
        {
            std::uint8_t flags = static_cast<std::uint8_t>(m_importedHeader.flags & 0xff);
            SDL_RWwrite(file.file, &flags, sizeof(flags), 1);
            SDL_RWwrite(file.file, &m_importedHeader.arrayCount, sizeof(CMFHeader::arrayCount), 1);
            SDL_RWwrite(file.file, &m_importedHeader.arrayOffset, sizeof(CMFHeader::arrayOffset), 1);
            SDL_RWwrite(file.file, m_importedHeader.arraySizes.data(), sizeof(std::int32_t), m_importedHeader.arraySizes.size());
            SDL_RWwrite(file.file, m_importedVBO.data(), sizeof(float), m_importedVBO.size());
            for (const auto& indices : m_importedIndexArrays)
            {
                SDL_RWwrite(file.file, indices.data(), sizeof(std::uint32_t), indices.size());
            }

            SDL_RWclose(file.file);
            file.file = nullptr;

            //create config file and save as cmt
            auto modelName = cro::FileSystem::getFileName(path);
            modelName = modelName.substr(0, modelName.find_last_of('.'));

            //auto meshPath = path.substr(m_preferences.workingDirectory.length() + 1);
            std::string meshPath;
            if (!m_sharedData.workingDirectory.empty() && path.find(m_sharedData.workingDirectory) != std::string::npos)
            {
                meshPath = path.substr(m_sharedData.workingDirectory.length() + 1);
            }
            else
            {
                meshPath = cro::FileSystem::getFileName(path);
            }

            std::replace(meshPath.begin(), meshPath.end(), '\\', '/');

            cro::ConfigFile cfg("model", modelName);
            cfg.addProperty("mesh", meshPath);

            //material placeholder for each sub mesh
            for (auto i = 0u; i < m_importedHeader.arrayCount; ++i)
            {
                auto material = cfg.addObject("material", "VertexLit");
                material->addProperty("colour", "1,0,1,1");
            }


            path.back() = 't';

            if (!modelOnly)
            {
                cfg.save(path);
            }

            m_preferences.lastExportDirectory = cro::FileSystem::getFilePath(path);
            savePrefs();

            if (openOnSave)
            {
                closeModel();
                openModelAtPath(path);
            }
        }
    }
}

void ModelState::applyImportTransform()
{
    const auto& transform = m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().getLocalTransform();
    auto meshData = m_entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
    auto vertexSize = meshData.vertexSize / sizeof(float);

    std::size_t normalOffset = 0;
    std::size_t tanOffset = 0;
    std::size_t bitanOffset = 0;

    //calculate the offset index into a single vertex which points
    //to any normal / tan / bitan values
    if (meshData.attributes[cro::Mesh::Attribute::Normal] != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Normal; ++i)
        {
            normalOffset += meshData.attributes[i];
        }
    }

    if (meshData.attributes[cro::Mesh::Attribute::Tangent] != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Tangent; ++i)
        {
            tanOffset += meshData.attributes[i];
        }
    }

    if (meshData.attributes[cro::Mesh::Attribute::Bitangent] != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Bitangent; ++i)
        {
            bitanOffset += meshData.attributes[i];
        }
    }

    auto applyTransform = [&](const glm::mat4& tx, std::size_t idx, bool normalise = false)
    {
        glm::vec4 v(m_importedVBO[idx], m_importedVBO[idx + 1], m_importedVBO[idx + 2], 1.f);
        v = tx * v;

        if (normalise)
        {
            v = glm::normalize(v);
        }

        m_importedVBO[idx] = v.x;
        m_importedVBO[idx + 1] = v.y;
        m_importedVBO[idx + 2] = v.z;
    };

    //loop over the vertex data and modify
    for (std::size_t i = 0u; i < m_importedVBO.size(); i += vertexSize)
    {
        //position
        applyTransform(transform, i);

        if (normalOffset != 0)
        {
            auto idx = i + normalOffset;
            applyTransform(transform, idx, true);
        }

        if (tanOffset != 0)
        {
            auto idx = i + tanOffset;
            applyTransform(transform, idx, true);
        }

        if (bitanOffset != 0)
        {
            auto idx = i + bitanOffset;
            applyTransform(transform, idx, true);
        }
    }

    //upload the data to the preview model
    glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, m_importedVBO.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_importedTransform = {};
    m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
    m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().setRotation(cro::Transform::QUAT_IDENTY);
}