#ifndef KANSTL_STL_LOADER_H
#define KANSTL_STL_LOADER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct MeshEdge {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t f0 = 0xFFFFFFFFu;
    uint32_t f1 = 0xFFFFFFFFu;
    bool feature = false;
};

struct Mesh {
    // Three vertices per triangle, in model space.
    // After loading, the mesh is centered and normalized so that the
    // largest half-extent is 1.0.
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;  // one unit face normal per triangle
    std::vector<Vec3> welded_vertices;  // unique positions for topology
    std::vector<MeshEdge> edges;        // all edges, indices into welded_vertices
    Vec3 min;
    Vec3 max;
    Vec3 center;
    float radius = 1.0f;
    std::string name;

    size_t triangleCount() const { return vertices.size() / 3; }
};

// Loads a binary or ASCII STL file. Returns false and fills `error` on failure.
// `progress` is called with 0.0..1.0 while parsing, when provided.
bool loadStl(const std::string& path, Mesh& mesh, std::string& error,
             const std::function<void(float)>& progress = {});

#endif  // KANSTL_STL_LOADER_H
