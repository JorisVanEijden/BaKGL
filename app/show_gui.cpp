#include "bak/camera.hpp"
#include "bak/coordinates.hpp"
#include "bak/inputHandler.hpp"
#include "bak/logger.hpp"
#include "bak/screens.hpp"
#include "bak/systems.hpp"
#include "bak/textureFactory.hpp"

#include "graphics/glfw.hpp"
#include "graphics/plane.hpp"
#include "graphics/meshObject.hpp"
#include "graphics/renderer.hpp"
#include "graphics/shaderProgram.hpp"
#include "graphics/texture.hpp"

#include "gui/gui.hpp"

#include "imgui/imguiWrapper.hpp"

#include "xbak/FileManager.h"
#include "xbak/FileBuffer.h"
#include "xbak/PaletteResource.h"
#include "xbak/RequestResource.h"

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <functional>
#include <memory>
#include <sstream>

#include <getopt.h>

int main(int argc, char** argv)
{
    const auto& logger = Logging::LogState::GetLogger("main");
    Logging::LogState::SetLevel(Logging::LogLevel::Debug);

    BAK::DialogStore dialogStore{};
    dialogStore.Load();

    auto width = 640.0f;
    auto height = 480.0f;
    auto window = Graphics::MakeGlfwWindow(
        height,
        width,
        "Show Scene");

    auto guiScale = glm::vec3{width / 320, height / 200, 0};

    glViewport(0, 0, width, height);

    ImguiWrapper::Initialise(window.get());
    
    // Dark blue background
    glClearColor(0.15f, 0.31f, 0.36f, 0.0f);

    auto guiShader = ShaderProgram{
        "gui.vert.glsl",
        "gui.frag.glsl"};
    auto guiShaderId = guiShader.Compile();
    
    auto textures = Graphics::TextureStore{};
    BAK::TextureFactory::AddScreenToTextureStore(textures, "FRAME.SCX", "OPTIONS.PAL");
    BAK::TextureFactory::AddToTextureStore(textures, "HEADS.BMX", "OPTIONS.PAL");
    unsigned off = textures.GetTextures().size();
    BAK::TextureFactory::AddToTextureStore(textures, "BICONS1.BMX", "OPTIONS.PAL");
    unsigned off2 = textures.GetTextures().size();
    BAK::TextureFactory::AddToTextureStore(textures, "BICONS2.BMX", "OPTIONS.PAL");

    RequestResource request;
    FileManager::GetInstance()->Load(&request, "REQ_MAIN.DAT");
    logger.Info() << request << "\n";

    std::vector<Gui::GuiElement> elements;
    elements.emplace_back(0, 0, 0); // background
    for (unsigned i = 0; i < request.GetSize(); i++)
    {
        auto data = request.GetRequestData(i);
        switch (data.widget)
        {
        case REQ_USERDEFINED:
        {
            int x = data.xpos + request.GetRectangle().GetXPos() + request.GetXOff();
            int y = data.ypos + request.GetRectangle().GetYPos() + request.GetYOff();
            elements.emplace_back(false, data.image + 1, data.image + 1, glm::vec3{x, y, 0}, glm::vec3{data.width, data.height, 0});
        }
            break;
        case REQ_IMAGEBUTTON:
        {
            int x = data.xpos + request.GetRectangle().GetXPos() + request.GetXOff();
            int y = data.ypos + request.GetRectangle().GetYPos() + request.GetYOff();
            elements.emplace_back(false, data.image + off, data.image + off2, glm::vec3{x, y, 0}, glm::vec3{data.width, data.height, 0});
        }
            break;
        default:
            logger.Info() << "Unhandled: " << i << "\n";
            break;
        }
    }

    Graphics::TextureBuffer textureBuffer{};
    textureBuffer.LoadTexturesGL(
        textures.GetTextures(),
        textures.GetMaxDim());

    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    auto objStore = Graphics::QuadStorage{};
    for (unsigned i = 0; i < textures.GetTextures().size(); i++)
    {
        const auto& tex = textures.GetTexture(i);
        objStore.AddObject(
            Graphics::Quad{
                static_cast<double>(tex.GetWidth()),
                static_cast<double>(tex.GetHeight()),
                static_cast<double>(textures.GetMaxDim()),
                i});
    }

    Graphics::GLBuffers buffers{};
    buffers.AddBuffer("vertex", 0, 3);
    buffers.AddBuffer("textureCoord", 1, 3);

    buffers.LoadBufferDataGL("vertex", GL_ARRAY_BUFFER, objStore.mVertices);
    buffers.LoadBufferDataGL("textureCoord", GL_ARRAY_BUFFER, objStore.mTextureCoords);
    buffers.LoadBufferDataGL(buffers.mElementBuffer, GL_ELEMENT_ARRAY_BUFFER, objStore.mIndices);
    buffers.BindArraysGL();
    glBindVertexArray(0);

    glm::mat4 scaleMatrix = glm::scale(glm::mat4{1}, guiScale);
    //glm::mat4 scaleMatrix = glm::mat4{1};
    glm::mat4 viewMatrix = glm::ortho(
        0.0f,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f,
        -1.0f,
        1.0f);  
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 MVP{1};

    InputHandler inputHandler{};
    InputHandler::BindMouseToWindow(window.get(), inputHandler);
    InputHandler::BindKeyboardToWindow(window.get(), inputHandler);
    inputHandler.Bind(GLFW_KEY_W, [&]{ modelMatrix = glm::translate(modelMatrix, {0, 50.0/60, 0}); });
    inputHandler.Bind(GLFW_KEY_S, [&]{ modelMatrix = glm::translate(modelMatrix, {0, -50.0/60, 0}); });
    inputHandler.Bind(GLFW_KEY_A, [&]{ modelMatrix = glm::translate(modelMatrix, {-50.0/60, 0, 0}); });
    inputHandler.Bind(GLFW_KEY_D, [&]{ modelMatrix = glm::translate(modelMatrix, {50.0/60, 0, 0}); });
    inputHandler.Bind(GLFW_KEY_Q, [&]{ scaleMatrix = glm::scale(scaleMatrix, {.9, .9, 0}); });
    inputHandler.Bind(GLFW_KEY_E, [&]{ scaleMatrix = glm::scale(scaleMatrix, {1.1, 1.1, 0}); });
    inputHandler.BindMouse(GLFW_MOUSE_BUTTON_LEFT,
        [&](auto x, auto y)
        {
            logger.Debug() << "mx: " << x << " my: " << y << "\n";
            for (auto& elem : elements)
            {
                auto scaledTL = guiScale * elem.mPosition;
                auto scaledBR = guiScale * (elem.mPosition + elem.mDims);
                if ((x >= scaledTL.x && x <= scaledBR.x)
                    && (y >= scaledTL.y && y <= scaledBR.y))
                {
                    elem.mPressed = true;
                }
            }
        },
        [&](auto x, auto y)
        {
            logger.Debug() << "mx: " << x << " my: " << y << "\n";
            for (auto& elem : elements)
            {
                auto scaledTL = guiScale * elem.mPosition;
                auto scaledBR = guiScale * (elem.mPosition + elem.mDims);
                if ((x >= scaledTL.x && x <= scaledBR.x)
                    && (y >= scaledTL.y && y <= scaledBR.y))
                {
                    elem.mPressed = false;
                }
            }
        }
    );

    double currentTime = 0;
    double lastTime = 0;
    float deltaTime = 0;

    glfwSetCursorPos(window.get(), width/2, height/2);

    glEnable(GL_MULTISAMPLE);  

    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureBuffer.mTextureBuffer);

    double pointerPosX, pointerPosY;

    glUseProgram(guiShaderId.GetHandle());
    double acc = 0;
    unsigned i = 0;
    do
    {
        currentTime = glfwGetTime();
        deltaTime = float(currentTime - lastTime);
        acc += deltaTime;
        if (acc > .2)
        { 
            i = (i + 1) % textures.GetTextures().size();
            acc = 0;
        }

        lastTime = currentTime;

        glfwPollEvents();
        glfwGetCursorPos(window.get(), &pointerPosX, &pointerPosY);

        glBindVertexArray(VertexArrayID);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto programId = guiShaderId.GetHandle();
        GLuint textureID = glGetUniformLocation(programId, "texture0");
        glUniform1i(textureID, 0);

        GLuint mvpMatrixID   = glGetUniformLocation(programId, "MVP");
        GLuint modelMatrixID = glGetUniformLocation(programId, "M");
        GLuint viewMatrixID  = glGetUniformLocation(programId, "V");

        for (const auto& [pressed, image, pImage, pos, dim] : elements)
        {
            modelMatrix = glm::translate(glm::mat4{1}, pos);
            MVP = viewMatrix * scaleMatrix * modelMatrix;

            glUniformMatrix4fv(mvpMatrixID,   1, GL_FALSE, glm::value_ptr(MVP));
            glUniformMatrix4fv(modelMatrixID, 1, GL_FALSE, glm::value_ptr(modelMatrix));
            glUniformMatrix4fv(viewMatrixID,  1, GL_FALSE, glm::value_ptr(viewMatrix));
        
            auto sel = pressed ? pImage : image;
            const auto [offset, length] = objStore.GetObject(sel);
            glDrawElementsBaseVertex(
                GL_TRIANGLES,
                length,
                GL_UNSIGNED_INT,
                (void*) (offset * sizeof(GLuint)),
                offset
            );
        }

        glfwSwapBuffers(window.get());
    }
    while (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) != GLFW_PRESS 
        && glfwWindowShouldClose(window.get()) == 0);

    glDeleteVertexArrays(1, &VertexArrayID);

    ImguiWrapper::Shutdown();

    glfwTerminate();

    return 0;
}


