//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef BANDINGCORRECTION_H
#define BANDINGCORRECTION_H

#pragma once
#include "AlgorithmModule.h"
#include <vector>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "../include/Canvas.h"
#include "imgui.h"
#include <unordered_map>
#include <random>
#include <set>
#include <glm/glm.hpp>
#include <unordered_set>
#include <functional>

template<>
struct std::hash<std::pair<int, int>> {
    size_t operator()(const std::pair<int, int>& p) const noexcept {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

class BandingCorrection final : public AlgorithmModule {
public:
    explicit BandingCorrection(Canvas& canvas) : AlgorithmModule(canvas) {}

    int expandIterations = 6;
    float probabilityToAddPixel = 0.3f;

    [[nodiscard]] std::string name() const override {
        return "Banding Correction";
    }

    void run() override {
        Canvas canvas = getCanvas();
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        std::vector<std::pair<Color, cv::Mat>> layers = extract_layers(canvas);

        if (layers.size() < 2) return; // safety

        // Create a blank Canvas for the processed (resulting) picture
        Canvas correctedCanvas(width, height);

        // Fill in using white background
        correctedCanvas.fill({255, 255, 255});

        // Fill in the subject outline and its first layer by default on this new canvas.
        for (size_t i = 0; i <= 1; ++i) {
            auto& [color, currentMask] = layers[i];
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    if (currentMask.at<uchar>(y, x))
                        correctedCanvas.setProcessedPixel({x, y}, color);
        }

        for (size_t i = 2; i < layers.size(); ++i) {
            auto& [color, currentMask] = layers[i];

            cv::Mat modified;
            cv::Mat kernel = cv::Mat::ones(3, 3, CV_8UC1);
            cv::erode(currentMask, modified, kernel, cv::Point(-1, -1), static_cast<int>(i));
            expandShape(modified, expandIterations);

            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    // Paint the modified pixels of the layer
                    // Do not add pixels outside the outline of the subject
                    if (modified.at<uchar>(y, x) && layers[0].second.at<uchar>(y, x))
                        correctedCanvas.setProcessedPixel({x, y}, color);
        }

        getCanvas().setProcessedPixels(correctedCanvas);
    }

    void renderUI() override {
        ImGui::Text("Shape Expansion Iterations");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##ExpandShape Iterations", &expandIterations, 0, 10);

        ImGui::Text("Probability to Add Pixel");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##ProbabilityToAddPixel", &probabilityToAddPixel, 0.0f, 1.0f, "%.3f");
    }



private:
    Canvas originalCanvas = Canvas(0, 0);
    std::default_random_engine generator;

    static std::vector<std::pair<Color, cv::Mat>> extract_layers(const Canvas& canvas) {
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        cv::Mat subjectMask = extractSubjectMask(canvas);

        std::unordered_map<Color, std::vector<cv::Point>> colorPixels;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                if (subjectMask.at<uchar>(y, x) == 0) continue;
                Color color = canvas.getPixel({x, y}).color;
                colorPixels[color].emplace_back(x, y);
            }

        std::vector<std::pair<Color, cv::Mat>> layers;
        for (auto& [color, points] : colorPixels) {
            cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
            for (const auto& pt : points)
                mask.at<uchar>(pt.y, pt.x) = 255;

            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            mask = cv::Mat::zeros(height, width, CV_8UC1);
            cv::drawContours(mask, contours, -1, 255, cv::FILLED);

            layers.emplace_back(color, mask);
        }

        // Sort by descending pixel count (surface area)
        std::ranges::sort(layers, [](const auto& a, const auto& b) {
            return cv::countNonZero(a.second) > cv::countNonZero(b.second);
        });

        return layers;
    }

    void expandShape(cv::Mat& input_shape, int iterations = 1) {
        std::vector<std::pair<int, int>> neighbors_8 = {
            {-1, -1}, {-1, 0}, {-1, 1},
            {0, -1},          {0, 1},
            {1, -1},  {1, 0}, {1, 1}
        };

        std::unordered_set<std::pair<int, int>> shape;

        for (int y = 0; y < input_shape.rows; ++y)
            for (int x = 0; x < input_shape.cols; ++x)
                if (input_shape.at<uchar>(y, x) > 0)
                    shape.emplace(x, y);

        for (int iter = 0; iter < iterations; ++iter) {
            std::unordered_map<std::pair<int, int>, int> candidate_counts;

            for (const auto& pt : shape) {
                for (const auto& [dx, dy] : neighbors_8) {
                    auto neighbor = std::make_pair(pt.first + dx, pt.second + dy);
                    if (!shape.contains(neighbor))
                        candidate_counts[neighbor]++;
                }
            }

            std::uniform_real_distribution<double> dist(0.0, 1.0);

            // First pass: probabilistic addition
            for (const auto& [pt, count] : candidate_counts) {
                if (count == 3 && dist(generator) < probabilityToAddPixel) {
                    shape.insert(pt); // Add directly to shape
                }
            }

            // Recompute candidate counts
            candidate_counts.clear();
            for (const auto& pt : shape) {
                for (const auto& [dx, dy] : neighbors_8) {
                    auto neighbor = std::make_pair(pt.first + dx, pt.second + dy);
                    if (!shape.contains(neighbor))
                        candidate_counts[neighbor]++;
                }
            }

            // Second pass: deterministic addition
            for (const auto& [pt, count] : candidate_counts) {
                if (count >= 4) {
                    shape.insert(pt);
                }
            }
        }

        cv::Mat result(input_shape.size(), CV_8UC1, cv::Scalar(0));
        for (const auto& [x, y] : shape)
            if (y >= 0 && y < input_shape.rows && x >= 0 && x < input_shape.cols)
                result.at<uchar>(y, x) = 255;

        input_shape = result;
    }

    static cv::Mat extractSubjectMask(const Canvas& canvas) {
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        cv::Mat mask(height, width, CV_8UC1, cv::Scalar(0));

        constexpr int threshold = 250;  // tolerance for "near-white"
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

#endif // BANDINGCORRECTION_H
