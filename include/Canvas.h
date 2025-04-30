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
     * Deep copy constructor for Canvas objects.
     * @param other canvas to copy.
     */
    Canvas(const Canvas& other);

    /**
     * Deep copy function.
     * @param other canvas to copy
     * @return deep-copied canvas
     */
    Canvas& operator=(const Canvas& other);

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
    void fill(const Color &color);

    /**
     * Set the pixel at a position pos with a color
     * @param pos coordinates
     * @param color color to set
     */
    void setPixel(Pos pos, Color color);

    /**
     * Get the pixel at a position pos
     * @param pos the (x, y) coordinates
     * @return pixel at position pos
     */
    [[nodiscard]] Pixel getPixel(Pos pos) const;

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
     * Returns the pixels as a vector of RGBA bytes, so that we can use it in OpenGL functions.
     * @return a vector of the RGBA bytes
     */
    [[nodiscard]] std::vector<unsigned char> getRGBAData() const;

    /**
     * Creates a new debug layer on top of the canvas
     */
    void setDebugPixel(Pos pos, Color color);

    /**
     * Removes the debug layer at a certain index
     */
    void clearDebugPixels();

    /**
     * Save the canvas to a file
     */
    bool saveToFile(const std::string &filepath) const;


private:
    int width, height;
    std::vector<Pixel> pixels;
    std::vector<std::optional<Pixel>> debugPixels;
};

#endif // CANVAS_H
