//
// Created by Rareș Biteș on 28.04.2025.
//

#ifndef CANVAS_H
#define CANVAS_H

#include "Pixel.h"
#include <vector>
#include <string>

/**
 * @class Canvas
 * A class that represents a 2D pixel canvas.
 */
class Canvas {
public:
    /**
     * Constructor for Canvas objects.
     * @param width in pixels.
     * @param height in pixels.
     */
    Canvas(int width, int height);

    /**
     * Loads an image file into the Canvas.
     * @param filepath path to the file.
     * @return true if successful, otherwise false.
     */
    bool loadFromFile(const std::string &filepath);

    /**
     * Fill the Canvas with a color.
     * @param color The Pixel color to fill the canvas with.
     */
    void fill(const Pixel &color);

    /**
     * Set the pixel at (x, y) given an input p
     * @param x coordinate
     * @param y coordinate
     * @param p pixel to set
     */
    void setPixel(int x, int y, Pixel p);

    /**
     * Get the pixel at (x, y)
     * @param x coordinate
     * @param y coordinate
     * @return pixel at (x, y)
     */
    [[nodiscard]] Pixel getPixel(int x, int y) const;

    /**
     * Get the Canvas width
     * @return the width
     */
    [[nodiscard]] int getWidth() const { return width; }

    /**
     * Get the Canvas height
     * @return the height
     */
    [[nodiscard]] int getHeight() const { return height; }

    /**
     * Get the pixels of the Canvas
     * @return a vector of Pixel
     */
    [[nodiscard]] const std::vector<Pixel> &getData() const { return pixels; }

    /**
     * Returns the pixels as a vector of RGBA bytes, so that we can use it in OpenGL functions.
     * @return a vector of the RGBA bytes
     */
    [[nodiscard]] std::vector<unsigned char> getRGBAData() const;

private:
    int width, height;
    std::vector<Pixel> pixels;
};

#endif // CANVAS_H
