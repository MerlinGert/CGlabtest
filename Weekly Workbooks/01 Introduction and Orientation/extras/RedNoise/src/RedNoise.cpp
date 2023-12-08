#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <CanvasPoint.h>
#include <Colour.h>
#include <algorithm>
#include <map>
#include "TextureMap.h"
#include "ModelTriangle.h"
#include "RayTriangleIntersection.h"
#include <cmath>
#include <numeric>


#define WIDTH 320
#define HEIGHT 240



std::vector<glm::vec3> interpolateThreeElementValues(glm::vec3 from, glm::vec3 to, int numberOfValues) {
    std::vector<glm::vec3> interpolatedValues;
    if (numberOfValues <= 1) return interpolatedValues;  // Edge case
    glm::vec3 step = (to - from) / float(numberOfValues - 1);
    for (int i = 0; i < numberOfValues; i++) {
        interpolatedValues.push_back(from + step * float(i));
    }
    return interpolatedValues;
}



void drawLine(DrawingWindow &window, const CanvasPoint &p1, const CanvasPoint &p2, const Colour &color) {
    int steps = std::max(abs(p2.x - p1.x), abs(p2.y - p1.y));
    auto points = interpolateThreeElementValues(glm::vec3(p1.x, p1.y, 0), glm::vec3(p2.x, p2.y, 0), steps + 1);

    uint32_t pixelColor = (255 << 24) + (color.red << 16) + (color.green << 8) + color.blue;

    for (const auto &point : points) {
        window.setPixelColour(round(point.x), round(point.y), pixelColor);
    }
}


void drawTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &color) {
    drawLine(window, triangle.vertices[0], triangle.vertices[1], color);
    drawLine(window, triangle.vertices[1], triangle.vertices[2], color);
    drawLine(window, triangle.vertices[0], triangle.vertices[2], color);
}


void drawTexturedLine(DrawingWindow &window, const CanvasPoint &start, const CanvasPoint &end, const TextureMap &texture) {
//    int steps = std::max(abs(end.x - start.x), abs(end.y - start.y));
//    float xStep = (end.x - start.x) / steps;
//    float yStep = (end.y - start.y) / steps;
    float deltaX = abs(end.x - start.x);
    float deltaY = abs(end.y - start.y);
    int steps = std::max(deltaX, deltaY);
    float xStep = deltaX / steps;
    float yStep = deltaY / steps;
    float uStep = (end.texturePoint.x - start.texturePoint.x) / steps;
    float vStep = (end.texturePoint.y - start.texturePoint.y) / steps;

    float x = start.x;
    float y = start.y;
    float u = start.texturePoint.x;
    float v = start.texturePoint.y;


    for (int i = 0; i <= steps; i++) {
        if (u < 0 || u >= texture.width || v < 0 || v >= texture.height) {
            std::cerr << "Texture coordinates out of bounds: u = " << u << ", v = " << v << std::endl;
            continue;
        }
        if (u >= 0 && u < texture.width && v >= 0 && v < texture.height) {
            uint32_t color = texture.pixels[int(v) * texture.width + int(u)];
            window.setPixelColour(round(x), round(y), color);
        } else {
            window.setPixelColour(round(x), round(y), 0);
        }

        x += xStep;
        y += yStep;
        u += uStep;
        v += vStep;
    }






}

std::array<CanvasPoint, 3> getSortedVertices(const CanvasTriangle &triangle) {
    std::array<CanvasPoint, 3> sortedVertices = {triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]};
    std::sort(sortedVertices.begin(), sortedVertices.end(), [](const CanvasPoint &a, const CanvasPoint &b) -> bool {
        if (a.y == b.y) {
            return a.x < b.x;
        }
        return a.y < b.y;
    });
    return sortedVertices;
}

    void fillTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &color) {
        std::array<CanvasPoint, 3> sortedVertices = getSortedVertices(triangle);

        float hypotenuseSlope = (sortedVertices[2].x - sortedVertices[0].x) / (sortedVertices[2].y - sortedVertices[0].y);

        int totalSteps = sortedVertices[2].y - sortedVertices[0].y;

        for (int i = 0; i <= totalSteps; i++) {
            int y = sortedVertices[0].y + i;


            int startX = std::round(sortedVertices[0].x + hypotenuseSlope * i);
            int endX;

            if (y < sortedVertices[1].y) {
                float slopeTop = (sortedVertices[1].x - sortedVertices[0].x) / (sortedVertices[1].y - sortedVertices[0].y);
                endX = std::round(sortedVertices[0].x + slopeTop * i);
            } else {
                float slopeBottom =
                        (sortedVertices[2].x - sortedVertices[1].x) / (sortedVertices[2].y - sortedVertices[1].y);
                endX = std::round(sortedVertices[1].x + slopeBottom * (i - (sortedVertices[1].y - sortedVertices[0].y)));
            }
            if (startX > endX) std::swap(startX, endX);
            drawLine(window, CanvasPoint(startX, y), CanvasPoint(endX, y), color);
        }

    }


glm::vec3 calculateTriangleNormal(const ModelTriangle &triangle) {
    glm::vec3 edge1 = triangle.vertices[1] - triangle.vertices[0];
    glm::vec3 edge2 = triangle.vertices[2] - triangle.vertices[0];
    return glm::normalize(glm::cross(edge1, edge2));
}


std::vector<ModelTriangle> loadOBJ(const std::string &filename, const std::map<std::string, Colour>& palette) {
    std::vector<ModelTriangle> triangles;
    std::vector<glm::vec3> vertices;
    std::vector<TexturePoint> texturePoints;
    Colour currentColour;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file: " << filename << std::endl;
        return triangles;
    }

    std::string line;
    std::string currentMaterial;

    while (std::getline(file, line)) {
        std::vector<std::string> tokens = split(line, ' ');
        if (tokens.empty()) continue;

        if (tokens[0] == "v") {
            vertices.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
        } else if (tokens[0] == "vt") {
            texturePoints.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]));
        } else if (tokens[0] == "usemtl") {
            currentMaterial = tokens[1];
            currentColour = palette.at(currentMaterial);
        } else if (tokens[0] == "f") {
            std::array<glm::vec3, 3> faceVertices;
            std::array<TexturePoint, 3> faceTexturePoints;

            for (int i = 0; i < 3; i++) {
                std::vector<std::string> faceTokens = split(tokens[i + 1], '/');
                int vertexIndex = std::stoi(faceTokens[0]) - 1;
                faceVertices[i] = vertices[vertexIndex];

                if (faceTokens.size() > 1 && !faceTokens[1].empty()) {
                    int textureIndex = std::stoi(faceTokens[1]) - 1;
                    faceTexturePoints[i] = texturePoints[textureIndex];
                }
            }

            glm::vec3 normal = glm::normalize(glm::cross(faceVertices[1] - faceVertices[0], faceVertices[2] - faceVertices[0]));
            ModelTriangle triangle(faceVertices[0], faceVertices[1], faceVertices[2], currentColour);
            triangle.texturePoints = faceTexturePoints;
            triangle.normal = calculateTriangleNormal(triangle);
            triangles.push_back(triangle);
        }
    }

    file.close();
    return triangles;
}

std::vector<ModelTriangle> loadOBJWithTexture(const std::string &filename, const std::map<std::string, Colour>& palette) {
    std::vector<ModelTriangle> triangles;
    std::vector<glm::vec3> vertices;
    std::vector<TexturePoint> texturePoints;
    Colour currentColour;
    bool isMirror = false;
    bool isMetal = false;
    bool isGlass  = false;
    float reflectivity;
    float refractiveIndex;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file: " << filename << std::endl;
        return triangles;
    }

    std::string line;
    std::string currentMaterial;

    while (std::getline(file, line)) {
        std::vector<std::string> tokens = split(line, ' ');
        if (tokens.empty()) continue;

        if (tokens[0] == "v") {
            vertices.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
        } else if (tokens[0] == "vt") {
            texturePoints.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]));
        } else if (tokens[0] == "usemtl") {
            currentMaterial = tokens[1];
            if (palette.find(currentMaterial) != palette.end()) {
                currentColour = palette.at(currentMaterial);
                isMirror = (currentMaterial == "Mirror");
                isMetal = (currentMaterial == "Metal");
                isGlass = (currentMaterial == "Glass");

                if (isMetal) {
                reflectivity = 1.3f;
                 }
                if (isGlass) {
                    refractiveIndex = 1.2f;
                }
            }
            else {
                std::cerr << "Material '" << currentMaterial << "' not found in palette." << std::endl;
                currentColour = Colour(255, 255, 255);
        } }else if (tokens[0] == "f") {
            std::array<glm::vec3, 3> faceVertices;
            std::array<TexturePoint, 3> faceTexturePoints;
            bool hasTexture = true;
            for (int i = 0; i < 3; i++) {
                std::vector<std::string> faceTokens = split(tokens[i + 1], '/');
                int vertexIndex = std::stoi(faceTokens[0]) - 1;
                faceVertices[i] = vertices[vertexIndex];

                if (faceTokens.size() > 1 && !faceTokens[1].empty()) {
                    int textureIndex = std::stoi(faceTokens[1]) - 1;
                    faceTexturePoints[i] = texturePoints[textureIndex];
                }else {
                    hasTexture = false;
                }
            }

            glm::vec3 normal = glm::normalize(glm::cross(faceVertices[1] - faceVertices[0], faceVertices[2] - faceVertices[0]));
            ModelTriangle triangle(faceVertices[0], faceVertices[1], faceVertices[2], currentColour);
            triangle.texturePoints = faceTexturePoints;
            triangle.normal = normal;
            triangle.isMirror = isMirror;
            triangle.isMetal = isMetal;
            triangle.hasTexture = hasTexture;
            triangle.isGlass = isGlass;
            triangle.refractiveIndex = refractiveIndex;
            triangles.push_back(triangle);
            if (currentMaterial == "Metal") {
                triangle.metalColor = glm::vec3(1.0f, 0.843f, 0.0f);
                triangle.reflectivity = 0.8f;
                triangle.roughness = 0.03f;
            }
        }
    }

    file.close();
    return triangles;
}



std::map<std::string, Colour> loadMTL(const std::string& filename) {
    std::map<std::string, Colour> palette;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open the MTL file: " << filename << std::endl;
        return palette;
    }

    std::string currentMaterial;
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "newmtl") {
            currentMaterial = line.substr(7);
        }
        else if (line.substr(0, 2) == "Kd" && !currentMaterial.empty()) {
            size_t pos1 = line.find(" ") + 1;
            size_t pos2 = line.find(" ", pos1) + 1;
            size_t pos3 = line.find(" ", pos2) + 1;

            float r = std::stof(line.substr(pos1, pos2 - pos1 - 1));
            float g = std::stof(line.substr(pos2, pos3 - pos2 - 1));
            float b = std::stof(line.substr(pos3));

            palette[currentMaterial] = Colour(r * 255, g * 255, b * 255);
        }
    }

    return palette;
}

struct Material {
    Colour colour;
    std::string texturePath;
};




float interpolate(float from, float to, float alpha) {
    return from + alpha * (to - from);
}

void drawLineWithDepth(DrawingWindow &window, const CanvasPoint &start, const CanvasPoint &end, const Colour &color, std::vector<std::vector<float>>& depthBuffer) {
    int deltaX = end.x - start.x;
    int deltaY = end.y - start.y;
    int numberOfSteps = std::max(std::abs(deltaX), std::abs(deltaY));
    float xIncrement = static_cast<float>(deltaX) / numberOfSteps;
    float yIncrement = static_cast<float>(deltaY) / numberOfSteps;

    float currentX = start.x;
    float currentY = start.y;

    for (int i = 0; i <= numberOfSteps; i++) {
        float alpha = static_cast<float>(i) / numberOfSteps;

        float depth = 1.0f / interpolate(1.0f / start.depth, 1.0f / end.depth, alpha);

        int x = static_cast<int>(currentX);
        int y = static_cast<int>(currentY);

        if (x >= 0 && x < depthBuffer.size() && y >= 0 && y < depthBuffer[0].size() && depth < depthBuffer[x][y]) {
            uint32_t packedColor = (color.red << 16) | (color.green << 8) | color.blue;
            window.setPixelColour(x, y, packedColor);
            depthBuffer[x][y] = depth;
        }

        currentX += xIncrement;
        currentY += yIncrement;
    }
}


void fillTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &color, std::vector<std::vector<float>>& depthBuffer) {
    std::array<CanvasPoint, 3> sortedVertices = getSortedVertices(triangle);

        auto computeIntersection = [](const CanvasPoint &a, const CanvasPoint &b, float y) {
            if (std::abs(b.y - a.y) < 1e-5) {

                return CanvasPoint(a.x, y,abs(a.depth));
            } else {
                float slope = (y - a.y) / (b.y - a.y);
                float x = a.x + slope * (b.x - a.x);
                float depth = std::abs(a.depth + slope * (b.depth - a.depth));
                return CanvasPoint(x, y, depth);
            };
        };

    for (float y = std::round(sortedVertices[0].y); y <= sortedVertices[1].y; y += 1.0f) {
        int yInt = std::round(y);
        CanvasPoint start = computeIntersection(sortedVertices[0], sortedVertices[2], yInt);
        CanvasPoint end = computeIntersection(sortedVertices[0], sortedVertices[1], yInt);


        start.x = std::max(std::min(start.x, std::max(sortedVertices[0].x, sortedVertices[2].x)), std::min(sortedVertices[0].x, sortedVertices[2].x));
        end.x = std::max(std::min(end.x, std::max(sortedVertices[0].x, sortedVertices[1].x)), std::min(sortedVertices[0].x, sortedVertices[1].x));

        if (start.x > end.x) std::swap(start, end);
        if (start.x != end.x) {
            drawLineWithDepth(window, start, end, color, depthBuffer);
        }
    }


    for (float y = std::round(sortedVertices[1].y); y <= sortedVertices[2].y; y += 1.0f) {
        int yInt = std::round(y);
        CanvasPoint start = computeIntersection(sortedVertices[1], sortedVertices[2], yInt);
        CanvasPoint end = computeIntersection(sortedVertices[0], sortedVertices[2], yInt);

        start.x = std::max(std::min(start.x, std::max(sortedVertices[1].x, sortedVertices[2].x)), std::min(sortedVertices[1].x, sortedVertices[2].x));
        end.x = std::max(std::min(end.x, std::max(sortedVertices[0].x, sortedVertices[2].x)), std::min(sortedVertices[0].x, sortedVertices[2].x));

        if (start.x > end.x) std::swap(start, end);
        drawLineWithDepth(window, start, end, color, depthBuffer);
    }

}




void drawTexLineWithDepth(DrawingWindow &window, const CanvasPoint &start, const CanvasPoint &end, TextureMap &textureMap, std::vector<std::vector<float>>& depthBuffer) {
    int deltaX = end.x - start.x;
    int deltaY = end.y - start.y;
    int numberOfSteps = std::max(std::abs(deltaX), std::abs(deltaY));
    float xIncrement = static_cast<float>(deltaX) / numberOfSteps;
    float yIncrement = static_cast<float>(deltaY) / numberOfSteps;

    float currentX = start.x;
    float currentY = start.y;

    for (int i = 0; i <= numberOfSteps; i++) {
        int x = static_cast<int>(currentX);
        int y = static_cast<int>(currentY);

        if (x >= 0 && x < window.width && y >= 0 && y < window.height) {
            float alpha = static_cast<float>(i) / numberOfSteps;


            float textureX = start.texturePoint.x + alpha * (end.texturePoint.x - start.texturePoint.x);
            float textureY = start.texturePoint.y + alpha * (end.texturePoint.y - start.texturePoint.y);


            uint32_t color = textureMap.getColourAt(textureX, textureY);


            window.setPixelColour(x, y, color);
        }

        currentX += xIncrement;
        currentY += yIncrement;
    }
}



void fillTexturedTriangle(DrawingWindow &window, const CanvasTriangle &triangle, TextureMap &textureMap, std::vector<std::vector<float>>& depthBuffer) {
    std::array<CanvasPoint, 3> sortedVertices = getSortedVertices(triangle);

    auto computeIntersection = [](const CanvasPoint &a, const CanvasPoint &b, float y) -> CanvasPoint {
        float alpha = (y - a.y) / (b.y - a.y);
        float x = a.x + alpha * (b.x - a.x);
        float u = a.texturePoint.x + alpha * (b.texturePoint.x - a.texturePoint.x);
        float v = a.texturePoint.y + alpha * (b.texturePoint.y - a.texturePoint.y);

        CanvasPoint resultPoint(x, y);
        resultPoint.texturePoint = TexturePoint(u, v);

        return resultPoint;
    };


    for (int y = static_cast<int>(sortedVertices[0].y); y < static_cast<int>(sortedVertices[1].y); y++) {
        CanvasPoint start = computeIntersection(sortedVertices[0], sortedVertices[2], y);
        CanvasPoint end = computeIntersection(sortedVertices[0], sortedVertices[1], y);

        if(start.x > end.x) {
            std::swap(start, end);
        }

        drawTexLineWithDepth(window, start, end, textureMap,depthBuffer);
    }

    for (int y = static_cast<int>(sortedVertices[1].y); y <= static_cast<int>(sortedVertices[2].y); y++) {
        CanvasPoint start = computeIntersection(sortedVertices[0], sortedVertices[2], y);
        CanvasPoint end = computeIntersection(sortedVertices[1], sortedVertices[2], y);

        if(start.x > end.x) {
            std::swap(start, end);
        }

        drawTexLineWithDepth(window, start, end, textureMap,depthBuffer);
    }

}



glm::mat3 rotationY(float angle) {
    return glm::mat3(
            cos(angle), 0.0f, sin(angle),
            0.0f, 1.0f, 0.0f,
            -sin(angle), 0.0f, cos(angle)
    );
}

glm::mat3 rotationX (float angle) {
    return glm::mat3(
            1.0f, 0.0f, 0.0f,
            0.0f, cos(angle), -sin(angle),
            0.0f, sin(angle), cos(angle)
    );
}



glm::mat3 cameraOrientation = glm::mat3(
        1.0f, 0.0f, 0.0f,  // Right
        0.0f, 1.0f, 0.0f,  // Up
        0.0f, 0.0f, 1.0f   // Forward
);




CanvasPoint getCanvasIntersectionPoint(const glm::vec3& cameraPosition, const glm::vec3& vertexPosition, float focalLength, int screenWidth, int screenHeight) {
    glm::vec3 relativeVertex = vertexPosition - cameraPosition;
    relativeVertex = cameraOrientation * relativeVertex;

    float scaleFactor = 240.0f;

    float x = -scaleFactor * (focalLength * (relativeVertex.x / relativeVertex.z)) + (screenWidth / 2);
    float y = scaleFactor * (focalLength * (relativeVertex.y / relativeVertex.z)) + (screenHeight / 2);
//    std::cout << "Projected point: (" << x << ", " << y << ")" << std::endl;


    return CanvasPoint(x, y, relativeVertex.z);
}


void animateCameraOrbit(glm::vec3 &cameraPosition) {
    float rotationAmount = glm::radians(0.01f);
    glm::mat3 rotation = rotationY(rotationAmount);
    cameraPosition = rotation * cameraPosition;
    glm::mat3 rotationMatriY = rotationY(-rotationAmount);
    cameraOrientation= rotationMatriY * cameraOrientation;
}

void lookAt(glm::vec3 &cameraPosition) {
    glm::vec3 lookAtTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 forward = glm::normalize(lookAtTarget - cameraPosition);
    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(upVector, forward));
    glm::vec3 up = glm::cross(forward, right);
    cameraOrientation = glm::mat3(-right, up, -forward);
}

bool isOrbiting = false;
void toggleOrbit() {
    isOrbiting = !isOrbiting;
}


void drawTexturedLineWithDepth(DrawingWindow &window, const CanvasPoint &start, const CanvasPoint &end, const TextureMap &texture, std::vector<std::vector<float>> &depthBuffer) {
    float deltaX = abs(end.x - start.x);
    float deltaY = abs(end.y - start.y);
    int steps = std::max(deltaX, deltaY);
    float xStep = deltaX / steps;
    float yStep = deltaY / steps;
    float uStep = (end.texturePoint.x - start.texturePoint.x) / steps;
    float vStep = (end.texturePoint.y - start.texturePoint.y) / steps;
    float depthStep = (end.depth - start.depth) / steps;

    float x = start.x;
    float y = start.y;
    float u = start.texturePoint.x;
    float v = start.texturePoint.y;
    float depth = start.depth;

    for (int i = 0; i <= steps; i++) {
        int xInt = round(x);
        int yInt = round(y);

        if (xInt < 0 || xInt >= window.width || yInt < 0 || yInt >= window.height) {
            x += xStep;
            y += yStep;
            u += uStep;
            v += vStep;
            depth += depthStep;
            continue;
        }


        if (depth < depthBuffer[xInt][yInt]) {
            depthBuffer[xInt][yInt] = depth;

            if (u >= 0 && u < texture.width && v >= 0 && v < texture.height) {
                uint32_t color = texture.pixels[int(v) * texture.width + int(u)];
                window.setPixelColour(xInt, yInt, color);
            } else {

            }
        }

        x += xStep;
        y += yStep;
        u += uStep;
        v += vStep;
        depth += depthStep;
    }
}


void fillTexturedTriangleWithDepth(DrawingWindow &window, const CanvasTriangle &triangle, const TextureMap &texture,std::vector<std::vector<float>> &depthBuffer) {
    std::array<CanvasPoint, 3> sortedVertices = getSortedVertices(triangle);
    auto computeIntersection = [](const CanvasPoint &a, const CanvasPoint &b, float y) -> CanvasPoint {
        float alpha = (y - a.y) / (b.y - a.y);
        float x = a.x + alpha * (b.x - a.x);
        float u = a.texturePoint.x + alpha * (b.texturePoint.x - a.texturePoint.x);
        float v = a.texturePoint.y + alpha * (b.texturePoint.y - a.texturePoint.y);
        float depth =std::abs(a.depth + alpha * (b.depth - a.depth));


        CanvasPoint resultPoint(x, y);
        resultPoint.texturePoint = TexturePoint(u, v);
        resultPoint.depth = depth;

        return resultPoint;
    };

    for (int y = static_cast<int>(sortedVertices[0].y); y < static_cast<int>(sortedVertices[1].y); y++) {
        CanvasPoint start = computeIntersection(sortedVertices[0], sortedVertices[2], y);
        CanvasPoint end = computeIntersection(sortedVertices[0], sortedVertices[1], y);

        if (start.x > end.x) {
            std::swap(start, end);
        }

        drawTexturedLine(window, start, end, texture);
    }

    for (int y = static_cast<int>(sortedVertices[1].y); y <= static_cast<int>(sortedVertices[2].y); y++) {
        CanvasPoint start = computeIntersection(sortedVertices[0], sortedVertices[2], y);
        CanvasPoint end = computeIntersection(sortedVertices[1], sortedVertices[2], y);

        if (start.x > end.x) {
            std::swap(start, end);
        }

        drawTexturedLineWithDepth(window, start, end, texture,depthBuffer);
    }


}


glm::vec3 getRayDirectionFromPixel(int x, int y, int screenWidth, int screenHeight, float focalLength, const glm::vec3& cameraPosition) {

    glm::vec3 Point;
    float scaleFactor = 60.0f;
    Point.x = (x - screenWidth / 2) /(focalLength * scaleFactor);
    Point.y = -(y - screenHeight / 2) / (focalLength * scaleFactor);
    Point.z =-focalLength;
    glm::vec3 rayDirection = Point - cameraPosition;
    glm::vec3 NormalizeRayDirection = normalize(Point - cameraPosition);
    return NormalizeRayDirection;
}

RayTriangleIntersection getClosestValidIntersection(
        const glm::vec3 &rayOrigin,
        const glm::vec3 &rayDirection,
        const std::vector<ModelTriangle> &triangles
) {
    RayTriangleIntersection closestIntersection;
    float closestDistance = std::numeric_limits<float>::infinity();
    size_t closestIndex = -1; // Initialized with an invalid index

    for (size_t i = 0; i < triangles.size(); ++i) {
        const ModelTriangle& triangle = triangles[i];
        glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
        glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
        glm::vec3 SPVector = rayOrigin - triangle.vertices[0];
        glm::mat3 DEMatrix(-rayDirection, e0, e1);
        glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

        float t = possibleSolution.x;
        float u = possibleSolution.y;
        float v = possibleSolution.z;

        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f && (u + v) <= 1.0f && t > 0) {
            if (t < closestDistance) {
                closestDistance = t;
                closestIndex = i;
                closestIntersection = RayTriangleIntersection(
                        rayOrigin + rayDirection * t,
                        t,
                        triangle,
                        closestIndex // Set the triangleIndex to the current index
                );
            }
        }
    }

    if (closestDistance == std::numeric_limits<float>::infinity()) {
        return RayTriangleIntersection(glm::vec3(), std::numeric_limits<float>::infinity(), ModelTriangle(), -1);
    }

    return closestIntersection;
}

