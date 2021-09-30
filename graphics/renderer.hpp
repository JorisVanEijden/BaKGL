#pragma once

#include "graphics/meshObject.hpp"
#include "graphics/opengl.hpp"
#include "graphics/shaderProgram.hpp"

namespace Graphics {

class Renderer
{
public:
    Renderer()
    :
        mModelShader{std::invoke([]{
            auto shader = ShaderProgram{
                "normal.vert.glsl",
                "normal.frag.glsl"};
            return shader.Compile();
        })},
        mSpriteShader{std::invoke([]{
            auto shader = ShaderProgram{
                "sprite.vert.glsl",
                "sprite.frag.glsl"};
            return shader.Compile();
        })},
        mVertexArrayObject{},
        mGLBuffers{},
        mTextureBuffer{}
    {}

    template <typename TextureStoreT>
    void LoadData(
        const MeshObjectStorage& objectStore,
        const TextureStoreT& textureStore)
    {
        mVertexArrayObject.BindGL();

        mGLBuffers.AddBuffer("vertex", 0, 3);
        mGLBuffers.AddBuffer("normal", 1, 3);
        mGLBuffers.AddBuffer("color", 2, 4);
        mGLBuffers.AddBuffer("textureCoord", 3, 3);
        mGLBuffers.AddBuffer("textureBlend", 4, 1);

        mGLBuffers.LoadBufferDataGL("vertex", GL_ARRAY_BUFFER, objectStore.mVertices);
        mGLBuffers.LoadBufferDataGL("normal", GL_ARRAY_BUFFER, objectStore.mNormals);
        mGLBuffers.LoadBufferDataGL("color", GL_ARRAY_BUFFER, objectStore.mColors);
        mGLBuffers.LoadBufferDataGL("textureCoord", GL_ARRAY_BUFFER, objectStore.mTextureCoords);
        mGLBuffers.LoadBufferDataGL("textureBlend", GL_ARRAY_BUFFER, objectStore.mTextureBlends);
        mGLBuffers.LoadBufferDataGL(mGLBuffers.mElementBuffer, GL_ELEMENT_ARRAY_BUFFER, objectStore.mIndices);

        mGLBuffers.BindArraysGL();

        mTextureBuffer.LoadTexturesGL(
            textureStore.GetTextures(),
            textureStore.GetMaxDim());
    }

    template <bool isModelShader, typename Renderables, typename Camera>
    void Draw(
        const Renderables& renderables,
        const glm::vec3& lightPosition,
        const Camera& camera)
    {
        mVertexArrayObject.BindGL();
        glActiveTexture(GL_TEXTURE0);
        mTextureBuffer.BindGL();

        auto& shader = isModelShader ? mModelShader : mSpriteShader;

        shader.UseProgramGL();
        const auto textureId = shader.GetUniformLocation("texture0");
        shader.SetUniform(textureId, 0);
        const auto lightId = shader.GetUniformLocation("lightPosition_worldspace");
        shader.SetUniform(lightId, lightPosition);

        const auto cameraPositionId = shader.GetUniformLocation("cameraPosition_worldspace");
        shader.SetUniform(cameraPositionId, camera.GetNormalisedPosition());

        const auto mvpMatrixId = shader.GetUniformLocation("MVP");
        const auto modelMatrixId = shader.GetUniformLocation("M");
        const auto viewMatrixId = shader.GetUniformLocation("V");

        auto viewMatrix = camera.GetViewMatrix();
        glm::mat4 MVP;

        for (const auto& item : renderables)
        {
            if (glm::distance(camera.GetPosition(), item.GetLocation()) > 128000.0) continue;
            const auto [offset, length] = item.GetObject();
            auto modelMatrix = item.GetModelMatrix();

            MVP = camera.GetProjectionMatrix() * viewMatrix * modelMatrix;

            shader.SetUniform(mvpMatrixId, MVP);
            shader.SetUniform(modelMatrixId, modelMatrix);
            shader.SetUniform(viewMatrixId, viewMatrix);

            glDrawElementsBaseVertex(
                GL_TRIANGLES,
                length,
                GL_UNSIGNED_INT,
                (void*) (offset * sizeof(GLuint)),
                offset
            );
        }
    }

private:
    ShaderProgramHandle mModelShader;
    ShaderProgramHandle mSpriteShader;
    VertexArrayObject mVertexArrayObject;
    GLBuffers mGLBuffers;
    TextureBuffer mTextureBuffer;
};

}
