//
// Created by Rareș Biteș on 28.04.2025.
//

#include "../include/PixelArtImage.h"
#include <iostream>
#include <ranges>
#include <opencv2/core/mat.hpp>
#include <utility>

#include "stb_image.h"
#include "stb_image_write.h"

PixelArtImage::PixelArtImage(const int width, const int height)
    : width(width), height(height), pixels(width * height), processedPixels(width * height),
      debugPixels(width * height) {
}

PixelArtImage::PixelArtImage(const PixelArtImage &other) = default;

PixelArtImage &PixelArtImage::operator=(const PixelArtImage &other) {
    if (this == &other) return *this;

    width = other.width;
    height = other.height;
    pixels = other.pixels;
    processedPixels = other.processedPixels;
    debugPixels = other.debugPixels;
    debugLines = other.debugLines;
    highlightedPixels = other.highlightedPixels;
    affectedSegments = other.affectedSegments;
    clusters = other.clusters;

    return *this;
}

bool PixelArtImage::loadFromFile(const std::string &filepath) {
    int w, h, channels;
    unsigned char *data = stbi_load(filepath.c_str(), &w, &h, &channels, 0);

    if (data == nullptr) {
        std::cerr << "Failed to load image: " << filepath << std::endl;
        return false;
    }

    width = w;
    height = h;
    pixels.resize(width * height);
    clearProcessedPixels();
    processedPixels.resize(width * height);
    clearDebugPixels();
    debugPixels.resize(width * height);
    clearHighlightedPixels();
    highlightedPixels.resize(width * height);
    clusters = segmentClusters();
    selectedSegment.clear();
    clearDrawnPath();

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

void PixelArtImage::fill(const Color &color) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            setPixel(Pos(x, y), color);
        }
    }
}

void PixelArtImage::setPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    pixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

void PixelArtImage::setPixels(const std::vector<Pixel>& pixels) {
    for (auto pixel : pixels) {
        setPixel(pixel.pos, pixel.color);
    }
}

Pixel PixelArtImage::getPixel(const Pos pos) const {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return {};

    int index = pos.y * width + pos.x;

    if (debugPixels[index].has_value()) {
        return debugPixels[index].value();
    }

    if (processedPixels[index].has_value()) {
        return processedPixels[index].value();
    }

    return pixels[index];
}


std::vector<unsigned char> PixelArtImage::getRGBAData() const {
    std::vector<unsigned char> rgba;
    rgba.reserve(width * height * 4);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Pixel p = getPixel(Pos(x, y));
            rgba.push_back(p.color.r);
            rgba.push_back(p.color.g);
            rgba.push_back(p.color.b);
            rgba.push_back(255);
        }
    }

    return rgba;
}

void PixelArtImage::setProcessedPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    processedPixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

void PixelArtImage::setProcessedPixels(const PixelArtImage &other) {
    for (int y = 0; y < other.getHeight(); ++y) {
        for (int x = 0; x < other.getWidth(); ++x) {
            this->setProcessedPixel({x, y}, other.getPixel({x, y}).color);
        }
    }
}

void PixelArtImage::clearProcessedPixels() {
    std::ranges::fill(processedPixels, std::nullopt);
}

void PixelArtImage::setDebugPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    debugPixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

void PixelArtImage::setDebugPixels(const PixelArtImage &other) {
    for (int y = 0; y < other.getHeight(); ++y) {
        for (int x = 0; x < other.getWidth(); ++x) {
            this->setDebugPixel({x, y}, other.getPixel({x, y}).color);
        }
    }
}

void PixelArtImage::clearDebugPixels() {
    std::ranges::fill(debugPixels, std::nullopt);
}

void PixelArtImage::addDebugLine(glm::vec2 start, glm::vec2 end, Color color) {
    debugLines.emplace_back(start, end, color);
}

const std::vector<std::tuple<glm::vec2, glm::vec2, Color> > &PixelArtImage::getDebugLines() const {
    return debugLines;
}

