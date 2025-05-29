//
// Created by Rareș Biteș on 28.04.2025.
//

#include "../include/Canvas.h"
#include <iostream>
#include <ranges>
#include <opencv2/core/mat.hpp>

#include "stb_image.h"
#include "stb_image_write.h"

Canvas::Canvas(const int width, const int height)
    : width(width), height(height), pixels(width * height), processedPixels(width * height), debugPixels(width * height) {
}

Canvas::Canvas(const Canvas &other) = default;

Canvas &Canvas::operator=(const Canvas &other) {
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

bool Canvas::loadFromFile(const std::string &filepath) {
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

void Canvas::fill(const Color &color) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            setPixel(Pos(x, y), color);
        }
    }
}

void Canvas::setPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    pixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

Pixel Canvas::getPixel(const Pos pos) const {
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


std::vector<unsigned char> Canvas::getRGBAData() const {
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

void Canvas::setProcessedPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    processedPixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

void Canvas::setProcessedPixels(const Canvas& other) {
    for (int y = 0; y < other.getHeight(); ++y) {
        for (int x = 0; x < other.getWidth(); ++x) {
            this->setProcessedPixel({x, y}, other.getPixel({x, y}).color);
        }
    }
}

void Canvas::clearProcessedPixels() {
    std::ranges::fill(processedPixels, std::nullopt);
}

void Canvas::setDebugPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    debugPixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

void Canvas::setDebugPixels(const Canvas& other) {
    for (int y = 0; y < other.getHeight(); ++y) {
        for (int x = 0; x < other.getWidth(); ++x) {
            this->setDebugPixel({x, y}, other.getPixel({x, y}).color);
        }
    }
}

void Canvas::clearDebugPixels() {
    std::ranges::fill(debugPixels, std::nullopt);
}

void Canvas::addDebugLine(glm::vec2 start, glm::vec2 end, Color color) {
    debugLines.emplace_back(start, end, color);
}

const std::vector<std::tuple<glm::vec2, glm::vec2, Color>>& Canvas::getDebugLines() const {
    return debugLines;
}

void Canvas::clearDebugLines() {
    debugLines.clear();
}


bool Canvas::saveToFile(const std::string &filepath) const {
    std::vector<unsigned char> rgba = getRGBAData();
    return stbi_write_png(filepath.c_str(), width, height, 4, rgba.data(), width * 4) != 0;
}


std::vector<std::vector<std::vector<Pixel>>> Canvas::segmentClusters(bool horizontalOrientation) {
    clearClusters();
    std::vector<bool> visited(width * height, false);

    cv::Mat mask = extractSubject(*this);

    std::vector<std::vector<std::vector<Pixel>>> clusteredSegments;

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

                    for (const auto &dir : std::vector<glm::ivec2>{{0, 1}, {0, -1}, {1, 0}, {-1, 0}}) {
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
                std::unordered_map<int, std::vector<Pixel>> lines;
                for (const auto &pixel : fullCluster) {
                    int key = horizontalOrientation ? pixel.pos.y : pixel.pos.x;
                    lines[key].push_back(pixel);
                }

                std::vector<std::vector<Pixel>> segmentsInCluster;

                for (auto &[_, linePixels] : lines) {
                    std::sort(linePixels.begin(), linePixels.end(), [&](const Pixel &a, const Pixel &b) {
                        return horizontalOrientation ? a.pos.x < b.pos.x : a.pos.y < b.pos.y;
                    });

                    std::vector<Pixel> segment;
                    for (size_t i = 0; i < linePixels.size(); ++i) {
                        if (segment.empty()) {
                            segment.push_back(linePixels[i]);
                        } else {
                            int prevCoord = horizontalOrientation ? segment.back().pos.x : segment.back().pos.y;
                            int currCoord = horizontalOrientation ? linePixels[i].pos.x : linePixels[i].pos.y;

                            if (currCoord == prevCoord + 1) {
                                segment.push_back(linePixels[i]);
                            } else {
                                if (!segment.empty()) segmentsInCluster.push_back(segment);
                                segment.clear();
                                segment.push_back(linePixels[i]);
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




void Canvas::clearHighlightedPixels() {
    std::ranges::fill(highlightedPixels, std::nullopt);
}

void Canvas::setHighlightedPixel(Pos pos, Color color) {
    if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) return;
    highlightedPixels[pos.y * width + pos.x] = Pixel{{color.r, color.g, color.b}, {pos.x, pos.y}};
}

void Canvas::setHighlightedPixels(const std::vector<Pos>& cluster, Color color) {
    for (const auto& pos : cluster) {
        setHighlightedPixel(pos, color);
    }
}

const std::vector<std::optional<Pixel>>& Canvas::getHighlightedPixels() const {
    return highlightedPixels;
}

[[nodiscard]] const std::vector<std::vector<std::vector<Pixel>>>& Canvas::getClusters() const {
    return clusters;
}

void Canvas::clearClusters() {
    clusters.clear();
}

[[nodiscard]] const std::vector<Pixel>& Canvas::getSelectedSegment() const {
    return selectedSegment;
}

void Canvas::clearSelectedSegment() {
    selectedSegment.clear();
}

void Canvas::setSelectedSegment(const std::vector<Pixel> &segment) {
    clearSelectedSegment();
    selectedSegment.assign(segment.begin(), segment.end());
}