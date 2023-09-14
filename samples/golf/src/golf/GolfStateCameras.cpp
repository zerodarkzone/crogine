/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "GolfState.hpp"
#include "FpsCameraSystem.hpp"
#include "CommandIDs.hpp"

#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>

#include <crogine/detail/glm/gtx/rotate_vector.hpp>

#include "../ErrorCheck.hpp"

void GolfState::createCameras()
{
    //update the 3D view - applied on player cam and transition cam
    auto updateView = [&](cro::Camera& cam)
        {
            auto winSize = glm::vec2(cro::App::getWindow().getSize());
            float maxScale = getViewScale();
            float scale = m_sharedData.pixelScale ? maxScale : 1.f;
            auto texSize = winSize / scale;

            //only want to resize the buffer once !!
            if (cam == m_cameras[CameraID::Player].getComponent<cro::Camera>())
            {
                std::uint32_t samples = m_sharedData.pixelScale ? 0 :
                    m_sharedData.antialias ? m_sharedData.multisamples : 0;

                cro::RenderTarget::Context ctx;
                ctx.depthBuffer = true;
#ifdef __APPLE__
                //*sigh*
                ctx.depthTexture = false;
#else
                ctx.depthTexture = true;
#endif
                ctx.samples = samples;
                ctx.width = static_cast<std::uint32_t>(texSize.x);
                ctx.height = static_cast<std::uint32_t>(texSize.y);

                m_sharedData.antialias =
                    m_gameSceneTexture.create(ctx)
                    && m_sharedData.multisamples != 0
                    && !m_sharedData.pixelScale;

                auto invScale = (maxScale + 1.f) - scale;
                glCheck(glPointSize(invScale * BallPointSize));
                glCheck(glLineWidth(invScale));

                m_scaleBuffer.setData(invScale);

                m_resolutionUpdate.resolutionData.resolution = texSize / invScale;
                m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);


                //this lets the shader scale leaf billboards correctly
                if (m_sharedData.treeQuality == SharedStateData::High)
                {
                    auto targetHeight = texSize.y;
                    glUseProgram(m_resources.shaders.get(ShaderID::TreesetLeaf).getGLHandle());
                    glUniform1f(m_resources.shaders.get(ShaderID::TreesetLeaf).getUniformID("u_targetHeight"), targetHeight);
                }
            }

            //fetch this explicitly so the transition cam also gets the correct zoom
            float zoom = m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().fov;
            cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad * zoom, texSize.x / texSize.y, 0.1f, static_cast<float>(MapSize.x), m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    camEnt.addComponent<CameraFollower::ZoomData>().target = 1.f; //used to set zoom when putting.
    camEnt.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& cam = e.getComponent<cro::Camera>();
            auto& zoom = e.getComponent<CameraFollower::ZoomData>();
            float diff = zoom.target - zoom.fov;

            CRO_ASSERT(zoom.target > 0, "");

            if (std::fabs(diff) > 0.001f)
            {
                zoom.fov += (diff * dt) * 3.f;
            }
            else
            {
                zoom.fov = zoom.target;
                e.getComponent<cro::Callback>().active = false;
            }

            auto fov = m_sharedData.fov * cro::Util::Const::degToRad * zoom.fov;
            cam.setPerspective(fov, cam.getAspectRatio(), 0.1f, static_cast<float>(MapSize.x), m_shadowQuality.cascadeCount);
            m_cameras[CameraID::Transition].getComponent<cro::Camera>().setPerspective(fov, cam.getAspectRatio(), 0.1f, static_cast<float>(MapSize.x), m_shadowQuality.cascadeCount);
        };

    m_cameras[CameraID::Player] = camEnt;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

    static constexpr std::uint32_t ReflectionMapSize = 1024u;
    const std::uint32_t ShadowMapSize = m_sharedData.hqShadows ? 4096u : 2048u;

    //used by transition callback to interp camera
    camEnt.addComponent<TargetInfo>().waterPlane = m_waterEnt;
    camEnt.getComponent<TargetInfo>().targetLookAt = m_holeData[0].target;
    cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    cam.reflectionBuffer.setSmooth(true);
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    cam.setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    cam.setShadowExpansion(20.f);

    //create an overhead camera
    auto setPerspective = [&](cro::Camera& cam)
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());

            cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.1f, /*vpSize.x*/320.f, m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(DefaultSkycamPosition);
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam) //use explicit callback so we can capture the entity and use it to zoom via CamFollowSystem
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * camEnt.getComponent<CameraFollower>().zoom.fov,
                vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.35f,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };

            //fades billboards with zoom
            if (m_currentCamera == CameraID::Sky)
            {
                m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;
                m_resolutionUpdate.targetFade += (ZoomFadeDistance / 2.f) + (camEnt.getComponent<CameraFollower>().zoom.progress * (ZoomFadeDistance / 2.f));
            }
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize / 2, ShadowMapSize / 2);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::CommandTarget>().ID = CommandID::SpectatorCam;
    camEnt.addComponent<CameraFollower>().radius = SkyCamRadius * SkyCamRadius;
    camEnt.getComponent<CameraFollower>().id = CameraID::Sky;
    camEnt.getComponent<CameraFollower>().zoom.target = 0.25f;// 0.1f;
    camEnt.getComponent<CameraFollower>().zoom.speed = SkyCamZoomSpeed;
    camEnt.getComponent<CameraFollower>().targetOffset = { 0.f,0.65f,0.f };
    camEnt.addComponent<cro::AudioListener>();

    //this holds the water plane ent when active
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Sky] = camEnt;


    //same as sky cam, but conrolled by the active player
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(DefaultSkycamPosition);
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * camEnt.getComponent<CameraFollower::ZoomData>().fov,
                vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };

            //fades billboards with zoom
            if (m_currentCamera == CameraID::Drone)
            {
                m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;
                m_resolutionUpdate.targetFade += (ZoomFadeDistance / 2.f) + (camEnt.getComponent<CameraFollower::ZoomData>().progress * (ZoomFadeDistance / 2.f));
            }
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize / 2, ShadowMapSize / 2);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::CommandTarget>().ID = CommandID::DroneCam;
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<CameraFollower::ZoomData>();

    //this holds the water plane ent when active
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Drone] = camEnt;



    //and a green camera
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * camEnt.getComponent<CameraFollower>().zoom.fov,
                vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::CommandTarget>().ID = CommandID::SpectatorCam;
    camEnt.addComponent<CameraFollower>().radius = GreenCamRadiusLarge * GreenCamRadiusLarge;
    camEnt.getComponent<CameraFollower>().id = CameraID::Green;
    camEnt.getComponent<CameraFollower>().zoom.speed = GreenCamZoomFast;
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Green] = camEnt;

    //bystander cam (when remote or CPU player is swinging)
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            //this cam has a slightly narrower FOV
            auto zoomFOV = camEnt.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>().fov;

            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * zoomFOV * 0.7f,
                vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowNearDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();

    CameraFollower::ZoomData zoomData;
    zoomData.speed = 3.f;
    zoomData.target = 0.45f;
    camEnt.addComponent<cro::Callback>().setUserData<CameraFollower::ZoomData>(zoomData);
    camEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& zoom = e.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>();

            zoom.progress = std::min(1.f, zoom.progress + (dt * zoom.speed));
            zoom.fov = glm::mix(1.f, zoom.target, cro::Util::Easing::easeInOutQuad(zoom.progress));
            e.getComponent<cro::Camera>().resizeCallback(e.getComponent<cro::Camera>());

            if (zoom.progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        };
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Bystander] = camEnt;


    //idle cam when player AFKs
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
        {
            //this cam has a slightly narrower FOV
            auto zoomFOV = camEnt.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>().fov;

            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * zoomFOV * 0.7f,
                vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f,
                m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowNearDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    camEnt.addComponent<cro::Callback>().setUserData<CameraFollower::ZoomData>(zoomData); //needed by resize callback, but not used HUM
    camEnt.getComponent<cro::Callback>().active = true;
    camEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            if (e.getComponent<cro::Camera>().active)
            {
                static constexpr glm::vec3 TargetOffset(0.f, 1.f, 0.f);
                auto target = m_currentPlayer.position + TargetOffset;

                static float rads = 0.f;
                rads += (dt * 0.1f);
                glm::vec3 pos(std::cos(rads), 0.f, std::sin(rads));
                pos *= 5.f;
                pos += target;
                pos.y = m_collisionMesh.getTerrain(pos).height + 2.f;

                e.getComponent<cro::Transform>().setPosition(pos);
                e.getComponent<cro::Transform>().setRotation(lookRotation(pos, target));
            }
        };
    setPerspective(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().updateMatrices(camEnt.getComponent<cro::Transform>());
    m_cameras[CameraID::Idle] = camEnt;

    //fly-by cam for transition
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback = updateView;
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(45.f);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    updateView(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Transition] = camEnt;


    //free cam
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&](cro::Camera& cam)
        {
            auto vpSize = glm::vec2(cro::App::getWindow().getSize());
            cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f, m_shadowQuality.cascadeCount);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(15.f);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<FpsCamera>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_freeCam = camEnt;