Colour Mix(const Colour& a, const Colour& b, float blend) {
    float inverseBlend = 1.0f - blend;
    return Colour(
            int(a.red * inverseBlend + b.red * blend),
            int(a.green * inverseBlend + b.green * blend),
            int(a.blue * inverseBlend + b.blue * blend)
    );
}

//AIGenerated
glm::vec3 randomInUnitSphere() {
    glm::vec3 p;
    do {
        p = 2.0f * glm::vec3(static_cast<float>(rand()) / RAND_MAX,
                             static_cast<float>(rand()) / RAND_MAX,
                             static_cast<float>(rand()) / RAND_MAX) - glm::vec3(1, 1, 1);
    } while (glm::length(p) >= 1.0f);
    return p;
}

Colour vec3ToColour(const glm::vec3& vec) {
    return Colour(static_cast<int>(vec.r * 255), static_cast<int>(vec.g * 255), static_cast<int>(vec.b * 255));
}


RayTriangleIntersection getReflectionIntersection(
        const glm::vec3 &rayOrigin,
        const glm::vec3 &rayDirection,
        const std::vector<ModelTriangle> &triangles,
        int depth = 0,
        const int maxDepth = 20
) {

    RayTriangleIntersection closestIntersection;
    closestIntersection.distanceFromCamera = std::numeric_limits<float>::infinity();
    closestIntersection.triangleIndex = -1;

    for (size_t i = 0; i < triangles.size(); ++i) {
        const ModelTriangle& triangle = triangles[i];
        glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
        glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
        glm::vec3 SPVector = rayOrigin - triangle.vertices[0];
        glm::mat3 DEMatrix(-rayDirection, e0, e1);
        glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

        float t = possibleSolution.x, u = possibleSolution.y, v = possibleSolution.z;
        float w = 1 - u - v;
        glm::vec2 textureCoords = w * glm::vec2(triangle.texturePoints[0].x, triangle.texturePoints[0].y) +
                                  u * glm::vec2(triangle.texturePoints[1].x, triangle.texturePoints[1].y) +
                                  v * glm::vec2(triangle.texturePoints[2].x, triangle.texturePoints[2].y);



        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f && (u + v) <= 1.0f && t > 0 && t < closestIntersection.distanceFromCamera) {
            closestIntersection.distanceFromCamera = t;
            closestIntersection.intersectedTriangle = triangle;
            closestIntersection.triangleIndex = i;
            closestIntersection.intersectionPoint = rayOrigin + rayDirection * t;
            closestIntersection.textureCoords=textureCoords;
        }
    }

    if (depth < maxDepth && closestIntersection.triangleIndex != -1) {
        const ModelTriangle& triangle = triangles[closestIntersection.triangleIndex];
        if (triangle.isMirror || triangle.isMetal) {
            glm::vec3 reflectionDirection = glm::reflect(rayDirection, closestIntersection.intersectedTriangle.normal);


            if (triangle.isMetal) {
                reflectionDirection += randomInUnitSphere() * triangle.roughness;
                reflectionDirection = glm::normalize(reflectionDirection);
            }

            glm::vec3 reflectionOrigin = closestIntersection.intersectionPoint + reflectionDirection * 0.001f;
            RayTriangleIntersection reflectedIntersection = getReflectionIntersection(
                    reflectionOrigin, reflectionDirection, triangles, depth + 1, maxDepth);

            if (triangle.isMetal && depth == maxDepth - 1) {
                reflectedIntersection.intersectedTriangle.colour = Mix(vec3ToColour(triangle.metalColor), reflectedIntersection.intersectedTriangle.colour, triangle.reflectivity);
            }

            return reflectedIntersection;
        }
    }

    return closestIntersection;
}


//AIGenerated
glm::vec3 ComputeRefractedRay(const glm::vec3& incident, const glm::vec3& normal, float indexOfRefraction) {
    float cosi = glm::clamp(glm::dot(incident, normal), -1.0f, 1.0f);
    float etai = 1, etat = indexOfRefraction;
    glm::vec3 n = normal;

    if (cosi < 0) {
        cosi = -cosi;
    } else {
        std::swap(etai, etat);
        n = -normal;
    }

    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);


    if (k < 0) {

        return incident - 2.0f * glm::dot(incident, n) * n;
    }


    return eta * incident + (eta * cosi - sqrt(k)) * n;
}



glm::vec3 lightposition(0,1,1);


glm::vec3 calculateScatteredDirection(const glm::vec3& normal) {

    glm::vec3 randomPoint = glm::vec3(
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
            static_cast<float>(rand()) / static_cast<float>(RAND_MAX)
    );
    randomPoint = glm::normalize(randomPoint);


    if (glm::dot(randomPoint, normal) < 0.0) {
        randomPoint = -randomPoint;
    }


    glm::vec3 scatteredDirection = glm::normalize(normal + randomPoint);
    return scatteredDirection;
}



Colour MixColours(const Colour &c1, const Colour &c2, float ratio) {
    return Colour(
            static_cast<int>(c1.red * ratio + c2.red * (1.0f - ratio)),
            static_cast<int>(c1.green * ratio + c2.green * (1.0f - ratio)),
            static_cast<int>(c1.blue * ratio + c2.blue * (1.0f - ratio))
    );
}

RayTriangleIntersection getClosestValidIntersectionWithReflection(
        const glm::vec3 &rayOrigin,
        const glm::vec3 &rayDirection,
        const std::vector<ModelTriangle> &triangles,
        int depth = 0,
        const int maxDepth = 5
) {

    RayTriangleIntersection closestIntersection;
    float closestDistance = std::numeric_limits<float>::infinity();
    size_t closestIndex = -1;


    for (size_t i = 0; i < triangles.size(); ++i) {
        const ModelTriangle& triangle = triangles[i];
        glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
        glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
        glm::vec3 SPVector = rayOrigin - triangle.vertices[0];
        glm::mat3 DEMatrix(-rayDirection, e0, e1);
        glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

        float t = possibleSolution.x;
        float u = possibleSolution.y;
        float v = possibleSolution.z;

        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f && (u + v) <= 1.0f && t > 0 && t < closestDistance) {
            closestDistance = t;
            closestIndex = i;
            closestIntersection = RayTriangleIntersection(
                    rayOrigin + rayDirection * t,
                    t,
                    triangle,
                    closestIndex
            );
        }
    }

    if (depth < maxDepth && closestIndex != -1) {
        const ModelTriangle &triangle = triangles[closestIndex];
        if (triangle.isMirror || triangle.isMetal) {
            glm::vec3 reflectionDirection = glm::reflect(rayDirection, triangle.normal);
            if (triangle.isMetal) {
                reflectionDirection += randomInUnitSphere() * triangle.roughness;
                reflectionDirection = glm::normalize(reflectionDirection);
            }
//            glm::vec3 reflectionOrigin = closestIntersection.intersectionPoint + reflectionDirection * 0.001f;
            glm::vec3 reflectionOrigin = closestIntersection.intersectionPoint + reflectionDirection * 0.001f;
            RayTriangleIntersection reflectedIntersection = getClosestValidIntersectionWithReflection(
                    reflectionOrigin, reflectionDirection, triangles, depth + 1, maxDepth);
            if (triangle.isMetal && depth == maxDepth - 1) {
                reflectedIntersection.intersectedTriangle.colour = Mix(vec3ToColour(triangle.metalColor),
                                                                       reflectedIntersection.intersectedTriangle.colour,
                                                                       triangle.reflectivity);
            }
            return reflectedIntersection;
        }
        if (triangle.isGlass) {
            glm::vec3 refractedDirection = ComputeRefractedRay(rayDirection, triangle.normal, triangle.refractiveIndex);

//          glm::vec3 refractedDirection = glm::refract(rayDirection-lightposition, triangle.normal, triangle.refractiveIndex);
            glm::vec3 refractedOrigin = closestIntersection.intersectionPoint + refractedDirection * 0.0001f;
//            glm::vec3 refractedOrigin = closestIntersection.intersectionPoint;
            RayTriangleIntersection refractedIntersection = getClosestValidIntersectionWithReflection(
                    refractedOrigin, refractedDirection, triangles, depth + 1, maxDepth);
            return refractedIntersection;
        }

        if (!triangle.isMirror && !triangle.isMetal && !triangle.isGlass) {
            glm::vec3 scatteredDirection = calculateScatteredDirection(triangle.normal);
            glm::vec3 scatteredOrigin = closestIntersection.intersectionPoint + scatteredDirection * 0.001f;
            RayTriangleIntersection scatteredIntersection = getClosestValidIntersectionWithReflection(
                    scatteredOrigin, scatteredDirection, triangles, depth + 1, maxDepth);
//            MixColours(closestIntersection.intersectedTriangle.colour, scatteredIntersection.intersectedTriangle.colour, 0.2f);
//            return scatteredIntersection;

        }

    }
    return closestIntersection;
}

RayTriangleIntersection getClosestValidIntersectionWithIndirect(
        const glm::vec3 &rayOrigin,
        const glm::vec3 &rayDirection,
        const std::vector<ModelTriangle> &triangles,
        int depth = 0,
        const int maxDepth = 2
) {

    RayTriangleIntersection closestIntersection;
    float closestDistance = std::numeric_limits<float>::infinity();
    size_t closestIndex = -1;


    for (size_t i = 0; i < triangles.size(); ++i) {
        const ModelTriangle& triangle = triangles[i];
        glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
        glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
        glm::vec3 SPVector = rayOrigin - triangle.vertices[0];
        glm::mat3 DEMatrix(-rayDirection, e0, e1);
        glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

        float t = possibleSolution.x;
        float u = possibleSolution.y;
        float v = possibleSolution.z;

        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f && (u + v) <= 1.0f && t > 0 && t < closestDistance) {
            closestDistance = t;
            closestIndex = i;
            closestIntersection = RayTriangleIntersection(
                    rayOrigin + rayDirection * t,
                    t,
                    triangle,
                    closestIndex
            );
        }
    }

    if (depth < maxDepth && closestIndex != -1) {
        const ModelTriangle &triangle = triangles[closestIndex];
        if (triangle.isMirror || triangle.isMetal) {
            glm::vec3 reflectionDirection = glm::reflect(rayDirection, triangle.normal);
            if (triangle.isMetal) {
                reflectionDirection += randomInUnitSphere() * triangle.roughness;
                reflectionDirection = glm::normalize(reflectionDirection);
            }
//            glm::vec3 reflectionOrigin = closestIntersection.intersectionPoint + reflectionDirection * 0.001f;
            glm::vec3 reflectionOrigin = closestIntersection.intersectionPoint + reflectionDirection * 0.001f;
            RayTriangleIntersection reflectedIntersection = getClosestValidIntersectionWithReflection(
                    reflectionOrigin, reflectionDirection, triangles, depth + 1, maxDepth);
            if (triangle.isMetal && depth == maxDepth - 1) {
                reflectedIntersection.intersectedTriangle.colour = Mix(vec3ToColour(triangle.metalColor),
                                                                       reflectedIntersection.intersectedTriangle.colour,
                                                                       triangle.reflectivity);
            }
            return reflectedIntersection;
        }
        if (triangle.isGlass) {
            glm::vec3 refractedDirection = ComputeRefractedRay(rayDirection, triangle.normal, triangle.refractiveIndex);

//          glm::vec3 refractedDirection = glm::refract(rayDirection-lightposition, triangle.normal, triangle.refractiveIndex);
            glm::vec3 refractedOrigin = closestIntersection.intersectionPoint + refractedDirection * 0.0001f;
//            glm::vec3 refractedOrigin = closestIntersection.intersectionPoint;
            RayTriangleIntersection refractedIntersection = getClosestValidIntersectionWithReflection(
                    refractedOrigin, refractedDirection, triangles, depth + 1, maxDepth);
            return refractedIntersection;
        }

        if (!triangle.isMirror && !triangle.isMetal && !triangle.isGlass) {
            glm::vec3 scatteredDirection = calculateScatteredDirection(triangle.normal);
            glm::vec3 scatteredOrigin = closestIntersection.intersectionPoint + scatteredDirection * 0.01f;
//            glm::vec3 scatteredOrigin = closestIntersection.intersectionPoint;
            RayTriangleIntersection scatteredIntersection = getClosestValidIntersectionWithReflection(
                    scatteredOrigin, scatteredDirection, triangles, depth + 1, maxDepth);
//            MixColours(closestIntersection.intersectedTriangle.colour, scatteredIntersection.intersectedTriangle.colour, 0.6f);
            return scatteredIntersection;

        }

    }
    return closestIntersection;
}




