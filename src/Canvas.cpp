//
// Created by Rareș Biteș on 28.04.2025.
//

#include "../include/Canvas.h"
#include <iostream>
#include "stb_image.h"

Canvas::Canvas(const int width, const int height)
    : width(width), height(height), pixels(width * height) {
}

bool Canvas::loadFromFile(const std::string &filepath) {
    int w, h, channels;
    unsigned char *data = stbi_load(filepath.c_str(), &w, &h, &channels, 0);

    if (data == nullptr) {
        std::cerr << "Failed to load image: " << filepath << std::endl;
        return false;
    }

    width = w;
    height = h;
    pixels.resize(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int pos = (y * width + x) * channels;

            unsigned char r = data[pos];
            unsigned char g = (channels > 1) ? data[pos + 1] : r;
            unsigned char b = (channels > 2) ? data[pos + 2] : r;

            setPixel(glm::ivec2(x, y), glm::u8vec3(r, g, b));
        }
    }

    stbi_image_free(data);
    return true;
}

void Canvas::fill(const Color &color) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            setPixel(Pos(x, y), color);
        }
    }
}

void Canvas::setPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    pixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

Pixel Canvas::getPixel(const Pos pos) const {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return {};
    return pixels[pos.y * width + pos.x];
}

std::vector<unsigned char> Canvas::getRGBAData() const {
    std::vector<unsigned char> rgba;
    rgba.reserve(width * height * 4);

    for (const auto &p : pixels) {
        rgba.push_back(p.color.r);
        rgba.push_back(p.color.g);
        rgba.push_back(p.color.b);
        rgba.push_back(255);
    }
    return rgba;
}
