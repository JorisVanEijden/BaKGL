#include "graphics/inputHandler.hpp"

#include "com/assert.hpp"

#include <functional>

namespace Graphics {

InputHandler::InputHandler() noexcept
:
    mHandleInput{true},
    mKeyBindings{},
    mMouseBindings{},
    mMouseMovedBinding{}
{}

void InputHandler::BindMouseToWindow(GLFWwindow* window, InputHandler& handler)
{
    sHandler = &handler;
    glfwSetMouseButtonCallback(window, InputHandler::MouseAction);
    glfwSetCursorPosCallback(window, InputHandler::MouseMotionAction);
}

void InputHandler::BindKeyboardToWindow(GLFWwindow* window, InputHandler& handler)
{
    sHandler = &handler;
    glfwSetKeyCallback(window, InputHandler::KeyboardAction);
}

void InputHandler::Bind(int key, KeyCallback&& callback)
{
    mKeyBindings.emplace(key, std::move(callback));
}

void InputHandler::BindMouse(int button, MouseCallback&& pressed, MouseCallback&& released)
{
    mMouseBindings.emplace(button,
        std::make_pair(
            std::move(pressed),
            std::move(released)));
}

void InputHandler::BindMouseMotion(MouseCallback&& moved)
{
    mMouseMovedBinding = std::move(moved);
}

void InputHandler::HandleInput(GLFWwindow* window)
{
    if (mHandleInput)
    {
        for (const auto& keyVal : mKeyBindings)
        {
            if (glfwGetKey(window, keyVal.first) == GLFW_PRESS)
            {
                std::invoke(keyVal.second);
            }
        }
    }
}

void InputHandler::HandleMouseCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (mHandleInput)
    {
        const auto it = mMouseBindings.find(button);
        if (it != mMouseBindings.end())
        {
            if (action == GLFW_PRESS)
            {
                double pointerX, pointerY;
                glfwGetCursorPos(window, &pointerX, &pointerY);
                std::invoke(it->second.first, glm::vec2{pointerX, pointerY});
            }
            else if (action == GLFW_RELEASE)
            {
                double pointerX, pointerY;
                glfwGetCursorPos(window, &pointerX, &pointerY);
                std::invoke(it->second.second, glm::vec2{pointerX, pointerY});
            }
        }
    }
}

void InputHandler::HandleMouseMotionCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (mMouseMovedBinding)
        std::invoke(mMouseMovedBinding, glm::vec2{xpos, ypos});
}

void InputHandler::HandleKeyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (mHandleInput)
    {
        const auto it = mKeyBindings.find(key);
        if (it != mKeyBindings.end())
        {
            if (action == GLFW_PRESS)
            {
                std::invoke(it->second);
            }
        }
    }
}

void InputHandler::HandleMouseInput(GLFWwindow* window)
{
    
    if (mHandleInput)
    {
        double pointerX, pointerY;
        glfwGetCursorPos(window, &pointerX, &pointerY);

        for (const auto& keyVal : mMouseBindings)
        {
            if (glfwGetMouseButton(window, keyVal.first) == GLFW_PRESS)
                std::invoke(keyVal.second.first, glm::vec2{pointerX, pointerY});
            if (glfwGetMouseButton(window, keyVal.first) == GLFW_RELEASE)
                std::invoke(keyVal.second.second, glm::vec2{pointerX, pointerY});
        }
    }
}

void InputHandler::MouseAction(GLFWwindow* window, int button, int action, int mods)
{
    ASSERT(sHandler);
    sHandler->HandleMouseCallback(window, button, action, mods);
}

void InputHandler::MouseMotionAction(GLFWwindow* window, double xpos, double ypos)
{
    ASSERT(sHandler);
    sHandler->HandleMouseMotionCallback(window, xpos, ypos);
}

void InputHandler::KeyboardAction(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ASSERT(sHandler);
    sHandler->HandleKeyboardCallback(window, key, scancode, action, mods);
}

InputHandler* InputHandler::sHandler = nullptr;

}
