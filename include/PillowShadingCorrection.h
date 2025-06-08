//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef PILLOWSHADINGCORRECTION_H
#define PILLOWSHADINGCORRECTION_H

#pragma once
#include "Algorithm.h"
#include <vector>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "../include/PixelArtImage.h"
#include "imgui.h"
#include <unordered_map>
#include <random>
#include <set>
#include <glm/glm.hpp>
#include <unordered_set>
#include <functional>

#include "BandingDetection.h"

template<>
struct std::hash<std::pair<int, int> > {
    size_t operator()(const std::pair<int, int> &p) const noexcept {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

class PillowShadingCorrection final : public Algorithm {
public:
    explicit PillowShadingCorrection(PixelArtImage &canvas) : Algorithm(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Pillow-Shading Correction";
    }

    void run() override {
        PixelArtImage &canvas = getPixelArtImage();
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        std::vector<std::pair<Color, cv::Mat> > layers = extractLayers(canvas);

        if (layers.size() < 2) return; // safety

        // Clear debug layers
        debugLayers.clear();
        debugNeighborCandidates.clear();
        showNeighborCandidates = false;

        // Create a blank PixelArtImage for the processed (resulting) picture
        auto error = 9999999;
        auto bestCorrectedCanvas = PixelArtImage(width, height);
        auto bestBandingLines = std::vector<std::tuple<int, int, int> >();
        for (int i = 0; i < PIPELINE_ITERATIONS; ++i) {
            PixelArtImage correctedCanvas(width, height);

            constructCorrectedCanvas(width, height, layers, correctedCanvas);

            auto detection = std::make_unique<BandingDetection>(correctedCanvas);
            auto newError = detection->bandingDetection();

            if (newError < error) {
                bestCorrectedCanvas = correctedCanvas;
                error = newError;
            }
        }

        auto detection = std::make_unique<BandingDetection>(canvas);
        auto originalError = detection->bandingDetection();

        errorImprovement = originalError - error;

        canvas.clearDebugLines();
        canvas.setProcessedPixels(bestCorrectedCanvas);
    }

    void reset() override {
        Algorithm::reset();
        debugLayers.clear();
        getPixelArtImage().clearDebugLines();
        getPixelArtImage().clearDebugPixels();
        getPixelArtImage().clearProcessedPixels();
        showDebug = false;
        selectedLayer = 0;
        debugNeighborCandidates.clear();
        showNeighborCandidates = false;
        errorImprovement = 0;
    }

    void renderUI() override {
        ImGui::Text("Pipeline Iterations");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##Pipeline Iterations", &PIPELINE_ITERATIONS, 1, 10);

        // Combo to select erosion mode
        ImGui::Text("Erosion Mode");
        const char *erosionModes[] = {"Constant Erosion Iterations", "Linear Erosion Iterations (On Layer #)"};
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::Combo("##Erosion Mode", &erosionMode, erosionModes, IM_ARRAYSIZE(erosionModes));

        // Show slider for linear erosion factor only if linear erosion is selected
        if (erosionMode == 1) {
            ImGui::Text("Linear Erosion Factor");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##LinearErosionFactor", &LINEAR_EROSION_FACTOR, 0.0f, 2.0f, "%.3f");
        }

        ImGui::Text("Expansion Iterations");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##ExpansionIterations", &EXPANSION_ITERATIONS, 0, 5);


        ImGui::Text("Probability to Add Candidate Pixel");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##ProbabilityToAddPixel", &PROB_ADD_CANDIDATE_PIXEL, 0.0f, 1.0f, "%.3f");

        ImGui::Separator();
        ImGui::Text("Error Decreased By: %d", errorImprovement);
    }

    void renderDebugUI() override {
        ImGui::Checkbox("Debug View", &showDebug);

        if (!showDebug) {
            getPixelArtImage().clearDebugPixels(); // Clear if debug turned off
            return;
        }

        if (debugLayers.empty()) {
            ImGui::Text("No debug layers available.");
            return;
        }

        ImGui::Checkbox("Show Neighbors", &showNeighborCandidates);

        std::vector<std::string> labels;
        for (size_t i = 0; i < debugLayers.size(); ++i)
            labels.emplace_back("Layer " + std::to_string(i));

        std::vector<const char *> labelPointers;
        for (const auto &label: labels)
            labelPointers.push_back(label.c_str());

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::Combo("##Select Layer", &selectedLayer, labelPointers.data(), static_cast<int>(labelPointers.size()));

        getPixelArtImage().clearDebugPixels();
        const cv::Mat &layer = debugLayers[selectedLayer];
        int width = getPixelArtImage().getWidth();
        int height = getPixelArtImage().getHeight();
        //
        // for (int y = 0; y < height; ++y) {
        //     for (int x = 0; x < width; ++x) {
        //         if (layer.at<uchar>(y, x) > 0) {
        //             getPixelArtImage().setDebugPixel({x, y}, Color(255, 0, 0)); // Red color for debug
        //         }
        //     }
        // }

        if (showNeighborCandidates && selectedLayer < debugNeighborCandidates.size()) {
            for (const auto &[x, y]: debugNeighborCandidates[selectedLayer]) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    getPixelArtImage().setDebugPixel({x, y}, Color(0, 0, 255)); // Blue
                }
            }
        }
    }

private:
    PixelArtImage originalCanvas = PixelArtImage(0, 0);
    std::default_random_engine generator{42}; // Seed for reproducibility
    std::vector<cv::Mat> debugLayers;
    bool showDebug = false;
    int selectedLayer = 0;
    std::vector<std::unordered_set<std::pair<int, int> > > debugNeighborCandidates;
    bool showNeighborCandidates = false;
    int erosionMode = 0; // 0 = constant, 1 = linear
    bool bandingDetection = false;
    int errorImprovement = 0;