bool isPointInShadow(
        const glm::vec3 &intersectionPoint,
        size_t intersectedTriangleIndex,
        const std::vector<ModelTriangle> &modelTriangles
) {
    glm::vec3 lightPosition = glm::vec3(0, 1, 1.5f);
    glm::vec3 shadowRayDirection = glm::normalize(intersectionPoint-lightPosition);
    RayTriangleIntersection Intersection = getClosestValidIntersection(lightPosition, shadowRayDirection, modelTriangles);
    return Intersection.triangleIndex == intersectedTriangleIndex;
}

bool isPointInShadow_fix(
        const glm::vec3 &intersectionPoint,
        size_t intersectedTriangleIndex,
        const std::vector<ModelTriangle> &modelTriangles
) {
    glm::vec3 lightPosition = glm::vec3(-0.2f, 0.8, 1.5f);
    glm::vec3 shadowRayDirection = glm::normalize(lightPosition - intersectionPoint);


    glm::vec3 shadowRayOrigin = intersectionPoint + 0.001f * shadowRayDirection; // shadow bias

    RayTriangleIntersection shadowIntersection = getClosestValidIntersection(
            shadowRayOrigin, shadowRayDirection, modelTriangles
    );

    bool shadowCheck = (shadowIntersection.triangleIndex != intersectedTriangleIndex);
    bool isInShadow = shadowCheck && shadowIntersection.distanceFromCamera < glm::length(lightPosition - shadowRayOrigin);

    return isInShadow;
}


void drawRasterisedScene_fix(DrawingWindow &window, glm::vec3 cameraPosition){
    const std::string filepath = "../04 Wireframes and Rasterising/models/cornell-box.obj";
    const std::map<std::string, Colour> palette = loadMTL("../04 Wireframes and Rasterising/models/cornell-box.mtl");
    std::vector<ModelTriangle> models = loadOBJ(filepath, palette);
    uint32_t colour;
    for (size_t y = 0; y < window.height; y++) {
        for (size_t x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x , y, window.width, window.height,1.0f,cameraPosition);
            RayTriangleIntersection rayIntersection = getClosestValidIntersection(cameraPosition, rayDirection, models);
            if (rayIntersection.distanceFromCamera != std::numeric_limits<float>::infinity()) {
                if (isPointInShadow_fix(rayIntersection.intersectionPoint, rayIntersection.triangleIndex, models)) {
                    uint32_t Black = (255 << 24) + (int(0) << 16) + (int(0) << 8) + int(0);
                    colour=Black;
                } else {
                    colour = (255 << 24) + (rayIntersection.intersectedTriangle.colour.red << 16) + (rayIntersection.intersectedTriangle.colour.green << 8) + rayIntersection.intersectedTriangle.colour.blue;
                }
                window.setPixelColour(x, y,colour);
            }
        }
    }

}

Colour adjustBrightness(const Colour &originalColour, float brightness) {
    return Colour(
            int(originalColour.red * brightness),
            int(originalColour.green * brightness),
            int(originalColour.blue * brightness)
    );

}


Colour multiplyColour(const Colour &colour, float factor) {
    int red = std::min(static_cast<int>(colour.red * factor), 255);
    int green = std::min(static_cast<int>(colour.green * factor), 255);
    int blue = std::min(static_cast<int>(colour.blue * factor), 255);
    return Colour(red, green, blue);
}

Colour addColours(const Colour &colour1, const Colour &colour2) {
    int red = std::min(colour1.red + colour2.red, 255);
    int green = std::min(colour1.green + colour2.green, 255);
    int blue = std::min(colour1.blue + colour2.blue, 255);
    return Colour(red, green, blue);
}

void drawRasterisedScene_A(DrawingWindow &window, glm::vec3 cameraPosition, glm::vec3 lightPosition){
    const std::string filepath = "../04 Wireframes and Rasterising/models/cornell-box.obj";
    const std::map<std::string, Colour> palette = loadMTL("../04 Wireframes and Rasterising/models/cornell-box.mtl");
//    const std::string filepath2 = "../07 Lighting and Shading (external lecture)/resources/sphere.obj";
//    const std::map<std::string, Colour> palette2;
    std::vector<ModelTriangle> models = loadOBJ(filepath, palette);
    uint32_t colour;
    for (size_t y = 0; y < window.height; y++) {
        for (size_t x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 1.0f, cameraPosition);
            RayTriangleIntersection rayIntersection = getClosestValidIntersection(cameraPosition, rayDirection, models);

            if (rayIntersection.distanceFromCamera != std::numeric_limits<float>::infinity()) {


                float distance = glm::length(lightPosition - rayIntersection.intersectionPoint);
                float distanceAttenuation = 100.0f / (5.0f * M_PI * distance * distance);


                glm::vec3 lightDir = glm::normalize(lightPosition - rayIntersection.intersectionPoint);
                float dotProduct = glm::dot(rayIntersection.intersectedTriangle.normal, lightDir);
                float normalBrightness = std::max(dotProduct, 0.0f);
                float ambientLightThreshold = 0.2f;
                float calculatedBrightness = std::min(normalBrightness * distanceAttenuation, 1.0f);
                float combinedBrightness = std::max(ambientLightThreshold, calculatedBrightness);

                glm::vec3 viewDir = glm::normalize(cameraPosition - rayIntersection.intersectionPoint);
                glm::vec3 reflectDir = glm::reflect(-lightDir, rayIntersection.intersectedTriangle.normal);


                float specIntensity = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 256);
                float specIntensity2 = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 128);
                float specIntensity3 = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 64);



                Colour originalColour = rayIntersection.intersectedTriangle.colour;
                Colour adjustedColour = adjustBrightness(originalColour, combinedBrightness);
                Colour specularColor = multiplyColour(Colour(255, 255, 255), specIntensity);
                Colour finalColor = addColours(adjustBrightness(originalColour, combinedBrightness), specularColor);

                if (isPointInShadow(rayIntersection.intersectionPoint, rayIntersection.triangleIndex, models)) {
                    uint32_t packedColour =
                            (255 << 24) + (finalColor.red << 16) + (finalColor.green << 8) + finalColor.blue;
                    window.setPixelColour(x, y, packedColour);
                }
                else{
                    finalColor = adjustBrightness(rayIntersection.intersectedTriangle.colour, 0.2f);
                    uint32_t packedColour =
                            (255 << 24) + (finalColor.red/2 << 16) + (finalColor.green << 8) + finalColor.blue/2;
                    window.setPixelColour(x, y, packedColour);
                }
            }
        }
    }


}
glm::vec3 getClearNormal(TextureMap textureMap, float x, float y) {
    int pixelX = static_cast<int>(x);
    int pixelY = static_cast<int>(y);

    uint32_t pixel = textureMap.getColourAt(pixelX, pixelY);

    int red = (pixel & 0x00FF0000) >> 16;
    int green = (pixel & 0x0000FF00) >> 8;
    int blue = (pixel & 0x000000FF);


    glm::vec3 bumpedNormal(
            (red / 255.0f) * 2.0f - 1.0f,
            (green / 255.0f) * 2.0f - 1.0f,
            (blue / 255.0f) * 2.0f - 1.0f
    );

    return glm::normalize(bumpedNormal);
}

glm::vec3 getBumpedNormal(TextureMap textureMap, float x, float y) {
    int pixelX = static_cast<int>(x);
    int pixelY = static_cast<int>(y);

    uint32_t currentPixel = textureMap.getColourAt(pixelX, pixelY);
    uint32_t rightPixel = textureMap.getColourAt(pixelX + 1, pixelY);
    uint32_t upPixel =  textureMap.getColourAt(pixelX, pixelY + 1);


    float heightCurrent = (currentPixel & 0x00FF0000) >> 16;
    float heightRight = (rightPixel & 0x00FF0000) >> 16;
    float heightUp = (upPixel & 0x00FF0000) >> 16;


    float F_x = heightRight - heightCurrent;
    float F_y = heightUp - heightCurrent;
    glm::vec3 perturbedNormal = glm::vec3(-F_x, -F_y, 1.0f);

    return glm::normalize(perturbedNormal);
}



void drawRasterisedScene_Texture(DrawingWindow &window, glm::vec3 cameraPosition, glm::vec3 lightPosition){
    const std::string filepath = "../05 Navigation and Transformation/models/textured-cornell-box.obj";
    const std::string filepath2 = "../05 Navigation and Transformation/models/sphere2.obj";
//    const std::map<std::string, Colour> palette2 = loadMTL("../05 Navigation and Transformation/models/wooden_sphere.mtl");

//    const std::string filepath = "../05 Navigation and Transformation/models/env.obj";
    const std::map<std::string, Colour> palette = loadMTL(
            "../05 Navigation and Transformation/models/textured-cornell-box.mtl");

    std::vector<ModelTriangle> models = loadOBJWithTexture(filepath, palette);
    TextureMap textureMap("../05 Navigation and Transformation/models/texture.ppm");
    uint32_t colour;
    for (size_t y = 0; y < window.height; y++) {
        for (size_t x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 1.0f, cameraPosition);
            RayTriangleIntersection rayIntersection = getReflectionIntersection(cameraPosition, rayDirection, models);

            if (rayIntersection.distanceFromCamera != std::numeric_limits<float>::infinity()) {


                float distance = glm::length(lightPosition - rayIntersection.intersectionPoint);
                float distanceAttenuation = 100.0f / (5.0f * M_PI * distance * distance);


                glm::vec3 lightDir = glm::normalize(lightPosition - rayIntersection.intersectionPoint);
                float dotProduct = glm::dot(rayIntersection.intersectedTriangle.normal, lightDir);
                float normalBrightness = std::max(dotProduct, 0.0f);
                float ambientLightThreshold = 0.2f;
                float calculatedBrightness = std::min(normalBrightness * distanceAttenuation, 1.0f);
                float combinedBrightness = std::max(ambientLightThreshold, calculatedBrightness);

                glm::vec3 viewDir = glm::normalize(cameraPosition - rayIntersection.intersectionPoint);
                glm::vec3 reflectDir = glm::reflect(-lightDir, rayIntersection.intersectedTriangle.normal);


                float specIntensity = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 256);
                float specIntensity2 = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 128);
                float specIntensity3 = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 64);


//
//                if (rayIntersection.intersectedTriangle.hasTexture) {
//                    float xTex = rayIntersection.textureCoords.x * textureMap.width;
//                    float yTex = rayIntersection.textureCoords.y * textureMap.height;
//                    glm::vec3 perterbed_normal = getClearNormal(textureMap, xTex, yTex);
//                    rayIntersection.intersectedTriangle.normal = glm::normalize(perterbed_normal);
//                }


                if (rayIntersection.intersectedTriangle.hasTexture) {
                    float xTex = rayIntersection.textureCoords.x * textureMap.width;
                    float yTex = rayIntersection.textureCoords.y * textureMap.height;
                    glm::vec3 perterbed_normal = getBumpedNormal(textureMap, xTex, yTex);
                    rayIntersection.intersectedTriangle.normal = glm::normalize(perterbed_normal);
                }

                Colour baseColour;
                if (rayIntersection.intersectedTriangle.hasTexture) {
                    uint32_t textureColour = textureMap.getColourAt(rayIntersection.textureCoords.x, rayIntersection.textureCoords.y);
                    baseColour = Colour(textureColour >> 16 & 0xFF, textureColour >> 8 & 0xFF, textureColour & 0xFF);
                } else {
                    baseColour = rayIntersection.intersectedTriangle.colour;
                }

                Colour adjustedColour = adjustBrightness(baseColour, combinedBrightness);
                Colour specularColor = multiplyColour(Colour(255, 255, 255), specIntensity);
                Colour finalColor = addColours(adjustBrightness(baseColour, combinedBrightness), specularColor);



                    uint32_t packedColour =
                            (255 << 24) + (finalColor.red << 16) + (finalColor.green << 8) + finalColor.blue;
                    window.setPixelColour(x, y, packedColour);



            }
        }
    }


}

