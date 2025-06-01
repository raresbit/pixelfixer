//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef GENERALBANDINGCORRECTION_H
#define GENERALBANDINGCORRECTION_H

#pragma once
#include "Algorithm.h"
#include <vector>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "../include/PixelArtImage.h"
#include <glm/glm.hpp>
#include <unordered_set>

class GeneralBandingCorrection final : public Algorithm {
public:
    explicit GeneralBandingCorrection(PixelArtImage &canvas)
        : Algorithm(canvas) {
    }


    [[nodiscard]] std::string name() const override {
        return "General Banding Correction";
    }

    void renderUI() override {
        if (horizontal)
            ImGui::Text("Horizontal Segments");
        else
            ImGui::Text("Vertical Segments");

        ImGui::SameLine();
        if (ImGui::Button("Switch")) {
            horizontal = !horizontal;
            getPixelArtImage().segmentClusters(horizontal);
            getPixelArtImage().clearSelectedSegment();
            getPixelArtImage().clearHighlightedPixels();
        }

        ImGui::Checkbox("Automatic", &automatic);

        if (!automatic) {
            // Operation dropdown
            const char *operations[] = {"Shrink Segment", "Expand Segment", "Remove Entirely", "Alter Colors"};
            ImGui::Text("What To Do?");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::Combo("##Operation", &operationIndex, operations, IM_ARRAYSIZE(operations));

            if (operationIndex == 0 || operationIndex == 1 || operationIndex == 3) {
                ImGui::Text("Endpoints To Alter:");
                if (horizontal) {
                    ImGui::Checkbox("Left", &alterLeftEdge);
                    ImGui::SameLine();
                    ImGui::Checkbox("Right", &alterRightEdge);
                } else {
                    ImGui::Checkbox("Top", &alterTopEdge);
                    ImGui::SameLine();
                    ImGui::Checkbox("Bottom", &alterBottomEdge);
                }
            }
        }
    }

    void run() override {
        if (automatic) {
            alterLeftEdge = true;
            alterRightEdge = true;
            alterTopEdge = true;
            alterBottomEdge = true;
        }

        if (operationIndex == 2) {
            colorMode = 0; // Mode 0: take the majority neighbor color
        } else {
            colorMode = 1; // Mode 1: take the endpoint continuation color
        }

        PixelArtImage &image = getPixelArtImage();

        auto allClusters = image.getClusters();
        auto selectedSegment = image.getSelectedSegment();
        if (selectedSegment.empty()) return;

        // Extract adjacent segments to the selected segments
        // for horizontal segments, consider only the top and bottom segments
        // for vertical segments, consider only the left and right segments
        // (banding can only happen between the selected segment and these neighboring ones)
        std::vector<std::vector<Pixel> > neighboringSegments = extractNeighboringSegments(allClusters, selectedSegment);

        // Banding detection and correction
        if (detectBanding(selectedSegment, neighboringSegments)) {
            image.setPixels(getReplacements(selectedSegment, neighboringSegments, image));
        }

        // Final cleanup
        image.segmentClusters(horizontal);
        image.clearSelectedSegment();
        image.clearHighlightedPixels();
    }


    void reset() override {
        Algorithm::reset();
        getPixelArtImage().clearDebugLines();
        getPixelArtImage().segmentClusters(horizontal);
    }

private:
    PixelArtImage originalCanvas = PixelArtImage(0, 0);
    bool horizontal = true;
    bool automatic = true;
    bool alterLeftEdge = true;
    bool alterRightEdge = true;
    bool alterTopEdge = true;
    bool alterBottomEdge = true;
    int operationIndex = 0;
    int colorMode = 0;


    [[nodiscard]] bool detectBanding(const std::vector<Pixel> &selectedSegment,
                                     const std::vector<std::vector<Pixel> > &neighboringSegments) const {
        bool bandingDetected = false;
        auto [selStart, selEnd] = getSegmentEndpoints(selectedSegment);

        for (const auto &neighboringSegment: neighboringSegments) {
            auto [nbStart, nbEnd] = getSegmentEndpoints(neighboringSegment);

            auto alignmentOpt = checkEndpointAlignment(selStart, selEnd, nbStart, nbEnd);
            if (alignmentOpt.has_value()) {
                bandingDetected = true;
                break;
            }
        }

        return bandingDetected;
    }

