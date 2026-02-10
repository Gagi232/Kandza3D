#include "../Header/Model.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <algorithm>

Model loadOBJ(const std::string& path) {
    Model result;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "Failed to open OBJ: " << path << std::endl;
        return result;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;

    struct VertexIndex { int p, t, n; };
    std::vector<VertexIndex> indices;

    std::string line;
    while (std::getline(file, line)) {
        if (line.size() < 2) continue;
        std::istringstream iss(line);
        std::string prefix; iss >> prefix;
        if (prefix == "v") {
            glm::vec3 p; iss >> p.x >> p.y >> p.z; positions.push_back(p);
        } else if (prefix == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z; normals.push_back(n);
        } else if (prefix == "vt") {
            glm::vec2 t; iss >> t.x >> t.y; texcoords.push_back(t);
        } else if (prefix == "f") {
            std::vector<std::string> faceParts;
            std::string part;
            while (iss >> part) faceParts.push_back(part);
            if (faceParts.size() < 3) continue;
            auto parseIdx=[&](const std::string &s)->VertexIndex{
                VertexIndex vi={-1,-1,-1};
                std::vector<std::string> tok;
                std::istringstream ss(s);
                std::string sub;
                while (std::getline(ss, sub, '/')) tok.push_back(sub);
                if (tok.size()>=1 && !tok[0].empty()) vi.p = std::stoi(tok[0]) - 1;
                if (tok.size()>=2 && !tok[1].empty()) vi.t = std::stoi(tok[1]) - 1;
                if (tok.size()>=3 && !tok[2].empty()) vi.n = std::stoi(tok[2]) - 1;
                return vi;
            };
            for (size_t i=1;i+1<faceParts.size();++i) {
                indices.push_back(parseIdx(faceParts[0]));
                indices.push_back(parseIdx(faceParts[i]));
                indices.push_back(parseIdx(faceParts[i+1]));
            }
        }
    }

    if (positions.empty()) {
        std::cout << "OBJ has no positions: " << path << std::endl;
        return result;
    }

    glm::vec3 minP = positions[0], maxP = positions[0];
    for (auto &p : positions) {
        minP = glm::min(minP, p);
        maxP = glm::max(maxP, p);
    }
    glm::vec3 center = (minP + maxP) * 0.5f;
    glm::vec3 extents = maxP - minP;
    float maxExtent = std::max(std::max(extents.x, extents.y), extents.z);
    float scale = 1.0f;
    if (maxExtent > 0.0f) scale = 1.0f / maxExtent;

    struct VT { float x,y,z; float nx,ny,nz; float u,v; };
    std::vector<VT> verts;
    verts.reserve(indices.size());

    float minY = 1e9f, maxY = -1e9f;
    float minX = 1e9f, maxX = -1e9f;
    float minZ = 1e9f, maxZ = -1e9f;
    for (auto &vi : indices) {
        VT v={0};
        if (vi.p>=0) {
            glm::vec3 p = positions[vi.p];
            p = (p - center) * scale;
            v.x = p.x; v.y = p.y; v.z = p.z;
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minZ = std::min(minZ, p.z);
            maxZ = std::max(maxZ, p.z);
        }
        if (vi.n>=0 && vi.n < (int)normals.size()) { v.nx = normals[vi.n].x * scale; v.ny = normals[vi.n].y * scale; v.nz = normals[vi.n].z * scale; }
        else { v.nx = v.ny = v.nz = 0.0f; }
        if (vi.t>=0 && vi.t < (int)texcoords.size()) { v.u = texcoords[vi.t].x; v.v = texcoords[vi.t].y; }
        verts.push_back(v);
    }

    bool needsNormals = true;
    for (auto &v : verts) { if (v.nx != 0.0f || v.ny != 0.0f || v.nz != 0.0f) { needsNormals = false; break; } }
    if (needsNormals) {
        std::vector<glm::vec3> accum(verts.size(), glm::vec3(0.0f));
        for (size_t i = 0; i + 2 < verts.size(); i += 3) {
            glm::vec3 p0 = glm::vec3(verts[i].x, verts[i].y, verts[i].z);
            glm::vec3 p1 = glm::vec3(verts[i+1].x, verts[i+1].y, verts[i+1].z);
            glm::vec3 p2 = glm::vec3(verts[i+2].x, verts[i+2].y, verts[i+2].z);
            glm::vec3 faceNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
            if (!glm::isnan(faceNormal.x)) {
                accum[i] += faceNormal;
                accum[i+1] += faceNormal;
                accum[i+2] += faceNormal;
            }
        }
        for (size_t i = 0; i < verts.size(); ++i) {
            glm::vec3 n = glm::normalize(accum[i]);
            if (glm::length(n) < 1e-6f) n = glm::vec3(0.0f, 1.0f, 0.0f);
            verts[i].nx = n.x; verts[i].ny = n.y; verts[i].nz = n.z;
        }
    }

    if (verts.empty()) {
        std::cout << "OBJ produced zero vertices: " << path << std::endl;
        return result;
    }

    GLuint VAO, VBO; glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(VT), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(VT),(void*)offsetof(VT,x));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(VT),(void*)offsetof(VT,nx));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(VT),(void*)offsetof(VT,u));

    result.VAO = VAO; result.vertexCount = (int)verts.size();
    result.center = center; result.scale = scale;
    result.halfHeight = (maxY - minY) * 0.5f;
    result.halfExtents = glm::vec3((maxX - minX) * 0.5f, (maxY - minY) * 0.5f, (maxZ - minZ) * 0.5f);
    std::cout << "Loaded OBJ: " << path << " vertices=" << result.vertexCount << " scale=" << scale << " center=(" << center.x << "," << center.y << "," << center.z << ") halfH=" << result.halfHeight << std::endl;
    return result;
}

Model loadOBJWithCandidates(const std::initializer_list<std::string>& candidates) {
    for (auto &s : candidates) {
        Model m = loadOBJ(s);
        if (m.VAO != 0) return m;
    }
    return Model();
}
