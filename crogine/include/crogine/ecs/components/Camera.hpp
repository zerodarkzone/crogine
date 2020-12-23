/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/Config.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/graphics/Spatial.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/DepthTexture.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <array>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <any>

namespace cro
{
    /*!
    \brief Render flags.
    Use these to filter renderable items which should be drawn for a
    specific pass. For example reflective plane geometry should only
    be rendered into the final buffer, not the reflection and refraction
    buffers.

    Set the render flags on the Model component to 'ReflectionPlane' to
    flag it as such. Then set the RenderFlags of the ModelRenderer to
    the inverse:
    \begincode
    scene.getSystem<ModelRenderer>().setFlags(~RenderFlags::ReflectionPlane);
    \endcode
    This will set all flags active on the renderer so that everything but
    reflection plane geometry will be drawn.

    When renderingto the final buffer return the flags to their default
    value to render all geometry again:
    \begincode
    scene.getSystem<ModelRenderer>().setFlags(RenderFlags::All);
    \endcode

    Custom flags can be created for any renderable component starting
    at (1<<1) and incrementing (1<<x) up to the minimum value listed
    here.
    */

    struct RenderFlags final
    {
        static constexpr std::uint64_t ReflectionPlane = ~(std::numeric_limits<std::uint64_t>::max() / 2);


        static constexpr std::uint64_t All = std::numeric_limits<std::uint64_t>::max();
    };


    /*!
    \brief Represents a camera within the scene.
    Use Scene::setActiveCamera() to use an entity with
    a camera component as the current view. The default
    output is a letterboxed perspecticve camera with a 16:9
    ratio. This can easily be overriden with a custom
    projection matrix and viewport - usually set via a
    callback applied to Camera::resizeCallback

    \sa resizeCallback
    */
    struct CRO_EXPORT_API Camera final
    {
        /*!
        \brief Maps a Renderable type to a list of drawable objects
        */
        using DrawList = std::unordered_map<std::type_index, std::any>;

        /*!
        \brief Default constructor
        */
        Camera();

        /*!
        \brief Contains the View, ViewProjection and draw list for a given render pass.

        Cameras are updated for multiple passes for (optionally) rendering reflections
        and refraction maps which can be used for flat planes in a Scene such as water.

        The matrices for each pass are updated by the camera system, and the draw lists
        are updated by any Renderable system. It is up to the user to implement this in
        any custom renderable systems.

        When rendering passes for reflection or refraction mapping the active pass has to
        be selected on the Camera first, before then rendering the Scene

        \begincode
        camera.setActivePass(Camera::Pass::Reflection);
        camera.reflectionBuffer.clear();
        scene.draw(camera.reflectionBuffer);
        camera.reflectionBuffer.display();

        camera.setActivePass(Camera::Pass::Refraction);
        camera.refractionBuffer.clear();
        scene.draw(camera.refrationBuffer);
        camera.refractionBuffer.display();

        camera.setActivePass(Camera::Pass::Final);
        scene.draw(renderWindow);
        \endcode

        Note that if any post process effects are active on the Scene that they should be
        disabled by first calling scene.setPostEnabled(false) and then re-enabling
        them for the final pass, to prevent post processes getting rendered into the
        reflection/refraction map buffers.
        */
        struct Pass final
        {
            /*!
            \brief List of entities which should be rendered when this camera is active.
            This is automatically cleared by the camera system each Scene update, and
            passed to the Scene so that Renderable systems can update it with visible
            entities, sorted in draw order. This list is then used when the Renderable
            system is drawn.
            type_index allows multiple systems to store draw lists of differing types
            which themselves are stored in std::any. It's up to the specific systems
            to maintain which types are stored.
            \see System::getType()
            \see Renderable::updateDrawList()
            \see Renderable::render()
            */
            DrawList drawList;

            /*!
            \brief View Matrix.
            This matrix is, by default, an identity matrix. This
            can be updated manually, although it is recommended that
            a Scene has an active CameraSystem added to it to automatically
            calculate both the View and ViewProjection matrices for
            all entities with a Camera component.
            */
            glm::mat4 viewMatrix = glm::mat4(1.f);

            /*!
            \brief ViewProjection matrix.
            \see viewMatrix
            */
            glm::mat4 viewProjectionMatrix = glm::mat4(1.f);

            enum
            {
                Final,
                Reflection,
                Refraction
            };

            /*!
            \brief Returns the camera frustum including any parent transformation,
            IE the frustum of the ViewProjection matrix. This requires an active
            CameraSystem in the Scene for the result to be up to date.
            */
            std::array<Plane, 6u> getFrustum() const { return m_frustum; }

            /*!
            \brief Returns the AABB of the current frustum.
            The Box returned is in world coordinates, encompassing the frustum.
            This is useful for world queries of the DynamicTree system to early
            cull objects which do not intersect the frustum. Submitting this
            to the DynamicTree system will return a list of entities which
            can then be further tested with the frustum culling during
            Scene::updateDrawLists()
            \see DynamicTreeSystem, Scene::updateDrawLists()
            \returns a Box encompassing the current frustum
            */
            Box getAABB() const { return m_aabb; }

            /*|
            \brief Returns the face to be culled in this pass, ie GL_FRONT or GL_BACK
            */
            std::uint32_t getCullFace() const { return m_cullFace; }

