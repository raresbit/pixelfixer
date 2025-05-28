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
#include "../include/Canvas.h"
#include <unordered_map>
#include <glm/glm.hpp>
#include <unordered_set>

class GeneralBandingCorrection final : public Algorithm {
public:
    explicit GeneralBandingCorrection(Canvas &canvas)
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
            getCanvas().detectClusters(horizontal);
            getCanvas().clearSelectedSegment();
            getCanvas().clearHighlightedPixels();
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

            if (operationIndex == 0 || operationIndex == 2) {
                ImGui::Text("Color Mode:");
                ImGui::SetNextItemWidth(-FLT_MIN);
                const char *colorModes[] = {
                    "Majority Neighbor Color",
                    "Endpoint Continuation Color"
                };
                ImGui::Combo("##ColorMode", &colorMode, colorModes, IM_ARRAYSIZE(colorModes));
            }

            if (operationIndex == 2) {
                colorMode = 0;
            }
        }
    }


    void run() override {
        Canvas &canvas = getCanvas();

        auto allClusters = canvas.getClusters();
        auto selectedSegment = canvas.getSelectedSegment();
        if (selectedSegment.empty()) return;


        // For fast lookup of positions: convert selected segment to a set of positions
        std::unordered_set<Pos> selectedSet;
        for (const auto &p: selectedSegment) {
            selectedSet.insert(p.pos);
        }

        // Find all segments that are not in the cluster of the selected segment,
        // and have at least one neighboring pixel with the selected segment
        std::vector<std::vector<Pixel> > neighboringSegments;
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

            // Take the segments that neighbor the selected segment
            for (const auto &segment: cluster) {
                for (const auto &p: segment) {
                    for (const Pos &n: {
                             Pos{p.pos.x + 1, p.pos.y},
                             Pos{p.pos.x - 1, p.pos.y},
                             Pos{p.pos.x, p.pos.y + 1},
                             Pos{p.pos.x, p.pos.y - 1}
                         }) {
                        if (selectedSet.contains(n)) {
                            neighboringSegments.push_back(segment);
                            break;
                        }
                    }
                }
            }
        }

        // Now iterate through the neighboringSegments and look for alignment
        std::unordered_map<Pos, Pixel> finalReplacements;

        // Get endpoints of the selected segment
        auto [selStart, selEnd] = getSegmentEndpoints(selectedSegment, !horizontal);
        bool appliedAlignment = false;

        for (const auto &neighboringSegment: neighboringSegments) {
            auto [nbStart, nbEnd] = getSegmentEndpoints(neighboringSegment, !horizontal);

            auto alignmentOpt = checkEndpointAlignment(selStart, selEnd, nbStart, nbEnd, !horizontal);
            if (alignmentOpt.has_value()) {
                if (automatic) {
                    alterLeftEdge = true;
                    alterRightEdge = true;
                    alterTopEdge = true;
                    alterBottomEdge = true;
                    operationIndex = 0;
                    colorMode = 1;
                }

                for (const auto &px: getReplacementsManual(selectedSegment, canvas)) {
                    finalReplacements[px.pos] = px;
                }

                appliedAlignment = true;
                break;
            }
        }

        for (const auto &[pos, px]: finalReplacements) {
            canvas.setPixel(pos, px.color);
        }

        // Refresh after first application
        canvas.detectClusters(horizontal);
        canvas.clearHighlightedPixels();

        if (!appliedAlignment) return;

        // Second round: re-check alignment after applying modifications
        if (selectedSegment.empty()) return;

        selStart = getSegmentEndpoints(selectedSegment, !horizontal).first;
        selEnd = getSegmentEndpoints(selectedSegment, !horizontal).second;

        selectedSet.clear();
        for (const auto &p: selectedSegment) selectedSet.insert(p.pos);
        for (const auto &neighboringSegment: neighboringSegments) {
            auto [nbStart, nbEnd] = getSegmentEndpoints(neighboringSegment, !horizontal);
            auto alignmentOpt = checkEndpointAlignment(selStart, selEnd, nbStart, nbEnd, !horizontal);
            if (!alignmentOpt.has_value()) continue;

            const std::string &alignment = alignmentOpt.value();

            std::vector<Pixel> edgeModifications;

            // Bump mod count by +1 for second-pass alignment effect
            edgeModifications = getReplacementsManual(selectedSegment, canvas, 1);

            // Apply final adjustments to canvas
            for (const auto &px: edgeModifications) {
                canvas.setPixel(px.pos, px.color);
            }

            break; // Only apply one second-phase alignment response
        }

        // Final cleanup
        canvas.detectClusters(horizontal);
        canvas.clearSelectedSegment();
        canvas.clearHighlightedPixels();
    }


    void reset() override {
        Algorithm::reset();
        getCanvas().clearDebugLines();
        getCanvas().detectClusters(horizontal);
    }

