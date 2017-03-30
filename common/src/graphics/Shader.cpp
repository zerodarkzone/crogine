/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/graphics/Shader.hpp>

#include "../glad/glad.h"
#include "../glad/GLCheck.hpp"

#include <vector>
#include <cstring>

using namespace cro;

Shader::Shader()
    : m_handle  (0),
    m_attribMap ({})
{
    resetAttribMap();
}

Shader::~Shader()
{
    if (m_handle)
    {
        glCheck(glDeleteProgram(m_handle));
    }
}

//public
bool Shader::loadFromFile(const std::string& vertex, const std::string& fragment)
{
    return loadFromString(parseFile(vertex), parseFile(fragment));
}

bool Shader::loadFromString(const std::string& vertex, const std::string& fragment)
{
    if (m_handle)
    {
        //remove existing program
        glCheck(glDeleteProgram(m_handle));
        resetAttribMap();
    }    
    
    //compile vert shader
    GLuint vertID = glCreateShader(GL_VERTEX_SHADER);
    
#ifdef __ANDROID__
    const char* src[] = { "#version 100", vertex.c_str() };
#else
    const char* src[] = { "#version 150", vertex.c_str() };
#endif //__ANDROID__

    glCheck(glShaderSource(vertID, 2, src, nullptr));
    glCheck(glCompileShader(vertID));

    GLint result = GL_FALSE;
    int resultLength = 0;

    glCheck(glGetShaderiv(vertID, GL_COMPILE_STATUS, &result));
    glCheck(glGetShaderiv(vertID, GL_INFO_LOG_LENGTH, &resultLength));
    if (result == GL_FALSE)
    {
        //failed compilation
        std::string str;
        str.resize(resultLength + 1);
        glCheck(glGetShaderInfoLog(vertID, resultLength, nullptr, &str[0]));
        Logger::log("Failed compiling vertex shader: " + std::to_string(result) + ", " + str, Logger::Type::Error);

        glCheck(glDeleteShader(vertID));
        return false;
    }
    
    //compile frag shader
    GLuint fragID = glCreateShader(GL_FRAGMENT_SHADER);
    src[1] = fragment.c_str();
    glCheck(glShaderSource(fragID, 2, src, nullptr));
    glCheck(glCompileShader(fragID));

    result = GL_FALSE;
    resultLength = 0;

    glCheck(glGetShaderiv(fragID, GL_COMPILE_STATUS, &result));
    glCheck(glGetShaderiv(fragID, GL_INFO_LOG_LENGTH, &resultLength));
    if (result == GL_FALSE)
    {
        std::string str;
        str.resize(resultLength + 1);
        glCheck(glGetShaderInfoLog(fragID, resultLength, nullptr, &str[0]));
        Logger::log("Failed compiling fragment shader: " + std::to_string(result) + ", " + str, Logger::Type::Error);

        glCheck(glDeleteShader(vertID));
        glCheck(glDeleteShader(fragID));
        return false;
    }

    //link shaders to program
    m_handle = glCreateProgram();
    if (m_handle)
    {
        glCheck(glAttachShader(m_handle, vertID));
        glCheck(glAttachShader(m_handle, fragID));
        glCheck(glLinkProgram(m_handle));

        result = GL_FALSE;
        resultLength = 0;
        glCheck(glGetProgramiv(m_handle, GL_LINK_STATUS, &result));
        glCheck(glGetProgramiv(m_handle, GL_INFO_LOG_LENGTH, &resultLength));
        if (result == GL_FALSE)
        {
            std::string str;
            str.resize(resultLength + 1);
            glCheck(glGetProgramInfoLog(m_handle, resultLength, nullptr, &str[0]));
            Logger::log("Failed to link shader program: " + std::to_string(result) + ", " + str, Logger::Type::Error);

            glCheck(glDetachShader(m_handle, vertID));
            glCheck(glDetachShader(m_handle, fragID));

            glCheck(glDeleteShader(vertID));
            glCheck(glDeleteShader(fragID));
            glCheck(glDeleteProgram(m_handle));
            m_handle = 0;
            return false;
        }
        else
        {
            //tidy
            glCheck(glDetachShader(m_handle, vertID));
            glCheck(glDetachShader(m_handle, fragID));

            glCheck(glDeleteShader(vertID));
            glCheck(glDeleteShader(fragID));

            //grab attributes
            if (!fillAttribMap())
            {
                glCheck(glDeleteProgram(m_handle));
                return false;
            }

            return true;
        }
    }

    return false;
}

uint32 Shader::getGLHandle() const
{
    return m_handle;
}

const std::array<int32, Mesh::Attribute::Total>& Shader::getAttribMap() const
{
    return m_attribMap;
}

//private
bool Shader::fillAttribMap()
{
    GLint activeAttribs;
    glCheck(glGetProgramiv(m_handle, GL_ACTIVE_ATTRIBUTES, &activeAttribs));
    if (activeAttribs > 0)
    {
        GLint length;
        glCheck(glGetProgramiv(m_handle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &length));
        if (length > 0)
        {
            std::vector<GLchar> attribName(length + 1);
            GLint attribSize;
            GLenum attribType;
            GLint attribLocation;

            for (auto i = 0; i < activeAttribs; ++i)
            {
                glCheck(glGetActiveAttrib(m_handle, i, length, nullptr, &attribSize, &attribType, attribName.data()));
                attribName[length] = '\0';

                glCheck(attribLocation = glGetAttribLocation(m_handle, attribName.data()));
                std::string name(attribName.data());
                if (name == "a_position")
                {
                    m_attribMap[Mesh::Position] = attribLocation;
                }
                else if (name == "a_colour")
                {
                    m_attribMap[Mesh::Colour] = attribLocation;
                }
                else if (name == "a_normal")
                {
                    m_attribMap[Mesh::Normal] = attribLocation;
                }
                else if (name == "a_tangent")
                {
                    m_attribMap[Mesh::Tangent] = attribLocation;
                }
                else if (name == "a_bitangent")
                {
                    m_attribMap[Mesh::Bitangent] = attribLocation;
                }
                else if (name == "a_texCoord0")
                {
                    m_attribMap[Mesh::UV0] = attribLocation;
                }
                else if (name == "a_texCoord1")
                {
                    m_attribMap[Mesh::UV1] = attribLocation;
                }
                else
                {
                    Logger::log(name + ": unknown vertex attribute. Shader compilation failed.", Logger::Type::Error);
                    return false;
                }              
            }
            return true;
        }
        Logger::log("Failed loading shader attributes for some reason...", Logger::Type::Error);
        return false;
    }
    Logger::log("No vertex attributes were found in shader - compilation failed.", Logger::Type::Error);
    return false;
}

void Shader::resetAttribMap()
{
    std::memset(m_attribMap.data(), -1, m_attribMap.size() * sizeof(int32));
}

std::string Shader::parseFile(const std::string& file)
{
    /*NOTE this won't work on android when trying to read resources frm the apk*/
    /*It may be better to embed shader code in a header file as a raw string instead*/

    std::string retVal;
    retVal.reserve(1000);

    //open file and verify

    //read line by line - 
    //if line starts with #include increase inclusion depth
    //if depth under limit append parseFile(include)
    //else append line

    //if line is a version number remove it 
    //as platform specific number is appeanded by loadFromString

    //close file

    return {};
}