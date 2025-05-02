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
struct std::hash<std::pair<int, int> > {
    size_t operator()(const std::pair<int, int> &p) const noexcept {
        // Combine the hashes of the two integers (x and y)
        return std::hash<int>()(p.first) ^ (hash<int>()(p.second) << 1);
    }
};


class BandingCorrection final : public AlgorithmModule {
public:
    explicit BandingCorrection(Canvas &canvas) : AlgorithmModule(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Banding Correction";
    }

    void run() override {
        Canvas canvas = getCanvas();
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        // Step 1: Extract the layers
        std::unordered_map<Color, cv::Mat> layers;
        std::vector<std::pair<Color, double> > layerDistances;
        extract_layers(canvas, layers, layerDistances);

        // Step 2: Create the corrected canvas
        Canvas correctedCanvas(width, height);
        correctedCanvas.fill({255, 255, 255});

        // Step 3: Paint the outline and first color (first 2 in the sorted list)
        for (size_t i = 0; i <= 1; ++i) {
            auto &[color, _] = layerDistances[i];
            cv::Mat &currentMask = layers[color];
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    if (currentMask.at<uchar>(y, x))
                        correctedCanvas.setProcessedPixel({x, y}, color);
        }

        // Step 4: Alter each of the other layers using the varyShape function
        for (size_t i = 2; i < layerDistances.size(); ++i) {
            auto &[color, _] = layerDistances[i];
            cv::Mat &mask = layers[color];
            mask = varyShape(mask);
        }

        // Step 5: Paint remaining layers
        for (size_t i = 2; i < layerDistances.size(); ++i) {
            const auto &[color, _] = layerDistances[i];
            const cv::Mat &mask = layers[color];
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    if (mask.at<uchar>(y, x))
                        correctedCanvas.setProcessedPixel({x, y}, color);
        }

        // Copy back to canvas
        getCanvas().setProcessedPixels(correctedCanvas);
    }


    void renderDebugUI() override {
    }

private:
    Canvas originalCanvas = Canvas(0, 0);
    std::default_random_engine generator;

    static void extract_layers(const Canvas &canvas,
                               std::unordered_map<Color, cv::Mat> &layers,
                               std::vector<std::pair<Color, double> > &layerDistances) {
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        // Step 1: Extract the subject mask
        cv::Mat subjectMask = extractSubjectMask(canvas);

        // Step 2: For each pixel in the subject, build color masks
        std::unordered_map<Color, std::vector<cv::Point> > colorPixels;

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                if (subjectMask.at<uchar>(y, x) == 0) continue;

                Color color = canvas.getPixel({x, y}).color;
                colorPixels[color].emplace_back(x, y);
            }

        // Step 3: Create binary masks and fill contours for each color
        for (auto &[color, points]: colorPixels) {
            cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
            for (const auto &pt: points)
                mask.at<uchar>(pt.y, pt.x) = 255;

            // Find and fill contours to create a solid mask
            std::vector<std::vector<cv::Point> > contours;
            cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            mask = cv::Mat::zeros(height, width, CV_8UC1);
            cv::drawContours(mask, contours, -1, 255, cv::FILLED);

            layers[color] = mask;
        }

        // Step 4: Compute distance to edge
        cv::Mat edge;
        cv::Canny(subjectMask, edge, 100, 200);

        for (auto &[color, mask]: layers) {
            cv::Mat distToEdge;
            cv::distanceTransform(255 - edge, distToEdge, cv::DIST_L2, 3);
            double meanDist = cv::mean(distToEdge, mask)[0];
            layerDistances.emplace_back(color, meanDist);
        }