void drawRasterisedScene_Ball(DrawingWindow &window, glm::vec3 cameraPosition, glm::vec3 lightPosition){
    const std::string filepath = "../05 Navigation and Transformation/models/textured-cornell-box.obj";
    const std::string filepath2 = "../05 Navigation and Transformation/models/sphere2.obj";
//    const std::map<std::string, Colour> palette2 = loadMTL("../05 Navigation and Transformation/models/wooden_sphere.mtl");

//    const std::string filepath = "../05 Navigation and Transformation/models/env.obj";
    const std::map<std::string, Colour> palette = loadMTL(
            "../05 Navigation and Transformation/models/textured-cornell-box.mtl");

    std::vector<ModelTriangle> models = loadOBJWithTexture(filepath2, palette);
    TextureMap textureMap("../05 Navigation and Transformation/models/texture.ppm");
    uint32_t colour;
    for (size_t y = 0; y < window.height; y++) {
        for (size_t x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 1.0f, cameraPosition);
            RayTriangleIntersection rayIntersection = getReflectionIntersection(cameraPosition, rayDirection, models);

            if (rayIntersection.distanceFromCamera != std::numeric_limits<float>::infinity()) {


                float distance = glm::length(lightPosition - rayIntersection.intersectionPoint);
                float distanceAttenuation = 100.0f / (5.0f * M_PI * distance * distance);


                glm::vec3 lightDir = glm::normalize(lightPosition - rayIntersection.intersectionPoint);
                float dotProduct = glm::dot(rayIntersection.intersectedTriangle.normal, lightDir);
                float normalBrightness = std::max(dotProduct, 0.0f);
                float ambientLightThreshold = 0.2f;
                float calculatedBrightness = std::min(normalBrightness * distanceAttenuation, 1.0f);
                float combinedBrightness = std::max(ambientLightThreshold, calculatedBrightness);

                glm::vec3 viewDir = glm::normalize(cameraPosition - rayIntersection.intersectionPoint);
                glm::vec3 reflectDir = glm::reflect(-lightDir, rayIntersection.intersectedTriangle.normal);


                float specIntensity = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 256);
                float specIntensity2 = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 128);
                float specIntensity3 = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 64);



//                if (rayIntersection.intersectedTriangle.hasTexture) {
//                    float xTex = rayIntersection.textureCoords.x * textureMap.width;
//                    float yTex = rayIntersection.textureCoords.y * textureMap.height;
//                    glm::vec3 perterbed_normal = getClearNormal(textureMap, xTex, yTex);
//                    rayIntersection.intersectedTriangle.normal = glm::normalize(perterbed_normal);
//                }

//
//                if (rayIntersection.intersectedTriangle.hasTexture) {
//                    float xTex = rayIntersection.textureCoords.x * textureMap.width;
//                    float yTex = rayIntersection.textureCoords.y * textureMap.height;
//                    glm::vec3 perterbed_normal = getBumpedNormal(textureMap, xTex, yTex);
//                    rayIntersection.intersectedTriangle.normal = glm::normalize(perterbed_normal);
//                }

                Colour baseColour;
                if (rayIntersection.intersectedTriangle.hasTexture) {
                    uint32_t textureColour = textureMap.getColourAt(rayIntersection.textureCoords.x, rayIntersection.textureCoords.y);
                    baseColour = Colour(textureColour >> 16 & 0xFF, textureColour >> 8 & 0xFF, textureColour & 0xFF);
                } else {
                    baseColour = rayIntersection.intersectedTriangle.colour;
                }

                Colour adjustedColour = adjustBrightness(baseColour, combinedBrightness);
                Colour specularColor = multiplyColour(Colour(255, 255, 255), specIntensity);
                Colour finalColor = addColours(adjustBrightness(baseColour, combinedBrightness), specularColor);



                uint32_t packedColour =
                        (255 << 24) + (finalColor.red << 16) + (finalColor.green << 8) + finalColor.blue;
                window.setPixelColour(x, y, packedColour);



            }
        }
    }


}



void drawRasterisedScene_Mirror(DrawingWindow &window, glm::vec3 cameraPosition, glm::vec3 lightPosition) {
    const std::string filepath = "../04 Wireframes and Rasterising/models/Mirror-box.obj";
    const std::map<std::string, Colour> palette = loadMTL("../04 Wireframes and Rasterising/models/cornell-box.mtl");
    std::vector<ModelTriangle> models = loadOBJWithTexture(filepath, palette);
    for (size_t y = 0; y < window.height; y++) {
        for (size_t x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 1.0f, cameraPosition);

            RayTriangleIntersection rayIntersection = getClosestValidIntersectionWithReflection(cameraPosition, rayDirection, models);

            if (rayIntersection.distanceFromCamera != std::numeric_limits<float>::infinity()) {
                if (rayIntersection.intersectedTriangle.isMirror) {

                    RayTriangleIntersection reflectedIntersection = getReflectionIntersection(
                            rayIntersection.intersectionPoint,
                            glm::reflect(rayDirection, rayIntersection.intersectedTriangle.normal),
                            models
                    );


                    Colour reflectedColour = reflectedIntersection.intersectedTriangle.colour;
                    uint32_t packedReflectedColour = (255 << 24) + (reflectedColour.red << 16) + (reflectedColour.green << 8) + reflectedColour.blue;
                    window.setPixelColour(x, y, packedReflectedColour);
                } else {

                    float distance = glm::length(lightPosition - rayIntersection.intersectionPoint);
                    float distanceAttenuation = 100.0f / (5.0f * M_PI * distance * distance);


                    glm::vec3 lightDir = glm::normalize(lightPosition - rayIntersection.intersectionPoint);
                    float dotProduct = glm::dot(rayIntersection.intersectedTriangle.normal, lightDir);
                    float normalBrightness = std::max(dotProduct, 0.0f);

                    float ambientLightThreshold = 0.2f;

                    float calculatedBrightness = std::min(normalBrightness * distanceAttenuation, 1.0f);
                    float combinedBrightness = std::max(ambientLightThreshold, calculatedBrightness);

                    glm::vec3 viewDir = glm::normalize(cameraPosition - rayIntersection.intersectionPoint);


                    glm::vec3 reflectDir = glm::reflect(-lightDir, rayIntersection.intersectedTriangle.normal);


                    float specIntensity = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 256);


                    Colour originalColour = rayIntersection.intersectedTriangle.colour;
                    Colour adjustedColour = adjustBrightness(originalColour, combinedBrightness);
                    Colour specularColor = multiplyColour(Colour(255, 255, 255), specIntensity);
                    Colour finalColor = addColours(adjustedColour, specularColor);

                    if (isPointInShadow(rayIntersection.intersectionPoint, rayIntersection.triangleIndex, models)) {
                        uint32_t packedColour = (255 << 24) + (finalColor.red << 16) + (finalColor.green << 8) + finalColor.blue;
                        window.setPixelColour(x, y, packedColour);
                    } else {
                        finalColor = adjustBrightness(rayIntersection.intersectedTriangle.colour, 0.2f);
                        uint32_t packedColour = (255 << 24) + (finalColor.red/2 << 16) + (finalColor.green << 8) + finalColor.blue/2;
                        window.setPixelColour(x, y, packedColour);
                    }
                }
            }
        }
    }
}


void drawRasterisedScene_indirect(DrawingWindow &window, glm::vec3 cameraPosition, glm::vec3 lightPosition) {
    const std::string filepath = "../04 Wireframes and Rasterising/models/cornell-box.obj";
    const std::map<std::string, Colour> palette = loadMTL("../04 Wireframes and Rasterising/models/cornell-box.mtl");
    std::vector<ModelTriangle> models = loadOBJWithTexture(filepath, palette);
    for (size_t y = 0; y < window.height; y++) {
        for (size_t x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 1.0f, cameraPosition);

            RayTriangleIntersection rayIntersection2 = getClosestValidIntersectionWithIndirect(cameraPosition, rayDirection, models);
            RayTriangleIntersection rayIntersection3 = getClosestValidIntersectionWithIndirect(cameraPosition, rayDirection, models);
            RayTriangleIntersection rayIntersection4 = getClosestValidIntersectionWithIndirect(cameraPosition, rayDirection, models);
            RayTriangleIntersection rayIntersection5 = getClosestValidIntersectionWithIndirect(cameraPosition, rayDirection, models);
            RayTriangleIntersection rayIntersection6 = getClosestValidIntersectionWithIndirect(cameraPosition, rayDirection, models);
            RayTriangleIntersection rayIntersection7 = getClosestValidIntersectionWithIndirect(cameraPosition, rayDirection, models);
            RayTriangleIntersection rayIntersection8 = getClosestValidIntersectionWithIndirect(cameraPosition, rayDirection, models);
            RayTriangleIntersection rayIntersection9 = getClosestValidIntersectionWithIndirect(cameraPosition, rayDirection, models);
            Colour colour2 = rayIntersection2.intersectedTriangle.colour;
            Colour colour3 = rayIntersection3.intersectedTriangle.colour;
            Colour colour4 = rayIntersection4.intersectedTriangle.colour;
            Colour colour5 = rayIntersection5.intersectedTriangle.colour;
            Colour colour6 = rayIntersection6.intersectedTriangle.colour;
            Colour colour7 = rayIntersection7.intersectedTriangle.colour;
            Colour colour8 = rayIntersection8.intersectedTriangle.colour;
            Colour colour9 = rayIntersection5.intersectedTriangle.colour;

            float totalRed = colour2.red + colour3.red + colour4.red + colour5.red +
                             colour6.red + colour7.red + colour8.red + colour9.red;
            float totalGreen = colour2.green + colour3.green + colour4.green + colour5.green +
                               colour6.green + colour7.green + colour8.green + colour9.green;
            float totalBlue = colour2.blue + colour3.blue + colour4.blue + colour5.blue +
                              colour6.blue + colour7.blue + colour8.blue + colour9.blue;


            int numberOfIntersections = 8;
            float averageRed = totalRed / numberOfIntersections;
            float averageGreen = totalGreen / numberOfIntersections;
            float averageBlue = totalBlue / numberOfIntersections;

            Colour averageColour(averageRed, averageGreen, averageBlue);





            RayTriangleIntersection rayIntersection = getClosestValidIntersectionWithReflection(cameraPosition,
                                                                                                rayDirection, models);
            if (rayIntersection.distanceFromCamera != std::numeric_limits<float>::infinity()) {
//                if (rayIntersection.intersectedTriangle.isMirror) {
//
//                    RayTriangleIntersection reflectedIntersection = getReflectionIntersection(
//                            rayIntersection.intersectionPoint,
//                            glm::reflect(rayDirection, rayIntersection.intersectedTriangle.normal),
//                            models
//                    );
//
//
//                    Colour reflectedColour = reflectedIntersection.intersectedTriangle.colour;
//                    uint32_t packedReflectedColour = (255 << 24) + (reflectedColour.red << 16) + (reflectedColour.green << 8) + reflectedColour.blue;
//                    window.setPixelColour(x, y, packedReflectedColour);
//                } else {

                    float distance = glm::length(lightPosition - rayIntersection.intersectionPoint);
                    float distanceAttenuation = 100.0f / (4.0f * M_PI * distance * distance);


                    glm::vec3 lightDir = glm::normalize(lightPosition - rayIntersection.intersectionPoint);
                    float dotProduct = glm::dot(rayIntersection.intersectedTriangle.normal, lightDir);
                    float normalBrightness = std::max(dotProduct, 0.0f);

                    float ambientLightThreshold = 0.1f;
                    float normalBrightnesss = std::max(ambientLightThreshold, normalBrightness);

                    float calculatedBrightness = std::min(normalBrightnesss * distanceAttenuation, 1.0f);
//                    float combinedBrightness = std::max(ambientLightThreshold, calculatedBrightness);


//                    glm::vec3 viewDir = glm::normalize(cameraPosition - rayIntersection.intersectionPoint);


//                    glm::vec3 reflectDir = glm::reflect(-lightDir, rayIntersection.intersectedTriangle.normal);


//                    float specIntensity = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 256);


//                    Colour originalColour = rayIntersection2.intersectedTriangle.colour;
//                    originalColour.green = 0;
//                    originalColour.red=255;
                    Colour ChangeColor=rayIntersection.intersectedTriangle.colour;
//                    Colour adjustedColour3 = adjustBrightness(ChangeColor,calculatedBrightness);

                    Colour adjustedColour = adjustBrightness(averageColour,calculatedBrightness);

                    Colour NewColor=MixColours(ChangeColor,adjustedColour,0.7);
                    Colour adjustedColour2 = adjustBrightness(NewColor,calculatedBrightness);
//                    Colour adjustedColour = adjustBrightness(NewColor, calculatedBrightness);
//                    Colour specularColor = multiplyColour(Colour(255, 255, 255), specIntensity);
//                    Colour finalColor = addColours(adjustedColour, specularColor);
                      Colour finalColor = adjustedColour2;


                        uint32_t packedColour = (255 << 24) + (finalColor.red << 16) + (finalColor.green << 8) + finalColor.blue;
                        window.setPixelColour(x, y, packedColour);



            }
        }
    }
}


