//
// Created by Rareș Biteș on 29.04.2025.
//

#ifndef PIXELARTPROCESSING_H
#define PIXELARTPROCESSING_H

#include "Pixel.h"
#include <vector>

/**
 * Namespace containing algorithms for processing pixel art.
 */
namespace pixproc {
    /**
     * Apply "pixel-perfect" optimization to the input pixel path (Han, 2018).
     *
     * Based on: Han, R. (2018, November 22). Pixel art algorithm: Pixel-perfect.
     * RickyHan.com. https://rickyhan.com/jekyll/update/2018/11/22/pixel-art-algorithm-pixel-perfect.html
     *
     * @param path A vector of Pixel to process.
     * @return processed pixel path
     */
    std::vector<Pixel> pixelPerfect(const std::vector<Pixel> &path);
} // namespace pixproc

#endif // PIXELARTPROCESSING_H
