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
        getCanvas().clearDebugLines();

        bool origHorizontal = horizontal;
        getCanvas().segmentClusters(horizontal);

        if (currentSelection == 2) {
            // Both
            horizontal = true;
            getCanvas().segmentClusters(true);
            runDetection();

            horizontal = false;
            getCanvas().segmentClusters(false);
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
        getCanvas().segmentClusters(horizontal);
    }

    void renderUI() override {
        // Dropdown options
        const char *options[] = {"Horizontal", "Vertical", "Both"};

        ImGui::Text("Segment Orientation");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Segment Orientation", options[currentSelection])) {
            for (int n = 0; n < IM_ARRAYSIZE(options); n++) {
                bool is_selected = (currentSelection == n);
                if (ImGui::Selectable(options[n], is_selected)) {
                    currentSelection = n;

                    // Clear and detect clusters accordingly
                    getCanvas().clearHighlightedPixels();
                    if (currentSelection == 0) {
                        horizontal = true;
                        getCanvas().segmentClusters(true);
                    } else if (currentSelection == 1) {
                        horizontal = false;
                        getCanvas().segmentClusters(false);
                    } else {
                        // Both: We'll handle this in run()
                    }
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Banding pair count: %d", error);
        ImGui::Checkbox("Debug View", &showDebug);
    }

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


    static void drawRectangle(Canvas &canvas, const std::vector<Pixel> &pixels, const Color &color) {
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
        canvas.addDebugLine(tl, tr, color);
        canvas.addDebugLine(tr, br, color);
        canvas.addDebugLine(br, bl, color);
        canvas.addDebugLine(bl, tl, color);
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
