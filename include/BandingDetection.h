//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef BANDINGDETECTION_H
#define BANDINGDETECTION_H

#pragma once
#include "Algorithm.h"
#include <vector>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "../include/Canvas.h"
#include "imgui.h"
#include <glm/glm.hpp>


class BandingDetection final : public Algorithm {
public:
    explicit BandingDetection(Canvas &canvas) : Algorithm(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Banding Detection";
    }

    int bandingDetection() {
        error = 0;
        debugPixels.clear();

        bool origHorizontal = horizontal;
        getCanvas().detectClusters(horizontal);

        if (currentSelection == 2) {
            // Both
            horizontal = true;
            getCanvas().detectClusters(true);
            runDetection();

            horizontal = false;
            getCanvas().detectClusters(false);
            runDetection();
        } else {
            runDetection();
        }

        horizontal = origHorizontal;

        return error;
    }

    void run() override {

        for (int i = 0; i < getCanvas().getWidth(); i++) {
            for (int j = 0; j < getCanvas().getHeight(); j++) {
                getCanvas().setPixel({i, j}, getCanvas().getPixel({i, j}).color);
            }
        }
        bandingDetection();
    }


    void reset() override {
        Algorithm::reset();
        debugPixels.clear();
        showDebug = false;
        getCanvas().clearDebugLines();
        getCanvas().clearHighlightedPixels();
        error = 0;
        getCanvas().detectClusters(horizontal);
    }

