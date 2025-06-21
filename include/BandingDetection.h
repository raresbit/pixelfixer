//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef BANDINGDETECTION_H
#define BANDINGDETECTION_H

#pragma once
#include "Algorithm.h"
#include <vector>
#include <iostream>
#include "../include/PixelArtImage.h"
#include "imgui.h"
#include <glm/glm.hpp>
#include <unordered_set>
#include <set>


class BandingDetection final : public Algorithm {
public:
    explicit BandingDetection(PixelArtImage &canvas) : Algorithm(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Banding Detection";
    }

    /**
     * Performs banding detection on a pixel art image by analyzing both horizontal
     * and vertical segments in the image to identify and group consecutive areas
     * prone to banding artifacts.
     *
     * The method processes the image to determine affected segment pairs,
     * groups consecutive banding segments into rectangular bounds, and aggregates
     * unique affected segments for further usage.
     *
     * @return A tuple containing:
     *         - An integer error value representing the banding error of the image.
     *         - A vector of unique pixel segments affected by banding (a flattened list of all banding pairs)
     *         - A vector of pairs where each pair represents a pair of banding pixel segments;
     *         the list is sorted to have all horizontal segments first, and then all vertical ones
     */
    std::tuple<int, std::vector<std::vector<Pixel>>, std::vector<std::pair<std::vector<Pixel>, std::vector<Pixel>>>> bandingDetection() {
        for (int i = 0; i < getPixelArtImage().getWidth(); i++) {
            for (int j = 0; j < getPixelArtImage().getHeight(); j++) {
                getPixelArtImage().setPixel({i, j}, getPixelArtImage().getPixel({i, j}).color);
            }
        }

        error = 0;
        debugPixels.clear();
        getPixelArtImage().clearDebugLines();

        // Both
        std::vector<std::pair<std::vector<Pixel>, std::vector<Pixel>>> horizontalAffectedSegmentPairs;
        getPixelArtImage().segmentClusters(true);
        auto newPairs = runDetection(true);
        horizontalAffectedSegmentPairs.insert(horizontalAffectedSegmentPairs.end(), newPairs.begin(), newPairs.end());

        std::vector<std::pair<std::vector<Pixel>, std::vector<Pixel>>> verticalAffectedSegmentPairs;
        getPixelArtImage().segmentClusters(false);
        newPairs = runDetection(false);
        verticalAffectedSegmentPairs.insert(verticalAffectedSegmentPairs.end(), newPairs.begin(), newPairs.end());


        // Draw rectangles around groups of consecutive banding segments
        drawGroupedRectangles(horizontalAffectedSegmentPairs, true);
        drawGroupedRectangles(verticalAffectedSegmentPairs, false);


        // Save unique segments
        struct SegmentHash {
            std::size_t operator()(const std::vector<Pixel>& segment) const {
                std::size_t h = 0;
                for (const auto& p : segment) {
                    h ^= std::hash<int>()(p.pos.x) ^ (std::hash<int>()(p.pos.y) << 1);
                }
                return h;
            }
        };

        struct SegmentEqual {
            bool operator()(const std::vector<Pixel>& a, const std::vector<Pixel>& b) const {
                if (a.size() != b.size()) return false;
                for (size_t i = 0; i < a.size(); ++i) {
                    if (a[i].pos != b[i].pos) return false;
                }
                return true;
            }
        };

        std::unordered_set<std::vector<Pixel>, SegmentHash, SegmentEqual> seen;
        std::vector<std::vector<Pixel>> flattened;

        std::vector<std::pair<std::vector<Pixel>, std::vector<Pixel>>> affectedSegmentPairs;
        affectedSegmentPairs.insert(affectedSegmentPairs.end(), horizontalAffectedSegmentPairs.begin(), horizontalAffectedSegmentPairs.end());
        affectedSegmentPairs.insert(affectedSegmentPairs.end(), verticalAffectedSegmentPairs.begin(), verticalAffectedSegmentPairs.end());

        for (const auto& pair : affectedSegmentPairs) {
            const auto& segA = pair.first;
            const auto& segB = pair.second;

            if (seen.insert(segA).second) {
                flattened.push_back(segA);
            }
            if (seen.insert(segB).second) {
                flattened.push_back(segB);
            }
        }

        return std::tuple{error, flattened, affectedSegmentPairs};
    }


    void run() override {
        bandingDetection();
    }


    void reset() override {
        Algorithm::reset();
        debugPixels.clear();
        getPixelArtImage().clearDebugLines();
        getPixelArtImage().clearHighlightedPixels();
        error = 0;
    }

