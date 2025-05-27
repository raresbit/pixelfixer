//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef DITHERING_H
#define DITHERING_H

#pragma once
#include "Algorithm.h"
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

#include "BandingDetection.h"

class Dithering final : public Algorithm {
public:
    explicit Dithering(Canvas &canvas) : Algorithm(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Dithering";
    }

    void run() override {
        Canvas &canvas = getCanvas();
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        std::vector<std::pair<Color, cv::Mat>> layers = extract_layers(canvas);
        if (layers.size() < 2) return;

        // Create the gradient canvas (original image for dithering)
        createGradientCanvas(width, height, layers, gradientCanvas);

        // Run Floyd-Steinberg dithering with gradientCanvas as original, canvas as quantized
        Canvas ditheredCanvas = floydSteinbergDither(gradientCanvas, canvas);

        // Collect pixels that changed from original to dithered
        canvas.setProcessedPixels(ditheredCanvas);

    }



    void reset() override {
        Algorithm::reset();
    }

    void renderDebugUI() override {
        ImGui::Separator();
        ImGui::Checkbox("Debug View", &showDebug);

        if (!showDebug) {
            getCanvas().clearDebugPixels(); // Clear if debug turned off
            return;
        }

        getCanvas().setDebugPixels(gradientCanvas);
    }

private:
    Canvas originalCanvas = Canvas(0, 0);
    Canvas gradientCanvas = Canvas(0, 0);
    bool showDebug = false;


    static Canvas floydSteinbergDither(const Canvas& original, const Canvas& quantized) {
    int width = original.getWidth();
    int height = original.getHeight();

    Canvas resultCanvas(width, height);
    resultCanvas.fill({255, 255, 255}); // or transparent/black depending on your background

    // Extract unique quantized colors to define the palette
    std::unordered_set<Color> paletteSet;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            paletteSet.insert(quantized.getPixel({x, y}).color);

    std::vector<Color> palette(paletteSet.begin(), paletteSet.end());

    // Helper to find closest color in palette (Euclidean RGB distance)
    auto findClosestColor = [&](const Color& c) -> Color {
        Color closest = palette[0];
        int minDist = INT_MAX;
        for (const auto& pc : palette) {
            int dr = static_cast<int>(c.r) - pc.r;
            int dg = static_cast<int>(c.g) - pc.g;
            int db = static_cast<int>(c.b) - pc.b;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < minDist) {
                minDist = dist;
                closest = pc;
            }
        }
        return closest;
    };

    // We'll store errors as float for accumulation
    std::vector<std::vector<float>> errR(height, std::vector<float>(width, 0.0f));
    std::vector<std::vector<float>> errG(height, std::vector<float>(width, 0.0f));
    std::vector<std::vector<float>> errB(height, std::vector<float>(width, 0.0f));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Color oldColor = original.getPixel({x, y}).color;

            // Add accumulated error
            float r = std::clamp(static_cast<float>(oldColor.r) + errR[y][x], 0.0f, 255.0f);
            float g = std::clamp(static_cast<float>(oldColor.g) + errG[y][x], 0.0f, 255.0f);
            float b = std::clamp(static_cast<float>(oldColor.b) + errB[y][x], 0.0f, 255.0f);

