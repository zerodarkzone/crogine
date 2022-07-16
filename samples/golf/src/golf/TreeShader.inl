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

#include <string>

const std::string BushVertex =
R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;

#if defined(INSTANCING)
    ATTRIBUTE mat4 a_instanceWorldMatrix;
    ATTRIBUTE mat3 a_instanceNormalMatrix;
#else
    uniform mat3 u_normalMatrix;
#endif
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform mat4 u_projectionMatrix;
    uniform vec3 u_cameraWorldPosition;

    uniform vec4 u_clipPlane;
    uniform float u_targetHeight = 300.0; //height of the render target multiplied by its viewport, ie height of the renderable area

    uniform float u_leafSize = 0.25; //world units, in this case metres
    uniform float u_randAmount = 0.2;

    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };

    layout (std140) uniform ScaledResolution
    {
        vec2 u_scaledResolution;
        float u_nearFadeDistance;
    };

    VARYING_OUT float v_ditherAmount;
    #if defined(HQ)    
    VARYING_OUT vec3 v_normal;
    #else
    flat VARYING_OUT vec3 v_normal;
    #endif
    VARYING_OUT vec4 v_colour;
    VARYING_OUT mat2 v_rotation;


    float rand(vec2 position)
    {
        return fract(sin(dot(position, vec2(12.9898, 4.1414))) * 43758.5453);
    }

    const float MaxWindOffset = 0.1f;

    void main()
    {
        int UID = gl_InstanceID << 16 | (gl_VertexID & 0x0000ffff);

    #if defined (INSTANCING)
        mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
        mat3 normalMatrix = mat3(u_worldMatrix) * a_instanceNormalMatrix;            
    #else
        mat4 worldMatrix = u_worldMatrix;
        mat3 normalMatrix = u_normalMatrix;
    #endif

        float randVal = rand(vec2(UID));
        float offset = randVal * u_randAmount;
        vec4 position = a_position;
    #if defined(HQ)
        position.xyz += (a_normal * offset);
    #endif

        v_normal = normalMatrix * a_normal;
        v_colour = a_colour * (1.0 - (u_randAmount - offset)); //darken less offset leaves

        gl_ClipDistance[0] = dot(a_position, u_clipPlane);

        vec4 worldPosition = worldMatrix * position;

//wind
    #if defined (HQ)
        float time = (u_windData.w * 5.0) + UID;
        float x = sin(time * 2.0) / 8.0;
        float y = cos(time) / 2.0;
        vec3 windOffset = vec3(x, y, x);

        vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
        float dirStrength = dot(v_normal, windDir);

        vec2 rot = vec2(sin(x * u_windData.y), cos(x * u_windData.y));
        v_rotation[0] = vec2(rot.y, -rot.x);
        v_rotation[1]= rot;

        dirStrength += 1.0;
        dirStrength /= 2.0;

        windOffset += windDir * u_windData.y * dirStrength * 2.0;
        worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;
    #else
        float time = (u_windData.w * 15.0) + gl_InstanceID;
        float x = sin(time * 2.0) / 8.0;
        float y = cos(time) / 2.0;
        vec3 windOffset = vec3(x, y, x) * a_colour.b * 0.1;


        vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
        float dirStrength = a_colour.b;

        windOffset += windDir * u_windData.y * dirStrength;
        worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;
    #endif
        gl_Position = u_viewProjectionMatrix * worldPosition;


//size calc
    #if defined(HQ)
        float variation = rand(-vec2(UID));
        variation = 0.5 + (0.5 * variation);

        float pointSize = u_leafSize + ((u_leafSize * 2.0) * offset);
        pointSize *= variation;

        vec3 camForward = vec3(u_viewMatrix[0][2], u_viewMatrix[1][2], u_viewMatrix[2][2]);
        vec3 eyeDir = normalize(u_cameraWorldPosition - worldPosition.xyz);
            
        float facingAmount = dot(v_normal, camForward);
        //pointSize *= 0.5 + (0.5 * facingAmount);
            
        //shrink 'backfacing' to zero
        pointSize *= step(0.0, facingAmount); 
            
        //we use the camera's forward vector to shrink any points out of view to zero
        pointSize *= clamp(dot(eyeDir, (camForward)), 0.0, 1.0);

            
        //shrink with perspective/distance and scale to world units
        pointSize *= u_targetHeight * (u_projectionMatrix[1][1] / gl_Position.w);

        //we scale point size by model matrix but it assumes all axis are
        //scaled equally, as we only use the X axis
        pointSize *= length(worldMatrix[0].xyz);

        gl_PointSize = pointSize;
    #endif
//proximity fade
        float fadeDistance = u_nearFadeDistance * 2.0;
        const float farFadeDistance = 360.f;
        float distance = length(worldPosition.xyz - u_cameraWorldPosition);

        v_ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
        v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / fadeDistance, 0.0, 1.0);
    })";

