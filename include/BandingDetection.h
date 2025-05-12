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
        detectBandingLines(canvas, bandingLines);

        // Drawing
        for (const auto &[y, startX, len]: bandingLines) {
            glm::vec2 start = glm::vec2(static_cast<float>(startX), static_cast<float>(y) + 0.5f);
            glm::vec2 end = glm::vec2(static_cast<float>(startX + len), static_cast<float>(y) + 0.5f);
            canvas.addDebugLine(start, end, {255, 0, 0}); // Red debug line
        }
    }

    void reset() override {
        Algorithm::reset();
        showDebug = false;
        getCanvas().clearDebugLines();
    }

    void renderUI() override {
        ImGui::Text("Color Similarity Threshold");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##Thresh", &threshold, 0, 254);
    }

    void detectBandingLines(const Canvas &canvas, std::vector<std::tuple<int, int, int>> &bandingLines) const {
        const int width = canvas.getWidth();
        const int height = canvas.getHeight();

        // Convert image to grayscale
        std::vector gray(height, std::vector<uint8_t>(width));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const Color color = canvas.getPixel({x, y}).color;
                gray[y][x] = static_cast<uint8_t>((color.r + color.g + color.b) / 3);
            }
        }

        // Detection: store matching lines
        for (int y = 0; y < height - 1; ++y) {
            for (int startX = 0; startX < width - 2; ++startX) {
                for (int len = 2; len < std::min(6, width - startX); ++len) {
                    MatchType match = this->is_similar(gray[y], gray[y + 1], startX, len);
                    if (match != MatchType::None) {
                        int adjustedStartX = startX;
                        int adjustedLen = len;

                        if (match == MatchType::ShiftRight) {
                            adjustedLen += 1;
                        } else if (match == MatchType::ShiftLeft && adjustedStartX > 0) {
                            adjustedStartX -= 1;
                            adjustedLen += 1;
                        }

                        bandingLines.emplace_back(y, adjustedStartX, adjustedLen);
                        break;
                    }
                }
            }
        }
    }

    void setThreshold(int threshold) {
        this->threshold = threshold;
    }

private:
    bool showDebug = false;
    int threshold = 1;

    enum class MatchType {
        None,
        Exact,
        ShiftLeft,
        ShiftRight
    };


    [[nodiscard]] MatchType is_similar(const std::vector<uint8_t> &row1,
                                   const std::vector<uint8_t> &row2,
                                   int start, int length) const {

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
