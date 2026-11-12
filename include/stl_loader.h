#ifndef STLVIEWER_STL_LOADER_H
#define STLVIEWER_STL_LOADER_H

#include <cstddef>
#include <string>
#include <vector>

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mesh {
    // Three vertices per triangle, in model space.
    // After loading, the mesh is centered and normalized so that the
    // largest half-extent is 1.0.
    std::vector<Vec3> vertices;
    Vec3 min;
    Vec3 max;
    Vec3 center;
    float radius = 1.0f;
    std::string name;

    size_t triangleCount() const { return vertices.size() / 3; }
};

// Loads a binary or ASCII STL file. Returns false and fills `error` on failure.
bool loadStl(const std::string& path, Mesh& mesh, std::string& error);

#endif  // STLVIEWER_STL_LOADER_H
