#include "stl_loader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace {

constexpr size_t kMaxTriangles = 2000000;
constexpr size_t kBinaryHeaderSize = 84;

bool isFinite(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool isDegenerate(const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    Vec3 cross{ab.y * ac.z - ab.z * ac.y,
               ab.z * ac.x - ab.x * ac.z,
               ab.x * ac.y - ab.y * ac.x};
    float area2 = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;
    return area2 < 1e-20f;
}

// STL files have no reliable winding guarantee. A closed surface with
// consistent winding has a non-zero signed volume; flip everything if it is
// negative so backface culling behaves predictably later.
void fixWinding(std::vector<Vec3>& vertices) {
    double volume = 0.0;
    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
        const Vec3& a = vertices[i];
        const Vec3& b = vertices[i + 1];
        const Vec3& c = vertices[i + 2];
        volume += static_cast<double>(a.x) * (b.y * c.z - b.z * c.y) -
                  static_cast<double>(a.y) * (b.x * c.z - b.z * c.x) +
                  static_cast<double>(a.z) * (b.x * c.y - b.y * c.x);
    }
    if (volume < 0.0) {
        for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
            std::swap(vertices[i + 1], vertices[i + 2]);
        }
    }
}

bool loadBinary(const std::string& path, uint32_t count, Mesh& mesh,
                std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open file";
        return false;
    }
    in.seekg(kBinaryHeaderSize);

    mesh.vertices.reserve(static_cast<size_t>(count) * 3);
    char record[50];
    for (uint32_t i = 0; i < count; ++i) {
        if (!in.read(record, sizeof(record))) {
            error = "truncated binary STL";
            return false;
        }
        float values[12];
        std::memcpy(values, record, sizeof(values));
        Vec3 a{values[3], values[4], values[5]};
        Vec3 b{values[6], values[7], values[8]};
        Vec3 c{values[9], values[10], values[11]};
        if (!isFinite(a) || !isFinite(b) || !isFinite(c)) continue;
        if (isDegenerate(a, b, c)) continue;
        mesh.vertices.push_back(a);
        mesh.vertices.push_back(b);
        mesh.vertices.push_back(c);
    }
    return true;
}

bool loadAscii(const std::string& path, Mesh& mesh, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open file";
        return false;
    }

    std::vector<Vec3> raw;
    std::string token;
    while (in >> token) {
        if (token != "vertex") continue;
        Vec3 v;
        if (!(in >> v.x >> v.y >> v.z)) {
            error = "malformed ASCII STL vertex";
            return false;
        }
        if (!isFinite(v)) {
            error = "non-finite vertex in ASCII STL";
            return false;
        }
        raw.push_back(v);
        if (raw.size() / 3 > kMaxTriangles) {
            error = "model exceeds the 2,000,000 triangle limit";
            return false;
        }
    }

    if (raw.empty() || raw.size() % 3 != 0) {
        error = "no triangles found (incomplete ASCII STL)";
        return false;
    }

    mesh.vertices.reserve(raw.size());
    for (size_t i = 0; i + 2 < raw.size(); i += 3) {
        if (isDegenerate(raw[i], raw[i + 1], raw[i + 2])) continue;
        mesh.vertices.push_back(raw[i]);
        mesh.vertices.push_back(raw[i + 1]);
        mesh.vertices.push_back(raw[i + 2]);
    }
    return true;
}

void computeNormals(Mesh& mesh) {
    const size_t count = mesh.triangleCount();
    mesh.normals.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const Vec3& a = mesh.vertices[i * 3];
        const Vec3& b = mesh.vertices[i * 3 + 1];
        const Vec3& c = mesh.vertices[i * 3 + 2];
        Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
        Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
        Vec3 n{ab.y * ac.z - ab.z * ac.y,
               ab.z * ac.x - ab.x * ac.z,
               ab.x * ac.y - ab.y * ac.x};
        const float length =
            std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (length > 1e-12f) {
            const float inv = 1.0f / length;
            n.x *= inv;
            n.y *= inv;
            n.z *= inv;
        } else {
            n = {0.0f, 0.0f, 1.0f};
        }
        mesh.normals[i] = n;
    }
}