    [[nodiscard]] std::vector<std::vector<Pixel> > extractNeighboringSegments(
        const std::vector<std::vector<std::vector<Pixel> > > &allClusters,
        const std::vector<Pixel> &selectedSegment) const {
        std::vector<std::vector<Pixel> > neighboringSegments;

        // For fast lookup of positions: convert selected segment to a set of positions
        std::unordered_set<Pos> selectedSet;
        for (const auto &p: selectedSegment) {
            selectedSet.insert(p.pos);
        }

        for (const auto &cluster: allClusters) {
            bool containsSelectedSegment = false;

            // Check if this cluster contains the selectedSegment and skip if so
            for (const auto &segment: cluster) {
                if (&segment == &selectedSegment) {
                    // Compare addresses for identity
                    containsSelectedSegment = true;
                    break;
                }
            }

            if (containsSelectedSegment) continue;

            // Take the segments that neighbor the selected segment: for horizontal segments, consider only the top and bottom segments
            // for vertical segments, consider only the left and right segments
            for (const auto &segment: cluster) {
                for (const auto &p: segment) {
                    std::vector<Pos> neighbors;

                    if (horizontal) {
                        // Check only top and bottom
                        neighbors = {
                            Pos{p.pos.x, p.pos.y - 1}, // Top
                            Pos{p.pos.x, p.pos.y + 1} // Bottom
                        };
                    } else {
                        // Check only left and right
                        neighbors = {
                            Pos{p.pos.x - 1, p.pos.y}, // Left
                            Pos{p.pos.x + 1, p.pos.y} // Right
                        };
                    }

                    for (const Pos &n: neighbors) {
                        if (selectedSet.contains(n)) {
                            neighboringSegments.push_back(segment);
                            goto next_segment; // skip the rest of the current segment
                        }
                    }
                }
            next_segment:;
            }
        }

        return neighboringSegments;
    }

    // Returns matched neighbor cluster index or -1 if no match.
    // Returns optional pair {matchedNeighborIndex, alignment} or std::nullopt if no match
    [[nodiscard]] std::pair<Pos, Pos> getSegmentEndpoints(const std::vector<Pixel> &segment) const {
        auto sortedSegment = segment;
        std::ranges::sort(sortedSegment, [this](const Pixel &a, const Pixel &b) {
            return !horizontal ? a.pos.y < b.pos.y : a.pos.x < b.pos.x;
        });
        return {sortedSegment.front().pos, sortedSegment.back().pos};
    }

    // Check if two segments are adjacent and aligned at endpoints
    [[nodiscard]] std::optional<std::string> checkEndpointAlignment(const Pos &segStart, const Pos &segEnd,
                                                                    const Pos &neighborStart,
                                                                    const Pos &neighborEnd) const {
        bool alignStart = false;
        bool alignEnd = false;
        bool sticked = false;

        if (!horizontal) {
            // Check if vertically adjacent (x differs by 1)
            sticked = (std::abs(segStart.x - neighborStart.x) == 1);

            if (!sticked) return std::nullopt;

            // Check endpoint alignment on row (y)
            alignStart = (segStart.y == neighborStart.y);
            alignEnd = (segEnd.y == neighborEnd.y);

            if (alignStart && alignEnd) return std::string("both");

        } else {
            // Horizontal case: check if horizontally adjacent (y differs by 1)
            sticked = (std::abs(segStart.y - neighborStart.y) == 1);

            if (!sticked) return std::nullopt;

            // Check endpoint alignment on column (x)
            alignStart = (segStart.x == neighborStart.x);
            alignEnd = (segEnd.x == neighborEnd.x);

            if (alignStart && alignEnd) return std::string("both");
        }

        // If no endpoint aligned, no match
        return std::nullopt;
    }

    std::vector<Pixel> getReplacements(std::vector<Pixel> &segment,
                                       const std::vector<std::vector<Pixel> > &neighboringSegments,
                                       const PixelArtImage &canvas) const {
        std::vector<Pixel> replacements;

        if (segment.empty()) return replacements;

        std::ranges::sort(segment, [this](const Pixel &a, const Pixel &b) {
            return horizontal ? a.pos.x < b.pos.x : a.pos.y < b.pos.y;
        });

        switch (operationIndex) {
            case 0:
            case 3:
                return handleShrinkOrColorChange(segment, neighboringSegments, canvas);
            case 1:
                return handleExpansion(segment, neighboringSegments, canvas);
            case 2:
                return handleRemoval(segment, canvas);
            default:
                return replacements;
        }
    }