    float LINEAR_EROSION_FACTOR = 1.0f; // Default factor
    int EXPANSION_ITERATIONS = 1;
    float PROB_ADD_CANDIDATE_PIXEL = 0.3f;
    int PIPELINE_ITERATIONS = 10;

    void constructCorrectedCanvas(int width, int height, std::vector<std::pair<Color, cv::Mat> > layers,
                                  PixelArtImage &correctedCanvas) {
        correctedCanvas.fill({255, 255, 255});

        // Fill subject outline and first layer
        for (size_t i = 0; i < 2; ++i) {
            auto &[color, currentMask] = layers[i];
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    if (currentMask.at<uchar>(y, x))
                        correctedCanvas.setPixel({x, y}, color);
        }

        std::optional<Pixel> generator = std::nullopt;
        std::optional<cv::Mat> drawnPathMask = std::nullopt;

        // First try to get the generator pixel
        if (auto dp = getPixelArtImage().getGenerator(); dp.has_value()) {
            generator = dp;
        } else {
            const auto &drawnPath = getPixelArtImage().getDrawnPath();
            if (!drawnPath.empty()) {
                std::vector<cv::Point> contour;
                for (const Pixel &p: drawnPath)
                    contour.emplace_back(p.pos.x, p.pos.y);

                cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
                std::vector<std::vector<cv::Point> > contours = {contour};
                cv::drawContours(mask, contours, 0, 255, cv::FILLED);

                // Compute the center of mass
                cv::Moments m = cv::moments(mask, true);
                if (m.m00 != 0.0) {
                    int cx = static_cast<int>(m.m10 / m.m00);
                    int cy = static_cast<int>(m.m01 / m.m00);
                    generator = Pixel{{0, 0, 0}, {cx, cy}};
                }

                drawnPathMask = mask;
            }
        }

        int iterations = 0;
        if (drawnPathMask.has_value()) {
            iterations = layers.size() - 2;
        } else {
            iterations = layers.size() - 1;
        }

        for (size_t i = 2; i <= iterations; ++i) {
            auto &[color, currentMask] = layers[i];
            cv::Mat translatedMask = cv::Mat::zeros(height, width, CV_8UC1);

            if (generator.has_value()) {
                int minX = width, minY = height, maxX = 0, maxY = 0;
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                        if (currentMask.at<uchar>(y, x)) {
                            minX = std::min(minX, x);
                            minY = std::min(minY, y);
                            maxX = std::max(maxX, x);
                            maxY = std::max(maxY, y);
                        }

                int centerX = (minX + maxX) / 2;
                int centerY = (minY + maxY) / 2;

                int dx = generator->pos.x - centerX;
                int dy = generator->pos.y - centerY;

                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                        if (currentMask.at<uchar>(y, x)) {
                            float attenuation = 1.0f / (static_cast<float>(layers.size()) - i);
                            int newX = x + dx * attenuation;
                            int newY = y + dy * attenuation;
                            if (newX >= 0 && newX < width && newY >= 0 && newY < height)
                                translatedMask.at<uchar>(newY, newX) = 255;
                        }
            } else {
                translatedMask = currentMask;
            }

            cv::Mat modified;
            cv::Mat kernel = cv::Mat::ones(3, 3, CV_8UC1);
            if (erosionMode == 0) {
                cv::erode(translatedMask, modified, kernel, cv::Point(-1, -1), 1);
            } else {
                cv::erode(translatedMask, modified, kernel, cv::Point(-1, -1),
                          static_cast<int>(LINEAR_EROSION_FACTOR * i));
            }

            debugLayers.push_back(translatedMask);
            std::unordered_set<std::pair<int, int> > neighbors;
            expandShape(modified, &neighbors, EXPANSION_ITERATIONS);
            debugNeighborCandidates.push_back(std::move(neighbors));

            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    if (modified.at<uchar>(y, x) && layers[1].second.at<uchar>(y, x))
                        correctedCanvas.setPixel({x, y}, color);
        }

        // If a drawn path mask was constructed, add it as the final layer as-is
        if (drawnPathMask.has_value()) {
            auto &mask = drawnPathMask.value();
            Color lastColor = layers.back().first; // Use the last layer color for consistency
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    if (mask.at<uchar>(y, x))
                        correctedCanvas.setPixel({x, y}, lastColor);
        }
    }


