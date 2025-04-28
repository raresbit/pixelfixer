//
// Created by Rareș Biteș on 28.04.2025.
//

#include "Canvas.h"

Canvas::Canvas(int width_, int height_)
    : width(width_), height(height_), pixels(width_ * height_) {
}

void Canvas::setPixel(int x, int y, Pixel p) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    pixels[y * width + x] = p;
}

Pixel Canvas::getPixel(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return Pixel();
    return pixels[y * width + x];
}
