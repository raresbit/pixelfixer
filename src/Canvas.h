//
// Created by Rareș Biteș on 28.04.2025.
//

#ifndef CANVAS_H
#define CANVAS_H


#include "Pixel.h"
#include <vector>

class Canvas {
public:
    Canvas(int width, int height);

    void setPixel(int x, int y, Pixel p);

    Pixel getPixel(int x, int y) const;

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    const std::vector<Pixel> &getData() const { return pixels; }

private:
    int width, height;
    std::vector<Pixel> pixels;
};


#endif //CANVAS_H