void PixelArtImage::setDebugLines(const std::vector<std::tuple<glm::vec2, glm::vec2, Color>> &debugLines) {
    this->debugLines = debugLines;
}

void PixelArtImage::clearDebugLines() {
    debugLines.clear();
}

void PixelArtImage::clearDebugLinesWithColor(const Color &color) {
    debugLines.erase(
        std::remove_if(
            debugLines.begin(),
            debugLines.end(),
            [&](const auto &line) {
                const Color &lineColor = std::get<2>(line);
                return lineColor == color;
            }
        ),
        debugLines.end()
    );
}


/**
 * Saves the current image to a PNG file at the given filepath.
 *
 * If debug lines have been drawn on the canvas, the output image will be scaled by a factor of 10
 * and the debug lines will be rendered over the scaled image. If no debug lines are present,
 * the image is saved at its original resolution without scaling.
 *
 * @param filepath The destination file path for the PNG image.
 * @return True if the file was saved successfully, false otherwise.
 */
bool PixelArtImage::saveToFile(const std::string &filepath) const {
    std::vector<unsigned char> rgba = getRGBAData();
    std::vector<std::tuple<glm::vec2, glm::vec2, Color>> debugLines = getDebugLines();

    if (!debugLines.empty()) {
        const int scale = 10;
        int scaledWidth = width * scale;
        int scaledHeight = height * scale;

        std::vector<unsigned char> scaledRGBA(scaledWidth * scaledHeight * 4, 255);

        // Scale original image
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int dy = 0; dy < scale; ++dy) {
                    for (int dx = 0; dx < scale; ++dx) {
                        int srcIndex = (y * width + x) * 4;
                        int dstX = x * scale + dx;
                        int dstY = y * scale + dy;
                        int dstIndex = (dstY * scaledWidth + dstX) * 4;
                        for (int c = 0; c < 4; ++c) {
                            scaledRGBA[dstIndex + c] = rgba[srcIndex + c];
                        }
                    }
                }
            }
        }

        // Draw debug lines
        auto drawLine = [&](glm::vec2 start, glm::vec2 end, const Color &color) {
            int x0 = static_cast<int>(start.x * scale);
            int y0 = static_cast<int>(start.y * scale);
            int x1 = static_cast<int>(end.x * scale);
            int y1 = static_cast<int>(end.y * scale);

            // Bresenham's line algorithm
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;

            while (true) {
                if (x0 >= 0 && x0 < scaledWidth && y0 >= 0 && y0 < scaledHeight) {
                    int idx = (y0 * scaledWidth + x0) * 4;
                    scaledRGBA[idx + 0] = color.r;
                    scaledRGBA[idx + 1] = color.g;
                    scaledRGBA[idx + 2] = color.b;
                    scaledRGBA[idx + 3] = 255;
                }
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        };

        for (const auto &line : debugLines) {
            glm::vec2 start = std::get<0>(line);
            glm::vec2 end = std::get<1>(line);
            Color color = std::get<2>(line);

            // Translate down by 0.5 * scale
            start.y += 0.5f;
            end.y += 0.5f;

            // Draw a three-px width line
            // First draw the base 1px one
            drawLine(start, end, color);

            // Determine if horizontal or vertical line (simple check)
            bool isHorizontal = std::abs(end.y - start.y) < std::abs(end.x - start.x);
            bool isVertical = !isHorizontal;

            float offset = 1.0f / scale;  // offset by 1 pixel in image space before scaling

            if (isHorizontal) {
                // Draw one line 1px above (subtract offset in y)
                glm::vec2 aboveStart = start; aboveStart.y -= offset;
                glm::vec2 aboveEnd = end; aboveEnd.y -= offset;
                drawLine(aboveStart, aboveEnd, color);

                // Draw one line 1px below (add offset in y)
                glm::vec2 belowStart = start; belowStart.y += offset;
                glm::vec2 belowEnd = end; belowEnd.y += offset;
                drawLine(belowStart, belowEnd, color);
            } else if (isVertical) {
                // Draw one line 1px left (subtract offset in x)
                glm::vec2 leftStart = start; leftStart.x -= offset;
                glm::vec2 leftEnd = end; leftEnd.x -= offset;
                drawLine(leftStart, leftEnd, color);

                // Draw one line 1px right (add offset in x)
                glm::vec2 rightStart = start; rightStart.x += offset;
                glm::vec2 rightEnd = end; rightEnd.x += offset;
                drawLine(rightStart, rightEnd, color);
            }

        }

        return stbi_write_png(filepath.c_str(), scaledWidth, scaledHeight, 4, scaledRGBA.data(), scaledWidth * 4) != 0;
    }

    return stbi_write_png(filepath.c_str(), width, height, 4, rgba.data(), width * 4) != 0;
}