Colour operator*(const Colour& colour, float factor) {
    return Colour(
            static_cast<int>(colour.red * factor),
            static_cast<int>(colour.green * factor),
            static_cast<int>(colour.blue * factor)
    );
}

Colour MixColours(Colour& c1, Colour& c2, float ratio) {
    return Colour(
            c1.red * ratio + c2.red * (1.0f - ratio),
            c1.green * ratio + c2.green * (1.0f - ratio),
            c1.blue * ratio + c2.blue * (1.0f - ratio)
    );
}

//Colour MixColours(const Colour& c1, const Colour& c2, float ratio) {
//    return Colour(
//            static_cast<int>(c1.red * ratio + c2.red * (1.0f - ratio)),
//            static_cast<int>(c1.green * ratio + c2.green * (1.0f - ratio)),
//            static_cast<int>(c1.blue * ratio + c2.blue * (1.0f - ratio))
//    );
//}

float ComputeFresnel(const glm::vec3 &I, const glm::vec3 &N, float refractiveIndex) {
    float cosi = glm::clamp(glm::dot(I, N), -1.0f, 1.0f);
    float etai = 1, etat = refractiveIndex;
    if (cosi > 0) { std::swap(etai, etat); }
    float sint = etai / etat * sqrtf(std::max(0.f, 1 - cosi * cosi));
    if (sint >= 1) {
        return 1;
    }
    else {
        float cost = sqrtf(std::max(0.f, 1 - sint * sint));
        cosi = fabsf(cosi);
        float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
        float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
        return (Rs * Rs + Rp * Rp) / 2;
    }
}


void drawRasterisedScene_Metal(DrawingWindow &window, glm::vec3 cameraPosition, glm::vec3 lightPosition) {
    const std::string filepath = "../04 Wireframes and Rasterising/models/cornell-box.obj";
    const std::map<std::string, Colour> palette = loadMTL(
            "../04 Wireframes and Rasterising/models/cornell-box.mtl");
    std::vector<ModelTriangle> models = loadOBJWithTexture(filepath, palette);
    for (size_t y = 0; y < window.height; y++) {
        for (size_t x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 1.0f, cameraPosition);

            RayTriangleIntersection rayIntersection = getClosestValidIntersectionWithReflection(cameraPosition,
                                                                                                rayDirection, models);

            if (rayIntersection.distanceFromCamera != std::numeric_limits<float>::infinity()) {
                if (rayIntersection.intersectedTriangle.isMirror) {

                    RayTriangleIntersection reflectedIntersection = getReflectionIntersection(
                            rayIntersection.intersectionPoint,
                            glm::reflect(rayDirection, rayIntersection.intersectedTriangle.normal),
                            models
                    );

                    Colour reflectedColour = reflectedIntersection.intersectedTriangle.colour;
                    uint32_t packedReflectedColour =
                            (255 << 24) + (reflectedColour.red << 16) + (reflectedColour.green << 8) +
                            reflectedColour.blue;
                    window.setPixelColour(x, y, packedReflectedColour);
                }
                if (rayIntersection.intersectedTriangle.isGlass) {

                    glm::vec3 reflectedDirection = glm::reflect(rayDirection, rayIntersection.intersectedTriangle.normal);
                    glm::vec3 reflectionOrigin = rayIntersection.intersectionPoint + reflectedDirection * 0.001f;

                    RayTriangleIntersection reflectedIntersection = getClosestValidIntersectionWithReflection(
                            reflectionOrigin, reflectedDirection, models);
                    Colour reflectedColour = reflectedIntersection.intersectedTriangle.colour;

                    glm::vec3 refractedDirection = glm::refract(rayDirection, rayIntersection.intersectedTriangle.normal, rayIntersection.intersectedTriangle.refractiveIndex);
                    glm::vec3 refractedOrigin = rayIntersection.intersectionPoint;

                    RayTriangleIntersection refractedIntersection = getClosestValidIntersectionWithReflection(
                            refractedOrigin, refractedDirection, models);
                    Colour refractedColour = refractedIntersection.intersectedTriangle.colour;

                    float fresnelReflectance = ComputeFresnel(rayDirection, rayIntersection.intersectedTriangle.normal, rayIntersection.intersectedTriangle.refractiveIndex);
//                 Colour finalColour = MixColours(reflectedColour, refractedColour, fresnelReflectance);
                    Colour finalColour=refractedColour;


                    uint32_t packedFinalColour =
                            (255 << 24) + (finalColour.red << 16) + (finalColour.green << 8) + finalColour.blue;


                    window.setPixelColour(x, y, packedFinalColour);
                }else {
                    float distance = glm::length(lightPosition - rayIntersection.intersectionPoint);
                    float distanceAttenuation = 100.0f / (5.0f * M_PI * distance * distance);

                    glm::vec3 lightDir = glm::normalize(lightPosition - rayIntersection.intersectionPoint);
                    float dotProduct = glm::dot(rayIntersection.intersectedTriangle.normal, lightDir);
                    float normalBrightness = std::max(dotProduct, 0.0f);

                    float ambientLightThreshold = 0.2f;
                    float calculatedBrightness = std::min(normalBrightness * distanceAttenuation, 1.0f);
                    float combinedBrightness = std::max(ambientLightThreshold, calculatedBrightness);

                    glm::vec3 viewDir = glm::normalize(cameraPosition - rayIntersection.intersectionPoint);


                    glm::vec3 reflectDir = glm::reflect(-lightDir, rayIntersection.intersectedTriangle.normal);

                    float specIntensity = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 256);

                    Colour originalColour = rayIntersection.intersectedTriangle.colour;
                    Colour adjustedColour = adjustBrightness(originalColour, combinedBrightness);
                    Colour specularColor = multiplyColour(Colour(255, 255, 255), specIntensity);
                    Colour finalColor = addColours(adjustedColour, specularColor);

                     if (isPointInShadow(rayIntersection.intersectionPoint, rayIntersection.triangleIndex, models)) {
                        uint32_t packedColour =
                                (255 << 24) + (finalColor.red << 16) + (finalColor.green << 8) + finalColor.blue;
                        window.setPixelColour(x, y, packedColour);
                    } else {
                        finalColor = adjustBrightness(rayIntersection.intersectedTriangle.colour, 0.2f);
                        uint32_t packedColour = (255 << 24) + (finalColor.red / 2 << 16) + (finalColor.green << 8) +
                                                finalColor.blue / 2;
                        window.setPixelColour(x, y, packedColour);
                    }
                }
            }
        }
    }

}


bool isPointInShadow_fix(
            const glm::vec3 &intersectionPoint,
            size_t intersectedTriangleIndex,
            const std::vector<ModelTriangle> &modelTriangles,
            const std::vector<glm::vec3> &lightPositions,
            float &shadowFactor
    ) {
        float shadowSum = 0.0f;
        int totalRays = lightPositions.size();

        for (const auto& lightPosition : lightPositions) {
            glm::vec3 shadowRayDirection = glm::normalize(lightPosition - intersectionPoint);
            glm::vec3 shadowRayOrigin = intersectionPoint + 0.001f * shadowRayDirection;

            RayTriangleIntersection shadowIntersection = getClosestValidIntersection(shadowRayOrigin, shadowRayDirection, modelTriangles);

            if (shadowIntersection.triangleIndex != intersectedTriangleIndex &&
                shadowIntersection.distanceFromCamera < glm::length(lightPosition - shadowRayOrigin)) {
                float shadowDistance = glm::length(lightPosition - intersectionPoint);
                float distanceFactor = std::min(100.0f / (shadowDistance * shadowDistance), 1.0f); // 限制衰减因子的最大值
                shadowSum += distanceFactor;
            }
        }

        float shadowScalingFactor = 1.0f;
        shadowFactor = (shadowSum / totalRays) * shadowScalingFactor;
        return shadowFactor > 0.0f;
}


void drawRasterisedScene_S(DrawingWindow &window, glm::vec3 cameraPosition, std::vector<glm::vec3> lightPositions) {
    const std::string filepath = "../04 Wireframes and Rasterising/models/cornell-box.obj";
//    const std::string filepath = "../07 Lighting and Shading (external lecture)/resources/sphere.obj";
    const std::map<std::string, Colour> palette = loadMTL("../04 Wireframes and Rasterising/models/cornell-box.mtl");
//    const std::map<std::string, Colour> palette;
    std::vector<ModelTriangle> models = loadOBJWithTexture(filepath, palette);
    for (size_t y = 0; y < window.height; y++) {
        for (size_t x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 1.0f, cameraPosition);
            RayTriangleIntersection rayIntersection = getClosestValidIntersection(cameraPosition, rayDirection, models);

            if (rayIntersection.distanceFromCamera != std::numeric_limits<float>::infinity()) {

                float shadowFactor=0.5f;
                bool inShadow = isPointInShadow_fix(rayIntersection.intersectionPoint, rayIntersection.triangleIndex, models, lightPositions, shadowFactor);


                if (inShadow) {
//
//                    Colour shadowColour = adjustBrightness(rayIntersection.intersectedTriangle.colour, 1.0f - shadowFactor);
//                    Colour shadowColour = adjustBrightness(shadowColours, 0.2f);
                    float minBrightness = 0.2f;
                    float shadowBrightness = std::max(1.0f - shadowFactor, minBrightness);
                    Colour shadowColour = adjustBrightness(rayIntersection.intersectedTriangle.colour, shadowBrightness);

                    uint32_t packedColour = (255 << 24) + (shadowColour.red/2<< 16) + (shadowColour.green/2 << 8) + shadowColour.blue/2;
                    window.setPixelColour(x, y, packedColour);
                    continue;
                }


                Colour finalColour = rayIntersection.intersectedTriangle.colour;
                glm::vec3 normal = rayIntersection.intersectedTriangle.normal;
                glm::vec3 viewDirection = glm::normalize(cameraPosition - rayIntersection.intersectionPoint);
                float totalDiffuse = 0.0f;
                float totalSpecular = 0.0f;

                for (const auto &lightPosition: lightPositions) {
                    glm::vec3 lightDirection = glm::normalize(lightPosition - rayIntersection.intersectionPoint);
                    float lightDistance = glm::length(lightPosition - rayIntersection.intersectionPoint);
//                    float attenuation = 5.0f / (5*M_PI*lightDistance * lightDistance);


                    float diffuse = std::max(glm::dot(normal, lightDirection), 0.0f);
//                    totalDiffuse += diffuse * attenuation;
                    totalDiffuse += diffuse;
                    totalDiffuse= std::max(totalDiffuse, 0.0f);
                    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
//                    float specular = std::pow(std::max(glm::dot(viewDirection, reflectDirection), 0.0f), 32);
//                    totalSpecular += specular * attenuation;
                }


                finalColour = adjustBrightness(finalColour, totalDiffuse);
                Colour specularColour(255, 255, 255);
                finalColour = addColours(finalColour, multiplyColour(specularColour, totalSpecular));

                uint32_t packedColour = (255 << 24) + (finalColour.red << 16) + (finalColour.green << 8) + finalColour.blue;
                window.setPixelColour(x, y, packedColour);
            }
        }
    }
}




float triangleArea(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) {
    return glm::length(glm::cross(b - a, c - a)) / 2.0f;
}

glm::vec3 vertexNormalCalculator(glm::vec3 vertex, const std::vector<ModelTriangle>& sphereModel) {
    glm::vec3 vertexNormal = glm::vec3(0.0f, 0.0f, 0.0f);
    float totalArea = 0.0f;

    for (const ModelTriangle &triangle : sphereModel) {
        if (triangle.vertices[0] == vertex || triangle.vertices[1] == vertex || triangle.vertices[2] == vertex) {
            float area = triangleArea(triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]);
            totalArea += area;
            vertexNormal += area * triangle.normal;
        }
    }
    if (totalArea > 0.0f) {
        vertexNormal /= totalArea;
    }
    return glm::normalize(vertexNormal);
}


float calculateVertexBrightness(const glm::vec3& vertex, const glm::vec3& vertexNormal,
                                const glm::vec3& lightPosition, const glm::vec3& cameraPosition,
                                float lightPower, float glossiness, float ambientLight) {

    glm::vec3 lightDirection = glm::normalize(lightPosition - vertex);
    float distance = glm::length(lightPosition - vertex);
    float distanceAttenuation = 100.0f / (5.0f * M_PI * distance * distance);

    glm::vec3 lightDir = glm::normalize(lightPosition -vertex);
    float dotProduct = glm::dot(vertexNormal, lightDir);
    float normalBrightness = std::max(dotProduct, 0.0f);
    float ambientLightThreshold = 0.1f;

    float calculatedBrightness = std::min(normalBrightness * distanceAttenuation, 1.0f);
//    float combinedBrightness = std::max(ambientLightThreshold, calculatedBrightness);
    float combinedBrightness = std::max(calculatedBrightness,0.0f);
    glm::vec3 viewDirection = glm::normalize(cameraPosition - vertex);
    glm::vec3 reflectionVector = glm::reflect(-lightDirection, vertexNormal);
    float specularIntensity =pow(std::max(0.0f,glm::dot(reflectionVector , viewDirection)), 64);

    float combinedLighting = std::min(1.0f, std::max(0.0f, combinedBrightness + specularIntensity));

    return combinedLighting;
}



