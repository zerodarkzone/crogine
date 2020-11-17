/*-----------------------------------------------------------------------

Matt Marchant 2020
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
#include "OriginIconBuilder.hpp"
#include "ResourceIDs.hpp"
#include "NormalVisMeshBuilder.hpp"
#include "GLCheck.hpp"
#include "UIConsts.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/gui/imgui.h>
#include <crogine/gui/imgui_stdlib.h>

#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/String.hpp>

#include <crogine/detail/glm/gtx/euler_angles.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <string_view>

namespace ai = Assimp;

const std::array<std::string, MaterialDefinition::Type::Count> MaterialDefinition::TypeStrings =
{
    "Unlit", "VertexLit", "PBR"
};

namespace
{
    const float DefaultFOV = 35.f * cro::Util::Const::degToRad;
    const float DefaultFarPlane = 50.f;
    const float MinZoom = 0.1f;
    const float MaxZoom = 2.5f;

    void updateView(cro::Entity entity, float farPlane, float fov)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        size.x -= (size.x * ui::InspectorWidth);
        size.y -= (size.y * ui::BrowserHeight);

        auto& cam3D = entity.getComponent<cro::Camera>();
        cam3D.projectionMatrix = glm::perspective(fov, size.x / size.y, 0.1f, farPlane);
        cam3D.viewport.left = ui::InspectorWidth;
        cam3D.viewport.width = 1.f - ui::InspectorWidth;
        cam3D.viewport.bottom = ui::BrowserHeight;
        cam3D.viewport.height = 1.f - ui::BrowserHeight;
    }


    const std::string prefPath = cro::FileSystem::getConfigDirectory("cro_model_viewer") + "prefs.cfg";

    //tooltip for UI
    void HelpMarker(const char* desc)
    {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    const std::array<float, 6> worldScales = { 0.01f, 0.1f, 1.f, 10.f, 100.f, 1000.f };
    const glm::vec3 DefaultCameraPosition({ 0.f, 0.25f, 5.f });

    std::array<std::int32_t, MaterialID::Count> materialIDs = {};
    std::array<cro::Entity, EntityID::Count> entities = {};

    enum WindowID
    {
        Inspector,
        Browser,

        Count
    };
    std::array<std::pair<glm::vec2, glm::vec2>, WindowID::Count> WindowLayouts =
    {
        std::make_pair(glm::vec2(0.f), glm::vec2(0.f)),
        std::make_pair(glm::vec2(0.f), glm::vec2(0.f))
    };

    void helpMarker(const char* desc)
    {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void drawTextureSlot(const std::string label, std::uint32_t& dest, std::uint32_t thumbnail)
    {
        glm::vec2 imgSize = glm::vec2(20.f);
        if (ImGui::ImageButton((void*)(std::size_t)thumbnail, { imgSize.x, imgSize.y }, { 0.f, 1.f }, { 1.f, 0.f }))
        {
            if (cro::FileSystem::showMessageBox("Confirm", "Clear this slot? Drag a texture from the browser to set it", cro::FileSystem::YesNo, cro::FileSystem::Question))
            {
                dest = 0;
            }
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_SRC"))
            {
                CRO_ASSERT(payload->DataSize == sizeof(std::uint32_t), "");
                dest = *(const std::uint32_t*)payload->Data;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        
        //seems a waste to keep calling this but hey
        auto textSize = ImGui::CalcTextSize("Aty");

        auto pos = ImGui::GetCursorPosY();
        pos += (imgSize.y - textSize.y);// / 2.f;
        ImGui::SetCursorPosY(pos);
        ImGui::Text("%s", label.c_str());

        ImGui::SameLine();
        helpMarker("Drag a texture from the Texture Browser to fill the slot, or click the icon to clear it.");
    }
}

ModelState::ModelState(cro::StateStack& stack, cro::State::Context context)
	: cro::State            (stack, context),
    m_scene                 (context.appInstance.getMessageBus()),
    m_showPreferences       (false),
    m_showGroundPlane       (false),
    m_showSkybox            (false),
    m_selectedTexture       (0),
    m_materialThumb         (0),
    m_selectedMaterial      (0)
{
    //launches a loading screen (registered in MyApp.cpp)
    context.mainWindow.loadResources([this]() {
        //add systems to scene
        addSystems();
        //load assets (textures, shaders, models etc)
        loadAssets();
        //create some entities
        createScene();
        //windowing
        buildUI();
    });

    context.appInstance.resetFrameTime();
    context.mainWindow.setTitle("Crogine Model Importer");
}

//public
bool ModelState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEMOTION:
        updateMouseInput(evt);
        break;
    case SDL_MOUSEWHEEL:
    {
        float acceleration = m_scene.getActiveCamera().getComponent<cro::Transform>().getPosition().z / DefaultCameraPosition.z;
        m_scene.getActiveCamera().getComponent<cro::Transform>().move(glm::vec3(0.f, 0.f, -(evt.wheel.y * 0.5f)) * worldScales[m_preferences.unitsPerMetre] * acceleration);
    }
        break;
    }

    m_scene.forwardEvent(evt);
	return false;
}

void ModelState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateLayout(data.data0, data.data1);
            updateView(m_scene.getActiveCamera(), DefaultFarPlane * worldScales[m_preferences.unitsPerMetre], DefaultFOV);
        }
    }

    m_scene.forwardMessage(msg);
}

bool ModelState::simulate(float dt)
{
    m_scene.simulate(dt);
	return false;
}

void ModelState::render()
{
	//draw any renderable systems
    m_scene.render(cro::App::getWindow());
}

//private
void ModelState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
}

void ModelState::loadAssets()
{
    //create a default material to display models on import
    auto flags = cro::ShaderResource::DiffuseColour;
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::VertexLit, flags);
    materialIDs[MaterialID::Default] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(materialIDs[MaterialID::Default]).setProperty("u_colour", cro::Colour(1.f, 0.f, 1.f));

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::Skinning | cro::ShaderResource::DepthMap);
    materialIDs[MaterialID::DefaultShadow] = m_resources.materials.add(m_resources.shaders.get(shaderID));

    //used for drawing debug lines
    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    materialIDs[MaterialID::DebugDraw] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(materialIDs[MaterialID::DebugDraw]).blendMode = cro::Material::BlendMode::Alpha;

    //texture for material thumbnail - TODO we'll unhackify this when rendering real time
    //thumbnail textures is implemented
    m_resources.textures.load(99999, "assets/images/material.png");
    m_materialThumb = m_resources.textures.get(99999).getGLHandle();

    cro::Image img;
    img.create(2, 2, cro::Colour::Black());
    LOG("Add creating textures from images", cro::Logger::Type::Info);
    m_blackTexture.create(2, 2);
    m_blackTexture.update(img.getPixelData());
}

void ModelState::createScene()
{
    //create ground plane
    const float GridWidth = 3.f;
    std::vector<float> verts;
    for (auto y = 0; y < 4; ++y)
    {
        for (auto x = 0; x < 4; ++x)
        {
            verts.push_back(static_cast<float>(x) - (GridWidth / 2.f));
            verts.push_back(0.f);
            verts.push_back(static_cast<float>(y) - (GridWidth / 2.f));

            verts.push_back(1.f);
            verts.push_back(1.f);
            verts.push_back(1.f);
            verts.push_back(1.f);
        }
    }
    std::vector<std::uint32_t> indices =
    {
        0,3,15,12,0,
        1,13,14,2,3,
        7,4,8,11
    };

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().setScale({ 0.f, 0.f, 0.f });

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    auto material = m_resources.materials.get(materialIDs[MaterialID::DebugDraw]);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto& meshData = entity.getComponent<cro::Model>().getMeshData();
    meshData.vertexCount = verts.size() / (meshData.vertexSize / sizeof(float));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto& submesh = meshData.indexData[0];
    submesh.indexCount = indices.size();
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    entities[EntityID::GroundPlane] = entity;

    //create the camera - using a custom camera prevents the scene updating the projection on window resize
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(DefaultCameraPosition);
    entity.addComponent<cro::Camera>();
    updateView(entity, DefaultFarPlane, DefaultFOV);
    m_scene.setActiveCamera(entity);

    entities[EntityID::CamController] = m_scene.createEntity();
    entities[EntityID::CamController].addComponent<cro::Transform>();
    //entities[EntityID::CamController].addComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entities[EntityID::GroundPlane].getComponent<cro::Transform>());

    //axis icon
    meshID = m_resources.meshes.loadMesh(OriginIconBuilder());
    material = m_resources.materials.get(materialIDs[MaterialID::DebugDraw]);
    material.enableDepthTest = false;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = worldScales[m_preferences.unitsPerMetre];
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
    };
    entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //set the default sunlight properties
    m_scene.getSystem<cro::ShadowMapRenderer>().setProjectionOffset({ 0.f, 6.f, -5.f });
    m_scene.getSunlight().setDirection({ -0.f, -1.f, -0.f });
    m_scene.getSunlight().setProjectionMatrix(glm::ortho(-5.6f, 5.6f, -5.6f, 5.6f, 0.1f, 80.f));
}

void ModelState::buildUI()
{
    loadPrefs();
    registerWindow([&]()
        {
            if(ImGui::BeginMainMenuBar())
            {
                //file menu
                if (ImGui::BeginMenu("File##Model"))
                {
                    if (ImGui::MenuItem("Open Model", nullptr, nullptr))
                    {
                        openModel();
                    }
                    if (ImGui::MenuItem("Close Model", nullptr, nullptr))
                    {
                        closeModel();
                    }

                    if (ImGui::MenuItem("Import Model", nullptr, nullptr))
                    {
                        importModel();
                    }
                    if (ImGui::MenuItem("Export Model", nullptr, nullptr, !m_importedVBO.empty()))
                    {
                        exportModel();
                    }
                    
                    if (getStateCount() > 1)
                    {
                        if (ImGui::MenuItem("Return To World Editor"))
                        {
                            getContext().mainWindow.setTitle("Crogine Editor");
                            requestStackPop();
                        }
                    }
                    
                    if (ImGui::MenuItem("Quit", nullptr, nullptr))
                    {
                        cro::App::quit();
                    }
                    ImGui::EndMenu();
                }

                //view menu
                if (ImGui::BeginMenu("View"))
                {
                    if (ImGui::MenuItem("Options", nullptr, nullptr))
                    {
                        m_showPreferences = true;
                    }
                    if (ImGui::MenuItem("Ground Plane", nullptr, &m_showGroundPlane))
                    {
                        if (m_showGroundPlane)
                        {
                            //set this to whichever world scale we're currently using
                            updateWorldScale();
                        }
                        else
                        {
                            entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale({ 0.f, 0.f, 0.f });
                        }
                        savePrefs();
                    }
                    if (ImGui::MenuItem("Show Skybox", nullptr, &m_showSkybox))
                    {
                        if (m_showSkybox)
                        {
                            m_scene.enableSkybox();
                        }
                        else
                        {
                            m_scene.disableSkybox();
                        }
                        savePrefs();
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            //options window
            if (m_showPreferences)
            {
                ImGui::SetNextWindowSize({ 400.f, 260.f });
                if (ImGui::Begin("Preferences", &m_showPreferences))
                {
                    ImGui::Text("%s", "Working Directory:");
                    if (m_preferences.workingDirectory.empty())
                    {
                        ImGui::Text("%s", "Not Set");
                    }
                    else
                    {
                        auto dir = m_preferences.workingDirectory.substr(0, 30) + "...";
                        ImGui::Text("%s", dir.c_str());
                        ImGui::SameLine();
                        HelpMarker(m_preferences.workingDirectory.c_str());
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse"))
                    {
                        auto path = cro::FileSystem::openFolderDialogue(m_preferences.workingDirectory);
                        if (!path.empty())
                        {
                            m_preferences.workingDirectory = path;
                            std::replace(m_preferences.workingDirectory.begin(), m_preferences.workingDirectory.end(), '\\', '/');
                        }
                    }

                    ImGui::PushItemWidth(100.f);
                    //world scale selection
                    const char* items[] = { "0.01", "0.1", "1", "10", "100", "1000" };
                    static const char* currentItem = items[m_preferences.unitsPerMetre];
                    if (ImGui::BeginCombo("World Scale (units per metre)", currentItem))
                    {
                        for (auto i = 0u; i < worldScales.size(); ++i)
                        {
                            bool selected = (currentItem == items[i]);
                            if (ImGui::Selectable(items[i], selected))
                            {
                                currentItem = items[i];
                                m_preferences.unitsPerMetre = i;
                                updateWorldScale();
                            }

                            if (selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();
                    
                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();

                    
                    if (ImGui::ColorEdit3("Sky Top", m_preferences.skyTop.asArray()))
                    {
                        m_scene.setSkyboxColours(m_preferences.skyBottom, m_preferences.skyTop);
                    }
                    if (ImGui::ColorEdit3("Sky Bottom", m_preferences.skyBottom.asArray()))
                    {
                        m_scene.setSkyboxColours(m_preferences.skyBottom, m_preferences.skyTop);
                    }

                    ImGui::NewLine();
                    ImGui::NewLine();
                    if (!m_showPreferences ||
                        ImGui::Button("Close"))
                    {
                        savePrefs();
                        m_showPreferences = false;
                    }
                }
                ImGui::End();
            }

            drawInspector();
            drawBrowser();
        });

    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);
}

void ModelState::openModel()
{
    auto path = cro::FileSystem::openFileDialogue(m_preferences.lastModelDirectory, "cmt");
    if (!path.empty()
        && cro::FileSystem::getFileExtension(path) == ".cmt")
    {
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
    closeModel();

    cro::ModelDefinition def(m_preferences.workingDirectory);
    if (def.loadFromFile(path, m_resources))
    {
        entities[EntityID::ActiveModel] = m_scene.createEntity();
        entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entities[EntityID::ActiveModel].addComponent<cro::Transform>());

        def.createModel(entities[EntityID::ActiveModel], m_resources);
        m_currentModelConfig.loadFromFile(path);

        if (entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData().boundingSphere.radius > (2.f * worldScales[m_preferences.unitsPerMetre]))
        {
            cro::Logger::log("Bounding sphere radius is very large - model may not be visible", cro::Logger::Type::Warning);
        }

        //if this is an IQM file load the vert data into the import
        //structures so we can adjust the model and re-export if needed
        if (cro::FileSystem::getFileExtension(path) == "*.iqm")
        {
            importIQM(path);
        }

        //parse all of the material properties into the material/texture browser
        const auto& objects = m_currentModelConfig.getObjects();
        for (const auto& obj : objects)
        {
            if (obj.getName() == "material")
            {
                std::string_view id = obj.getId();
                if (id == "Unlit")
                {
                    //TODO add an unlit material
                }
                else if (id == "VertexLit")
                {
                    //TODO add a vertex lit material
                }
                //TODO PBR shader

                //read textures
                const auto& properties = obj.getProperties();
                for (const auto& prop : properties)
                {
                    std::string_view name = prop.getName();
                    if (name == "diffuse"
                        || name == "mask"
                        || name == "normal")
                    {
                        auto path = m_preferences.workingDirectory + "/" + prop.getValue<std::string>();
                        addTextureToBrowser(path);

                        //TODO map these to the current created material properties
                    }
                }
            }
        }
    }
    else
    {
        cro::Logger::log("Check current working directory (Options)?", cro::Logger::Type::Error);
    }
}

void ModelState::closeModel()
{
    if (entities[EntityID::ActiveModel].isValid())
    {
        m_scene.destroyEntity(entities[EntityID::ActiveModel]);

        m_importedIndexArrays.clear();
        m_importedVBO.clear();
    }

    if (entities[EntityID::NormalVis].isValid())
    {
        m_scene.destroyEntity(entities[EntityID::NormalVis]);
    }

    m_currentModelConfig = {};
}

void ModelState::importModel()
{
    auto calcFlags = [](const aiMesh * mesh, std::uint32_t& vertexSize)->std::uint8_t
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

    auto path = cro::FileSystem::openFileDialogue(m_preferences.lastImportDirectory, "obj,dae,fbx");
    if (!path.empty())
    {
        ai::Importer importer;
        auto* scene = importer.ReadFile(path, 
                            aiProcess_CalcTangentSpace
                            | aiProcess_GenSmoothNormals
                            | aiProcess_Triangulate
                            | aiProcess_JoinIdenticalVertices
                            | aiProcess_OptimizeMeshes
                            | aiProcess_GenBoundingBoxes);

        if(scene && scene->HasMeshes())
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

            //update header with array sizes
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

                
                //TODO check the size and flush after a certain amount
                //remembering to reload the ground plane..
                //m_resources.meshes.flush();
                ImportedMeshBuilder builder(m_importedHeader, m_importedVBO, m_importedIndexArrays, header.flags);
                auto meshID = m_resources.meshes.loadMesh(builder);

                entities[EntityID::ActiveModel] = m_scene.createEntity();
                entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entities[EntityID::ActiveModel].addComponent<cro::Transform>());
                entities[EntityID::ActiveModel].addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(materialIDs[MaterialID::Default]));
                
                for (auto i = 0; i < header.arrayCount; ++i)
                {
                    entities[EntityID::ActiveModel].getComponent<cro::Model>().setShadowMaterial(i, m_resources.materials.get(materialIDs[MaterialID::DefaultShadow]));
                }
                entities[EntityID::ActiveModel].addComponent<cro::ShadowCaster>();

                m_importedTransform = {};

                //updateNormalVis();
            }
            else
            {
                cro::Logger::log("Failed opening " + path, cro::Logger::Type::Error);
                cro::Logger::log("Missing index or vertex data", cro::Logger::Type::Error);
            }
        }

        m_preferences.lastImportDirectory = cro::FileSystem::getFilePath(path);
        savePrefs();
    }
}

void ModelState::exportModel()
{
    //TODO assert we at least have valid header data
    //prevent accidentally writing a bad file

    auto path = cro::FileSystem::saveFileDialogue(m_preferences.lastExportDirectory, "cmf");
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
            SDL_RWwrite(file.file, &m_importedHeader.flags, sizeof(CMFHeader::flags), 1);
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
            if (!m_preferences.workingDirectory.empty() && path.find(m_preferences.workingDirectory) != std::string::npos)
            {
                meshPath = path.substr(m_preferences.workingDirectory.length() + 1);
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
            
            //TODO make this an option so we don't accidentally overwrite files
            if (!cro::FileSystem::fileExists(path))
            {
                cfg.save(path);
            }

            m_preferences.lastExportDirectory = cro::FileSystem::getFilePath(path);
            savePrefs();

            openModelAtPath(path);
        }
    }
}

void ModelState::applyImportTransform()
{
    //keep rotation separate as we don't apply scale to normal data
    auto rotation = glm::toMat4(glm::toQuat(glm::orientate3(m_importedTransform.rotation)));
    auto transform = rotation * glm::scale(glm::mat4(1.f), glm::vec3(m_importedTransform.scale));

    auto meshData = entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
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

    auto applyTransform = [&](const glm::mat4& tx, std::size_t idx)
    {
        glm::vec4 v(m_importedVBO[idx], m_importedVBO[idx + 1], m_importedVBO[idx + 2], 1.f);
        v = tx * v;
        m_importedVBO[idx] = v.x;
        m_importedVBO[idx+1] = v.y;
        m_importedVBO[idx+2] = v.z;
    };

    //loop over the vertex data and modify
    for (std::size_t i = 0u; i < m_importedVBO.size(); i += vertexSize)
    {
        //position
        applyTransform(transform, i);

        if (normalOffset != 0)
        {
            auto idx = i + normalOffset;
            applyTransform(rotation, idx);
        }

        if (tanOffset != 0)
        {
            auto idx = i + tanOffset;
            applyTransform(rotation, idx);
        }

        if (bitanOffset != 0)
        {
            auto idx = i + bitanOffset;
            applyTransform(rotation, idx);
        }
    }

    //upload the data to the preview model
    glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, m_importedVBO.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_importedTransform = {};
    entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
    entities[EntityID::ActiveModel].getComponent<cro::Transform>().setRotation(glm::vec3(0.f));
}

void ModelState::loadPrefs()
{
    cro::ConfigFile prefs;
    if (prefs.loadFromFile(prefPath))
    {
        const auto& props = prefs.getProperties();
        for (const auto& prop : props)
        {
            auto name = cro::Util::String::toLower(prop.getName());
            if (name == "working_dir")
            {
                m_preferences.workingDirectory = prop.getValue<std::string>();
                std::replace(m_preferences.workingDirectory.begin(), m_preferences.workingDirectory.end(), '\\', '/');
            }
            else if (name == "units_per_metre")
            {
                m_preferences.unitsPerMetre = cro::Util::Maths::clamp(static_cast<std::size_t>(prop.getValue<std::int32_t>()), std::size_t(0u), worldScales.size());
            }
            else if (name == "show_groundplane")
            {
                m_showGroundPlane = prop.getValue<bool>();
            }
            else if (name == "show_skybox")
            {
                m_showSkybox = prop.getValue<bool>();
                if (m_showSkybox)
                {
                    m_scene.enableSkybox();
                }
            }
            else if (name == "sky_top")
            {
                m_preferences.skyTop = prop.getValue<cro::Colour>();
                m_scene.setSkyboxColours(m_preferences.skyBottom, m_preferences.skyTop);
            }
            else if (name == "sky_bottom")
            {
                m_preferences.skyBottom = prop.getValue<cro::Colour>();
                m_scene.setSkyboxColours(m_preferences.skyBottom, m_preferences.skyTop);
            }
            else if (name == "import_dir")
            {
                m_preferences.lastImportDirectory = prop.getValue<std::string>();
            }
            else if (name == "export_dir")
            {
                m_preferences.lastExportDirectory = prop.getValue<std::string>();
            }
            else if (name == "model_dir")
            {
                m_preferences.lastModelDirectory = prop.getValue<std::string>();
            }
        }

        updateWorldScale();
    }
}

void ModelState::savePrefs()
{
    auto toString = [](cro::Colour c)->std::string
    {
        return std::to_string(c.getRed()) + ", " + std::to_string(c.getGreen()) + ", " + std::to_string(c.getBlue()) + ", " + std::to_string(c.getAlpha());
    };

    cro::ConfigFile prefsOut;
    prefsOut.addProperty("working_dir", m_preferences.workingDirectory);
    prefsOut.addProperty("units_per_metre", std::to_string(m_preferences.unitsPerMetre));
    prefsOut.addProperty("show_groundplane", m_showGroundPlane ? "true" : "false");
    prefsOut.addProperty("show_skybox", m_showSkybox ? "true" : "false");
    prefsOut.addProperty("sky_top", toString(m_preferences.skyTop));
    prefsOut.addProperty("sky_bottom", toString(m_preferences.skyBottom));

    prefsOut.addProperty("import_dir", m_preferences.lastImportDirectory);
    prefsOut.addProperty("export_dir", m_preferences.lastExportDirectory);
    prefsOut.addProperty("model_dir", m_preferences.lastModelDirectory);

    prefsOut.save(prefPath);
}

void ModelState::updateWorldScale()
{
    const float scale = worldScales[m_preferences.unitsPerMetre];
    if (m_showGroundPlane)
    {
        entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale({ scale, scale, scale });
    }
    else
    {
        entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    }
    m_scene.getActiveCamera().getComponent<cro::Transform>().setPosition(DefaultCameraPosition * scale);
    
    entities[EntityID::CamController].getComponent<cro::Transform>().setPosition(glm::vec3(0.f));
    updateView(m_scene.getActiveCamera(), DefaultFarPlane * worldScales[m_preferences.unitsPerMetre], DefaultFOV);
}

void ModelState::updateNormalVis()
{
    if (entities[EntityID::ActiveModel].isValid())
    {
        if (entities[EntityID::NormalVis].isValid())
        {
            m_scene.destroyEntity(entities[EntityID::NormalVis]);
        }

        auto meshData = entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();

        //pass to mesh builder - TODO we would be better recycling the VBO with new vertex data, rather than
        //destroying and creating a new one (unique instances will build up in the resource manager)
        auto meshID = m_resources.meshes.loadMesh(NormalVisMeshBuilder(meshData, m_importedVBO));

        entities[EntityID::NormalVis] = m_scene.createEntity();
        entities[EntityID::NormalVis].addComponent<cro::Transform>();
        entities[EntityID::NormalVis].addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(materialIDs[MaterialID::DebugDraw]));
    }
}

void ModelState::updateMouseInput(const cro::Event& evt)
{
    const float moveScale = 0.004f;
    if (evt.motion.state & SDL_BUTTON_LMASK)
    {
        float pitchMove = static_cast<float>(evt.motion.yrel)* moveScale;
        float yawMove = static_cast<float>(evt.motion.xrel)* moveScale;

        auto& tx = entities[EntityID::CamController].getComponent<cro::Transform>();

        glm::quat pitch = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), pitchMove, glm::vec3(1.f, 0.f, 0.f));
        glm::quat yaw = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yawMove, glm::vec3(0.f, 1.f, 0.f));

        auto rotation =  pitch * yaw * tx.getRotationQuat();
        tx.setRotation(rotation);
    }
    else if (evt.motion.state & SDL_BUTTON_RMASK)
    {
        //do roll
        float rollMove = static_cast<float>(-evt.motion.xrel)* moveScale;

        auto& tx = entities[EntityID::CamController].getComponent<cro::Transform>();

        glm::quat roll = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), rollMove, glm::vec3(0.f, 0.f, 1.f));

        auto rotation = roll * tx.getRotationQuat();
        tx.setRotation(rotation);
    }
    else if (evt.motion.state & SDL_BUTTON_MMASK)
    {
        auto& tx = entities[EntityID::CamController].getComponent<cro::Transform>();
        //TODO multiply this by zoom factor
        tx.move((glm::vec3(evt.motion.xrel, -evt.motion.yrel, 0.f) / 60.f) * worldScales[m_preferences.unitsPerMetre]);
    }
}

std::uint32_t ModelState::addTextureToBrowser(const std::string& path)
{
    auto fileName = cro::FileSystem::getFileName(path);

    std::uint32_t id = 0;
    for (auto& [i, t] : m_materialTextures)
    {
        if (t.name == fileName)
        {
            id = i;
            break;
        }
    }
    m_materialTextures.erase(id);
    m_selectedTexture = 0;

    MaterialTexture tex;
    tex.texture = std::make_unique<cro::Texture>();
    if (tex.texture->loadFromFile(path))
    {
        m_selectedTexture = tex.texture->getGLHandle();
        tex.name = fileName;
        m_materialTextures.insert(std::make_pair(tex.texture->getGLHandle(), std::move(tex)));
        return m_selectedTexture;
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed to open image", cro::FileSystem::OK, cro::FileSystem::Error);
    }
    return 0;
}

void ModelState::updateLayout(std::int32_t w, std::int32_t h)
{
    float width = static_cast<float>(w);
    float height = static_cast<float>(h);

    WindowLayouts[WindowID::Inspector] = 
        std::make_pair(glm::vec2(0.f, ui::TitleHeight), 
                        glm::vec2(width * ui::InspectorWidth, height - (ui::TitleHeight/* + (height * ui::BrowserHeight)*/)));

    WindowLayouts[WindowID::Browser] = 
        std::make_pair(glm::vec2(width * ui::InspectorWidth, height - (height * ui::BrowserHeight)),
                        glm::vec2(width - (width * ui::InspectorWidth), height * ui::BrowserHeight));
}

