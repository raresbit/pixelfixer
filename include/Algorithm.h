//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef ALGORITHM_H
#define ALGORITHM_H

#pragma once
#include "PixelArtImage.h"
#include <string>

class Algorithm {
public:
    virtual ~Algorithm() = default;
    explicit Algorithm(PixelArtImage& canvasToSet) : canvas(canvasToSet) {}

    [[nodiscard]] virtual std::string name() const = 0;

    [[nodiscard]] PixelArtImage& getPixelArtImage() const {
        return this->canvas;
    }
    virtual void run() = 0;
    virtual void renderUI() {
        ImGui::Text("No options available.");
    }
    virtual void renderDebugUI() {
        ImGui::Text("Not implemented.");
    }
    virtual void reset() {
        getPixelArtImage().clearProcessedPixels();
        getPixelArtImage().clearDebugPixels();
    }
private:
    PixelArtImage& canvas;
};


#endif //ALGORITHM_H