            Color newColor = findClosestColor({static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)});
            resultCanvas.setPixel({x, y}, newColor);

            // Calculate quantization error
            float errRed = r - newColor.r;
            float errGreen = g - newColor.g;
            float errBlue = b - newColor.b;

            // Distribute the error using Floyd-Steinberg weights:
            //     * x+1, y     : 7/16
            //     * x-1, y+1   : 3/16
            //     * x,   y+1   : 5/16
            //     * x+1, y+1   : 1/16

            if (x + 1 < width) {
                errR[y][x + 1] += errRed * 7.0f / 16.0f;
                errG[y][x + 1] += errGreen * 7.0f / 16.0f;
                errB[y][x + 1] += errBlue * 7.0f / 16.0f;
            }
            if (x > 0 && y + 1 < height) {
                errR[y + 1][x - 1] += errRed * 3.0f / 16.0f;
                errG[y + 1][x - 1] += errGreen * 3.0f / 16.0f;
                errB[y + 1][x - 1] += errBlue * 3.0f / 16.0f;
            }
            if (y + 1 < height) {
                errR[y + 1][x] += errRed * 5.0f / 16.0f;
                errG[y + 1][x] += errGreen * 5.0f / 16.0f;
                errB[y + 1][x] += errBlue * 5.0f / 16.0f;
            }
            if (x + 1 < width && y + 1 < height) {
                errR[y + 1][x + 1] += errRed * 1.0f / 16.0f;
                errG[y + 1][x + 1] += errGreen * 1.0f / 16.0f;
                errB[y + 1][x + 1] += errBlue * 1.0f / 16.0f;
            }
        }
    }

    return resultCanvas;
}




    void createGradientCanvas(int width, int height, std::vector<std::pair<Color, cv::Mat> > layers,
                              Canvas &correctedCanvas) {
        correctedCanvas = Canvas(width, height);
        correctedCanvas.fill({255, 255, 255});

        auto blend = [](const Color &a, const Color &b, float alpha) -> Color {
            return {
                static_cast<uint8_t>(a.r * (1 - alpha) + b.r * alpha),
                static_cast<uint8_t>(a.g * (1 - alpha) + b.g * alpha),
                static_cast<uint8_t>(a.b * (1 - alpha) + b.b * alpha)
            };
        };

        // Fill in the subject outline and its first layer by default on this new canvas.
        auto &[color, currentMask] = layers[0];
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (currentMask.at<uchar>(y, x))
                    correctedCanvas.setPixel({x, y}, color);


        cv::Mat previousMask = layers[0].second.clone();
        for (size_t i = 1; i < layers.size(); ++i) {
            const auto &[currentColor, currentMask] = layers[i];
            std::vector<cv::Mat> sublayers = extractSublayers(currentMask, previousMask);

            size_t numSublayers = sublayers.size();

            if (numSublayers == 1) {
                const cv::Mat &sublayer = sublayers[0];
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        if (sublayer.at<uchar>(y, x)) {
                            correctedCanvas.setPixel({x, y}, currentColor);
                        }
                    }
                }
            } else {
                Color prevColor = layers[i - 1].first;
                Color curColor = layers[i].first;

                for (size_t j = 0; j < numSublayers / 2; ++j) {
                    Color interpolated = blend(prevColor, curColor, static_cast<float>(2 * j + 1) / (numSublayers));

                    const cv::Mat &sublayer = sublayers[j];
                    for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                            if (sublayer.at<uchar>(y, x)) {
                                correctedCanvas.setPixel({x, y}, interpolated);
                            }
                        }
                    }
                }

                Color nextColor = (i + 1 < layers.size()) ? layers[i + 1].first : curColor;
                for (size_t j = numSublayers / 2; j <= numSublayers - 1; ++j) {
                    Color interpolated = blend(curColor, nextColor,
                                               static_cast<float>(2 * j - numSublayers + 1) / (numSublayers));

                    const cv::Mat &sublayer = sublayers[j];
                    for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                            if (sublayer.at<uchar>(y, x)) {
                                correctedCanvas.setPixel({x, y}, interpolated);
                            }
                        }
                    }
                }
            }

            previousMask = currentMask.clone();
        }
    }

    static std::vector<cv::Mat> extractSublayers(const cv::Mat &currentMask, const cv::Mat &previousLayer) {
        std::vector<cv::Mat> sublayers;
        if (cv::countNonZero(currentMask) == 0)
            return sublayers;

        const int h = currentMask.rows;
        const int w = currentMask.cols;

        cv::Mat current = currentMask.clone();
        cv::Mat accumulated = previousLayer.clone(); // start with previous layer

        while (cv::countNonZero(current) > 0) {
            cv::Mat sublayer = cv::Mat::zeros(h, w, CV_8UC1);

            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (current.at<uchar>(y, x) == 0)
                        continue;

                    bool touchesPrevious = false;

                    if (x > 0 && accumulated.at<uchar>(y, x - 1)) touchesPrevious = true;
                    else if (x < w - 1 && accumulated.at<uchar>(y, x + 1)) touchesPrevious = true;
                    else if (y > 0 && accumulated.at<uchar>(y - 1, x)) touchesPrevious = true;
                    else if (y < h - 1 && accumulated.at<uchar>(y + 1, x)) touchesPrevious = true;

                    if (touchesPrevious)
                        sublayer.at<uchar>(y, x) = 255;
                }
            }

            if (cv::countNonZero(sublayer) == 0)
                break;

            sublayers.push_back(sublayer);
            accumulated |= sublayer; // grow the boundary with the new sublayer
            current.setTo(0, sublayer); // subtract sublayer from current
        }

        return sublayers;
    }


    static std::vector<std::pair<Color, cv::Mat> > extract_layers(const Canvas &canvas) {
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

        // Post-process to remove overlaps: subtract previous masks
        for (size_t i = 0; i < layers.size() - 1; ++i) {
            cv::Mat &currentMask = layers[i].second;
            cv::Mat &nextMask = layers[i + 1].second;
            currentMask.setTo(0, nextMask);
        }

        return layers;
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

#endif // DITHERING_H