    static std::vector<std::pair<Color, cv::Mat> > extractLayers(const PixelArtImage &canvas) {
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        cv::Mat subjectMask = extractSubjectMask(canvas);

        std::unordered_map<Color, std::vector<cv::Point> > colorPixels;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                if (subjectMask.at<uchar>(y, x) == 0) continue;
                Color color = canvas.getPixel({x, y}).color;
                colorPixels[color].emplace_back(x, y);
            }

        std::vector<std::pair<Color, cv::Mat> > layers;
        std::vector<std::pair<Color, cv::Mat> > unchangedLayers;

        for (auto &[color, points]: colorPixels) {
            cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
            for (const auto &pt: points)
                mask.at<uchar>(pt.y, pt.x) = 255;

            int originalCount = static_cast<int>(points.size());

            // Check number of connected components
            cv::Mat labels;
            int numComponents = cv::connectedComponents(mask, labels, 8);

            std::vector<std::vector<cv::Point> > contours;
            if (numComponents <= 2) {
                cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            } else {
                std::vector<cv::Point> rawContour;
                for (const auto &pt: points)
                    rawContour.emplace_back(pt.x, pt.y);

                std::vector<cv::Point> hull;
                cv::convexHull(rawContour, hull);
                contours = {hull};
            }

            cv::Mat filledMask = cv::Mat::zeros(height, width, CV_8UC1);
            cv::drawContours(filledMask, contours, -1, 255, cv::FILLED);

            // If the filled mask is identical to the original, mark it as unchanged
            if (cv::countNonZero(mask != filledMask) == 0) {
                unchangedLayers.emplace_back(color, filledMask);
            } else {
                layers.emplace_back(color, filledMask);
            }
        }

        // Sort remaining layers in *reverse* order of original pixel count
        std::ranges::sort(layers, [&](const auto &a, const auto &b) {
            int countA = cv::countNonZero(a.second);
            int countB = cv::countNonZero(b.second);
            return countA > countB; // reverse: biggr ones first
        });

        // Append unchanged layers at the end
        layers.insert(layers.end(), unchangedLayers.begin(), unchangedLayers.end());