std::vector<std::vector<std::vector<Pixel> > > PixelArtImage::segmentClusters(bool horizontalOrientation) {
    clearClusters();
    std::vector<bool> visited(width * height, false);

    cv::Mat mask = extractSubject(*this);

    std::vector<std::vector<std::vector<Pixel> > > clusteredSegments;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (mask.at<uchar>(y, x) != 255) continue;

            Pos pos(x, y);
            if (!visited[y * width + x]) {
                Color clusterColor = getPixel(pos).color;
                std::vector<Pixel> fullCluster;
                std::stack<Pos> stack;
                stack.push(pos);
                visited[y * width + x] = true;

                while (!stack.empty()) {
                    Pos current = stack.top();
                    stack.pop();
                    fullCluster.push_back(getPixel(current));

                    for (const auto &dir: std::vector<glm::ivec2>{{0, 1}, {0, -1}, {1, 0}, {-1, 0}}) {
                        Pos neighbor = current + dir;

                        if (neighbor.x >= 0 && neighbor.x < width &&
                            neighbor.y >= 0 && neighbor.y < height &&
                            mask.at<uchar>(neighbor.y, neighbor.x) == 255 &&
                            !visited[neighbor.y * width + neighbor.x] &&
                            getPixel(neighbor).color == clusterColor) {
                            visited[neighbor.y * width + neighbor.x] = true;
                            stack.push(neighbor);
                        }
                    }
                }

                // Segment fullCluster by rows or columns into segments
                std::unordered_map<int, std::vector<Pixel> > lines;
                for (const auto &pixel: fullCluster) {
                    int key = horizontalOrientation ? pixel.pos.y : pixel.pos.x;
                    lines[key].push_back(pixel);
                }

                std::vector<std::vector<Pixel> > segmentsInCluster;

                for (auto &linePixels: lines | std::views::values) {
                    std::ranges::sort(linePixels, [&](const Pixel &a, const Pixel &b) {
                        return horizontalOrientation ? a.pos.x < b.pos.x : a.pos.y < b.pos.y;
                    });

                    std::vector<Pixel> segment;
                    for (auto & i : linePixels) {
                        if (segment.empty()) {
                            segment.push_back(i);
                        } else {
                            int prevCoord = horizontalOrientation ? segment.back().pos.x : segment.back().pos.y;
                            int currCoord = horizontalOrientation ? i.pos.x : i.pos.y;

                            if (currCoord == prevCoord + 1) {
                                segment.push_back(i);
                            } else {
                                if (!segment.empty()) segmentsInCluster.push_back(segment);
                                segment.clear();
                                segment.push_back(i);
                            }
                        }
                    }

                    if (!segment.empty()) {
                        segmentsInCluster.push_back(segment);
                    }
                }

                if (!segmentsInCluster.empty()) {
                    clusters.push_back(segmentsInCluster);
                }
            }
        }
    }

    return clusters;
}


void PixelArtImage::clearHighlightedPixels() {
    std::ranges::fill(highlightedPixels, std::nullopt);
}

void PixelArtImage::setHighlightedPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    highlightedPixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

void PixelArtImage::setHighlightedPixels(const std::vector<Pos> &cluster, Color color) {
    for (const auto &pos: cluster) {
        setHighlightedPixel(pos, color);
    }
}

const std::vector<std::optional<Pixel> > &PixelArtImage::getHighlightedPixels() const {
    return highlightedPixels;
}

