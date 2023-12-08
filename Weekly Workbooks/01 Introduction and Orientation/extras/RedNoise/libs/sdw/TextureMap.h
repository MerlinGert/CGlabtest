#pragma once

#include <iostream>
#include <fstream>
#include <stdexcept>
#include "Utils.h"
//
//class TextureMap {
//public:
//	size_t width;
//	size_t height;
//	std::vector<uint32_t> pixels;
//
//	TextureMap();
//	TextureMap(const std::string &filename);
//	friend std::ostream &operator<<(std::ostream &os, const TextureMap &point);
//
//
//
//};
//

class TextureMap {
public:
    size_t width;
    size_t height;
    std::vector<uint32_t> pixels;

    TextureMap();
    TextureMap(const std::string &filename);

    uint32_t getColourAt(float x, float y) {
        int ix = static_cast<int>(x * width);
        int iy = static_cast<int>(y * height);
        ix = std::min(std::max(ix, 0), static_cast<int>(width) - 1);
        iy = std::min(std::max(iy, 0), static_cast<int>(height) - 1);

        return pixels[iy * width + ix];
    }

    friend std::ostream &operator<<(std::ostream &os, const TextureMap &textureMap);
};
