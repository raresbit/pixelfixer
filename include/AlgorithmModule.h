//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef ALGORITHMMODULE_H
#define ALGORITHMMODULE_H

#pragma once
#include "Canvas.h"
#include <string>

class AlgorithmModule {
public:
    virtual ~AlgorithmModule() = default;
    explicit AlgorithmModule(Canvas& canvasToSet) : canvas(canvasToSet) {}

    [[nodiscard]] virtual std::string name() const = 0;

    [[nodiscard]] Canvas& getCanvas() const {
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
        getCanvas().clearProcessedPixels();
    }
private:
    Canvas& canvas;
};


#endif //ALGORITHMMODULE_H