#ifdef PATH_TRACING
    initBallDebug();
#endif
    addCameraDebugging();



    //unfortuantely this kills the FPS

    const auto createFlightTexture =
        [&]()
    {
            auto texSize = MapSize.y / 2u;

            auto windowScale = getViewScale();
            float scale = m_sharedData.pixelScale ? windowScale : 1.f;
            scale = (windowScale + 1.f) - scale;
            texSize *= static_cast<std::uint32_t>(scale);

            std::uint32_t samples = m_sharedData.pixelScale ? 0 :
                m_sharedData.antialias ? m_sharedData.multisamples : 0;

            cro::RenderTarget::Context ctx;
            ctx.depthBuffer = true;
#ifdef __APPLE__
            //*sigh*
            ctx.depthTexture = false;
#else
            ctx.depthTexture = true;
#endif
            ctx.samples = samples;
            ctx.width = texSize;
            ctx.height = texSize;

            m_flightTexture.create(ctx);

            //TODO this is the UI entity to which we're attached - if we use the green
            //ent we can let the other callback do this for us
            //greenEnt.getComponent<cro::Sprite>().setTexture(m_greenBuffer.getTexture());

            //auto targetScale = glm::vec2(1.f / scale);
            //if (m_currentPlayer.terrain == TerrainID::Green)
            //{
            //    greenEnt.getComponent<cro::Transform>().setScale(targetScale);
            //}
            //greenEnt.getComponent<cro::Transform>().setOrigin({ (texSize / 2), (texSize / 2) }); //must divide to a whole pixel!
            //greenEnt.getComponent<cro::Callback>().getUserData<GreenCallbackData>().targetScale = targetScale.x;
    };
    //createFlightTexture();

    ////follows the ball in flight
    //camEnt = m_gameScene.createEntity();
    //camEnt.addComponent<cro::Transform>();
    //camEnt.addComponent<cro::Camera>().resizeCallback =
    //    [&, createFlightTexture](cro::Camera& cam)
    //    {
    //        createFlightTexture();

    //        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, 1.f, 0.1f, static_cast<float>(MapSize.x) * 1.25f, m_shadowQuality.cascadeCount);
    //        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    //    };
    //camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    //camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    //camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    //camEnt.getComponent<cro::Camera>().active = false;
    //camEnt.getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);
    //camEnt.getComponent<cro::Camera>().setShadowExpansion(15.f);
    //m_flightCam = camEnt;
}

