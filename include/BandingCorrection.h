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

class BandingCorrection final : public AlgorithmModule {
public:
    explicit BandingCorrection(Canvas &canvas) : AlgorithmModule(canvas) {
    }

    [[nodiscard]] std::string name() const override {
        return "Banding Correction";
    }

    void run() override {

    }

    void renderDebugUI() override {
        ImGui::Checkbox("Show (ensure to first \"Run\")", &debugEnabled);

        if (debugEnabled) {

        } else {
            restoreOriginalCanvas();
        }
    }

    void reset() override {

    }

private:
    bool debugEnabled = false;

    void restoreOriginalCanvas() const {
        getCanvas().clearDebugPixels();
    }

    void highlightSelectedPixels() const {

    }
};


#endif //BANDINGCORRECTION_H