    void renderUI() override {
        ImGui::Text("Banding pair count: %d", error);
    }

private:
    std::vector<Pixel> debugPixels;
    int error = 0;


    std::vector<std::pair<std::vector<Pixel>, std::vector<Pixel>>> runDetection(bool horizontalOrientation) {
        PixelArtImage &canvas = getPixelArtImage();
        const auto allClusters = canvas.getClusters();

        std::vector<std::pair<std::vector<Pixel>, std::vector<Pixel>>> affectedSegmentPairs;

        std::unordered_set<const std::vector<Pixel> *> alreadyChecked;
        std::set<std::pair<const void *, const void *> > countedPairs;

        for (const auto &clusterA: allClusters) {
            for (const auto &segmentA: clusterA) {
                if (alreadyChecked.contains(&segmentA)) continue;
                if (segmentA.size() <= 1) continue;

                std::unordered_set<Pos> segmentAPosSet;
                for (const auto &p: segmentA) segmentAPosSet.insert(p.pos);

                auto [startA, endA] = getSegmentEndpoints(segmentA, !horizontalOrientation);

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
                                    auto [startB, endB] = getSegmentEndpoints(segmentB, !horizontalOrientation);
                                    auto alignmentOpt = checkEndpointAlignment(startA, endA, startB, endB, !horizontalOrientation);

                                    if (alignmentOpt.has_value()) {
                                        error++;
                                        countedPairs.insert(pairKey);

                                        // Color red(255, 0, 0);
                                        // for (const auto &px: segmentA) {
                                        //     Pixel redPixel = px;
                                        //     redPixel.color = red;
                                        // }
                                        // for (const auto &px: segmentB) {
                                        //     Pixel redPixel = px;
                                        //     redPixel.color = red;
                                        // }

                                        std::vector<Pixel> combined = segmentA;
                                        combined.insert(combined.end(), segmentB.begin(), segmentB.end());
                                        // canvas.drawRectangle(combined, red);

                                        affectedSegmentPairs.emplace_back(segmentA, segmentB);

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
        return affectedSegmentPairs;
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

    void drawGroupedRectangles(const std::vector<std::pair<std::vector<Pixel>, std::vector<Pixel>>> &segmentPairs, bool horizontal) const {
        Color red(255, 0, 0);

        std::vector<std::vector<Pixel>> allSegments;
        for (auto &pair : segmentPairs) {
            allSegments.push_back(pair.first);
            allSegments.push_back(pair.second);
        }

        std::vector<bool> visited(allSegments.size(), false);
        std::vector<std::vector<std::vector<Pixel>>> groupedSegments;

        for (size_t i = 0; i < allSegments.size(); ++i) {
            if (visited[i]) continue;

            std::vector<std::vector<Pixel>> group;
            group.push_back(allSegments[i]);
            visited[i] = true;

            bool added = true;
            while (added) {
                added = false;
                for (size_t j = 0; j < allSegments.size(); ++j) {
                    if (visited[j]) continue;

                    for (const auto& seg : group) {
                        const auto& aStart = seg.front().pos;
                        const auto& aEnd = seg.back().pos;
                        const auto& bStart = allSegments[j].front().pos;
                        const auto& bEnd = allSegments[j].back().pos;

                        bool isConsecutive = false;

                        if (horizontal) {
                            // Consecutive segments must have y differ by 1 and matching x coordinates for both ends
                            bool yDiff = std::abs(aStart.y - bStart.y) == 1 && std::abs(aEnd.y - bEnd.y) == 1;
                            bool xMatch = (aStart.x == bStart.x) && (aEnd.x == bEnd.x);
                            isConsecutive = yDiff && xMatch;
                        } else {
                            // Consecutive segments must have x differ by 1 and matching y coordinates for both ends
                            bool xDiff = std::abs(aStart.x - bStart.x) == 1 && std::abs(aEnd.x - bEnd.x) == 1;
                            bool yMatch = (aStart.y == bStart.y) && (aEnd.y == bEnd.y);
                            isConsecutive = xDiff && yMatch;
                        }

                        if (isConsecutive) {
                            group.push_back(allSegments[j]);
                            visited[j] = true;
                            added = true;
                            break;
                        }
                    }
                }
            }

            groupedSegments.push_back(group);
        }

        for (const auto &group : groupedSegments) {
            std::vector<Pixel> combined;
            for (const auto &seg : group) {
                combined.insert(combined.end(), seg.begin(), seg.end());
            }
            getPixelArtImage().drawRectangle(combined, red);
        }
    }

};

#endif // BANDINGDETECTION_H
