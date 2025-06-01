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
 * @class PixelArtImage
 * A class that represents a 2D pixel canvas.
 */
class PixelArtImage {
public:
    /**
     * Constructor for PixelArtImage objects.
     * @param width in pixels.
     * @param height in pixels.
     */
    PixelArtImage(int width, int height);

    /**
     * Deep copy constructor for PixelArtImage objects.
     * @param other canvas to copy.
     */
    PixelArtImage(const PixelArtImage &other);

    /**
     * Deep copy function.
     * @param other canvas to copy
     * @return deep-copied canvas
     */
    PixelArtImage &operator=(const PixelArtImage &other);

    /**
     * Loads an image file into the PixelArtImage.
     * @param filepath path to the file.
     * @return true if successful, otherwise false.
     */
    bool loadFromFile(const std::string &filepath);

    /**
     * Fill the PixelArtImage with a color.
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
     * Draw the input pixels on the image
     * @param pixels input pixels to draw
     */
    void setPixels(const std::vector<Pixel>& pixels);


    /**
     * Get the pixel at a position pos
     * @param pos the (x, y) coordinates
     * @return pixel at position pos
     */
    [[nodiscard]] Pixel getPixel(Pos pos) const;

    /**
     * Get the PixelArtImage width
     * @return the width
     */
    [[nodiscard]] int getWidth() const { return width; }

    /**
     * Get the PixelArtImage height
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
    void setProcessedPixels(const PixelArtImage &other);

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
    void setDebugPixels(const PixelArtImage &other);

    /**
     * Clears the debug layer
     */
    void clearDebugPixels();

    /**
     * Adds a debug line to the canvas.
     *
     * This method allows the addition of a line represented by its start, end coordinates,
     * and a specific color to the debug layer of the canvas.
     *
     * @param start The starting point of the debug line in 2D space.
     * @param end The endpoint of the debug line in 2D space.
     * @param color The color of the debug line.
     */
    void addDebugLine(glm::vec2 start, glm::vec2 end, Color color);

    /**
     * @brief Retrieves the debug lines currently set on the canvas.
     *
     * Debug lines are represented as a collection of tuples, where each tuple contains
     * the start point, end point, and color of a line.
     *
     * @return A reference to a vector of debug lines, where each line is specified
     *         as a tuple of glm::vec2 (start point), glm::vec2 (end point), and Color (line color).
     */
    [[nodiscard]] const std::vector<std::tuple<glm::vec2, glm::vec2, Color> > &getDebugLines() const;

    /**
     * Sets the debug lines for the canvas. Debug lines are represented as a collection of tuples,
     * where each tuple contains two points (start and end) and a color.
     *
     * @param debugLines A vector of tuples, each containing:
     *                   - glm::vec2: Start point of the line.
     *                   - glm::vec2: End point of the line.
     *                   - Color: The color of the line.
     */
    void setDebugLines(const std::vector<std::tuple<glm::vec2, glm::vec2, Color> > &debugLines);

    /**
     * @brief Clears all debug lines on the canvas.
     *
     * Removes any visual debug markers or lines drawn on the canvas' debug layer.
     * This is typically used to reset the debug state of the canvas for a new operation or render.
     */
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

    /**
     * @brief Clears the highlighted pixels layer on the canvas.
     *
     * Resets all highlighted pixels to their initial state, effectively removing any
     * visual highlighting or hover effects applied to those pixels. This ensures
     * a clean visual state for the highlighted pixel layer.
     */
    void clearHighlightedPixels();

    /**
     * @brief Sets a specific pixel on the canvas as highlighted.
     *
     * Updates the pixel at the given position with the specified color.
     * If the position is outside the canvas boundaries, the function does nothing.
     *
     * @param pos The position of the pixel to highlight.
     * @param color The color to assign to the highlighted pixel.
     */
    void setHighlightedPixel(Pos pos, Color color);

    /**
     * @brief Highlights a cluster of pixels with a specified color.
     *
     * Updates the appearance of multiple pixels on the canvas by setting their highlight
     * color based on the given cluster of positions.
     *
     * @param cluster A vector containing the positions of the pixels to be highlighted.
     * @param color The color to apply to the highlighted pixels.
     */
    void setHighlightedPixels(const std::vector<Pos> &cluster, Color color);