            /*!
            \brief Returns which direction the water plane should be facing.
            When implementing a custom render which uses the reflect/refract passes
            multiply the Scenes water plane by this value so that it points in
            the correct direction for clipping.
            */
            float getClipPlaneMultiplier() const { return m_planeMultiplier; }

        private:
            std::array<Plane, 6u> m_frustum = {};
            Box m_aabb;

            std::uint32_t m_cullFace = 0;
            float m_planeMultiplier = 1.f;;

            friend struct Camera;
            friend class CameraSystem;
        };

        /*!
        \brief Sets the active render pass.
        By default this is set to 'Final'. If you are not rendering any
        additional passes then this is not necessary to call.
        \param pass Pass index to set.
        */
        void setActivePass(std::uint32_t pass);

        /*!
        \brief Returns a reference to the active pass.
        Use this to get the View and ViewProjection matrices for
        the current pass when rendering.
        */
        const Pass& getActivePass() const { return m_passes[m_passIndex]; }

        /*!
        \brief Returns a reference to the requested pass.
        \param pass The Pass enum value requested.
        */
        const Pass& getPass(std::uint32_t pass) const;

        /*!
        \brief Returns a reference to the drawlist at the requested pass
        */
        DrawList& getDrawList(std::uint32_t pass);

        /*!
        \brief Sets the projection matrix to a perspective projection
        \param fov y-axis field of view in radians
        \param aspect The aspect ratio of width to height
        \param nearPlane Distance to near plane
        \param farPlane Distance to far plane
        */
        void setPerspective(float fov, float aspect, float nearPlane, float farPlane);

        /*!
        \brief Sets the projection matrix to an orthographic projection
        \param left Left side
        \param right Right side
        \param bottom Bottom side
        \param top Top side
        \param nearPlane Distance to near plane
        \param farPlane Distance to far plane
        */
        void setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);

        /*!
        \brief Returns a reference to the projection matrix
        By default it is constructed to a perspective matrix to match
        the current window size.
        */
        const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }

        /*!
        \brief Viewport.
        This is in normalised (0 - 1) coordinates to describe which part
        of the window this camera should render to. By default it is 0, 0, 1, 1
        which covers the entire screen. Changing this is useful when letterboxing
        the output to maintain a fixed aspect ratio, or for rendering sub-areas of
        the screen such as a minimap or split-screen multiplayer.
        */
        FloatRect viewport;

        /*!
        \brief Resize callback.
        Optional callback automatically called by the camera's Scene if the camera
        is currently the active one. Useful for resizing viewports or letterboxing
        2D views when the current window has changed aspect ratio. The callback
        takes the current camera as a parameter and returns void
        */
        std::function<void(Camera&)> resizeCallback;

        /*!
        \brief If this is set to false the CameraSystem will ignore
        this Camera component.
        */
        bool active = true;

        /*!
        \brief Target to use as this camera's reflection buffer.
        This is up to the user to initiate and draw to - by default
        the buffer remains uninitialised.
        \see Camera::Pass
        */
        RenderTexture reflectionBuffer;

        /*!
        \brief Target to use as this camera's refraction buffer.
        This is up to the user to initiate and draw to - by default
        the buffer remains uninitialised.
        \see Camera::Pass
        */
        RenderTexture refractionBuffer;

        /*!
        \brief Only renderables which match these flags when AND together
        will be drawn when this camera is active.
        */
        std::uint64_t renderFlags = RenderFlags::All;

        /*
        \brief Depth map buffer used for rendering directional shadow maps.
        This must first be explicitly created at the desired resolution
        before shadows will be rendered with this camera. Once the buffer
        has been created a ShadowMapRenderer system must be active in the
        Scene to which this component belongs. The ShadowMapSystem will
        then automatically cull and render any entities with a Model and
        ShadowCaster component (and valid shadow material) to this buffer.
        The ModelRenderer will automatically bind and pass this buffer to
        any relevant shaders when the Scene is rendered with this camera.
        */
#ifdef PLATFORM_DESKTOP
        cro::DepthTexture depthBuffer;
#else
        cro::RenderTexture depthBuffer;
#endif

        /*!
        \brief View-projection matrix used when rendering the depth buffer.
        This contains the view-projection matrix used by the ShadowMapRenderer
        to render the depth buffer for the Scene's directional light. This is
        automatically bound to any materials used by the ModelRenderer when
        shadow casting is enabled. When rendering shadows for this camera with
        any custom materials or render system this should be used with the
        camera's depthBuffer property.
        */
        glm::mat4 depthViewProjectionMatrix = glm::mat4(1.f);

        /*!
        \brief View matrix used to render the depthBuffer.
        Automatically updated by the ShadowMapRenderer system
        */
        glm::mat4 depthViewMatrix = glm::mat4(1.f);

        /*!
        \brief Projection matrix used to render the depth buffer
        Automatically updated by the ShadowMapRenderer system
        */
        glm::mat4 depthProjectionMatrix = glm::mat4(1.f);

    private:

        std::array<Pass, 2u> m_passes = {}; //final pass and refraction pass share the same data
        std::uint32_t m_passIndex = Pass::Final;

        friend class CameraSystem;

        glm::mat4 m_projectionMatrix = glm::mat4(1.f);
        float m_aspectRatio;
        float m_verticalFOV;
        float m_nearPlane;
        float m_farPlane;

        friend class ShadowMapRenderer;
    };
}