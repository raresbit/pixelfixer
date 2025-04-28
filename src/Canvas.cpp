//
// Created by Rareș Biteș on 28.04.2025.
//

#include "Canvas.h"
#include <iostream>

Canvas::Canvas(int width_, int height_)
    : width(width_), height(height_), pixels(width_ * height_) {}

bool Canvas::loadFromFile(const std::string& filepath) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(1); // << VERY IMPORTANT
    unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &channels, 0);

    if (data == nullptr) {
        std::cerr << "Failed to load image: " << filepath << std::endl;
        return false;
    }

    width = w;
    height = h;
    pixels.resize(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int offset = (y * width + x) * channels;

            unsigned char r = data[offset];
            unsigned char g = (channels > 1) ? data[offset + 1] : r;
            unsigned char b = (channels > 2) ? data[offset + 2] : r;

            setPixel(x, y, Pixel(r, g, b));
        }
    }

    stbi_image_free(data);
    return true;
}

void Canvas::setPixel(int x, int y, Pixel p) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    pixels[y * width + x] = p;
}

Pixel Canvas::getPixel(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return Pixel();
    return pixels[y * width + x];
}

std::vector<unsigned char> Canvas::getRGBAData() const {
    std::vector<unsigned char> rgba;
    rgba.reserve(width * height * 4);

    for (const auto& p : pixels) {
        rgba.push_back(p.r);
        rgba.push_back(p.g);
        rgba.push_back(p.b);
        rgba.push_back(255); // Force Alpha=255
    }
    return rgba;
}
