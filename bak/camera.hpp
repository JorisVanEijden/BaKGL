#pragma once

#include "bak/constants.hpp"
#include "bak/coordinates.hpp"

#include "com/logger.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    Camera(
        unsigned width,
        unsigned height,
        float moveSpeed,
        float turnSpeed)
    :
        mMoveSpeed{moveSpeed},
        mTurnSpeed{turnSpeed},
        mDeltaTime{0},
        mPosition{0,1.4,0},
        mLastPosition{mPosition},
        mProjectionMatrix{CalculatePerspectiveMatrix(width, height)},
        mAngle{3.14, 0}
    {}

    glm::mat4 CalculateOrthoMatrix(unsigned width, unsigned height)
    {
        const auto w = static_cast<float>(width);
        const auto h = static_cast<float>(height);
        return glm::ortho(
            -w,
            h,
            -w,
            h,
            1.0f,
            1000.0f
            //-1000.0f,
            //1000.0f
        );
    }

    glm::mat4 CalculatePerspectiveMatrix(unsigned width, unsigned height)
    {
        return glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(width) / static_cast<float>(height),
            1.0f,
            4000.0f
        );
    }

    void UseOrthoMatrix(unsigned width, unsigned height)
    {
        mProjectionMatrix = CalculateOrthoMatrix(width, height);
    }

    void UsePerspectiveMatrix(unsigned width, unsigned height)
    {
        mProjectionMatrix = CalculatePerspectiveMatrix(width, height);
    }

    void SetGameLocation(const BAK::GamePositionAndHeading& location)
    {
        auto pos = BAK::ToGlCoord<float>(location.mPosition);
        //pos.y = mPosition.y;
        pos.y = 100;
        SetPosition(pos);
        SetAngle(BAK::ToGlAngle(location.mHeading));
    }

    BAK::GamePositionAndHeading GetGameLocation() const
    {
        auto pos = glm::uvec2{mPosition.x, -mPosition.z};
        return BAK::GamePositionAndHeading{pos, GetGameAngle()};
    }

    glm::uvec2 GetGameTile() const
    {
        const auto gamePosition = GetGameLocation();
        return BAK::GetTile(gamePosition.mPosition);
    }

    void SetPosition(const glm::vec3& position)
    {
        mLastPosition = mPosition;
        mPosition = position;
    }
    
    void SetAngle(glm::vec2 angle)
    {
        mAngle = angle;
        mAngle.x = BAK::NormaliseRadians(mAngle.x);
        mAngle.y = BAK::NormaliseRadians(mAngle.y);
    }

    BAK::GameHeading GetHeading()
    {
        // Make sure to normalise this between 0 and 1
        const auto bakAngle = BAK::ToBakAngle(mAngle.x);
        return static_cast<std::uint16_t>(bakAngle);
    }

    void SetDeltaTime(double dt)
    {
        mDeltaTime = dt;
    }

    void MoveForward()
    {
        mLastPosition = mPosition;
        mPosition += GetDirection() * (mMoveSpeed * mDeltaTime);
    }

    void MoveBackward()
    {
        mLastPosition = mPosition;
        mPosition -= GetDirection() * (mMoveSpeed * mDeltaTime);
    }

    void StrafeForward()
    {
        mLastPosition = mPosition;
        mPosition += GetForward() * (mMoveSpeed * mDeltaTime);
    }

    void StrafeBackward()
    {
        mLastPosition = mPosition;
        mPosition -= GetForward() * (mMoveSpeed * mDeltaTime);
    }

    void StrafeRight()
    {
        mLastPosition = mPosition;
        mPosition += GetRight() * (mMoveSpeed * mDeltaTime);
    }

    void StrafeLeft()
    {
        mLastPosition = mPosition;
        mPosition -= GetRight() * (mMoveSpeed * mDeltaTime);
    }

    void StrafeUp()
    {
        mLastPosition = mPosition;
        mPosition += GetUp() * (mMoveSpeed * mDeltaTime);
    }

    void StrafeDown()
    {
        mLastPosition = mPosition;
        mPosition -= GetUp() * (mMoveSpeed * mDeltaTime);
    }

    void UndoPositionChange()
    {
        mPosition = mLastPosition;
    }

    void RotateLeft()
    {
        SetAngle(
            glm::vec2{
                mAngle.x + mTurnSpeed * mDeltaTime,
                mAngle.y});
    }

    void RotateRight()
    {
        SetAngle(
            glm::vec2{
                mAngle.x - mTurnSpeed * mDeltaTime,
                mAngle.y});
    }

    void RotateVerticalUp()
    {
        SetAngle(
            glm::vec2{
                mAngle.x,
                mAngle.y + mTurnSpeed * mDeltaTime});
    }

    void RotateVerticalDown()
    {
        SetAngle(
            glm::vec2{
                mAngle.x,
                mAngle.y - mTurnSpeed * mDeltaTime});
    }

    glm::vec3 GetDirection() const
    {
        return {
            cos(mAngle.y) * sin(mAngle.x),
            sin(mAngle.y),
            cos(mAngle.y) * cos(mAngle.x)
        };
    }

    glm::vec3 GetForward() const
    {
        return {
            cos(mAngle.x - 3.14f/2.0f),
            0,
            -sin(mAngle.x - 3.14f/2.0f)
        };
    }

    glm::vec3 GetRight() const
    {
        return {
            sin(mAngle.x - 3.14f/2.0f),
            0,
            cos(mAngle.x - 3.14f/2.0f)
        };
    }

    glm::vec3 GetUp() const
    {
        return glm::cross(GetRight(), GetDirection());
    }

    glm::vec2 GetAngle() const
    {
        return mAngle;
    }

    BAK::GameHeading GetGameAngle() const
    {
        return BAK::ToBakAngle(mAngle.x);
    }

    glm::mat4 GetViewMatrix() const
    { 
        return glm::lookAt(
            GetNormalisedPosition(),
            GetNormalisedPosition() + GetDirection(),
            GetUp()
        );
    }

    glm::vec3 GetPosition() const { return mPosition; }
    glm::vec3 GetNormalisedPosition() const { return mPosition / BAK::gWorldScale; }
    const glm::mat4& GetProjectionMatrix() const { return mProjectionMatrix; }

private:
    float mMoveSpeed;
    float mTurnSpeed;
    float mDeltaTime;

    glm::vec3 mPosition;
    glm::vec3 mLastPosition;
    glm::mat4 mProjectionMatrix;
    glm::vec2 mAngle;
};

