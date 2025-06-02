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

        std::vector<std::pair<Color, cv::Mat> > layers = extract_layers(canvas);

        if (layers.size() < 2) return; // safety

        // Clear debug layers
        debugLayers.clear();

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

        if (!getPixelArtImage().getDrawnPath().empty() || getPixelArtImage().getGenerator().has_value()) {
            ImGui::Text("Translation Factor");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##TranslationFactor", &TRANSLATION_FACTOR, 0.0f, 2.0f, "%.3f");
        }

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

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (layer.at<uchar>(y, x) > 0) {
                    getPixelArtImage().setDebugPixel({x, y}, Color(255, 0, 0)); // Red color for debug
                }
            }
        }

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
    float TRANSLATION_FACTOR = 1.0f; // Default factor
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
                            float attenuation = TRANSLATION_FACTOR * 1.0f / (static_cast<float>(layers.size()) - i);
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


    static std::vector<std::pair<Color, cv::Mat> > extract_layers(const PixelArtImage &canvas) {
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
        for (auto &[color, points]: colorPixels) {
            cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
            for (const auto &pt: points)
                mask.at<uchar>(pt.y, pt.x) = 255;

            std::vector<std::vector<cv::Point> > contours;
            cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            mask = cv::Mat::zeros(height, width, CV_8UC1);
            cv::drawContours(mask, contours, -1, 255, cv::FILLED);

            layers.emplace_back(color, mask);
        }

        // Sort by descending pixel count (surface area)
        std::ranges::sort(layers, [](const auto &a, const auto &b) {
            return cv::countNonZero(a.second) > cv::countNonZero(b.second);
        });

        return layers;
    }

    void expandShape(cv::Mat &input_shape, std::unordered_set<std::pair<int, int> > *out_candidate_neighbors = nullptr,
                     int iterations = 1) {
        if (iterations == 0) return;

        std::vector<std::pair<int, int> > neighbors = {
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
            for (const auto &[dx, dy]: neighbors) {
                auto neighbor = std::make_pair(pt.first + dx, pt.second + dy);
                if (!shape.contains(neighbor))
                    candidate_counts[neighbor]++;
            }
        }

        // Set neighbours for debug purposes
        for (const auto &[pt, count]: candidate_counts) {
            out_candidate_neighbors->insert(pt);
        }

        std::uniform_real_distribution<double> dist(0.0, 1.0);


        cv::Mat dilated;
        for (int i = 0; i < iterations; ++i) {
            // Recompute candidate pixels
            candidate_counts.clear();
            for (const auto &pt: shape) {
                for (const auto &[dx, dy]: neighbors) {
                    auto neighbor = std::make_pair(pt.first + dx, pt.second + dy);
                    if (!shape.contains(neighbor))
                        candidate_counts[neighbor]++;
                }
            }
            // First pass: probabilistic addition
            for (const auto &[pt, count]: candidate_counts) {
                if (count == 3 && dist(generator) < PROB_ADD_CANDIDATE_PIXEL) {
                    shape.insert(pt);
                }
            }

            // Convert shape back to binary image
            cv::Mat temp(input_shape.size(), CV_8UC1, cv::Scalar(0));
            for (const auto &[x, y]: shape) {
                if (x >= 0 && x < input_shape.cols && y >= 0 && y < input_shape.rows)
                    temp.at<uchar>(y, x) = 255;
            }

            // Second pass: apply a dilation to smooth out the result
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
            cv::dilate(temp, dilated, kernel, cv::Point(-1, -1), 1);
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