    std::vector<Pixel> handleRemoval(std::vector<Pixel> &segment, const PixelArtImage &canvas) const {
        std::vector<Pixel> replacements;

        for (const auto &p: segment) {
            Color newColor = determineReplacementColor(p.pos, canvas, p.color);
            replacements.emplace_back(Pixel{newColor, p.pos});
        }

        segment.clear();
        return replacements;
    }

    std::vector<Pixel> handleShrinkOrColorChange(std::vector<Pixel> &segment, const std::vector<std::vector<Pixel> > &neighboringSegments, const PixelArtImage &canvas) const {
        std::vector<Pixel> replacements;

        struct EdgeOp {
            bool enabled;
            bool front;
            EdgeDirection dir;
            Pos pos;
            Color color;
            bool valid = false;
        };

        std::vector<EdgeOp> edges;

        auto prepareEdge = [&](bool enabled, bool front, EdgeDirection dir) -> EdgeOp {
            if (!enabled || segment.empty()) return {};

            const Pixel &px = front ? segment.front() : segment.back();
            return {
                .enabled = true,
                .front = front,
                .dir = dir,
                .pos = px.pos,
                .color = determineReplacementColor(px.pos, canvas, px.color, dir),
                .valid = true
            };
        };

        if (horizontal) {
            edges.push_back(prepareEdge(alterLeftEdge, true, EdgeDirection::Left));
            edges.push_back(prepareEdge(alterRightEdge, false, EdgeDirection::Right));
        } else {
            edges.push_back(prepareEdge(alterTopEdge, true, EdgeDirection::Top));
            edges.push_back(prepareEdge(alterBottomEdge, false, EdgeDirection::Bottom));
        }

        auto applyEdge = [&](const EdgeOp &e) {
            if (!e.valid || segment.empty()) return;

            const Pos pos = e.front ? segment.front().pos : segment.back().pos;
            replacements.emplace_back(Pixel{e.color, pos});

            if (e.front)
                segment.erase(segment.begin());
            else
                segment.pop_back();
        };

        for (const EdgeOp &e : edges) {
            applyEdge(e);
        }

        if (!segment.empty() && detectBanding(segment, neighboringSegments)) {
            for (const EdgeOp &e : edges) {
                applyEdge(e);
            }
        }

        return replacements;
    }

    std::vector<Pixel> handleExpansion(std::vector<Pixel> &segment, const std::vector<std::vector<Pixel> > &neighboringSegments, const PixelArtImage &canvas) const {
        std::vector<Pixel> replacements;
        const Color originalColor = segment.front().color;

        struct ExpandOp {
            bool enabled;
            int dx;
            int dy;
            bool front;
        };

        auto tryExpand = [&](const Pos &from, int dx, int dy, bool toFront) {
            Pos candidate = {from.x + dx, from.y + dy};

            if (candidate.x < 0 || candidate.x >= canvas.getWidth() ||
                candidate.y < 0 || candidate.y >= canvas.getHeight()) {
                return;
                }

            Pixel newPixel = {originalColor, candidate};

            if (toFront)
                segment.insert(segment.begin(), newPixel);
            else
                segment.push_back(newPixel);

            replacements.emplace_back(newPixel);
        };

        std::vector<ExpandOp> ops;

        if (horizontal) {
            if (alterLeftEdge) ops.push_back({true, -1, 0, true});
            if (alterRightEdge) ops.push_back({true, 1, 0, false});
        } else {
            if (alterTopEdge) ops.push_back({true, 0, -1, true});
            if (alterBottomEdge) ops.push_back({true, 0, 1, false});
        }

        auto applyOp = [&](const ExpandOp &op) {
            if (!segment.empty()) {
                const Pos &ref = op.front ? segment.front().pos : segment.back().pos;
                tryExpand(ref, op.dx, op.dy, op.front);
            }
        };

        for (const auto &op : ops) {
            applyOp(op);
        }

        if (!segment.empty() && detectBanding(segment, neighboringSegments)) {
            for (const auto &op : ops) {
                applyOp(op); // expand one more time
            }
        }

        return replacements;
    }

    enum class EdgeDirection {
        None,
        Top,
        Bottom,
        Left,
        Right
    };


