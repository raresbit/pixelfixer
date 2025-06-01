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
#include "../include/PixelArtImage.h"
#include "imgui.h"
#include <glm/glm.hpp>


class BandingDetection final : public Algorithm {
public:
    explicit BandingDetection(PixelArtImage &canvas) : Algorithm(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Banding Detection";
    }

    int bandingDetection() {
        error = 0;
        debugPixels.clear();
        getPixelArtImage().clearDebugLines();

        bool origHorizontal = horizontal;
        getPixelArtImage().segmentClusters(horizontal);

        if (currentSelection == 2) {
            // Both
            horizontal = true;
            getPixelArtImage().segmentClusters(true);
            runDetection();

            horizontal = false;
            getPixelArtImage().segmentClusters(false);
            runDetection();
        } else {
            runDetection();
        }

        horizontal = origHorizontal;

        return error;
    }

    void run() override {
        for (int i = 0; i < getPixelArtImage().getWidth(); i++) {
            for (int j = 0; j < getPixelArtImage().getHeight(); j++) {
                getPixelArtImage().setPixel({i, j}, getPixelArtImage().getPixel({i, j}).color);
            }
        }
        bandingDetection();
    }


    void reset() override {
        Algorithm::reset();
        debugPixels.clear();
        showDebug = false;
        getPixelArtImage().clearDebugLines();
        getPixelArtImage().clearHighlightedPixels();
        error = 0;
        getPixelArtImage().segmentClusters(horizontal);
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
                    getPixelArtImage().clearHighlightedPixels();
                    if (currentSelection == 0) {
                        horizontal = true;
                        getPixelArtImage().segmentClusters(true);
                    } else if (currentSelection == 1) {
                        horizontal = false;
                        getPixelArtImage().segmentClusters(false);
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
        PixelArtImage &canvas = getPixelArtImage();
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
                                        canvas.drawRectangle(combined, red);

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