const std::string BushFragment =
R"(
    OUTPUT

    uniform sampler2D u_diffuseMap;
    uniform vec3 u_lightDirection;
    uniform vec3 u_colour = vec3(1.0);

    layout (std140) uniform PixelScale
    {
        float u_pixelScale;
    };

    VARYING_IN float v_ditherAmount;
    #if defined(HQ)    
    VARYING_IN vec3 v_normal;
    #else
    flat VARYING_IN vec3 v_normal;
    #endif
    VARYING_IN vec4 v_colour;
    VARYING_IN mat2 v_rotation;

    //function based on example by martinsh.blogspot.com
    const int MatrixSize = 8;
    float findClosest(int x, int y, float c0)
    {
        /* 8x8 Bayer ordered dithering */
        /* pattern. Each input pixel */
        /* is scaled to the 0..63 range */
        /* before looking in this table */
        /* to determine the action. */

        const int dither[64] = int[64](
            0, 32, 8, 40, 2, 34, 10, 42, 
        48, 16, 56, 24, 50, 18, 58, 26, 
        12, 44, 4, 36, 14, 46, 6, 38, 
        60, 28, 52, 20, 62, 30, 54, 22, 
            3, 35, 11, 43, 1, 33, 9, 41, 
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47, 7, 39, 13, 45, 5, 37,
        63, 31, 55, 23, 61, 29, 53, 21 );

        float limit = 0.0;
        if (x < MatrixSize)
        {
            limit = (dither[y * MatrixSize + x] + 1) / 64.0;
        }

        if (c0 < limit)
        {
            return 0.0;
        }
        return 1.0;
    }


    vec3 rgb2hsv(vec3 c)
    {
        vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
        vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
        vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

        float d = q.x - min(q.w, q.y);
        float e = 1.0e-10;
        return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
    }

    vec3 hsv2rgb(vec3 c)
    {
        vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    }

    vec3 complementaryColour(vec3 c)
    {
        vec3 a = rgb2hsv(c);
        a.x += 0.25;
        a.z *= 0.5;
        c = hsv2rgb(a);
        return c;
    }

    void main()
    {
        float amount = dot(normalize(v_normal), -u_lightDirection);
        amount *= 2.0;
        amount = round(amount);
        amount /= 2.0;
        amount = 0.4 + (amount * 0.6);
#if defined(VERTEX_COLOURED)
        vec3 colour = mix(complementaryColour(v_colour.rgb), v_colour.rgb, amount);
#else
        vec3 colour = mix(complementaryColour(u_colour.rgb), u_colour.rgb, amount);
        //multiply by v_colour.a to darken on leaf depth - looks nice but not used
#endif

        vec2 coord = gl_PointCoord.xy;
        coord = v_rotation * (coord - vec2(0.5));
        coord += vec2(0.5);

//use texture and dither amount to see if we discard
#if defined(HQ)
        vec4 textureColour = TEXTURE(u_diffuseMap, coord);
#else
        vec4 textureColour = vec4(1.0);
#endif
        vec2 xy = gl_FragCoord.xy / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));

        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
        if (textureColour.a * alpha < 0.3) discard;


        FRAG_OUT = vec4(colour, 1.0) * textureColour;
    })";

