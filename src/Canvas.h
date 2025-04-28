//
// Created by Rareș Biteș on 28.04.2025.
//

#ifndef CANVAS_H
#define CANVAS_H

#include "Pixel.h"
#include <vector>
#include <string>

#include "stb_image.h"

class Canvas {
public:
    Canvas(int width, int height);
    bool loadFromFile(const std::string& filepath);

    void setPixel(int x, int y, Pixel p);
    Pixel getPixel(int x, int y) const;

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    const std::vector<Pixel>& getData() const { return pixels; }

    std::vector<unsigned char> getRGBAData() const;

private:
    int width, height;
    std::vector<Pixel> pixels;
};

#endif // CANVAS_H