    [[nodiscard]] Color determineReplacementColor(Pos pos, const PixelArtImage &canvas,
                                                  const Color &removedColor = Color{-1, -1, -1},
                                                  EdgeDirection edge = EdgeDirection::None) const {
        const int width = canvas.getWidth();
        const int height = canvas.getHeight();

        auto subjectMask = PixelArtImage::extractSubject(canvas); // [y][x] == true means subject pixel

        auto isInsideSubject = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < width && y < height &&
                   subjectMask.at<uchar>(y, x) != 0;
        };

        std::vector<Color> neighborColors;
        static const std::vector<std::pair<int, int> > directions = {
            {0, -1}, {-1, 0}, {1, 0}, {0, 1} // up, left, right, down
        };

        for (const auto &[dx, dy]: directions) {
            int nx = pos.x + dx;
            int ny = pos.y + dy;

            if (!isInsideSubject(nx, ny)) continue;

            Color c = canvas.getPixel({nx, ny}).color;
            if (c != removedColor) {
                neighborColors.push_back(c);
            }
        }

        if (neighborColors.empty()) {
            return Color{255, 255, 255}; // fallback
        }

        // === OperationIndex == 3 → use averaging ===
        if (operationIndex == 3) {
            if (colorMode == 0) {
                // Average all neighbor colors
                int r = 0, g = 0, b = 0;
                for (const auto &c: neighborColors) {
                    r += c.r;
                    g += c.g;
                    b += c.b;
                }
                int n = static_cast<int>(neighborColors.size());
                return Color{r / n, g / n, b / n};
            } else if (colorMode == 1) {
                // Use continuation logic and average with removedColor
                Pos neighborPos;
                switch (edge) {
                    case EdgeDirection::Left: neighborPos = {pos.x - 1, pos.y};
                        break;
                    case EdgeDirection::Right: neighborPos = {pos.x + 1, pos.y};
                        break;
                    case EdgeDirection::Top: neighborPos = {pos.x, pos.y - 1};
                        break;
                    case EdgeDirection::Bottom: neighborPos = {pos.x, pos.y + 1};
                        break;
                    default: neighborPos = pos;
                        break;
                }

                Color continuation = removedColor;
                if (neighborPos.x >= 0 && neighborPos.y >= 0 &&
                    neighborPos.x < width && neighborPos.y < height) {
                    Color c = canvas.getPixel(neighborPos).color;
                    if (c != removedColor) {
                        continuation = c;
                    }
                }

                return Color{
                    (removedColor.r + continuation.r) / 2,
                    (removedColor.g + continuation.g) / 2,
                    (removedColor.b + continuation.b) / 2
                };
            }
        }

        // === OperationIndex != 3 ===
        if (colorMode == 0) {
            // Majority logic
            std::map<Color, int, bool(*)(const Color &, const Color &)> freq([](const Color &a, const Color &b) {
                return std::tie(a.r, a.g, a.b) < std::tie(b.r, b.g, b.b);
            });

            for (const auto &c: neighborColors) {
                freq[c]++;
            }

            int maxCount = 0;
            std::vector<Color> mostFrequent;
            for (const auto &[color, count]: freq) {
                if (count > maxCount) {
                    maxCount = count;
                    mostFrequent = {color};
                } else if (count == maxCount) {
                    mostFrequent.push_back(color);
                }
            }

            static std::default_random_engine generator{42}; // reproducible
            std::uniform_int_distribution<> distrib(0, static_cast<int>(mostFrequent.size()) - 1);
            return mostFrequent[distrib(generator)];
        } else if (colorMode == 1) {
            // Continuation logic using edge direction only
            Pos neighborPos;
            switch (edge) {
                case EdgeDirection::Left: neighborPos = {pos.x - 1, pos.y};
                    break;
                case EdgeDirection::Right: neighborPos = {pos.x + 1, pos.y};
                    break;
                case EdgeDirection::Top: neighborPos = {pos.x, pos.y - 1};
                    break;
                case EdgeDirection::Bottom: neighborPos = {pos.x, pos.y + 1};
                    break;
                default: return Color{255, 255, 255}; // No directional guidance
            }

            if (neighborPos.x >= 0 && neighborPos.y >= 0 &&
                neighborPos.x < width && neighborPos.y < height) {
                Color cont = canvas.getPixel(neighborPos).color;
                if (cont != removedColor) {
                    return cont;
                }
            }
        }

        return Color{255, 255, 255}; // ultimate fallback
    }
};

#endif // GENERALBANDINGCORRECTION_H