void GolfState::setGreenCamPosition()
{
    auto holePos = m_holeData[m_currentHole].pin;

    if (m_holeData[m_currentHole].puttFromTee)
    {
        auto teePos = m_holeData[m_currentHole].tee;
        auto targetPos = m_holeData[m_currentHole].target;

        if ((glm::length2(m_currentPlayer.position - teePos) <
            glm::length2(m_currentPlayer.position - holePos))
            &&
            (glm::length2(m_currentPlayer.position - targetPos) >
                /*glm::length2(m_currentPlayer.position - holePos)*/16.f))
        {
            holePos = targetPos;
        }
    }

    m_cameras[CameraID::Green].getComponent<cro::Transform>().setPosition({ holePos.x, GreenCamHeight, holePos.z });

    float heightOffset = GreenCamHeight;

    if (m_holeData[m_currentHole].puttFromTee)
    {
        auto direction = holePos - m_holeData[m_currentHole].tee;
        direction = glm::normalize(direction) * 2.f;
        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = 3.5f * 3.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 1.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = 0.8f;
        heightOffset *= 0.18f;
    }
    else if (auto direction = holePos - m_currentPlayer.position; m_currentPlayer.terrain == TerrainID::Green)
    {
        direction = glm::normalize(direction) * 4.2f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.5f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusSmall * GreenCamRadiusSmall;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 1.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.3f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomSlow;

        heightOffset *= 0.15f;
    }

    else if ((glm::length2(direction) < (50.f * 50.f)))
    {
        //player is within larger radius
        direction = glm::normalize(direction) * 5.5f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.5f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusMedium * GreenCamRadiusMedium;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 9.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomSlow / 2.f;

        heightOffset *= 0.15f;
    }

    else
    {
        direction = glm::normalize(direction) * 15.f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.35f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusLarge * GreenCamRadiusLarge;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 16.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.25f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomFast;
    }

    //double check terrain height and make sure camera is always above
    auto result = m_collisionMesh.getTerrain(m_cameras[CameraID::Green].getComponent<cro::Transform>().getPosition());
    result.height = std::max(result.height, holePos.y);
    result.height += heightOffset;

    auto pos = m_cameras[CameraID::Green].getComponent<cro::Transform>().getPosition();
    pos.y = result.height;

    m_cameras[CameraID::Green].getComponent<cro::Transform>().setRotation(lookRotation(pos, m_holeData[m_currentHole].pin));
    m_cameras[CameraID::Green].getComponent<cro::Transform>().setPosition(pos);
}