private:
    Canvas originalCanvas = Canvas(0, 0);
    bool horizontal = true;
    bool automatic = true;
    bool alterLeftEdge = true;
    bool alterRightEdge = true;
    bool alterTopEdge = true;
    bool alterBottomEdge = true;
    int operationIndex = 0;
    int colorMode = 0;

    // Returns matched neighbor cluster index or -1 if no match.
    // Returns optional pair {matchedNeighborIndex, alignment} or std::nullopt if no match
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

    struct Segment {
        cv::Point start;
        cv::Point end;
    };

    std::vector<Pixel> getReplacementsManual(std::vector<Pixel> &segment, const Canvas &canvas,
                                             int modCountOverride = -1) const {
        std::vector<Pixel> replacements;

        if (segment.empty()) return replacements;

        const size_t len = segment.size();

        std::ranges::sort(segment, [this](const Pixel &a, const Pixel &b) {
            return horizontal ? a.pos.x < b.pos.x : a.pos.y < b.pos.y;
        });

        // Remove Entirely
        if (operationIndex == 2) {
            for (const auto &p: segment) {
                Color newColor = determineReplacementColor(p.pos, canvas, p.color);
                replacements.emplace_back(Pixel{newColor, p.pos});
            }
            segment.clear();
            return replacements;
        }

        int modCount = (modCountOverride >= 0) ? modCountOverride : ((len >= 5) ? 2 : 1);
        Color originalColor = segment.front().color;

        auto tryExpand = [&](const Pos &from, int dx, int dy, bool toFront) {
            Pos candidate = {from.x + dx, from.y + dy};

            if (candidate.x < 0 || candidate.x >= canvas.getWidth() ||
                candidate.y < 0 || candidate.y >= canvas.getHeight()) {
                return;
            }

            auto newPixel = Pixel{originalColor, candidate};

            if (toFront)
                segment.insert(segment.begin(), newPixel);
            else
                segment.push_back(newPixel);

            replacements.emplace_back(newPixel);
        };

        if (operationIndex == 0 || operationIndex == 3) {
            // Shrink or Alter Color Modes
            if (horizontal) {
                if (alterLeftEdge && segment.size() >= modCount) {
                    const Pixel &px = segment.front();
                    Color newColor = determineReplacementColor(px.pos, canvas, px.color, EdgeDirection::Left);

                    for (int i = 0; i < modCount && !segment.empty(); ++i) {
                        replacements.emplace_back(Pixel{newColor, segment.front().pos});
                        segment.erase(segment.begin());
                    }
                }
                if (alterRightEdge && segment.size() >= modCount) {
                    const Pixel &px = segment.back();
                    Color newColor = determineReplacementColor(px.pos, canvas, px.color, EdgeDirection::Right);

                    for (int i = 0; i < modCount && !segment.empty(); ++i) {
                        replacements.emplace_back(Pixel{newColor, segment.back().pos});
                        segment.pop_back();
                    }
                }
            } else {
                if (alterTopEdge && segment.size() >= modCount) {
                    const Pixel &px = segment.front();
                    Color newColor = determineReplacementColor(px.pos, canvas, px.color, EdgeDirection::Top);

                    for (int i = 0; i < modCount && !segment.empty(); ++i) {
                        replacements.emplace_back(Pixel{newColor, segment.front().pos});
                        segment.erase(segment.begin());
                    }
                }
                if (alterBottomEdge && segment.size() >= modCount) {
                    const Pixel &px = segment.back();
                    Color newColor = determineReplacementColor(px.pos, canvas, px.color, EdgeDirection::Bottom);

                    for (int i = 0; i < modCount && !segment.empty(); ++i) {
                        replacements.emplace_back(Pixel{newColor, segment.back().pos});
                        segment.pop_back();
                    }
                }
            }
        } else if (operationIndex == 1) {
            // Expand
            if (horizontal) {
                if (alterLeftEdge) {
                    for (int i = 0; i < modCount; ++i) {
                        if (!segment.empty()) {
                            tryExpand(segment.front().pos, -1, 0, true);
                        }
                    }
                }
                if (alterRightEdge) {
                    for (int i = 0; i < modCount; ++i) {
                        if (!segment.empty()) {
                            tryExpand(segment.back().pos, 1, 0, false);
                        }
                    }
                }
            } else {
                if (alterTopEdge) {
                    for (int i = 0; i < modCount; ++i) {
                        if (!segment.empty()) {
                            tryExpand(segment.front().pos, 0, -1, true);
                        }
                    }
                }
                if (alterBottomEdge) {
                    for (int i = 0; i < modCount; ++i) {
                        if (!segment.empty()) {
                            tryExpand(segment.back().pos, 0, 1, false);
                        }
                    }
                }
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



    Color determineReplacementColor(Pos pos, const Canvas &canvas,
                                const Color &removedColor = Color{-1, -1, -1},
                                EdgeDirection edge = EdgeDirection::None) const {
    const int width = canvas.getWidth();
    const int height = canvas.getHeight();

    auto subjectMask = Canvas::extractSubject(canvas); // [y][x] == true means subject pixel

    auto isInsideSubject = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height &&
               subjectMask.at<uchar>(y, x) != 0;
    };

    std::vector<Color> neighborColors;
    static const std::vector<std::pair<int, int>> directions = {
        {0, -1}, {-1, 0}, {1, 0}, {0, 1}  // up, left, right, down
    };

    for (const auto &[dx, dy] : directions) {
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
            for (const auto &c : neighborColors) {
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
                case EdgeDirection::Left:   neighborPos = {pos.x - 1, pos.y}; break;
                case EdgeDirection::Right:  neighborPos = {pos.x + 1, pos.y}; break;
                case EdgeDirection::Top:    neighborPos = {pos.x, pos.y - 1}; break;
                case EdgeDirection::Bottom: neighborPos = {pos.x, pos.y + 1}; break;
                default:                    neighborPos = pos; break;
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

        for (const auto &c : neighborColors) {
            freq[c]++;
        }

        int maxCount = 0;
        std::vector<Color> mostFrequent;
        for (const auto &[color, count] : freq) {
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
            case EdgeDirection::Left:   neighborPos = {pos.x - 1, pos.y}; break;
            case EdgeDirection::Right:  neighborPos = {pos.x + 1, pos.y}; break;
            case EdgeDirection::Top:    neighborPos = {pos.x, pos.y - 1}; break;
            case EdgeDirection::Bottom: neighborPos = {pos.x, pos.y + 1}; break;
            default:                    return Color{255, 255, 255}; // No directional guidance
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
