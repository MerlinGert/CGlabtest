#pragma once

#include <glm/glm.hpp>
#include <string>
#include <array>
#include "Colour.h"
#include "TexturePoint.h"

struct ModelTriangle {
    std::array<glm::vec3, 3> vertices{};
	std::array<TexturePoint, 3> texturePoints{};
	Colour colour{};
	glm::vec3 normal{};
    bool isMirror = false;
    bool isMetal  = false;
	bool isGlass  = false;
    bool hasTexture;
    float reflectivity;
    float roughness;
    glm::vec3 metalColor;
    float refractiveIndex;


    std::array<Colour, 3> vertexColours{};


	ModelTriangle();
    ModelTriangle(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2, Colour trigColour);
	friend std::ostream &operator<<(std::ostream &os, const ModelTriangle &triangle);
};
