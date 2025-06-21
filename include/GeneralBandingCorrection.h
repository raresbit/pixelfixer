//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef GENERALBANDINGCORRECTION_H
#define GENERALBANDINGCORRECTION_H

#pragma once
#include "Algorithm.h"
#include <vector>
#include <iostream>
#include "../include/PixelArtImage.h"
#include <glm/glm.hpp>
#include <unordered_set>

class GeneralBandingCorrection final : public Algorithm {
public:
    explicit GeneralBandingCorrection(PixelArtImage &canvas)
        : Algorithm(canvas) {
    }


    [[nodiscard]] std::string name() const override {
        return "Banding Correction";
    }

    void renderUI() override {
        // Operation dropdown
        const char *operations[] = {"Shrink (Copy Neigh. Color)", "Shrink (Average With Neigh. Color)", "Expand Segment"};
        ImGui::Text("Correction Strategy:");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::Combo("##Operation", &operationIndex, operations, IM_ARRAYSIZE(operations));

        ImGui::Text("Segment Endpoints To Alter:");
        ImGui::Checkbox("Left/Top", &alterLeftOrTopEdge);
        ImGui::SameLine();
        ImGui::Checkbox("Right/Bottom", &alterRightOrBottomEdge);


        ImGui::Separator();

        ImGui::Text("Banding Error: %d", getPixelArtImage().getError());
    }

    void run() override {
        // Uncomment to fix the seed (removes variety: always gives the same result).
        generator = std::default_random_engine{42};

        PixelArtImage &image = getPixelArtImage();

        auto allClusters = image.getClusters();
        auto selectedSegment = image.getSelectedSegment();

        if (selectedSegment.empty()) {
            bool cLeftOrTop = alterLeftOrTopEdge;
            bool cRightOrBottom = alterRightOrBottomEdge;
            // Run the algorithm on all segments until banding error converges
            auto detection = std::make_unique<BandingDetection>(image);

            auto bandingPairs = std::get<2>(detection->bandingDetection());

            while (!bandingPairs.empty()) {
                // Select the first affected segment
                for (const auto& pair: bandingPairs) {
                    const auto &seg1 = pair.first;
                    const auto &seg2 = pair.second;

                    // Pick either rightmost segment (if vertical) or bottom segment (if horizontal)
                    // This propagates the error all the way to the bottom right corner,
                    // Ensuring that the loop ends eventually
                    std::vector<Pixel> affectedSegment;
                    bool horizontal = isSegmentHorizontal(seg1);

                    if (horizontal) {
                        if (seg1.front().pos.x > seg2.front().pos.x) {
                            affectedSegment = seg1;
                        } else {
                            affectedSegment = seg2;
                        }
                    } else {
                        if (seg1.front().pos.y > seg2.front().pos.y) {
                            affectedSegment = seg1;
                        } else {
                            affectedSegment = seg2;
                        }
                    }

                    image.segmentClusters(horizontal);
                    allClusters = image.getClusters();

                    // Find the segment in allClusters that matches 'first' by comparing contents
                    selectedSegment.clear();
                    for (const auto &cluster: allClusters) {
                        for (const auto &seg: cluster) {
                            if (seg.size() == affectedSegment.size()) {
                                bool equal = true;
                                for (size_t i = 0; i < seg.size(); ++i) {
                                    if (seg[i].pos != affectedSegment[i].pos || seg[i].color != affectedSegment[i].color) {
                                        equal = false;
                                        break;
                                    }
                                }
                                if (equal) {
                                    selectedSegment = seg;
                                    break;
                                }
                            }
                        }
                    }

                    image.setSelectedSegment(selectedSegment);

                    horizontal = isSegmentHorizontal(selectedSegment);
                    allClusters = image.segmentClusters(horizontal);

                    // Extract neighboring segments based on current selection
                    std::vector<std::vector<Pixel> > neighboringSegments = extractNeighboringSegments(selectedSegment, horizontal, allClusters);

                    // Apply banding correction
                    image.setPixels(getReplacements(selectedSegment, neighboringSegments, image));

                    // Reset parameters
                    alterLeftOrTopEdge = cLeftOrTop;
                    alterRightOrBottomEdge = cRightOrBottom;
                }
                detection = std::make_unique<BandingDetection>(image);
                auto [err, aff, _] = detection->bandingDetection();
                image.setError(err);
                bandingPairs = std::get<2>(detection->bandingDetection());
            }
        } else {

            // Extract adjacent segments to the selected segments
            // for horizontal segments, consider only the top and bottom segments
            // for vertical segments, consider only the left and right segments
            // (banding can only happen between the selected segment and these neighboring ones)
            bool horizontal = isSegmentHorizontal(selectedSegment);
            allClusters = image.segmentClusters(horizontal);

            std::vector<std::vector<Pixel> > neighboringSegments = extractNeighboringSegments(selectedSegment, horizontal, allClusters);

            // Banding detection and correction
            if (detectBanding(selectedSegment, neighboringSegments)) {
                image.setPixels(getReplacements(selectedSegment, neighboringSegments, image));
            }
        }

        // Final cleanup
        auto detection = std::make_unique<BandingDetection>(originalCanvas);
        image.setAffectedSegments(std::get<1>(detection->bandingDetection()));
        image.setError(std::get<0>(detection->bandingDetection()));
        image.clearSelectedSegment();
        image.clearHighlightedPixels();
    }


