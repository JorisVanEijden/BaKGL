#pragma once


// BAK coordinate system
// 
// Origin is at 0
// positive x goes right
// positive y goes up
//
// e.g.
//
//                            T1117 (736'000, 1'120'000)
// T1016 (672'000, 1'056'000) T1116 (736'000, 1'056'000)
//
// Need to convert this to open gl system by swapping y and z
// and negating z (depth goes towards us)

#include "graphics/glm.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "xbak/Geometry.h"

#include <ostream>

namespace BAK {

using GamePosition = glm::vec<2, unsigned>;
using GameHeading  = std::uint16_t;

struct GamePositionAndHeading
{
    GamePosition mPosition;
    GameHeading mHeading;
};

std::ostream& operator<<(std::ostream& os, const GamePositionAndHeading&);

GamePosition MakeGamePositionFromTileAndOffset(
    glm::vec<2, unsigned> tile,
    glm::vec<2, std::uint8_t> offset);

template <typename T>
glm::vec<3, T> ToGlCoord(const Vector3D& coord)
{
    return glm::vec<3, T>{
        static_cast<T>(coord.GetX()),
        static_cast<T>(coord.GetZ()),
        -static_cast<T>(coord.GetY())};
}

template <typename T>
glm::vec<3, T> ToGlCoord(const glm::vec<2, int>& coord)
{
    return glm::vec<3, T>{
        static_cast<T>(coord.x),
        static_cast<T>(0),
        -static_cast<T>(coord.y)};
}

// Convert a 16 bit BAK angle to radians
glm::vec3 ToGlAngle(const Vector3D& angle);

// Convert a 16 bit BAK angle to radians
glm::vec2 ToGlAngle(GameHeading);

template <typename T, typename C>
glm::vec<4, T> ToGlColor(const C& color)
{
    const auto F = [](auto x){
        return static_cast<T>(x) / 256.; };

    return glm::vec<4, T>{
        F(color.r),
        F(color.g),
        F(color.b),
        F(color.a)};
}

}