void GolfState::setActiveCamera(std::int32_t camID)
{
    m_sharedData.minimapData.active = (camID == CameraID::Player && !m_holeData[m_currentHole].puttFromTee);

    if (m_photoMode)
    {
        return;
    }

    CRO_ASSERT(camID >= 0 && camID < CameraID::Count, "");

    if (m_cameras[camID].isValid()
        && camID != m_currentCamera)
    {
        if (camID != CameraID::Player
            && (camID < m_currentCamera))
        {
            //don't switch back to the previous camera
            //ie if we're on the green cam don't switch
            //back to sky
            return;
        }

        //reset existing zoom
        if (m_cameras[m_currentCamera].hasComponent<CameraFollower>())
        {
            m_cameras[m_currentCamera].getComponent<CameraFollower>().reset(m_cameras[m_currentCamera]);
        }

        //set the water plane ent on the active camera
        if (m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane.isValid())
        {
            m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane = {};
        }
        m_cameras[m_currentCamera].getComponent<cro::Camera>().active = false;


        if (camID == CameraID::Player)
        {
            auto target = m_currentPlayer.position;
            m_waterEnt.getComponent<cro::Callback>().setUserData<glm::vec3>(target.x, WaterLevel, target.z);
        }

        //set scene camera
        m_gameScene.setActiveCamera(m_cameras[camID]);
        if (camID != CameraID::Sky
            && camID != CameraID::Drone)
        {
            m_gameScene.setActiveListener(m_cameras[camID]);

            //reset the fade distance which may have been modified by a zooming follower.
            m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;
            //m_resolutionUpdate.resolutionData.nearFadeDistance = m_resolutionUpdate.targetFade - 0.01f;
        }
        m_currentCamera = camID;

        m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane = m_waterEnt;
        m_cameras[m_currentCamera].getComponent<cro::Camera>().active = true;

        if (m_cameras[m_currentCamera].hasComponent<CameraFollower>())
        {
            m_cameras[m_currentCamera].getComponent<CameraFollower>().reset(m_cameras[m_currentCamera]);
            m_cameras[m_currentCamera].getComponent<CameraFollower>().currentFollowTime = CameraFollower::MinFollowTime;
        }
        auto* post = m_cameras[m_currentCamera].getComponent<TargetInfo>().postProcess;
        m_courseEnt.getComponent<cro::Drawable2D>().setShader(post->shader);
        for (const auto& [name, val] : post->uniforms)
        {
            m_courseEnt.getComponent<cro::Drawable2D>().bindUniform(name, val);
        }
    }
}

void GolfState::updateCameraHeight(float movement)
{
    //lets the player move the camera up and down when putting
    if (m_currentCamera == CameraID::Player
        && m_currentPlayer.terrain == TerrainID::Green)
    {
        auto camPos = m_cameras[CameraID::Player].getComponent<cro::Transform>().getPosition();

        static constexpr float DistanceIncrease = 5.f;
        float distanceToHole = glm::length(m_holeData[m_currentHole].pin - camPos);
        float heightMultiplier = std::clamp(distanceToHole - DistanceIncrease, 0.f, DistanceIncrease);

        const auto MaxOffset = m_cameras[CameraID::Player].getComponent<TargetInfo>().targetHeight + 0.2f;
        const auto TargetHeight = MaxOffset + m_collisionMesh.getTerrain(camPos).height;

        camPos.y = std::clamp(camPos.y + movement, TargetHeight - (MaxOffset * 0.5f), TargetHeight + (MaxOffset * 0.6f) + (heightMultiplier / DistanceIncrease));


        //correct for any target offset that may have been added in transition
        auto& lookAtOffset = m_cameras[CameraID::Player].getComponent<TargetInfo>().finalLookAtOffset;
        auto movement = lookAtOffset * (1.f / 30.f);
        lookAtOffset += movement;

        auto& lookAt = m_cameras[CameraID::Player].getComponent<TargetInfo>().finalLookAt;
        lookAt -= movement;

        m_cameras[CameraID::Player].getComponent<cro::Transform>().setPosition(camPos);
        m_cameras[CameraID::Player].getComponent<cro::Transform>().setRotation(lookRotation(camPos, lookAt));
    }
}

