//
// Created by Rareș Biteș on 28.04.2025.
//

#include "../include/PixelArtImage.h"
#include <iostream>
#include <ranges>
#include <opencv2/core/mat.hpp>

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


bool PixelArtImage::saveToFile(const std::string &filepath) const {
    std::vector<unsigned char> rgba = getRGBAData();
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

void PixelArtImage::setSelectedSegment(const std::vector<Pixel> &segment) {
    clearSelectedSegment();
    selectedSegment.assign(segment.begin(), segment.end());
}

cv::Mat PixelArtImage::extractSubject(const PixelArtImage &canvas) {
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