void computeTopology(Mesh& mesh) {
    struct EdgeFaces {
        uint32_t f0 = 0xFFFFFFFFu;
        uint32_t f1 = 0xFFFFFFFFu;
        int count = 0;
    };

    constexpr float kQuantize = 100000.0f;
    std::map<std::array<int, 3>, uint32_t> ids;
    std::vector<uint32_t> corner(mesh.vertices.size());
    mesh.welded_vertices.clear();
    mesh.edges.clear();

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const Vec3& v = mesh.vertices[i];
        const std::array<int, 3> key = {
            static_cast<int>(std::lround(v.x * kQuantize)),
            static_cast<int>(std::lround(v.y * kQuantize)),
            static_cast<int>(std::lround(v.z * kQuantize))};
        auto found = ids.find(key);
        if (found == ids.end()) {
            const uint32_t id = static_cast<uint32_t>(mesh.welded_vertices.size());
            ids.emplace(key, id);
            mesh.welded_vertices.push_back(v);
            corner[i] = id;
        } else {
            corner[i] = found->second;
        }
    }

    std::map<std::pair<uint32_t, uint32_t>, EdgeFaces> edges;
    auto add_edge = [&](uint32_t a, uint32_t b, uint32_t face) {
        if (a > b) std::swap(a, b);
        EdgeFaces& info = edges[{a, b}];
        if (info.count == 0) {
            info.f0 = face;
        } else if (info.count == 1) {
            info.f1 = face;
        }
        ++info.count;
    };

    for (size_t t = 0; t < mesh.triangleCount(); ++t) {
        const uint32_t a = corner[t * 3];
        const uint32_t b = corner[t * 3 + 1];
        const uint32_t c = corner[t * 3 + 2];
        add_edge(a, b, static_cast<uint32_t>(t));
        add_edge(b, c, static_cast<uint32_t>(t));
        add_edge(c, a, static_cast<uint32_t>(t));
    }

    // 40 degrees: flat panel borders and sharp creases, but not the small
    // angle between adjacent facets of a smooth sphere.
    constexpr float kCosLimit = 0.7660f;
    for (const auto& item : edges) {
        const EdgeFaces& info = item.second;
        bool feature = info.count != 2;
        if (!feature) {
            const Vec3& n1 = mesh.normals[info.f0];
            const Vec3& n2 = mesh.normals[info.f1];
            const float dot = n1.x * n2.x + n1.y * n2.y + n1.z * n2.z;
            feature = dot < kCosLimit;
        }
        mesh.edges.push_back({item.first.first, item.first.second, info.f0,
                              info.f1, feature});
    }
}

void normalize(Mesh& mesh) {
    mesh.min = {1e30f, 1e30f, 1e30f};
    mesh.max = {-1e30f, -1e30f, -1e30f};

    for (const Vec3& v : mesh.vertices) {
        mesh.min.x = std::min(mesh.min.x, v.x);
        mesh.min.y = std::min(mesh.min.y, v.y);
        mesh.min.z = std::min(mesh.min.z, v.z);
        mesh.max.x = std::max(mesh.max.x, v.x);
        mesh.max.y = std::max(mesh.max.y, v.y);
        mesh.max.z = std::max(mesh.max.z, v.z);
    }

    mesh.center = {(mesh.min.x + mesh.max.x) * 0.5f,
                   (mesh.min.y + mesh.max.y) * 0.5f,
                   (mesh.min.z + mesh.max.z) * 0.5f};

    float extent = std::max({mesh.max.x - mesh.min.x,
                             mesh.max.y - mesh.min.y,
                             mesh.max.z - mesh.min.z});
    mesh.radius = extent * 0.5f;
    if (mesh.radius < 1e-6f) mesh.radius = 1.0f;

    const float inv = 1.0f / mesh.radius;
    for (Vec3& v : mesh.vertices) {
        v.x = (v.x - mesh.center.x) * inv;
        v.y = (v.y - mesh.center.y) * inv;
        v.z = (v.z - mesh.center.z) * inv;
    }
}

}  // namespace

bool loadStl(const std::string& path, Mesh& mesh, std::string& error) {
    std::error_code ec;
    uintmax_t file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        error = "cannot stat file: " + ec.message();
        return false;
    }
    if (file_size < 15) {
        error = "file is too small to be an STL";
        return false;
    }

    mesh = Mesh{};
    mesh.name = std::filesystem::path(path).filename().string();

    // Read the header and triangle count to detect the binary layout.
    char header[kBinaryHeaderSize] = {0};
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            error = "cannot open file";
            return false;
        }
        in.read(header, std::min<uintmax_t>(file_size, sizeof(header)));
    }

    uint32_t binary_count = 0;
    if (file_size >= kBinaryHeaderSize) {
        std::memcpy(&binary_count, header + 80, sizeof(binary_count));
    }

    const bool starts_with_solid =
        std::strncmp(header, "solid", 5) == 0;
    const uintmax_t expected =
        kBinaryHeaderSize + static_cast<uintmax_t>(binary_count) * 50;

    bool binary = false;
    if (binary_count > 0 && binary_count <= kMaxTriangles &&
        file_size == expected) {
        binary = true;
    } else if (!starts_with_solid && binary_count > 0 &&
               binary_count <= kMaxTriangles && file_size >= expected) {
        binary = true;
    }

    if (!binary && binary_count > kMaxTriangles && file_size >= expected &&
        !starts_with_solid) {
        error = "model exceeds the 2,000,000 triangle limit";
        return false;
    }

    if (binary) {
        if (!loadBinary(path, binary_count, mesh, error)) return false;
    } else {
        if (!loadAscii(path, mesh, error)) return false;
    }

    if (mesh.vertices.empty()) {
        error = "no valid triangles found";
        return false;
    }

    fixWinding(mesh.vertices);
    normalize(mesh);
    computeNormals(mesh);
    computeTopology(mesh);
    return true;
}