    void reset() override {
        Algorithm::reset();
        getPixelArtImage().clearDebugLines();
    }

private:
    PixelArtImage originalCanvas = PixelArtImage(0, 0);
    bool alterLeftOrTopEdge = true;
    bool alterRightOrBottomEdge = true;
    int operationIndex = 0;
    std::default_random_engine generator{42}; // Seed for reproducibility


    [[nodiscard]] bool detectBanding(const std::vector<Pixel> &selectedSegment,
                                     const std::vector<std::vector<Pixel> > &neighboringSegments) const {
        bool bandingDetected = false;
        auto [selStart, selEnd] = getSegmentEndpoints(selectedSegment);
        if (selStart == selEnd) return false;

        auto colorA = selectedSegment.front().color;

        for (const auto &neighboringSegment: neighboringSegments) {
            auto [nbStart, nbEnd] = getSegmentEndpoints(neighboringSegment);

            if (nbStart == nbEnd) continue;

            auto colorB = neighboringSegment.front().color;
            if (colorB == colorA)
                continue;

            auto alignmentOpt = checkEndpointAlignment(selStart, selEnd, nbStart, nbEnd);
            if (alignmentOpt.has_value()) {
                bandingDetected = true;
                break;
            }
        }

        return bandingDetected;
    }

    [[nodiscard]] static std::vector<std::vector<Pixel> > extractNeighboringSegments(
        const std::vector<Pixel> &selectedSegment,
        bool horizontal,
        const std::vector<std::vector<std::vector<Pixel> > > &allClusters) {
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
        bool horizontal = isSegmentHorizontal(sortedSegment);
        std::ranges::sort(sortedSegment, [this, horizontal](const Pixel &a, const Pixel &b) {
            return !horizontal ? a.pos.y < b.pos.y : a.pos.x < b.pos.x;
        });
        return {sortedSegment.front().pos, sortedSegment.back().pos};
    }

    // Check if two segments are adjacent and aligned at endpoints
    [[nodiscard]] static std::optional<std::string> checkEndpointAlignment(const Pos &segStart, const Pos &segEnd,
                                                                    const Pos &neighborStart,
                                                                    const Pos &neighborEnd) {
        bool alignStart = false;
        bool alignEnd = false;
        bool sticked = false;

        bool horizontal = segStart.x == neighborStart.x;

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
                                       const PixelArtImage &canvas) {
        std::vector<Pixel> replacements;

        if (segment.empty()) return replacements;

        bool horizontal = isSegmentHorizontal(segment);

        std::ranges::sort(segment, [this, horizontal](const Pixel &a, const Pixel &b) {
            return horizontal ? a.pos.x < b.pos.x : a.pos.y < b.pos.y;
        });

        switch (operationIndex) {
            case 0:
            case 1:
                return handleShrinkOrColorChange(segment, neighboringSegments, canvas);
            case 2:
                return handleExpansion(segment, neighboringSegments, canvas);
            default:
                return replacements;
        }
    }


