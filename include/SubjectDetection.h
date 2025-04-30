//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef SUBJECTDETECTION_H
#define SUBJECTDETECTION_H

#pragma once
#include "AlgorithmModule.h"
#include <vector>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "../include/Canvas.h"
#include "imgui.h"

class SubjectDetection final : public AlgorithmModule {
public:
    explicit SubjectDetection(Canvas &canvas) : AlgorithmModule(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Subject Detection";
    }

    void run() override {
        int width = getCanvas().getWidth();
        int height = getCanvas().getHeight();

        // Step 1: Convert canvas to grayscale
        cv::Mat image(height, width, CV_8UC1);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Color color = getCanvas().getPixel({x, y}).color;
                auto intensity = static_cast<uchar>(0.299f * color.r + 0.587f * color.g + 0.114f * color.b);
                image.at<uchar>(y, x) = intensity;
            }
        }

        // Step 2: Threshold - isolate non-white pixels
        cv::Mat binary;
        cv::threshold(image, binary, 250, 255, cv::THRESH_BINARY_INV); // Keep darker regions, treat white as background

        // Step 3: Find contours
        std::vector<std::vector<cv::Point> > contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        // Step 4: Find largest contour
        int largestIndex = -1;
        double maxArea = 0;
        for (int i = 0; i < contours.size(); ++i) {
            double area = cv::contourArea(contours[i]);
            if (area > maxArea) {
                maxArea = area;
                largestIndex = i;
            }
        }

        // Step 5: Draw mask of largest contour
        cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
        if (largestIndex >= 0) {
            cv::drawContours(mask, contours, largestIndex, 255, cv::FILLED);
        }

        // Step 6: Collect selected pixels
        std::vector<Pixel> subjectPixels;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (mask.at<uchar>(y, x) != 0) {
                    subjectPixels.push_back(getCanvas().getPixel({x, y}));
                }
            }
        }
        selectedRegion = subjectPixels;
    }

    void renderDebugUI() override {
        ImGui::Checkbox("Show (ensure to first \"Run\")", &debugEnabled);

        if (debugEnabled) {
            highlightSelectedPixels();
        } else {
            restoreOriginalCanvas();
        }
    }

    void reset() override {
        selectedRegion.clear();
    }

private:
    bool debugEnabled = false;
    std::vector<Pixel> selectedRegion;

    void restoreOriginalCanvas() const {
        getCanvas().clearDebugPixels();
    }

    void highlightSelectedPixels() const {
        for (const auto &pixel: selectedRegion) {
            getCanvas().setDebugPixel(pixel.pos, {255, 0, 0});
        }
    }
};


#endif //SUBJECTDETECTION_H
