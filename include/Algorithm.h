//
// Created by Rareș Biteș on 30.04.2025.
//

#ifndef ALGORITHM_H
#define ALGORITHM_H

#pragma once
#include "Canvas.h"
#include <string>

class Algorithm {
public:
    virtual ~Algorithm() = default;
    explicit Algorithm(Canvas& canvasToSet) : canvas(canvasToSet) {}

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
        getCanvas().clearDebugPixels();
    }
private:
    Canvas& canvas;
};


#endif //ALGORITHM_H