    std::vector<Pixel> handleShrinkOrColorChange(std::vector<Pixel> &segment,
                                                 const std::vector<std::vector<Pixel> > &neighboringSegments,
                                                 const PixelArtImage &canvas) {
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

        // Avoid edge case with shrink strategy on a segment of length two with
        // both endpoint continuations of the same color; happens for avg color variant.

        auto fr = segment.front();
        auto bk = segment.back();
        if (isSegmentHorizontal(segment)) {
            if (segment.size() == 2) {
                auto fr_replacement = determineReplacementColor(fr.pos, canvas, fr.color, EdgeDirection::Left);
                auto bk_replacement = determineReplacementColor(bk.pos, canvas, bk.color, EdgeDirection::Right);

                if (fr_replacement == bk_replacement) {
                    alterLeftOrTopEdge = false;
                    alterRightOrBottomEdge = false;
                    // Pick one randomly
                    if (generator() % 2 == 0) {
                        alterLeftOrTopEdge = true;
                    } else {
                        alterRightOrBottomEdge = true;
                    }
                }
            }

            edges.push_back(prepareEdge(alterLeftOrTopEdge, true, EdgeDirection::Left));
            edges.push_back(prepareEdge(alterRightOrBottomEdge, false, EdgeDirection::Right));

        } else {
            if (segment.size() == 2) {
                auto fr_replacement = determineReplacementColor(fr.pos, canvas, fr.color, EdgeDirection::Top);
                auto bk_replacement = determineReplacementColor(bk.pos, canvas, bk.color, EdgeDirection::Bottom);

                if (fr_replacement == bk_replacement) {
                    alterLeftOrTopEdge = false;
                    alterRightOrBottomEdge = false;
                    // Pick one randomly
                    if (generator() % 2 == 0) {
                        alterLeftOrTopEdge = true;
                    } else {
                        alterRightOrBottomEdge = true;
                    }
                }
            }

            edges.push_back(prepareEdge(alterLeftOrTopEdge, true, EdgeDirection::Top));
            edges.push_back(prepareEdge(alterRightOrBottomEdge, false, EdgeDirection::Bottom));
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

        for (const EdgeOp &e: edges) {
            applyEdge(e);
        }

        if (!segment.empty() && detectBanding(segment, neighboringSegments)) {
            for (const EdgeOp &e: edges) {
                applyEdge(e);
            }
        }

        return replacements;
    }

    std::vector<Pixel> handleExpansion(std::vector<Pixel> &segment,
                                       const std::vector<std::vector<Pixel> > &neighboringSegments,
                                       const PixelArtImage &canvas) const {
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

        if (isSegmentHorizontal(segment)) {
            if (alterLeftOrTopEdge) ops.push_back({true, -1, 0, true});
            if (alterRightOrBottomEdge) ops.push_back({true, 1, 0, false});
        } else {
            if (alterLeftOrTopEdge) ops.push_back({true, 0, -1, true});
            if (alterRightOrBottomEdge) ops.push_back({true, 0, 1, false});
        }

        auto applyOp = [&](const ExpandOp &op) {
            if (!segment.empty()) {
                const Pos &ref = op.front ? segment.front().pos : segment.back().pos;
                tryExpand(ref, op.dx, op.dy, op.front);
            }
        };

        for (const auto &op: ops) {
            applyOp(op);
        }

        if (!segment.empty() && detectBanding(segment, neighboringSegments)) {
            for (const auto &op: ops) {
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
                if (operationIndex == 0) {
                    return cont;
                } else {
                    auto avgR = (removedColor.r + cont.r) / 2;
                    auto avgG = (removedColor.g + cont.g) / 2;
                    auto avgB = (removedColor.b + cont.b) / 2;
                    return {avgR, avgG, avgB}; // average
                }
            }
        }

        return removedColor;
    }

    static bool isSegmentHorizontal(const std::vector<Pixel>& selectedSegment) {
        int minX = std::numeric_limits<int>::max();
        int maxX = std::numeric_limits<int>::min();
        int minY = std::numeric_limits<int>::max();
        int maxY = std::numeric_limits<int>::min();

        for (const Pixel &px: selectedSegment) {
            if (px.pos.x < minX) minX = px.pos.x;
            if (px.pos.x > maxX) maxX = px.pos.x;
            if (px.pos.y < minY) minY = px.pos.y;
            if (px.pos.y > maxY) maxY = px.pos.y;
        }

        // Horizontal if width > height, else vertical
        return (maxX - minX) >= (maxY - minY);
    }

};


#endif // GENERALBANDINGCORRECTION_H