[[nodiscard]] const std::vector<std::vector<std::vector<Pixel> > > &PixelArtImage::getClusters() const {
    return clusters;
}

void PixelArtImage::clearClusters() {
    clusters.clear();
}

[[nodiscard]] const std::vector<Pixel> &PixelArtImage::getSelectedSegment() const {
    return selectedSegment;
}

void PixelArtImage::clearSelectedSegment() {
    selectedSegment.clear();
}

std::vector<std::vector<Pixel>> PixelArtImage::getAffectedSegments() const {
    return affectedSegments;
}

void PixelArtImage::setAffectedSegments(std::vector<std::vector<Pixel>> affectedSegs) {
    affectedSegments = std::move(affectedSegs);
}

void PixelArtImage::setSelectedSegment(const std::vector<Pixel> &segment) {
    clearSelectedSegment();
    selectedSegment.assign(segment.begin(), segment.end());
}

void PixelArtImage::setError(int err) {
    error = err;
}

int PixelArtImage::getError() const {
    return error;
}

cv::Mat PixelArtImage::extractSubject(const PixelArtImage &canvas) {
    int width = canvas.getWidth();
    int height = canvas.getHeight();

    cv::Mat mask(height, width, CV_8UC1, cv::Scalar(0));

    constexpr int threshold = 254; // tolerance for "near-white"
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

std::optional<Pixel> PixelArtImage::getGenerator() const {
    return generator;
}

void PixelArtImage::setGenerator(const Pixel &generator) {
    this->generator = generator;
}

void PixelArtImage::clearGenerator() {
    generator = std::nullopt;
}

void PixelArtImage::clearDrawnPath() {
    drawnPath.clear();
}

void PixelArtImage::addDrawnPath(const Pixel &pixel) {
    drawnPath.push_back(pixel);
}

[[nodiscard]] const std::vector<Pixel> &PixelArtImage::getDrawnPath() const {
    return drawnPath;
}

void PixelArtImage::drawRectangle(const std::vector<Pixel> &pixels, const Color &color) {
    if (pixels.empty()) return;

    // Find bounding box of all pixels
    int minX = pixels[0].pos.x;
    int maxX = pixels[0].pos.x;
    int minY = pixels[0].pos.y;
    int maxY = pixels[0].pos.y;

    for (const auto &pixel: pixels) {
        if (pixel.pos.x < minX) minX = pixel.pos.x;
        if (pixel.pos.x > maxX) maxX = pixel.pos.x;
        if (pixel.pos.y < minY) minY = pixel.pos.y;
        if (pixel.pos.y > maxY) maxY = pixel.pos.y;
    }

    // Use float for coordinates
    auto x0 = static_cast<float>(minX);
    auto x1 = static_cast<float>(maxX);
    auto y0 = static_cast<float>(minY);
    auto y1 = static_cast<float>(maxY);

    // Define corners around the bounding box (pixel edges)
    glm::vec2 topLeft = glm::vec2(x0 - 0.5f, y0 - 0.5f);
    glm::vec2 topRight = glm::vec2(x1 + 0.5f, y0 - 0.5f);
    glm::vec2 bottomRight = glm::vec2(x1 + 0.5f, y1 + 0.5f);
    glm::vec2 bottomLeft = glm::vec2(x0 - 0.5f, y1 + 0.5f);

    // Offset all corners 0.5 to the right as per your previous request
    glm::vec2 tl = glm::vec2(topLeft.x + 0.5f, topLeft.y);
    glm::vec2 tr = glm::vec2(topRight.x + 0.5f, topRight.y);
    glm::vec2 br = glm::vec2(bottomRight.x + 0.5f, bottomRight.y);
    glm::vec2 bl = glm::vec2(bottomLeft.x + 0.5f, bottomLeft.y);

    // Draw rectangle around the bounding box
    addDebugLine(tl, tr, color);
    addDebugLine(tr, br, color);
    addDebugLine(br, bl, color);
    addDebugLine(bl, tl, color);
}