void GolfState::toggleFreeCam()
{
    if (!m_photoMode)
    {
        //only switch if we're the active player and the input is active
        if (!m_inputParser.getActive() || m_currentCamera != CameraID::Player)
        {
            return;
        }
    }

    cro::Command cmd;
    cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;

    m_photoMode = !m_photoMode;
    if (m_photoMode)
    {
        m_defaultCam = m_gameScene.setActiveCamera(m_freeCam);
        m_defaultCam.getComponent<cro::Camera>().active = false;
        m_gameScene.setActiveListener(m_freeCam);

        auto tx = glm::lookAt(m_currentPlayer.position + glm::vec3(0.f, 3.f, 0.f), m_holeData[m_currentHole].pin, glm::vec3(0.f, 1.f, 0.f));
        m_freeCam.getComponent<cro::Transform>().setLocalTransform(glm::inverse(tx));


        m_freeCam.getComponent<FpsCamera>().resetOrientation(m_freeCam);
        m_freeCam.getComponent<cro::Camera>().active = true;

        //hide stroke indicator
        cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Model>().setHidden(true); };

        //reduce fade distance
        m_resolutionUpdate.targetFade = 0.2f;

        //hide UI by bringing scene ent to front
        auto origin = m_courseEnt.getComponent<cro::Transform>().getOrigin();
        origin.z = -3.f;
        m_courseEnt.getComponent<cro::Transform>().setOrigin(origin);
    }
    else
    {
        m_gameScene.setActiveCamera(m_defaultCam);
        m_gameScene.setActiveListener(m_defaultCam);

        m_defaultCam.getComponent<cro::Camera>().active = true;
        m_freeCam.getComponent<cro::Camera>().active = false;

        //restore fade distance
        m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;

        //unhide UI
        auto origin = m_courseEnt.getComponent<cro::Transform>().getOrigin();
        origin.z = 0.5f;
        m_courseEnt.getComponent<cro::Transform>().setOrigin(origin);


        //and stroke indicator
        cmd.action = [&](cro::Entity e, float)
            {
                auto localPlayer = m_currentPlayer.client == m_sharedData.clientConnection.connectionID;
                e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU));
            };
    }
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    m_gameScene.setSystemActive<FpsCameraSystem>(m_photoMode);
    m_waterEnt.getComponent<cro::Callback>().active = !m_photoMode;
    m_inputParser.setActive(!m_photoMode && m_restoreInput, m_currentPlayer.terrain);
    cro::App::getWindow().setMouseCaptured(m_photoMode);
}

void GolfState::applyShadowQuality()
{
    m_shadowQuality.update(m_sharedData.hqShadows);

    m_gameScene.getSystem<cro::ShadowMapRenderer>()->setRenderInterval(m_sharedData.hqShadows ? 2 : 3);

    auto applyShadowSettings =
        [&](cro::Camera& cam, std::int32_t camID)
        {
            std::uint32_t shadowMapSize = m_sharedData.hqShadows ? 4096u : 2048u;
            if (camID == CameraID::Sky)
            {
                shadowMapSize /= 2;
            }

            float camDistance = m_shadowQuality.shadowFarDistance;
            if ((camID == CameraID::Player
                && m_currentPlayer.terrain == TerrainID::Green)
                || camID == CameraID::Bystander)
            {
                camDistance = m_shadowQuality.shadowNearDistance;
            }

            cam.shadowMapBuffer.create(shadowMapSize, shadowMapSize);
            cam.setMaxShadowDistance(camDistance);

            cam.setPerspective(cam.getFOV(), cam.getAspectRatio(), cam.getNearPlane(), cam.getFarPlane(), m_shadowQuality.cascadeCount);
        };

    for (auto i = 0; i < static_cast<std::int32_t>(m_cameras.size()); ++i)
    {
        applyShadowSettings(m_cameras[i].getComponent<cro::Camera>(), i);
    }

#ifdef CRO_DEBUG_
    applyShadowSettings(m_freeCam.getComponent<cro::Camera>(), -1);
#endif // CRO_DEBUG_

}