    /**
     * Retrieves the collection of highlighted pixels on the canvas.
     * These pixels may be highlighted due to selected areas or other visual markers.
     *
     * @return A reference to a vector containing optional Pixel objects representing the highlighted pixels.
     */
    [[nodiscard]] const std::vector<std::optional<Pixel> > &getHighlightedPixels() const;

    /**
     * Retrieves the clusters of pixels stored in the canvas.
     * Clusters represent grouped pixels organized in a hierarchical vector structure.
     *
     * @return A constant reference to the 3D vector containing the pixel clusters.
     */
    [[nodiscard]] const std::vector<std::vector<std::vector<Pixel> > > &getClusters() const;

    /**
     * Clears all the clusters stored in the PixelArtImage.
     *
     * This method removes all the cluster data associated with the PixelArtImage,
     * leaving it in an empty state with no clusters present.
     */
    void clearClusters();

    /**
     * Returns a reference to the selected segment of pixels on the canvas.
     *
     * @return A constant reference to a vector containing the selected pixels.
     */
    [[nodiscard]] const std::vector<Pixel> &getSelectedSegment() const;

    /**
     * Clears the currently selected segment on the canvas.
     *
     * This method resets the selected segment of the canvas by
     * removing its content. Useful for situations where any
     * user selection needs to be reset or modified.
     */
    void clearSelectedSegment();

    /**
     * Sets the selected segment of pixels for the canvas.
     * Replaces the current selected segment with the provided segment.
     *
     * @param segment A vector of pixels representing the segment to be selected.
     */
    void setSelectedSegment(const std::vector<Pixel> &segment);

    /**
     * Extracts the subject from a given canvas by creating a mask that highlights non-white areas.
     *
     * @param canvas The input Canvas object from which the subject will be extracted.
     * @return A cv::Mat object representing a binary mask, where the subject areas are highlighted.
     */
    static cv::Mat extractSubject(const PixelArtImage &canvas);

    /**
     * Retrieves the generator pixel of the canvas, if available.
     *
     * @return An optional Pixel representing the generator or empty if no generator is set.
     */
    [[nodiscard]] std::optional<Pixel> getGenerator() const;

    /**
     * Sets the generator pixel used internally by the canvas.
     *
     * @param generator The Pixel instance to be set as the generator.
     */
    void setGenerator(const Pixel &generator);

    /**
     * @brief Clears the generator associated with the Canvas.
     *
     * Resets the generator to a null optional state, effectively removing any
     * existing generator associated with the PixelArtImage.
     */
    void clearGenerator();

    /**
     * Clears the currently drawn path on the canvas.
     *
     * This method removes all elements from the internal representation
     * of the drawn path, effectively resetting it to an empty state.
     */
    void clearDrawnPath();

    /**
     * Adds a drawn path by appending the specified pixel to the drawn path.
     *
     * @param pixel The pixel to be added to the drawn path.
     */
    void addDrawnPath(const Pixel &pixel);

    /**
     * @brief Retrieves the path of drawn pixels on the canvas.
     *
     * @return A constant reference to a vector containing the pixels
     *         representing the drawn path.
     */
    [[nodiscard]] const std::vector<Pixel> &getDrawnPath() const;


    /**
     * @brief Draws a rectangle around the bounding box of a set of pixels on the canvas.
     *
     * This method calculates the bounding box of the provided pixel positions and then draws
     * a rectangle around it using the given color. The rectangle is drawn slightly offset
     * to align with pixel edges.
     *
     * @param pixels A collection of pixels whose bounding box is used to define the rectangle.
     * @param color The color used to draw the rectangle.
     */
    void drawRectangle(const std::vector<Pixel> &pixels, const Color &color);

private:
    int width, height;
    std::vector<Pixel> pixels;
    std::vector<std::optional<Pixel> > processedPixels;
    std::vector<std::optional<Pixel> > debugPixels;
    std::vector<std::tuple<glm::vec2, glm::vec2, Color> > debugLines;
    std::vector<std::optional<Pixel> > highlightedPixels; // Stores pixels that are highlighted (hovered over)
    std::vector<std::vector<std::vector<Pixel> > > clusters;
    std::vector<Pixel> selectedSegment;
    std::optional<Pixel> generator;
    std::vector<Pixel> drawnPath;
};

#endif // CANVAS_H
