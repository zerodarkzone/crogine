/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/RenderTarget.hpp>
#include <crogine/graphics/MaterialData.hpp>

#include <array>

namespace cro
{
    /*!
    \brief A render buffer with a position buffer, normal buffer and a depth component attached.
    MultiRenderTextures are non-copyable objects but *can* be moved. MultiRenderTextures are used
    as g-buffers, accumulating scene data that can be used for screen space effects such as
    ambient occlusion, reflections, depth of field and so on. Models require materials which use
    the cro::ShaderResource::BuiltIn::GBuffer shader. Fragment positions, normals and depths are
    written to the corresponding buffers, in world-view space, unless a custom shader is supplied.
    MultiRenderTextures are not available on mobile platforms.
    */
    class CRO_EXPORT_API MultiRenderTexture final : public RenderTarget, public Detail::SDLResource
    {
    public:
        /*!
        \brief Constructor.
        By default MultiRenderTextures are in an invalid state until create() has been called
        at least once.
        To draw to an MRT first call its clear() function, which then activates
        it as the current target. All drawing operations will then be performed on it
        until display() is called. Both clear AND display() *must* be called either side of
        drawing to prevent undefined results.
        */
        MultiRenderTexture();
        ~MultiRenderTexture();

        MultiRenderTexture(const MultiRenderTexture&) = delete;
        MultiRenderTexture(MultiRenderTexture&&) noexcept;
        const MultiRenderTexture& operator = (const MultiRenderTexture&) = delete;
        MultiRenderTexture& operator = (MultiRenderTexture&&) noexcept;

        /*!
        \brief Creates (or recreates) the multi render texture
        \param width Width of the texture to create. This should be
        power of 2 on mobile platforms
        \param height Height of the texture.
        \returns true on success, else returns false
        */
        bool create(std::uint32_t width, std::uint32_t height);

        /*!
        \brief Returns the current size in pixels of the texture (zero if not yet created)
        */
        glm::uvec2 getSize() const override;

        /*!
        \brief Clears the texture ready for drawing
        This should be called to activate the texture as the current drawing target.
        From then on all drawing operations will be applied to the MultiRenderTexture until display()
        is called. For every clear() call there must be exactly one display() call. This
        also attempts to save and restore any existing viewport, while also applying its
        own during rendering.
        */
        void clear();

        /*!
        \brief This must be called once for each call to clear to properly validate
        the final state of the texture. Failing to do this will result in undefined
        behaviour.
        */
        void display();

        /*!
        \brief Defines the area of the MultiRenderTexture on to which to draw.
        */
        void setViewport(URect);

        /*!
        \brief Returns the active viewport for this texture
        */
        URect getViewport() const;

        /*!
        \brief Returns the default viewport of the MultiRenderTexture
        */
        URect getDefaultViewport() const;

        /*!
        \brief Returns true if the render texture is available for drawing.
        If create() has not yet been called, or previously failed then this
        will return false
        */
        bool available() const { return m_fboID != 0; }

        /*!
        \brief Returns the texture ID of the Position texture wrappped 
        in a handle which can be bound to material uniforms.
        */
        TextureID getPositionTexture() const;

        /*!
        \brief Returns the texture ID of the Normal texture wrappped
        in a handle which can be bound to material uniforms.
        */
        TextureID getNormalTexture() const;

        /*!
        \brief Returns the texture ID of the Depth texture wrappped
        in a handle which can be bound to material uniforms.
        */
        TextureID getDepthTexture() const;

    private:
        std::uint32_t m_fboID;
        
        enum TextureIndex
        {
            Position, Normal, Depth,
            Count
        };
        std::array<std::uint32_t, TextureIndex::Count> m_textureIDs;

        glm::uvec2 m_size;
        URect m_viewport;
        std::array<std::int32_t, 4u> m_lastViewport = {};
        std::int32_t m_lastBuffer;
    };
}