//
// Created by Rareș Biteș on 29.04.2025.
//

#include "../include/pixproc.h"

namespace pixproc {

    std::vector<Pixel> pixelPerfect(const std::vector<Pixel>& path) {
        if (path.size() <= 1) {
            return path;
        }

        std::vector<Pixel> ret;
        size_t c = 0;

        while (c < path.size()) {
            if (
                c > 0 && c + 1 < path.size() &&
                (path[c - 1].x == path[c].x || path[c - 1].y == path[c].y) &&
                (path[c + 1].x == path[c].x || path[c + 1].y == path[c].y) &&
                (path[c - 1].x != path[c + 1].x) &&
                (path[c - 1].y != path[c + 1].y)
            ) {
                c += 1;
            }

            ret.push_back(path[c]);
            c += 1;
        }

        return ret;
    }

} // namespace pixproc