void drawSphereWithGourandShading(DrawingWindow &window, const std::vector<ModelTriangle>& sphereModel, glm::vec3 cameraPosition, glm::vec3 lightPosition, float focalLength, float lightPower, float ambient) {
    for(int y = 0; y < window.height; y++) {
        for(int x = 0; x < window.width; x++) {
            CanvasPoint canvasPoint = CanvasPoint(float(x), float(y));
              glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 2.0f, cameraPosition);

            RayTriangleIntersection closestIntersection = getClosestValidIntersection(cameraPosition, rayDirection, sphereModel);
            if(closestIntersection.intersectionPoint == glm::vec3(0, 0, 0)) {
                uint32_t c = (255 << 24) + (0 << 16) + (0 << 8) + (0);
                window.setPixelColour(x, y, c);
                continue;
            }

            glm::vec3 v0 = closestIntersection.intersectedTriangle.vertices[0];
            glm::vec3 v1 = closestIntersection.intersectedTriangle.vertices[1];
            glm::vec3 v2 = closestIntersection.intersectedTriangle.vertices[2];
            glm::vec3 point = closestIntersection.intersectionPoint;

            glm::vec3 vn0 = vertexNormalCalculator(v0, sphereModel);
            glm::vec3 vn1 = vertexNormalCalculator(v1, sphereModel);
            glm::vec3 vn2 = vertexNormalCalculator(v2, sphereModel);

            float brightness0 = calculateVertexBrightness(v0, vn0, lightPosition, cameraPosition, lightPower, 256, 0.1);
            float brightness1 = calculateVertexBrightness(v1, vn1, lightPosition, cameraPosition, lightPower, 256, 0.1);
            float brightness2 = calculateVertexBrightness(v2, vn2, lightPosition, cameraPosition, lightPower, 256, 0.1);

            // Calculate Barycentric coordinates
            float areaTotal = triangleArea(v0, v1, v2);
            float u = triangleArea(point, v1, v2) / areaTotal;
            float v = triangleArea(point, v2, v0) / areaTotal;
            float w = 1.0f - u - v;
            // Interpolation
//            canvasPoint.brightness = u* brightness0 +v * brightness1 + w* brightness2;
//            std::cout << "canvasPoint.brightness: " << canvasPoint.brightness << std::endl;
//
            glm::vec3 interpolatedNormal = u * vn0 + v * vn1 + w * vn2;
            interpolatedNormal = glm::normalize(interpolatedNormal);

            canvasPoint.brightness = calculateVertexBrightness(point, interpolatedNormal, lightPosition, cameraPosition, lightPower, 64, 0.1);


            // Draw colour
            Colour originalColour(255,0,0);
            Colour adjustedColour = adjustBrightness(originalColour, canvasPoint.brightness);

            // 设置像素颜色
            uint32_t c = (255 << 24) + (adjustedColour.red << 16) + (adjustedColour.green << 8) + adjustedColour.blue;
            window.setPixelColour(x, y, c);
        }
    }
}



glm::vec3 interpolateNormalAtPoint(const glm::vec3& targetPoint,
                            const glm::vec3& vertex0, const glm::vec3& vertex1, const glm::vec3& vertex2,
                            const glm::vec3& normal0, const glm::vec3& normal1, const glm::vec3& normal2) {

    float sharedTermForU = -(vertex0.x - vertex1.x) * (vertex2.y - vertex1.y) + (vertex0.y - vertex1.y) * (vertex2.x - vertex1.x);
    float sharedTermForV = -(vertex1.x - vertex2.x) * (vertex0.y - vertex2.y) + (vertex1.y - vertex2.y) * (vertex0.x - vertex2.x);
    float uNumerator = -(targetPoint.x - vertex1.x) * (vertex2.y - vertex1.y) + (targetPoint.y - vertex1.y) * (vertex2.x - vertex1.x);
    float u = uNumerator / sharedTermForU;
    float vNumerator = -(targetPoint.x - vertex2.x) * (vertex0.y - vertex2.y) + (targetPoint.y - vertex2.y) * (vertex0.x - vertex2.x);
    float v = vNumerator / sharedTermForV;

    float w = 1 - u - v;

    glm::vec3 interpolatedNormal = u * normal0 + v * normal1 + w * normal2;

    return glm::normalize(interpolatedNormal);
}


void drawRaytracingPhongCameraView(DrawingWindow &window, glm::vec3 campos, std::vector<ModelTriangle> sphereModel, glm::vec3 lightPosition){
    for (int y = 0; y < window.height; y++) {
        for (int x = 0; x < window.width; x++) {
            glm::vec3 rayDirection = getRayDirectionFromPixel(x, y, window.width, window.height, 2.0f, campos);
            RayTriangleIntersection closestIntersection = getClosestValidIntersection(campos, rayDirection,
                                                                                      sphereModel);

            if (closestIntersection.triangleIndex != -1) {
                glm::vec3 v0 = closestIntersection.intersectedTriangle.vertices[0];
                glm::vec3 v1 = closestIntersection.intersectedTriangle.vertices[1];
                glm::vec3 v2 = closestIntersection.intersectedTriangle.vertices[2];
                glm::vec3 vn0 = vertexNormalCalculator(v0, sphereModel);
                glm::vec3 vn1 = vertexNormalCalculator(v1, sphereModel);
                glm::vec3 vn2 = vertexNormalCalculator(v2, sphereModel);

                glm::vec3 point = closestIntersection.intersectionPoint;
                glm::vec3 Normal = interpolateNormalAtPoint(point, v0, v1, v2, vn0, vn1, vn2);
                glm::vec3 lightDirection = glm::normalize(lightPosition - point);
                float diffuseIntensity = std::max(glm::dot(Normal, lightDirection), 0.0f);
                glm::vec3 viewDir = glm::normalize(campos - point);

                glm::vec3 reflectDir = glm::reflect(-lightDirection, Normal);

                float specularIntensity = 1.0f * pow(std::max(glm::dot(reflectDir, viewDir), 0.0f), 64);

                float ambient = 0.1f;
                float combinedBrightness = std::max(ambient, diffuseIntensity);
                Colour originalColour(255, 0, 0);
                Colour adjustedColour = adjustBrightness(originalColour, diffuseIntensity);

                Colour specularColor = multiplyColour(Colour(255, 255, 255), specularIntensity);
                Colour finalColor = addColours(adjustBrightness(originalColour, combinedBrightness), specularColor);

                    uint32_t packedColour =
                            (255 << 24) + (finalColor.red << 16) + (finalColor.green << 8) + finalColor.blue;
                    window.setPixelColour(x, y, packedColour);


                }
            }
        }
    }




enum class RenderMode {
    Wireframe,
    Rasterization,
    RayTracing,
    Texture,
    light,
    ball,
    ball2,
    depth,
    SoftShadows,
    Mirror,
    Refrection
};

RenderMode currentRenderMode = RenderMode::Rasterization;

void renderScene(DrawingWindow &window, glm::vec3 &cameraPosition, glm::vec3 &lightPosition,glm::vec3 &lightPosition1, glm::vec3 &lightPosition2,std::vector<ModelTriangle> sphereModel,std::vector<ModelTriangle> models,std::vector<ModelTriangle> Texturemodels,TextureMap textureMap) {

    Colour white(255, 255, 255);
    glm::vec3 cameraForGouraud(0, 0, 100);
    Colour colour;
    std::vector<glm::vec3> lightPositions = {
            glm::vec3(0.1, 1.3f ,0.91f),
            glm::vec3(0.01, 1.02f,0.92f),
            glm::vec3(0.2, 1.03f,0.93f),
            glm::vec3(0.23, 1.04f,0.95f),
            glm::vec3(0.24, 1.05f,0.96f),
            glm::vec3(0.3, 1.06f,0.97f),
            glm::vec3(0.4, 1.07f,0.98f),
            glm::vec3(0.5, 1.08f,0.99f),
            glm::vec3(0.6, 1.09f,1.05f),
            glm::vec3(0.49, 1.10f,1.01f),
            glm::vec3(0.33, 1.10f,1.01f),
            glm::vec3(0.46, 1.10f,1.01f),
            glm::vec3(0.55, 1.1f ,1.15f),
            glm::vec3(0.2, 1.1f ,1.15f),
            glm::vec3(0.26, 1.21f ,1.14f),
            glm::vec3(0.23, 1.31f ,1.21f),
            glm::vec3(-0.1, 1.31f ,1.22f),
            glm::vec3(-0.12, 1.21f ,1.3f),
            glm::vec3(-0.13, 1.41f ,1.4f),
            glm::vec3(-0.14, 1.21f ,1.4f),
            glm::vec3(-0.15, 1.11f ,1.4f),
            glm::vec3(-0.12, 1.22f ,1.45f),
            glm::vec3(-0.21, 1.26f ,1.464f),
            glm::vec3(-0.22, 1.16f ,1.15f),
            glm::vec3(-0.23, 1.19f ,1.15f),
            glm::vec3(-0.25, 1.19f ,1.15f),
            glm::vec3(-0.24, 1.19f ,1.15f),
            glm::vec3(-0.23, 1.19f ,1.15f),



    };


    std::vector<std::vector<float>> depthBuffer(3 * WIDTH, std::vector<float>(3 * HEIGHT, std::numeric_limits<float>::infinity()));

        switch (currentRenderMode) {

            case RenderMode::Rasterization: {
                for (const ModelTriangle &triangle: models) {
                    CanvasPoint points[3];
                    for (int i = 0; i < 3; i++) {
                        const glm::vec3 &vertex = triangle.vertices[i];
                        points[i] = getCanvasIntersectionPoint(cameraPosition, vertex, 2.0, 3 * WIDTH, 3 * HEIGHT);
                        points[i].texturePoint = triangle.texturePoints[i];
                    }

                    CanvasTriangle canvasTriangle(points[0], points[1], points[2]);
                    colour = triangle.colour;
                    fillTriangle(window, canvasTriangle, colour, depthBuffer);
                }
                std::cout << "Switched to Rasterization mode." << std::endl;
                break;
            }
            case RenderMode::Wireframe:{
                for (const ModelTriangle &triangle: models) {
                    CanvasPoint points[3];
                    for (int i = 0; i < 3; i++) {
                        const glm::vec3 &vertex = triangle.vertices[i];
                        points[i] = getCanvasIntersectionPoint(cameraPosition, vertex, 2.0, 3 * WIDTH, 3 * HEIGHT);
                        points[i].texturePoint = triangle.texturePoints[i];
                    }

                    CanvasTriangle canvasTriangle(points[0], points[1], points[2]);
                    colour=triangle.colour;
                    drawTriangle(window,canvasTriangle , white);
                }
                std::cout << "Switched to Wireframe mode." << std::endl;
                break;}
            case RenderMode::Texture: {
                for (const ModelTriangle &triangle: Texturemodels) {
                    CanvasPoint points[3];
                    for (int i = 0; i < 3; i++) {
                        const glm::vec3 &vertex = triangle.vertices[i];
                        points[i] = getCanvasIntersectionPoint(cameraPosition, vertex, 2.0, 3 * WIDTH, 3 * HEIGHT);
                        points[i].texturePoint = triangle.texturePoints[i];

//
                    }
                    CanvasTriangle canvasTriangle(points[0], points[1], points[2]);


                    if (triangle.hasTexture) {
//                        fillTextureTriangle(window, canvasTriangle, textureMap,depthBuffer);
                        fillTexturedTriangle(window, canvasTriangle, textureMap,depthBuffer);
                    } else {
                        fillTriangle(window, canvasTriangle, triangle.colour, depthBuffer);
                    }
                }


                break;
            }

            case RenderMode::RayTracing: {
//                drawRasterisedScene_fix(window, cameraPosition);
                drawRasterisedScene_Texture(window, cameraPosition,lightPosition2);
                break;
            }
            case RenderMode::light: {
                drawRasterisedScene_A(window, cameraPosition,lightPosition2);
                break;
            }
            case RenderMode::ball: {
                drawSphereWithGourandShading(window, sphereModel, cameraForGouraud, lightPosition, 2.0, 1, 0);
                break;
            }
            case RenderMode::ball2: {
                drawRaytracingPhongCameraView(window, cameraForGouraud, sphereModel, lightPosition1);
                break;
            }
            case RenderMode::SoftShadows: {
                drawRasterisedScene_S(window, cameraPosition,lightPositions);
                break;
            }
            case RenderMode::Mirror: {
                drawRasterisedScene_Mirror(window, cameraPosition,lightPosition2);
                break;
            }
            case RenderMode::Refrection: {
                drawRasterisedScene_Metal(window, cameraPosition,lightPosition2);

                break;
            }
                std::cout << "Switched to RayTracing mode." << std::endl;
                break;
        }


    }




