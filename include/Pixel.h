//
// Created by Rareș Biteș on 28.04.2025.
//

#ifndef PIXEL_H
#define PIXEL_H

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <functional>

using Color = glm::u8vec3;
using Pos   = glm::ivec2;

/**
 * Data structure representing a Pixel.
 */
struct Pixel {
    Color color;
    Pos pos;

    bool operator==(const Pixel& other) const {
        return color == other.color && pos == other.pos;
    }
};

template <>
struct std::hash<Color> {
    std::size_t operator()(const glm::u8vec3& color) const noexcept {
        return (static_cast<std::size_t>(color.r) << 16) |
               (static_cast<std::size_t>(color.g) << 8)  |
               static_cast<std::size_t>(color.b);
    }
};

template <>
struct std::hash<Pos> {
    std::size_t operator()(const glm::ivec2& v) const noexcept {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
    }

    hash() = default;
    hash(const hash&) = default;
    hash& operator=(const hash&) = default;
};


#endif //PIXEL_H