        std::ranges::sort(layerDistances, [](auto &a, auto &b) {
            return a.second < b.second;
        });
    }

    // Private method to apply the expansion logic
    cv::Mat expandShape(const cv::Mat &input_shape, int iterations = 1) {
        // Define the 8 possible neighbors (offsets)
        std::vector<std::pair<int, int> > neighbors_8 = {
            {-1, -1}, {-1, 0}, {-1, 1},
            {0, -1}, {0, 1},
            {1, -1}, {1, 0}, {1, 1}
        };

        // Create a set to store the coordinates of the shape
        std::unordered_set<std::pair<int, int> > shape;

        // Loop through the input matrix to find the non-zero points (the shape)
        for (int i = 0; i < input_shape.rows; ++i) {
            for (int j = 0; j < input_shape.cols; ++j) {
                if (input_shape.at<uchar>(i, j) > 0) {
                    shape.insert({j, i}); // Use (x, y) format
                }
            }
        }

        // Perform expansion for the given number of iterations
        for (int iter = 0; iter < iterations; ++iter) {
            std::unordered_map<std::pair<int, int>, int, std::hash<std::pair<int, int> > > candidate_counts;

            // Loop through each point in the shape
            for (const auto &point: shape) {
                // Check each of the 8 neighboring points
                for (const auto &neighbor: neighbors_8) {
                    std::pair<int, int> neighbor_point = {point.first + neighbor.first, point.second + neighbor.second};

                    if (!shape.contains(neighbor_point)) {
                        candidate_counts[neighbor_point]++;
                    }
                }
            }

            std::uniform_real_distribution<double> distribution(0.0, 1.0);
            // Set to store the new pixels to be added
            std::unordered_set<std::pair<int, int> > new_pixels;

            // Add new pixels based on the neighbor counts
            for (const auto &candidate: candidate_counts) {
                const std::pair<int, int> &neighbor_point = candidate.first;
                int neighbor_count = candidate.second;

                if (neighbor_count >= 4) {
                    // Add pixels surrounded on 4+ sides — avoids lone pixels
                    new_pixels.insert(neighbor_point);
                } else if (neighbor_count == 3) {
                    // Sometimes allow 3-neighbor pixels to break perfect symmetry
                    if (distribution(generator) < 0.3) {
                        // tweak this to control shape softness
                        new_pixels.insert(neighbor_point);
                    }
                }
            }

            // Merge the new pixels with the existing shape
            for (const auto &new_pixel: new_pixels) {
                shape.insert(new_pixel);
            }
        }

        // Create the output matrix (same size as input)
        cv::Mat expanded_shape(input_shape.size(), CV_8U, cv::Scalar(0));

        // Fill the output matrix with the expanded shape
        for (const auto &point: shape) {
            expanded_shape.at<uchar>(point.second, point.first) = 255; // Set the pixel to white
        }

        return expanded_shape;
    }


    // Private method to apply erosion and expansion
    cv::Mat varyShape(const cv::Mat &input_shape) {
        cv::Mat eroded;
        cv::Mat kernel = cv::Mat::ones(2, 2, CV_8UC1);
        cv::erode(input_shape, eroded, kernel);

        return this->expandShape(eroded, 2);
    }


    static cv::Mat extractSubjectMask(const Canvas &canvas) {
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        cv::Mat image(height, width, CV_8UC1);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                Color color = canvas.getPixel({x, y}).color;
                auto intensity = static_cast<uchar>(0.299f * color.r + 0.587f * color.g + 0.114f * color.b);
                image.at<uchar>(y, x) = intensity;
            }

        // Threshold to isolate non-white regions
        cv::Mat binary;
        cv::threshold(image, binary, 250, 255, cv::THRESH_BINARY_INV);

        // Find largest contour
        std::vector<std::vector<cv::Point> > contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        int largestIndex = -1;
        double maxArea = 0;
        for (int i = 0; i < contours.size(); ++i) {
            double area = cv::contourArea(contours[i]);
            if (area > maxArea) {
                maxArea = area;
                largestIndex = i;
            }
        }

        // Return mask of largest contour
        cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
        if (largestIndex >= 0)
            cv::drawContours(mask, contours, largestIndex, 255, cv::FILLED);

        return mask;
    }
};


#endif //BANDINGCORRECTION_H