std::string BranchVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec2 a_texCoord0;

#if defined(INSTANCING)
    ATTRIBUTE mat4 a_instanceWorldMatrix;
    ATTRIBUTE mat3 a_instanceNormalMatrix;
#else
    uniform mat3 u_normalMatrix;
#endif

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform vec4 u_clipPlane;
    uniform vec3 u_cameraWorldPosition;

    layout (std140) uniform WindValues
    {
        vec4 u_windData; //dirX, strength, dirZ, elapsedTime
    };

    layout (std140) uniform ScaledResolution
    {
        vec2 u_scaledResolution;
        float u_nearFadeDistance;
    };

    VARYING_OUT float v_ditherAmount;
    VARYING_OUT vec2 v_texCoord;
    VARYING_OUT vec3 v_normal;

    const float MaxWindOffset = 0.2;

    void main()
    {
    #if defined (INSTANCING)
        mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
        mat3 normalMatrix = mat3(u_worldMatrix) * a_instanceNormalMatrix;            
    #else
        mat4 worldMatrix = u_worldMatrix;
        mat3 normalMatrix = u_normalMatrix;
    #endif

        vec4 worldPosition = worldMatrix * a_position;

        float time = (u_windData.w * 15.0) + gl_InstanceID;
        float x = sin(time * 2.0) / 8.0;
        float y = cos(time) / 2.0;
        vec3 windOffset = vec3(x, y, x) * a_colour.b * 0.1;


        vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
        float dirStrength = a_colour.b;

        windOffset += windDir * u_windData.y * dirStrength;// * 2.0;
        worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;


        gl_Position = u_viewProjectionMatrix * worldPosition;
        gl_ClipDistance[0] = dot(u_clipPlane, a_position);

        v_texCoord = a_texCoord0;
        v_normal = normalMatrix * a_normal;

//proximity fade
        float fadeDistance = u_nearFadeDistance * 2.0;
        const float farFadeDistance = 360.f;
        float distance = length(worldPosition.xyz - u_cameraWorldPosition);

        v_ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
        v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / fadeDistance, 0.0, 1.0);
    })";

std::string BranchFragment = R"(
    OUTPUT

    uniform sampler2D u_diffuseMap;
    uniform vec3 u_lightDirection;

    layout (std140) uniform PixelScale
    {
        float u_pixelScale;
    };

    VARYING_IN float v_ditherAmount;
    VARYING_IN vec2 v_texCoord;
    VARYING_IN vec3 v_normal;

    //function based on example by martinsh.blogspot.com
    const int MatrixSize = 8;
    float findClosest(int x, int y, float c0)
    {
        const int dither[64] = int[64](
            0, 32, 8, 40, 2, 34, 10, 42, 
        48, 16, 56, 24, 50, 18, 58, 26, 
        12, 44, 4, 36, 14, 46, 6, 38, 
        60, 28, 52, 20, 62, 30, 54, 22, 
            3, 35, 11, 43, 1, 33, 9, 41, 
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47, 7, 39, 13, 45, 5, 37,
        63, 31, 55, 23, 61, 29, 53, 21 );

        float limit = 0.0;
        if (x < MatrixSize)
        {
            limit = (dither[y * MatrixSize + x] + 1) / 64.0;
        }

        if (c0 < limit)
        {
            return 0.0;
        }
        return 1.0;
    }

    void main()
    {
        vec4 colour = TEXTURE(u_diffuseMap, v_texCoord);

        float amount = dot(normalize(v_normal), -u_lightDirection);
        amount *= 2.0;
        amount = round(amount);
        amount /= 2.0;
        amount = 0.6 + (amount * 0.4);

        colour.rgb *= amount;
        FRAG_OUT = colour;

        vec2 xy = gl_FragCoord.xy / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));

        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
        if (alpha < 0.1) discard;
    })";