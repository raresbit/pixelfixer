//
// Created by Rareș Biteș on 28.04.2025.
//

#ifndef CANVAS_H
#define CANVAS_H

#include "Pixel.h"
#include <vector>
#include <string>
#include <opencv2/core/mat.hpp>

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
    Canvas(const Canvas &other);

    /**
     * Deep copy function.
     * @param other canvas to copy
     * @return deep-copied canvas
     */
    Canvas &operator=(const Canvas &other);

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
     * Sets a pixel on the canvas' processed layer
     */
    void setProcessedPixel(Pos pos, Color color);

    /**
     * Sets the processed pixels based on an existing canvas
     */
    void setProcessedPixels(const Canvas &other);

    /**
     * Clears the processed layer
     */
    void clearProcessedPixels();

    /**
     * Sets a pixel on the canvas' debug layer
     */
    void setDebugPixel(Pos pos, Color color);

    /**
     * Sets the debug pixels based on an existing canvas
     */
    void setDebugPixels(const Canvas &other);

    /**
     * Clears the debug layer
     */
    void clearDebugPixels();

    void addDebugLine(glm::vec2 start, glm::vec2 end, Color color);

    [[nodiscard]] const std::vector<std::tuple<glm::vec2, glm::vec2, Color> > &getDebugLines() const;

    void setDebugLines(const std::vector<std::tuple<glm::vec2, glm::vec2, Color>> &debug_lines) {
        debugLines = debug_lines;
    }

    void clearDebugLines();


    /**
     * Save the canvas to a file
     */
    [[nodiscard]] bool saveToFile(const std::string &filepath) const;


    /**
     * Detects color-based pixel clusters in the canvas and splits them into linear segments (horizontal or vertical).
     * @param horizontalOrientation If true, clusters are split into horizontal segments (by rows);
     *                              if false, they are split into vertical segments (by columns).
     * @return A list of clusters, where each cluster is represented as a list of segments,
     *         and each segment is a list of contiguous pixels.
     *         - Outer vector: List of clusters
     *         - Middle vector: List of segments within a cluster
     *         - Inner vector: Pixels belonging to one linear segment
     */
    std::vector<std::vector<std::vector<Pixel> > > segmentClusters(bool horizontalOrientation = true);

    void clearHighlightedPixels();

    void setHighlightedPixel(Pos pos, Color color);

    void setHighlightedPixels(const std::vector<Pos> &cluster, Color color);

    const std::vector<std::optional<Pixel> > &getHighlightedPixels() const;

    const std::vector<std::vector<std::vector<Pixel>>> &getClusters() const;

    void clearClusters();

    const std::vector<Pixel> &getSelectedSegment() const;

    void clearSelectedSegment();

    void setSelectedSegment(const std::vector<Pixel> &segment);

    static cv::Mat extractSubject(const Canvas &canvas) {
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        cv::Mat mask(height, width, CV_8UC1, cv::Scalar(0));

        constexpr int threshold = 250; // tolerance for "near-white"
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Color color = canvas.getPixel({x, y}).color;
                if (color.r < threshold || color.g < threshold || color.b < threshold) {
                    mask.at<uchar>(y, x) = 255;
                }
            }
        }

        return mask;
    }

    [[nodiscard]] std::optional<Pixel> getGenerator() const {
        return generator;
    }

    void setGenerator(const Pixel &generator) {
        this->generator = generator;
    }

    void clearGenerator() {
        generator = std::nullopt;
    }

    void clearDrawnPath() {
        drawnPath.clear();
    }

    void addDrawnPath(const Pixel &pixel) {
        drawnPath.push_back(pixel);
    }

    [[nodiscard]] const std::vector<Pixel> &getDrawnPath() const {
        return drawnPath;
    }

private:
    int width, height;
    std::vector<Pixel> pixels;
    std::vector<std::optional<Pixel> > processedPixels;
    std::vector<std::optional<Pixel> > debugPixels;
    std::vector<std::tuple<glm::vec2, glm::vec2, Color> > debugLines;
    std::vector<std::optional<Pixel> > highlightedPixels; // Stores pixels that are highlighted (hovered over)
    std::vector<std::vector<std::vector<Pixel>> > clusters;
    std::vector<Pixel> selectedSegment;
    std::optional<Pixel> generator;
    std::vector<Pixel> drawnPath;
};

#endif // CANVAS_H
