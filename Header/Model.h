#pragma once
#include <GL/glew.h>
#include <string>
#include <glm/glm.hpp>

struct Model {
    unsigned int VAO = 0;
    int vertexCount = 0;
    glm::vec3 center = glm::vec3(0.0f);
    float scale = 1.0f; 
    float halfHeight = 0.5f; 
    glm::vec3 halfExtents = glm::vec3(0.5f); 
};

Model loadOBJWithCandidates(const std::initializer_list<std::string>& candidates);
Model loadOBJ(const std::string& path);