void handleEvent_week7(SDL_Event event, DrawingWindow &window, glm::vec3 &cameraPosition,glm::vec3 &lightPosition,glm::vec3 &lightPosition1,glm::vec3 &lightPosition2,std::vector<ModelTriangle> sphereModel,std::vector<ModelTriangle> models,std::vector<ModelTriangle> Texturemodels,TextureMap textureMap) {

//    glm::vec3 cameraPosition(0, 0, 8.0f);
//    glm::vec3 lightPosition(0, 5.1f,5);
//    glm::vec3 lightPosition1(0.4f, 1.8f, 2.4f);
//    glm::vec3 lightPosition2(0, 0, 1);
        float focalLength = 2.0f;
        if (event.type == SDL_KEYDOWN) {
            float translationAmount = 1.0f;
            float rotationAmount = glm::radians(5.0f);

            // 处理平移
            if (event.key.keysym.sym == SDLK_LEFT) {
                std::cout << "LEFT" << std::endl;
                cameraPosition.x -= translationAmount;
            } else if (event.key.keysym.sym == SDLK_RIGHT) {
                std::cout << "RIGHT" << std::endl;
                cameraPosition.x += translationAmount;
            } else if (event.key.keysym.sym == SDLK_UP) {
                std::cout << "UP" << std::endl;
                cameraPosition.y-= translationAmount;
            } else if (event.key.keysym.sym == SDLK_DOWN) {
                std::cout << "DOWN" << std::endl;
                cameraPosition.y += translationAmount;
            } else if (event.key.keysym.sym == SDLK_w) {
                cameraPosition.z -= translationAmount;
            } else if (event.key.keysym.sym == SDLK_s) {
                cameraPosition.z += translationAmount;
            }
            if (event.key.keysym.sym == SDLK_a) {
                glm::mat3 rotation = rotationY(-rotationAmount);
                glm::mat3 rotationMatrix = rotationY(rotationAmount);
                cameraPosition = rotation * cameraPosition;
                cameraOrientation = rotationMatrix * cameraOrientation;
            }
            if (event.key.keysym.sym == SDLK_d) {  // Rotate camera right around the origin (pan right)
                glm::mat3 rotation = rotationY(rotationAmount);
                glm::mat3 rotationMatrix = rotationY(-rotationAmount);
                cameraPosition = rotation * cameraPosition;
                cameraOrientation = rotationMatrix * cameraOrientation;
            }
            if (event.key.keysym.sym == SDLK_q) {
            glm::mat3 rotation = rotationX(rotationAmount);
            glm::mat3 rotationMatrix = rotationX(-rotationAmount);
            cameraPosition = rotation * cameraPosition;
            cameraOrientation=rotationMatrix  * cameraOrientation;
        }
        if (event.key.keysym.sym == SDLK_e) {
            glm::mat3 rotation = rotationX(-rotationAmount);
            glm::mat3 rotationMatrix = rotationX(rotationAmount);
            cameraPosition = rotation * cameraPosition;
            cameraOrientation=rotationMatrix  * cameraOrientation;

        }

          else if (event.key.keysym.sym == SDLK_f) {
                // Generate three random vertices
//            CanvasPoint p1(rand() % window.width, rand() % window.height);
//            CanvasPoint p2(rand() % window.width, rand() % window.height);
//            CanvasPoint p3(rand() % window.width, rand() % window.height);

                CanvasPoint p1(160, 10);
                p1.texturePoint = TexturePoint(195, 5);
                CanvasPoint p2(300, 230);
                p2.texturePoint = TexturePoint(395, 380);
                CanvasPoint p3(10, 150);
                p3.texturePoint = TexturePoint(65, 330);
                CanvasTriangle triangle(p1, p2, p3);

                // Generate a random color
                Colour randomColor(rand() % 256, rand() % 256, rand() % 256);
                fillTriangle(window, triangle, randomColor);
                // Draw the triangle
                Colour whiteColor(255, 255, 255);
                drawTriangle(window, triangle, whiteColor);
            } else if (event.key.keysym.sym == SDLK_u) {
                CanvasPoint p1(rand() % window.width, rand() % window.height);
                CanvasPoint p2(rand() % window.width, rand() % window.height);
                CanvasPoint p3(rand() % window.width, rand() % window.height);
                CanvasTriangle triangle(p1, p2, p3);
                Colour randomColor(rand() % 256, rand() % 256, rand() % 256);
                drawTriangle(window, triangle, randomColor);
            }
            if (event.key.keysym.sym == SDLK_1) {
                window.clearPixels();
                currentRenderMode = RenderMode::Rasterization;
            } else if (event.key.keysym.sym == SDLK_2) {
                window.clearPixels();

                currentRenderMode = RenderMode::Wireframe;
            } else if (event.key.keysym.sym == SDLK_3) {
                window.clearPixels();
                currentRenderMode = RenderMode::Texture;
            } else if (event.key.keysym.sym == SDLK_4) {
                window.clearPixels();
//                currentRenderMode = RenderMode::light;
//                currentRenderMode = RenderMode::RayTracing;
                currentRenderMode = RenderMode::SoftShadows;

            } else if (event.key.keysym.sym == SDLK_5) {
                window.clearPixels();
                currentRenderMode = RenderMode::ball;
            } else if (event.key.keysym.sym == SDLK_6) {
                window.clearPixels();
                currentRenderMode = RenderMode::ball2;
            }else if (event.key.keysym.sym == SDLK_7) {
                    window.clearPixels();
                    currentRenderMode = RenderMode::SoftShadows;}
            else if (event.key.keysym.sym == SDLK_8) {
                    window.clearPixels();
                    currentRenderMode = RenderMode::Mirror;
            }
            else if (event.key.keysym.sym == SDLK_9) {
                window.clearPixels();
                currentRenderMode = RenderMode::Refrection;
            }

            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                window.savePPM("output.ppm");
                window.saveBMP("output.bmp");
            }else if (event.key.keysym.sym == SDLK_i) {
            std::cout << "Light UP" << std::endl;
            lightPosition2.y += translationAmount;
            lightPosition1.y += translationAmount;
            lightPosition.y += translationAmount;
        }
        else if (event.key.keysym.sym == SDLK_k) {
            std::cout << "Light DOWN" << std::endl;
            lightPosition2.y -= translationAmount;
            lightPosition1.y -= translationAmount;
            lightPosition.y -= translationAmount;
        }
        else if (event.key.keysym.sym == SDLK_j) {
            std::cout << "Light LEFT" << std::endl;
            lightPosition2.x -= translationAmount;
            lightPosition1.x -= translationAmount;
            lightPosition.x -= translationAmount;
        }
        else if (event.key.keysym.sym == SDLK_l) {
            std::cout << "Light RIGHT" << std::endl;
           lightPosition2.x += translationAmount;
           lightPosition1.x += translationAmount;
           lightPosition.x += translationAmount;

            }

        }
    window.clearPixels();
    std::cout << "Camera Position: x=" << cameraPosition.x << ", y=" << cameraPosition.y << ", z=" << cameraPosition.z << std::endl;
    renderScene(window,cameraPosition,lightPosition,lightPosition1,lightPosition2,sphereModel,models,Texturemodels,textureMap);

}


std::vector<glm::vec3> lightPositions = {
        glm::vec3(0.1, 1.3f ,0.91f),
        glm::vec3(0.01, 1.02f,0.92f),
        glm::vec3(0.2, 1.03f,0.93f),
        glm::vec3(0.23, 1.04f,0.95f),
        glm::vec3(0.24, 1.05f,0.96f),
        glm::vec3(0.3, 1.06f,0.97f),
        glm::vec3(0.4, 1.07f,0.98f),
        glm::vec3(0.5, 1.08f,0.99f),
        glm::vec3(0.6, 1.09f,1.05f),
        glm::vec3(0.49, 1.10f,1.01f),
        glm::vec3(0.33, 1.10f,1.01f),
        glm::vec3(0.46, 1.10f,1.01f),
        glm::vec3(0.55, 1.1f ,1.15f),
        glm::vec3(0.2, 1.1f ,1.15f),
        glm::vec3(0.26, 1.21f ,1.14f),
        glm::vec3(0.23, 1.31f ,1.21f),
        glm::vec3(-0.1, 1.31f ,1.22f),
        glm::vec3(-0.12, 1.21f ,1.3f),
        glm::vec3(-0.13, 1.41f ,1.4f),
        glm::vec3(-0.14, 1.21f ,1.4f),
        glm::vec3(-0.15, 1.11f ,1.4f),
        glm::vec3(-0.12, 1.22f ,1.45f),
        glm::vec3(-0.21, 1.26f ,1.464f),
        glm::vec3(-0.22, 1.16f ,1.15f),
        glm::vec3(-0.23, 1.19f ,1.15f),
        glm::vec3(-0.25, 1.19f ,1.15f),
        glm::vec3(-0.24, 1.19f ,1.15f),
        glm::vec3(-0.23, 1.19f ,1.15f),



};



int main() {

//    const std::string filepath = "../07 Lighting and Shading (external lecture)/resources/sphere.obj";
//    const std::map<std::string, Colour> palette;
//    std::vector<ModelTriangle> model = loadOBJ(filepath, palette);

    const std::string filepath = "../07 Lighting and Shading (external lecture)/resources/sphere.obj";
    const std::map<std::string, Colour> palette;
    std::vector<ModelTriangle> sphereModel = loadOBJ(filepath, palette);

    const std::string filepath2 = "../04 Wireframes and Rasterising/models/cornell-box.obj";
    const std::map<std::string, Colour> palette2 = loadMTL(
            "../04 Wireframes and Rasterising/models/cornell-box.mtl");
    std::vector<ModelTriangle> models = loadOBJ(filepath2, palette2);

    const std::string filepath3 = "../05 Navigation and Transformation/models/textured-cornell-box.obj";
    const std::map<std::string, Colour> palette3 = loadMTL(
            "../05 Navigation and Transformation/models/textured-cornell-box.mtl");
    std::vector<ModelTriangle> Texturemodels = loadOBJWithTexture(filepath3, palette3);
    TextureMap textureMap("../05 Navigation and Transformation/models/texture.ppm");


    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return -1;
    }
    DrawingWindow window(3 * WIDTH, 3 * HEIGHT, false);
    bool running = true;

    glm::vec3 cameraPosition(0, 0, 8.0f);
    glm::vec3 cameraForGouraud(0.0f,1,100);
    float focalLength = 2.0f;

//    glm::vec3 lightPosition(0, 1.9f, 1.4f);

//    glm::vec3 lightPosition1(0.4f, 1.8f, 2.4f);
    glm::vec3 lightPosition(0, 5.1f,5);
    glm::vec3 lightPosition1(0.4f, 1.8f, 2.4f);
    glm::vec3 lightPosition2(0, 0, 1);


    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            //if u want to see other model, just //here
            handleEvent_week7(event,window,cameraPosition,lightPosition,lightPosition1,lightPosition2,sphereModel,models,Texturemodels,textureMap);


            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
//        drawRasterisedScene_M(window, cameraPosition,lightPosition);
        window.renderFrame();
//
//       renderScene(window,cameraPosition,lightPosition,lightPosition1,lightPosition2,sphereModel,models,Texturemodels,textureMap);

//         this is fuction u can see in the video
//        drawRasterisedScene_Texture(window, cameraPosition,lightPosition2);
//        drawSphereWithGourandShading(window, sphereModel, cameraForGouraud, lightPosition, 2.0, 1, 0);
//        drawRaytracingPhongCameraView(window, cameraForGouraud, sphereModel, lightPosition1);
//        drawRasterisedScene_Mirror(window, cameraPosition,lightPosition2);
//         drawRasterisedScene_indirect(window, cameraPosition,lightPosition2);
//        drawRasterisedScene_S(window, cameraPosition,lightPositions);
    }





    SDL_Quit();
    return 0;

}