void ModelState::drawInspector()
{
    auto [pos, size] = WindowLayouts[WindowID::Inspector];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::BeginTabBar("##a");

        if (ImGui::BeginTabItem("Model"))
        {
            //model details
            if (entities[EntityID::ActiveModel].isValid())
            {
                std::string worldScale("World Scale:\n");
                worldScale += std::to_string(worldScales[m_preferences.unitsPerMetre]);
                worldScale += " units per metre";
                ImGui::Text("%s", worldScale.c_str());

                if (!m_importedVBO.empty())
                {
                    ImGui::Separator();

                    std::string flags = "Flags:\n";
                    if (m_importedHeader.flags & cro::VertexProperty::Position)
                    {
                        flags += "Position\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::Colour)
                    {
                        flags += "Colour\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::Normal)
                    {
                        flags += "Normal\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::Tangent)
                    {
                        flags += "Tan/Bitan\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::UV0)
                    {
                        flags += "Texture Coords\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::UV1)
                    {
                        flags += "Shadowmap Coords\n";
                    }
                    ImGui::Text("%s", flags.c_str());
                    ImGui::Text("Materials: %d", m_importedHeader.arrayCount);

                    ImGui::NewLine();
                    ImGui::Text("Transform"); ImGui::SameLine(); HelpMarker("Double Click to change Values");
                    if (ImGui::DragFloat3("Rotation", &m_importedTransform.rotation[0], -180.f, 180.f))
                    {
                        entities[EntityID::ActiveModel].getComponent<cro::Transform>().setRotation(m_importedTransform.rotation * cro::Util::Const::degToRad);
                    }
                    if (ImGui::DragFloat("Scale", &m_importedTransform.scale, 0.1f, 10.f))
                    {
                        //scale needs to be uniform, else we'd have to recalc all the normal data
                        entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(m_importedTransform.scale));
                    }
                    if (ImGui::Button("Apply"))
                    {
                        applyImportTransform();
                    }
                    ImGui::SameLine();
                    HelpMarker("Applies this transform directly to the vertex data, before exporting the model.\nUseful if an imported model uses z-up coordinates, or is much\nlarger or smaller than other models in the scene.");
                }


                if (entities[EntityID::ActiveModel].hasComponent<cro::Skeleton>())
                {
                    auto& skeleton = entities[EntityID::ActiveModel].getComponent<cro::Skeleton>();

                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();

                    ImGui::Text("Animations: %d", skeleton.animations.size());
                    static std::string label("Stopped");
                    if (skeleton.animations.empty())
                    {
                        label = "No Animations Found.";
                    }
                    else
                    {
                        static int currentAnim = 0;
                        auto prevAnim = currentAnim;

                        if (ImGui::InputInt("Anim", &currentAnim, 1, 1)
                            && !skeleton.animations[currentAnim].playing)
                        {
                            currentAnim = std::min(currentAnim, static_cast<int>(skeleton.animations.size()) - 1);
                        }
                        else
                        {
                            currentAnim = prevAnim;
                        }

                        ImGui::SameLine();
                        if (skeleton.animations[currentAnim].playing)
                        {
                            if (ImGui::Button("Stop"))
                            {
                                skeleton.animations[currentAnim].playing = false;
                                label = "Stopped";
                            }
                        }
                        else
                        {
                            if (ImGui::Button("Play"))
                            {
                                skeleton.play(currentAnim);
                                label = "Playing " + skeleton.animations[currentAnim].name;
                            }
                            else
                            {
                                label = "Stopped";
                            }
                        }
                    }
                    ImGui::Text("%s", label.c_str());
                }
            }
            
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Material"))
        {
            if (!m_materialDefs.empty())
            {
                ImGui::PushItemWidth(size.x * ui::TextBoxWidth);
                ImGui::InputText("##name", &m_materialDefs[m_selectedMaterial].name);
                ImGui::PopItemWidth();

                auto imgSize = size;
                imgSize.x *= ui::MaterialPreviewWidth;
                imgSize.y = imgSize.x;

                ImGui::SetCursorPos({ pos.x + ((size.x - imgSize.y) / 2.f), pos.y + 60.f });
                ImGui::Image((void*)(std::size_t)m_materialThumb, { imgSize.x, imgSize.y }, { 0.f, 1.f }, { 1.f, 0.f });

                ImGui::NewLine();
                ImGui::Text("Shader Type:");
                ImGui::PushItemWidth(size.x* ui::TextBoxWidth);
                if (ImGui::BeginCombo("##Shader", MaterialDefinition::TypeStrings[m_materialDefs[m_selectedMaterial].type].c_str()))
                {
                    for (auto i = 0; i < MaterialDefinition::TypeStrings.size(); ++i)
                    {
                        bool selected = m_materialDefs[m_selectedMaterial].type == i;
                        if (ImGui::Selectable(MaterialDefinition::TypeStrings[i].c_str(), selected))
                        {
                            m_materialDefs[m_selectedMaterial].type = static_cast<MaterialDefinition::Type>(i);
                        }

                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();


                ImGui::NewLine();
                ImGui::Text("Texture Maps:");
                auto type = m_materialDefs[m_selectedMaterial].type;
                std::string slotLabel;
                std::uint32_t thumb = m_blackTexture.getGLHandle();
                if (type != MaterialDefinition::PBR)
                {
                    //diffuse map
                    slotLabel = "Diffuse";
                    
                    if (m_materialDefs[m_selectedMaterial].diffuse == 0)
                    {
                        slotLabel += ": Empty";
                    }
                    else
                    {
                        slotLabel += ": " + m_materialTextures.at(m_materialDefs[m_selectedMaterial].diffuse).name;
                        thumb = m_materialDefs[m_selectedMaterial].diffuse;
                    }
                    drawTextureSlot(slotLabel, m_materialDefs[m_selectedMaterial].diffuse, thumb);

                    //lightmap
                    slotLabel = "Light Map";
                    if (m_materialDefs[m_selectedMaterial].lightmap == 0)
                    {
                        slotLabel += ": Empty";
                        thumb = m_blackTexture.getGLHandle();
                    }
                    else
                    {
                        slotLabel += ": " + m_materialTextures.at(m_materialDefs[m_selectedMaterial].lightmap).name;
                        thumb = m_materialDefs[m_selectedMaterial].lightmap;
                    }
                    drawTextureSlot(slotLabel, m_materialDefs[m_selectedMaterial].lightmap, thumb);
                }

                if (type == MaterialDefinition::VertexLit)
                {
                    //mask map
                    slotLabel = "Mask Map";
                    if (m_materialDefs[m_selectedMaterial].mask == 0)
                    {
                        slotLabel += ": Empty";
                        thumb = m_blackTexture.getGLHandle();
                    }
                    else
                    {
                        slotLabel += ": " + m_materialTextures.at(m_materialDefs[m_selectedMaterial].mask).name;
                        thumb = m_materialDefs[m_selectedMaterial].mask;
                    }
                    drawTextureSlot(slotLabel, m_materialDefs[m_selectedMaterial].mask, thumb);
                }

                if (type != MaterialDefinition::Unlit)
                {
                    //normal map
                    slotLabel = "Normal Map";
                    if (m_materialDefs[m_selectedMaterial].normal == 0)
                    {
                        slotLabel += ": Empty";
                        thumb = m_blackTexture.getGLHandle();
                    }
                    else
                    {
                        slotLabel += ": " + m_materialTextures.at(m_materialDefs[m_selectedMaterial].normal).name;
                        thumb = m_materialDefs[m_selectedMaterial].normal;
                    }
                    drawTextureSlot(slotLabel, m_materialDefs[m_selectedMaterial].normal, thumb);
                }

                if (type == MaterialDefinition::PBR)
                {
                    ImGui::Text("Coming Soon!");
                    //metal map
                    //drawTextureSlot("Metalness", m_materialDefs[m_selectedMaterial].metalness, m_materialThumb);
                    //spec map
                    //ao map
                }
            }
            else
            {
                ImGui::Text("Add a material in the Browser window");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();

    
}

void ModelState::drawBrowser()
{
    ImGui::ShowDemoWindow();

    auto [pos, size] = WindowLayouts[WindowID::Browser];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Browser", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("##b");
        if (ImGui::BeginTabItem("Materials"))
        {
            auto thumbSize = size;
            thumbSize.y *= ui::ThumbnailHeight;
            thumbSize.x = thumbSize.y;

            std::int32_t thumbsPerRow = static_cast<std::int32_t>(std::floor(size.x / thumbSize.x) - 1);
            auto currentRow = m_selectedMaterial / thumbsPerRow;

            if (ImGui::Button("Add##01"))
            {
                m_materialDefs.emplace_back();
                m_selectedMaterial = m_materialDefs.size() - 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Open##01"))
            {

            }
            ImGui::SameLine();
            if (ImGui::Button("Remove##01"))
            {
                if (!m_materialDefs.empty()
                    && cro::FileSystem::showMessageBox("Delete?", "Remove the selected material?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                {
                    m_materialDefs.erase(m_materialDefs.begin() + m_selectedMaterial);
                    if (m_selectedMaterial >= m_materialDefs.size()
                        && !m_materialDefs.empty())
                    {
                        m_selectedMaterial--;
                    }
                }
            }

            std::int32_t count = 0;
            for (const auto& material : m_materialDefs)
            {
                ImGui::PushStyleColor(ImGuiCol_Border, count == m_selectedMaterial ? ImVec4(1.f, 1.f, 0.f, 1.f) : ImVec4(0.f, 0.f, 0.f, 1.f));
                ImGui::PushID(9999 + count);
                if (ImGui::ImageButton((void*)(std::size_t)m_materialThumb, { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f }))
                {
                    m_selectedMaterial = count;
                }
                ImGui::PopID();
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    //set payload to carry the index of the material
                    ImGui::SetDragDropPayload("MATERIAL_SRC", &count, sizeof(cro::int32));

                    //display preview
                    ImGui::Image((void*)(std::size_t)m_materialThumb, { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f });
                    ImGui::Text("%s", material.name.c_str());
                    ImGui::Text("%s", MaterialDefinition::TypeStrings[material.type]);
                    ImGui::EndDragDropSource();

                    m_selectedMaterial = count;
                }

                //put on same line if we fit
                count++;
                if (count % thumbsPerRow != 0)
                {
                    ImGui::SameLine();
                }
            }
            //scroll to selected
            /*if (currentRow != m_selectedMaterial / thumbsPerRow)
            {
                ImGui::SetScrollY((currentRow + 1) * thumbSize.y);
            }*/

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Textures"))
        {
            if (ImGui::Button("Add##00"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
                if (!path.empty())
                {
                    addTextureToBrowser(path);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove##00"))
            {
                if (m_selectedTexture != 0
                    && cro::FileSystem::showMessageBox("Delete?", "Remove this texture?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                {
                    m_materialTextures.erase(m_selectedTexture);
                    m_selectedTexture = m_materialTextures.empty() ? 0 : m_materialTextures.begin()->first;

                    //TODO update any materials which were using this texture...
                    LOG("UPDATE MATERIALS WHICH USED THIS TEXTURE", cro::Logger::Type::Info);
                }
            }

            auto thumbSize = size;
            thumbSize.y *= ui::ThumbnailHeight;
            thumbSize.x = thumbSize.y;

            std::int32_t thumbsPerRow = static_cast<std::int32_t>(std::floor(size.x / thumbSize.x) - 1);
            std::int32_t count = 0;
            for (const auto& [id, tex] : m_materialTextures)
            {
                ImGui::PushStyleColor(ImGuiCol_Border, id == m_selectedTexture ? ImVec4(1.f, 1.f, 0.f, 1.f) : ImVec4(0.f,0.f,0.f,1.f));
                if (ImGui::ImageButton((void*)(std::size_t)id, { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f }))
                {
                    m_selectedTexture = id;
                }
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    //set payload to carry the GL id of the texture
                    ImGui::SetDragDropPayload("TEXTURE_SRC", &id, sizeof(cro::uint32));

                    //display preview (could be anything, e.g. when dragging an image we could decide to display
                    //the filename and a small preview of the image, etc.)
                    ImGui::Image((void*)(std::size_t)id, { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f });
                    ImGui::Text("%s", tex.name.c_str());
                    ImGui::EndDragDropSource();

                    m_selectedTexture = id;
                }
                
                //put on same line if we fit
                count++;
                if (count % thumbsPerRow != 0)
                {
                    ImGui::SameLine();
                }
            }
            
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}