        return layers;
    }

    void expandShape(cv::Mat &input_shape, std::unordered_set<std::pair<int, int> > *out_candidate_neighbors = nullptr,
                     int iterations = 1) {
        if (iterations == 0) return;

        std::vector<std::pair<int, int> > neighbors_8 = {
            {-1, -1}, {-1, 0}, {-1, 1},
            {0, -1}, {0, 1},
            {1, -1}, {1, 0}, {1, 1}
        };

        std::unordered_set<std::pair<int, int> > shape;

        for (int y = 0; y < input_shape.rows; ++y)
            for (int x = 0; x < input_shape.cols; ++x)
                if (input_shape.at<uchar>(y, x) > 0)
                    shape.emplace(x, y);

        std::unordered_map<std::pair<int, int>, int> candidate_counts;

        for (const auto &pt: shape) {
            for (const auto &[dx, dy]: neighbors_8) {
                auto neighbor = std::make_pair(pt.first + dx, pt.second + dy);
                if (!shape.contains(neighbor))
                    candidate_counts[neighbor]++;
            }
        }


        cv::Mat dilated;
        for (int i = 0; i < iterations; ++i) {
            // Recompute candidate pixels
            candidate_counts.clear();
            for (const auto &pt: shape) {
                for (const auto &[dx, dy]: neighbors_8) {
                    auto neighbor = std::make_pair(pt.first + dx, pt.second + dy);
                    if (!shape.contains(neighbor))
                        candidate_counts[neighbor]++;
                }
            }

            // First pass: probabilistic addition
            std::unordered_set<std::pair<int, int>> addedPixels;
            std::uniform_real_distribution dist(0.0, 1.0);

            for (const auto &[pt, count]: candidate_counts) {
                if (count == 3 && dist(generator) < PROB_ADD_CANDIDATE_PIXEL) {
                    // Add the pixel
                    addedPixels.insert(pt);
                    shape.insert(pt);
                }
            }

            // Convert shape back to binary image
            cv::Mat temp(input_shape.size(), CV_8UC1, cv::Scalar(0));
            for (const auto &[x, y]: shape) {
                if (x >= 0 && x < input_shape.cols && y >= 0 && y < input_shape.rows)
                    temp.at<uchar>(y, x) = 255;
            }

            // Second pass: apply a dilation to get back to the original size
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
            cv::dilate(temp, dilated, kernel, cv::Point(-1, -1), 1);


            // Third pass: fill in gaps in the contour, to remove lone pixels

            // Extract contour and neighbors of "dilated"
            std::vector<std::pair<int, int> > neighbors_4 = {
                        {-1, 0},
                {0, -1},    {0, 1},
                        {1, 0},
            };

            std::set<std::pair<int, int>> contour;
            std::set<std::pair<int, int>> neighbors;

            for (int y = 1; y < dilated.rows - 1; ++y) {
                for (int x = 1; x < dilated.cols - 1; ++x) {
                    if (dilated.at<uchar>(y, x) > 0) {
                        for (const auto &[dx, dy]: neighbors_4) {
                            if (dilated.at<uchar>(y+dy, x+dx) == 0) {
                                contour.insert(std::pair(x, y));
                                neighbors.insert(std::pair(x+dx, y+dy));
                            }
                        }
                    }
                }
            }


            // Iterate through the contour
            // For every pixel on the contour, check which directions are free: in other words
            // check which of the neighbors_4 are not part of the contour as well
            // If such pixels are found (say we found one above), iterate through each, doing the following:
            // Try to add as many pixels in that direction as possible (so add as many pixels directly above)
            // Until we "meet" another pixel from the contour
            // And also while ensuring those added pixels are part of the neighbors set; once we find one not part
            // of the neighbor set, you can directly "continue" the loop, not adding any of those pixels at all
            // The point is that this technique would close "vertical/horizontal" gaps in contours
            for (const auto &[x, y] : contour) {
                for (const auto &[dx, dy] : neighbors_4) {
                    int nx = x + dx;
                    int ny = y + dy;

                    // If neighbor is not part of contour, consider it a direction to try
                    if (!contour.contains({nx, ny})) {
                        std::vector<std::pair<int, int>> to_add;
                        int cx = nx;
                        int cy = ny;

                        while (true) {
                            // If out of bounds, break
                            if (cx < 0 || cx >= dilated.cols || cy < 0 || cy >= dilated.rows)
                                break;

                            std::pair<int, int> current = {cx, cy};

                            // Stop if this is not a neighbor
                            if (!neighbors.contains(current))
                                break;

                            // Stop if we encounter another contour pixel
                            if (contour.contains(current))
                                break;

                            // Otherwise, queue this pixel for addition
                            to_add.push_back(current);

                            cx += dx;
                            cy += dy;
                        }

                        // If we ended because of a contour pixel, it's a valid bridge
                        if (contour.contains({cx, cy})) {
                            for (const auto &[px, py] : to_add) {
                                dilated.at<uchar>(py, px) = 255;
                                out_candidate_neighbors->insert({px, py});
                            }
                        }
                    }
                }
            }

            for (const auto &pt: contour) {
                out_candidate_neighbors->insert(pt);
            }

        }

        // Final result: union of original shape and dilated result
        input_shape = dilated;
    }

    static cv::Mat extractSubjectMask(const PixelArtImage &canvas) {
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

#endif // PILLOWSHADINGCORRECTION_H