    void renderUI() override {
        // Dropdown options
        const char *options[] = {"Horizontal", "Vertical", "Both"};

        if (ImGui::BeginCombo("Segment Orientation", options[currentSelection])) {
            for (int n = 0; n < IM_ARRAYSIZE(options); n++) {
                bool is_selected = (currentSelection == n);
                if (ImGui::Selectable(options[n], is_selected)) {
                    currentSelection = n;

                    // Clear and detect clusters accordingly
                    getCanvas().clearHighlightedPixels();
                    if (currentSelection == 0) {
                        horizontal = true;
                        getCanvas().detectClusters(true);
                    } else if (currentSelection == 1) {
                        horizontal = false;
                        getCanvas().detectClusters(false);
                    } else {
                        // Both: We'll handle this in run()
                    }
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Error (# of pairs of \"banding\" segments): %d", error);
        ImGui::Checkbox("Debug View", &showDebug);
    }


    // NO LONGER IN USE: Previous banding detection method
    // void detectBandingLines(const Canvas &canvas, std::vector<std::tuple<int, int, int>> &bandingLines) const {
    //     const int width = canvas.getWidth();
    //     const int height = canvas.getHeight();
    //
    //     // Convert image to grayscale
    //     std::vector gray(height, std::vector<uint8_t>(width));
    //     for (int y = 0; y < height; ++y) {
    //         for (int x = 0; x < width; ++x) {
    //             const Color color = canvas.getPixel({x, y}).color;
    //             gray[y][x] = static_cast<uint8_t>((color.r + color.g + color.b) / 3);
    //         }
    //     }
    //
    //     // Detection: store matching lines
    //     for (int y = 0; y < height - 1; ++y) {
    //         for (int startX = 0; startX < width - 2; ++startX) {
    //             for (int len = 2; len < std::min(maxBandLength, width - startX); ++len) {
    //                 MatchType match = this->is_similar(gray[y], gray[y + 1], startX, len);
    //                 if (match != MatchType::None) {
    //                     int adjustedStartX = startX;
    //                     int adjustedLen = len;
    //
    //                     if (match == MatchType::ShiftRight) {
    //                         adjustedLen += 1;
    //                     } else if (match == MatchType::ShiftLeft && adjustedStartX > 0) {
    //                         adjustedStartX -= 1;
    //                         adjustedLen += 1;
    //                     }
    //
    //                     bandingLines.emplace_back(y, adjustedStartX, adjustedLen);
    //                     break;
    //                 }
    //             }
    //         }
    //     }
    // }

private:
    bool showDebug = false;
    bool horizontal = true;
    std::vector<Pixel> debugPixels;
    int error = 0;
    int currentSelection = 2;


    void runDetection() {
        Canvas &canvas = getCanvas();
        const auto allClusters = canvas.getClusters();

        std::unordered_set<const std::vector<Pixel> *> alreadyChecked;
        std::set<std::pair<const void *, const void *> > countedPairs;

        for (const auto &clusterA: allClusters) {
            for (const auto &segmentA: clusterA) {
                if (alreadyChecked.contains(&segmentA)) continue;
                if (segmentA.size() <= 1) continue;

                std::unordered_set<Pos> segmentAPosSet;
                for (const auto &p: segmentA) segmentAPosSet.insert(p.pos);

                auto [startA, endA] = getSegmentEndpoints(segmentA, !horizontal);

                bool foundMatchForSegmentA = false;

                for (const auto &clusterB: allClusters) {
                    if (&clusterB == &clusterA) continue;

                    for (const auto &segmentB: clusterB) {
                        auto ptrA = static_cast<const void *>(&segmentA);
                        auto ptrB = static_cast<const void *>(&segmentB);
                        std::pair<const void *, const void *> pairKey = (ptrA < ptrB)
                                                                            ? std::make_pair(ptrA, ptrB)
                                                                            : std::make_pair(ptrB, ptrA);

                        if (countedPairs.contains(pairKey)) continue;

                        for (const auto &p: segmentB) {
                            for (const Pos &n: {
                                     Pos{p.pos.x + 1, p.pos.y},
                                     Pos{p.pos.x - 1, p.pos.y},
                                     Pos{p.pos.x, p.pos.y + 1},
                                     Pos{p.pos.x, p.pos.y - 1}
                                 }) {
                                if (segmentAPosSet.contains(n)) {
                                    auto [startB, endB] = getSegmentEndpoints(segmentB, !horizontal);
                                    auto alignmentOpt = checkEndpointAlignment(startA, endA, startB, endB, !horizontal);

                                    if (alignmentOpt.has_value()) {
                                        error++;
                                        countedPairs.insert(pairKey);

                                        Color red(255, 0, 0);
                                        for (const auto &px: segmentA) {
                                            Pixel redPixel = px;
                                            redPixel.color = red;
                                            debugPixels.push_back(redPixel);
                                        }
                                        for (const auto &px: segmentB) {
                                            Pixel redPixel = px;
                                            redPixel.color = red;
                                            debugPixels.push_back(redPixel);
                                        }

                                        std::vector<Pixel> combined = segmentA;
                                        combined.insert(combined.end(), segmentB.begin(), segmentB.end());
                                        drawRectangle(canvas, combined, red);

                                        foundMatchForSegmentA = true;
                                        break;
                                    }
                                }
                            }
                            if (foundMatchForSegmentA) break;
                        }
                        if (foundMatchForSegmentA) break;
                    }
                    if (foundMatchForSegmentA) break;
                }

                alreadyChecked.insert(&segmentA);
            }
        }
    }


    void drawRectangle(Canvas &canvas, const std::vector<Pixel> &pixels, const Color &color) {
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
        float x0 = static_cast<float>(minX);
        float x1 = static_cast<float>(maxX);
        float y0 = static_cast<float>(minY);
        float y1 = static_cast<float>(maxY);

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
        canvas.addDebugLine(tl, tr, color);
        canvas.addDebugLine(tr, br, color);
        canvas.addDebugLine(br, bl, color);
        canvas.addDebugLine(bl, tl, color);
    }


    enum class MatchType {
        None,
        Exact,
        ShiftLeft,
        ShiftRight
    };


    // [[nodiscard]] MatchType is_similar(const std::vector<uint8_t> &row1,
    //                                const std::vector<uint8_t> &row2,
    //                                int start, int length) const {
    //
    //     // Ensure at least 3 different colors exist in the region
    //     // std::unordered_set<uint8_t> unique_colors;
    //     // for (int i = 0; i < length; ++i) {
    //     //     unique_colors.insert(row1[start + i]);
    //     //     unique_colors.insert(row2[start + i]);
    //     //     if (unique_colors.size() >= 3)
    //     //         break;
    //     // }
    //     // if (unique_colors.size() < 3)
    //     //     return MatchType::None;
    //
    //     // Check internal smoothness of row1
    //     for (int i = start + 1; i < start + length; ++i) {
    //         if (std::abs(row1[i] - row1[i - 1]) > threshold)
    //             return MatchType::None;
    //     }
    //
    //     // Check internal smoothness of row2
    //     for (int i = start + 1; i < start + length; ++i) {
    //         if (std::abs(row2[i] - row2[i - 1]) > threshold)
    //             return MatchType::None;
    //     }
    //
    //     // Skip perfectly flat regions
    //     bool identical = true;
    //     for (int i = 0; i < length; ++i) {
    //         if (row1[start + i] != row2[start + i]) {
    //             identical = false;
    //             break;
    //         }
    //     }
    //     if (identical) return MatchType::None;
    //
    //     if ((row1[start] == row2[start]) || (row1[start + length - 1] == row2[start + length - 1]))
    //         return MatchType::None;
    //
    //     // Shift right match
    //     bool right_match = true;
    //     for (int i = 1; i < length; ++i) {
    //         if (row1[start + i] != row2[start + i - 1]) {
    //             right_match = false;
    //             break;
    //         }
    //     }
    //     if (right_match) return MatchType::ShiftRight;
    //
    //     // Shift left match
    //     bool left_match = true;
    //     for (int i = 0; i < length - 1; ++i) {
    //         if (row1[start + i] != row2[start + i + 1]) {
    //             left_match = false;
    //             break;
    //         }
    //     }
    //     if (left_match) return MatchType::ShiftLeft;
    //
    //     // // Check if the rows are similar
    //     // bool no_shift_similar = true;
    //     // for (int i = 0; i < length - 1; ++i) {
    //     //     if (std::abs(row1[start + i] - row2[start + i]) > threshold) {
    //     //         no_shift_similar = false;
    //     //         break;
    //     //     }
    //     // }
    //     // if (no_shift_similar) return MatchType::Exact;
    //
    //     return MatchType::None;
    // }

    static cv::Mat extractSubjectMask(const Canvas &canvas) {
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


    static std::pair<Pos, Pos> getSegmentEndpoints(const std::vector<Pixel> &segment, bool isVertical) {
        auto sortedSegment = segment;
        std::ranges::sort(sortedSegment, [isVertical](const Pixel &a, const Pixel &b) {
            return isVertical ? a.pos.y < b.pos.y : a.pos.x < b.pos.x;
        });
        return {sortedSegment.front().pos, sortedSegment.back().pos};
    }


    // Check if two segments are adjacent and aligned at endpoints
    static std::optional<std::string> checkEndpointAlignment(const Pos &segStart, const Pos &segEnd,
                                                             const Pos &neighborStart, const Pos &neighborEnd,
                                                             bool isVertical) {
        bool alignStart = false;
        bool alignEnd = false;
        bool sticked = false;

        if (isVertical) {
            // Check if vertically adjacent (x differs by 1)
            sticked = (std::abs(segStart.x - neighborStart.x) == 1);

            if (!sticked) return std::nullopt;

            // Check endpoint alignment on row (y)
            alignStart = (segStart.y == neighborStart.y);
            alignEnd = (segEnd.y == neighborEnd.y);

            if (alignStart && alignEnd) return std::string("both");

            // TODO: COMMENT OUT THE BELOW 2 LINES
            // if (alignStart) return std::string("top");
            // if (alignEnd) return std::string("bottom");
        } else {
            // Horizontal case: check if horizontally adjacent (y differs by 1)
            sticked = (std::abs(segStart.y - neighborStart.y) == 1);

            if (!sticked) return std::nullopt;

            // Check endpoint alignment on column (x)
            alignStart = (segStart.x == neighborStart.x);
            alignEnd = (segEnd.x == neighborEnd.x);

            if (alignStart && alignEnd) return std::string("both");

            // TODO: COMMENT OUT THE BELOW 2 LINES
            // if (alignStart) return std::string("left");
            // if (alignEnd) return std::string("right");
        }

        // If no endpoint aligned, no match
        return std::nullopt;
    }
};

#endif // BANDINGDETECTION_H


//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef BANDINGDETECTION_H
#define BANDINGDETECTION_H

#pragma once
#include "Algorithm.h"
#include <vector>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "../include/Canvas.h"
#include "imgui.h"
#include <glm/glm.hpp>


class BandingDetection final : public Algorithm {
public:
    explicit BandingDetection(Canvas &canvas) : Algorithm(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Banding Detection";
    }

    void run() override {
        Canvas& canvas = getCanvas();
        std::vector<std::tuple<int, int, int>> bandingLines;
        !!! detect here and draw on canvas the red pixels

    void reset() override {
        Algorithm::reset();
        showDebug = false;
        getCanvas().clearDebugLines();
    }

    void renderUI() override {
        ImGui::Text("Color Similarity Threshold");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##Thresh", &threshold, 0, 254);

        ImGui::Text("Maximum Banding Line Length");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##LineLength", &maxBandLength, 2, 20);
    }

    void setThreshold(int threshold) {
        this->threshold = threshold;
    }

private:
    bool showDebug = false;
    int threshold = 1;
    int maxBandLength = 5;

    enum class MatchType {
        None,
        Exact,
        ShiftLeft,
        ShiftRight
    };


    [[nodiscard]] MatchType is_similar(const std::vector<uint8_t> &row1,
                                   const std::vector<uint8_t> &row2,
                                   int start, int length) const {

        // Ensure at least 3 different colors exist in the region
        // std::unordered_set<uint8_t> unique_colors;
        // for (int i = 0; i < length; ++i) {
        //     unique_colors.insert(row1[start + i]);
        //     unique_colors.insert(row2[start + i]);
        //     if (unique_colors.size() >= 3)
        //         break;
        // }
        // if (unique_colors.size() < 3)
        //     return MatchType::None;

        // Check internal smoothness of row1
        for (int i = start + 1; i < start + length; ++i) {
            if (std::abs(row1[i] - row1[i - 1]) > threshold)
                return MatchType::None;
        }

        // Check internal smoothness of row2
        for (int i = start + 1; i < start + length; ++i) {
            if (std::abs(row2[i] - row2[i - 1]) > threshold)
                return MatchType::None;
        }

        // Skip perfectly flat regions
        bool identical = true;
        for (int i = 0; i < length; ++i) {
            if (row1[start + i] != row2[start + i]) {
                identical = false;
                break;
            }
        }
        if (identical) return MatchType::None;

        if ((row1[start] == row2[start]) || (row1[start + length - 1] == row2[start + length - 1]))
            return MatchType::None;

        // Shift right match
        bool right_match = true;
        for (int i = 1; i < length; ++i) {
            if (row1[start + i] != row2[start + i - 1]) {
                right_match = false;
                break;
            }
        }
        if (right_match) return MatchType::ShiftRight;

        // Shift left match
        bool left_match = true;
        for (int i = 0; i < length - 1; ++i) {
            if (row1[start + i] != row2[start + i + 1]) {
                left_match = false;
                break;
            }
        }
        if (left_match) return MatchType::ShiftLeft;

        // // Check if the rows are similar
        // bool no_shift_similar = true;
        // for (int i = 0; i < length - 1; ++i) {
        //     if (std::abs(row1[start + i] - row2[start + i]) > threshold) {
        //         no_shift_similar = false;
        //         break;
        //     }
        // }
        // if (no_shift_similar) return MatchType::Exact;

        return MatchType::None;
    }

    static cv::Mat extractSubjectMask(const Canvas &canvas) {
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
};

#endif // BANDINGDETECTION_H
