#pragma once

#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cstdint>

enum class SlopeDir : int {
    NONE  = 0,
    POS_X = 1, // Sobe na direção +X
    NEG_X = 2, // Sobe na direção -X
    POS_Z = 3, // Sobe na direção +Z
    NEG_Z = 4  // Sobe na direção -Z
};

struct WorldBlock {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 size = { 1.0f, 1.0f, 1.0f };
    Color color = GRAY;
    BoundingBox bounds = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    int slopeDir = 0; // 0 = Caixa AABB padrão, 1..4 = Rampa inclinada (SlopeDir)

    bool IsSlope() const noexcept {
        return slopeDir >= 1 && slopeDir <= 4;
    }

    BoundingBox ComputeBounds() const noexcept {
        Vector3 half = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
        return {
            { pos.x - half.x, pos.y - half.y, pos.z - half.z },
            { pos.x + half.x, pos.y + half.y, pos.z + half.z }
        };
    }

    BoundingBox GetAABB() const noexcept {
        if (bounds.max.x > bounds.min.x || bounds.max.y > bounds.min.y || bounds.max.z > bounds.min.z) {
            return bounds;
        }
        return ComputeBounds();
    }

    // Retorna a altura exata da superfície inclinada no ponto (x, z)
    float GetSurfaceYAt(float x, float z) const noexcept {
        BoundingBox bb = GetAABB();
        if (!IsSlope()) return bb.max.y;

        float t = 1.0f;
        switch (static_cast<SlopeDir>(slopeDir)) {
            case SlopeDir::POS_X: {
                float span = std::max(0.001f, bb.max.x - bb.min.x);
                t = std::clamp((x - bb.min.x) / span, 0.0f, 1.0f);
                break;
            }
            case SlopeDir::NEG_X: {
                float span = std::max(0.001f, bb.max.x - bb.min.x);
                t = std::clamp((bb.max.x - x) / span, 0.0f, 1.0f);
                break;
            }
            case SlopeDir::POS_Z: {
                float span = std::max(0.001f, bb.max.z - bb.min.z);
                t = std::clamp((z - bb.min.z) / span, 0.0f, 1.0f);
                break;
            }
            case SlopeDir::NEG_Z: {
                float span = std::max(0.001f, bb.max.z - bb.min.z);
                t = std::clamp((bb.max.z - z) / span, 0.0f, 1.0f);
                break;
            }
            default:
                t = 1.0f;
                break;
        }
        return bb.min.y + t * (bb.max.y - bb.min.y);
    }

    // Maior altura da rampa sob uma AABB de personagem
    float GetMaxSurfaceUnderBox(const BoundingBox& actorBox) const noexcept {
        BoundingBox bb = GetAABB();
        if (!IsSlope()) return bb.max.y;

        float minX = std::clamp(actorBox.min.x, bb.min.x, bb.max.x);
        float maxX = std::clamp(actorBox.max.x, bb.min.x, bb.max.x);
        float minZ = std::clamp(actorBox.min.z, bb.min.z, bb.max.z);
        float maxZ = std::clamp(actorBox.max.z, bb.min.z, bb.max.z);

        switch (static_cast<SlopeDir>(slopeDir)) {
            case SlopeDir::POS_X: return GetSurfaceYAt(maxX, (minZ + maxZ) * 0.5f);
            case SlopeDir::NEG_X: return GetSurfaceYAt(minX, (minZ + maxZ) * 0.5f);
            case SlopeDir::POS_Z: return GetSurfaceYAt((minX + maxX) * 0.5f, maxZ);
            case SlopeDir::NEG_Z: return GetSurfaceYAt((minX + maxX) * 0.5f, minZ);
            default:              return bb.max.y;
        }
    }
};

// Renderiza blocos comuns ou cunhas 3D inclinadas (rampas)
inline void DrawWorldBlock(const WorldBlock& block) {
    if (!block.IsSlope()) {
        DrawCubeV(block.pos, block.size, block.color);
        DrawCubeWiresV(block.pos, block.size, BLACK);
        return;
    }

    BoundingBox bb = block.GetAABB();
    float x0 = bb.min.x, x1 = bb.max.x;
    float y0 = bb.min.y, y1 = bb.max.y;
    float z0 = bb.min.z, z1 = bb.max.z;

    // Define os 4 cantos da base e os 2 cantos elevados da rampa conforme a direção
    Vector3 b00 = { x0, y0, z0 };
    Vector3 b10 = { x1, y0, z0 };
    Vector3 b11 = { x1, y0, z1 };
    Vector3 b01 = { x0, y0, z1 };
    Vector3 tA{}, tB{};

    Color sideCol = Color{
        static_cast<unsigned char>(block.color.r * 0.85f),
        static_cast<unsigned char>(block.color.g * 0.85f),
        static_cast<unsigned char>(block.color.b * 0.85f),
        255
    };

    switch (static_cast<SlopeDir>(block.slopeDir)) {
        case SlopeDir::POS_X: {
            tA = { x1, y1, z0 };
            tB = { x1, y1, z1 };
            // Rampa inclinada (-X -> +X)
            DrawTriangle3D(b00, b01, tB, block.color);
            DrawTriangle3D(b00, tB, tA, block.color);
            // Parede traseira (+X)
            DrawTriangle3D(b10, tA, tB, sideCol);
            DrawTriangle3D(b10, tB, b11, sideCol);
            // Laterais triangulares
            DrawTriangle3D(b00, tA, b10, sideCol);
            DrawTriangle3D(b01, b11, tB, sideCol);
            break;
        }
        case SlopeDir::NEG_X: {
            tA = { x0, y1, z0 };
            tB = { x0, y1, z1 };
            DrawTriangle3D(tA, tB, b11, block.color);
            DrawTriangle3D(tA, b11, b10, block.color);
            DrawTriangle3D(b00, b01, tB, sideCol);
            DrawTriangle3D(b00, tB, tA, sideCol);
            DrawTriangle3D(b00, tA, b10, sideCol);
            DrawTriangle3D(b01, b11, tB, sideCol);
            break;
        }
        case SlopeDir::POS_Z: {
            tA = { x0, y1, z1 };
            tB = { x1, y1, z1 };
            DrawTriangle3D(b00, tA, tB, block.color);
            DrawTriangle3D(b00, tB, b10, block.color);
            DrawTriangle3D(b01, b11, tB, sideCol);
            DrawTriangle3D(b01, tB, tA, sideCol);
            DrawTriangle3D(b00, b01, tA, sideCol);
            DrawTriangle3D(b10, tB, b11, sideCol);
            break;
        }
        case SlopeDir::NEG_Z: {
            tA = { x0, y1, z0 };
            tB = { x1, y1, z0 };
            DrawTriangle3D(tA, b01, b11, block.color);
            DrawTriangle3D(tA, b11, tB, block.color);
            DrawTriangle3D(b00, tA, tB, sideCol);
            DrawTriangle3D(b00, tB, b10, sideCol);
            DrawTriangle3D(b00, b01, tA, sideCol);
            DrawTriangle3D(b10, tB, b11, sideCol);
            break;
        }
        default:
            break;
    }

    // Base inferior
    DrawTriangle3D(b00, b10, b11, sideCol);
    DrawTriangle3D(b00, b11, b01, sideCol);

    // Arestas de contorno da rampa
    DrawLine3D(b00, b10, BLACK);
    DrawLine3D(b10, b11, BLACK);
    DrawLine3D(b11, b01, BLACK);
    DrawLine3D(b01, b00, BLACK);
    DrawLine3D(tA, tB, BLACK);
    if (block.slopeDir == (int)SlopeDir::POS_X) {
        DrawLine3D(b00, tA, BLACK); DrawLine3D(b01, tB, BLACK);
        DrawLine3D(b10, tA, BLACK); DrawLine3D(b11, tB, BLACK);
    } else if (block.slopeDir == (int)SlopeDir::NEG_X) {
        DrawLine3D(b10, tA, BLACK); DrawLine3D(b11, tB, BLACK);
        DrawLine3D(b00, tA, BLACK); DrawLine3D(b01, tB, BLACK);
    } else if (block.slopeDir == (int)SlopeDir::POS_Z) {
        DrawLine3D(b00, tA, BLACK); DrawLine3D(b10, tB, BLACK);
        DrawLine3D(b01, tA, BLACK); DrawLine3D(b11, tB, BLACK);
    } else if (block.slopeDir == (int)SlopeDir::NEG_Z) {
        DrawLine3D(b01, tA, BLACK); DrawLine3D(b11, tB, BLACK);
        DrawLine3D(b00, tA, BLACK); DrawLine3D(b10, tB, BLACK);
    }
}

struct Tracer {
    Vector3 start;
    Vector3 end;
    Color color;
    float timer;
};

enum class MapPreset {
    SQUAD_ARENA = 0,
    MOVEMENT_COURSE,
    FIRING_RANGE
};

inline bool CheckOverlapStrict(const BoundingBox& a, const BoundingBox& b) noexcept {
    bool overlapX = (a.min.x < b.max.x) && (a.max.x > b.min.x);
    bool overlapY = (a.min.y < b.max.y) && (a.max.y > b.min.y);
    bool overlapZ = (a.min.z < b.max.z) && (a.max.z > b.min.z);
    return overlapX && overlapY && overlapZ;
}

inline bool CheckOverlapStrict(Vector3 posA, Vector3 sizeA, Vector3 posB, Vector3 sizeB) noexcept {
    float halfAx = sizeA.x * 0.5f, halfAy = sizeA.y * 0.5f, halfAz = sizeA.z * 0.5f;
    float halfBx = sizeB.x * 0.5f, halfBy = sizeB.y * 0.5f, halfBz = sizeB.z * 0.5f;

    bool overlapX = (posA.x - halfAx < posB.x + halfBx) && (posA.x + halfAx > posB.x - halfBx);
    bool overlapY = (posA.y - halfAy < posB.y + halfBy) && (posA.y + halfAy > posB.y - halfBy);
    bool overlapZ = (posA.z - halfAz < posB.z + halfBz) && (posA.z + halfAz > posB.z - halfBz);

    return overlapX && overlapY && overlapZ;
}

// =============================================================================
// SPATIAL HASH GRID (4m x 4m XZ) — SAVEPOINT_20
// =============================================================================
class SpatialHashGrid {
public:
    static constexpr float CELL_SIZE = 4.0f;
    static constexpr float INV_CELL_SIZE = 1.0f / CELL_SIZE;

    struct CellKey {
        int cx;
        int cz;

        bool operator==(const CellKey& other) const noexcept {
            return cx == other.cx && cz == other.cz;
        }
    };

    struct CellKeyHash {
        std::size_t operator()(const CellKey& k) const noexcept {
            std::size_t h1 = static_cast<std::size_t>(k.cx) * 73856093u;
            std::size_t h2 = static_cast<std::size_t>(k.cz) * 19349663u;
            return h1 ^ h2;
        }
    };

    static int WorldToCell(float coord) noexcept {
        return static_cast<int>(std::floor(coord * INV_CELL_SIZE));
    }

    void Clear() {
        cells.clear();
        visitedStamp.clear();
        currentStamp = 1;
        indexedBlockCount = 0;
        indexedEntryCount = 0;
    }

    void Build(const std::vector<WorldBlock>& blocks) {
        cells.clear();
        indexedBlockCount = blocks.size();
        indexedEntryCount = 0;

        if (visitedStamp.size() < blocks.size()) {
            visitedStamp.resize(blocks.size(), 0);
        }

        cells.reserve(blocks.size() * 2 + 16);

        for (int i = 0; i < static_cast<int>(blocks.size()); i++) {
            BoundingBox bb = blocks[i].GetAABB();

            int minCx = WorldToCell(bb.min.x);
            int maxCx = WorldToCell(bb.max.x - 1e-4f);
            int minCz = WorldToCell(bb.min.z);
            int maxCz = WorldToCell(bb.max.z - 1e-4f);

            if (maxCx < minCx) maxCx = minCx;
            if (maxCz < minCz) maxCz = minCz;

            for (int cx = minCx; cx <= maxCx; cx++) {
                for (int cz = minCz; cz <= maxCz; cz++) {
                    cells[{ cx, cz }].push_back(i);
                    indexedEntryCount++;
                }
            }
        }
    }

    template <typename Visitor>
    void QueryAABB(Vector3 center, Vector3 size, Visitor&& visitor) const {
        if (indexedBlockCount == 0) return;

        uint32_t stamp = NextStamp();
        float halfX = size.x * 0.5f;
        float halfZ = size.z * 0.5f;

        int minCx = WorldToCell(center.x - halfX);
        int maxCx = WorldToCell(center.x + halfX);
        int minCz = WorldToCell(center.z - halfZ);
        int maxCz = WorldToCell(center.z + halfZ);

        for (int cx = minCx; cx <= maxCx; cx++) {
            for (int cz = minCz; cz <= maxCz; cz++) {
                auto it = cells.find({ cx, cz });
                if (it == cells.end()) continue;

                for (int idx : it->second) {
                    if (visitedStamp[idx] == stamp) continue;
                    visitedStamp[idx] = stamp;
                    if (!visitor(idx)) return;
                }
            }
        }
    }

    template <typename Visitor>
    void TraverseRayXZ(Vector3 origin, Vector3 dir, float maxDist3D, Visitor&& visitor) const {
        if (indexedBlockCount == 0 || maxDist3D <= 0.0f) return;

        uint32_t stamp = NextStamp();

        float horizLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        int cx = WorldToCell(origin.x);
        int cz = WorldToCell(origin.z);

        if (horizLen < 1e-6f) {
            auto it = cells.find({ cx, cz });
            if (it != cells.end()) {
                for (int idx : it->second) {
                    if (visitedStamp[idx] == stamp) continue;
                    visitedStamp[idx] = stamp;
                    if (!visitor(idx)) return;
                }
            }
            return;
        }

        float invHoriz = 1.0f / horizLen;
        float ux = dir.x * invHoriz;
        float uz = dir.z * invHoriz;
        float maxDist2D = maxDist3D * horizLen;

        int stepX = (ux > 0.0f) ? 1 : ((ux < 0.0f) ? -1 : 0);
        int stepZ = (uz > 0.0f) ? 1 : ((uz < 0.0f) ? -1 : 0);

        float tMaxX = 1e30f;
        float tDeltaX = 1e30f;
        if (stepX > 0) {
            tMaxX = ((static_cast<float>(cx + 1) * CELL_SIZE) - origin.x) / ux;
            tDeltaX = CELL_SIZE / ux;
        } else if (stepX < 0) {
            tMaxX = ((static_cast<float>(cx) * CELL_SIZE) - origin.x) / ux;
            tDeltaX = -CELL_SIZE / ux;
        }

        float tMaxZ = 1e30f;
        float tDeltaZ = 1e30f;
        if (stepZ > 0) {
            tMaxZ = ((static_cast<float>(cz + 1) * CELL_SIZE) - origin.z) / uz;
            tDeltaZ = CELL_SIZE / uz;
        } else if (stepZ < 0) {
            tMaxZ = ((static_cast<float>(cz) * CELL_SIZE) - origin.z) / uz;
            tDeltaZ = -CELL_SIZE / uz;
        }

        const int maxSteps = static_cast<int>(maxDist2D * INV_CELL_SIZE) + 4;
        for (int step = 0; step <= maxSteps; step++) {
            auto it = cells.find({ cx, cz });
            if (it != cells.end()) {
                for (int idx : it->second) {
                    if (visitedStamp[idx] == stamp) continue;
                    visitedStamp[idx] = stamp;
                    if (!visitor(idx)) return;
                }
            }

            if (tMaxX < tMaxZ) {
                if (tMaxX > maxDist2D) break;
                cx += stepX;
                tMaxX += tDeltaX;
            } else {
                if (tMaxZ > maxDist2D) break;
                cz += stepZ;
                tMaxZ += tDeltaZ;
            }
        }
    }

    size_t GetActiveCellCount() const noexcept { return cells.size(); }
    size_t GetIndexedBlockCount() const noexcept { return indexedBlockCount; }
    size_t GetIndexedEntryCount() const noexcept { return indexedEntryCount; }

private:
    std::unordered_map<CellKey, std::vector<int>, CellKeyHash> cells;
    mutable std::vector<uint32_t> visitedStamp;
    mutable uint32_t currentStamp = 1;
    size_t indexedBlockCount = 0;
    size_t indexedEntryCount = 0;

    uint32_t NextStamp() const noexcept {
        currentStamp++;
        if (currentStamp == 0) {
            std::fill(visitedStamp.begin(), visitedStamp.end(), 0);
            currentStamp = 1;
        }
        return currentStamp;
    }
};

inline const SpatialHashGrid& GetWorldSpatialGrid(const std::vector<WorldBlock>& world) {
    static SpatialHashGrid cachedGrid;
    static const WorldBlock* lastPtr = nullptr;
    static size_t lastSize = static_cast<size_t>(-1);
    static uint64_t lastFingerprint = 0;

    uint64_t fp = static_cast<uint64_t>(world.size()) * 1469598103934665603ULL;
    if (!world.empty()) {
        size_t step = std::max<size_t>(1, world.size() / 16);
        for (size_t i = 0; i < world.size(); i += step) {
            uint32_t px = 0, py = 0, pz = 0, sx = 0, sy = 0, sz = 0;
            std::memcpy(&px, &world[i].pos.x, sizeof(uint32_t));
            std::memcpy(&py, &world[i].pos.y, sizeof(uint32_t));
            std::memcpy(&pz, &world[i].pos.z, sizeof(uint32_t));
            std::memcpy(&sx, &world[i].size.x, sizeof(uint32_t));
            std::memcpy(&sy, &world[i].size.y, sizeof(uint32_t));
            std::memcpy(&sz, &world[i].size.z, sizeof(uint32_t));
            fp ^= (static_cast<uint64_t>(px) << 32) ^ static_cast<uint64_t>(pz);
            fp *= 1099511628211ULL;
            fp ^= (static_cast<uint64_t>(sx) << 32) ^ static_cast<uint64_t>(sz);
            fp *= 1099511628211ULL;
            fp ^= (static_cast<uint64_t>(py) << 32) ^ (static_cast<uint64_t>(sy) ^ static_cast<uint64_t>(world[i].slopeDir));
            fp *= 1099511628211ULL;
        }
    }

    const WorldBlock* curPtr = world.empty() ? nullptr : world.data();
    if (curPtr != lastPtr || world.size() != lastSize || fp != lastFingerprint) {
        cachedGrid.Build(world);
        lastPtr = curPtr;
        lastSize = world.size();
        lastFingerprint = fp;
    }

    return cachedGrid;
}

// =============================================================================
// CONSULTAS FÍSICAS & RAYCAST ACELERADAS PELO SPATIAL HASH GRID
// =============================================================================
inline bool OverlapsAnyBlock(Vector3 pos, Vector3 size, const std::vector<WorldBlock>& world, const SpatialHashGrid& grid) {
    BoundingBox queryBox = {
        { pos.x - size.x * 0.5f, pos.y - size.y * 0.5f, pos.z - size.z * 0.5f },
        { pos.x + size.x * 0.5f, pos.y + size.y * 0.5f, pos.z + size.z * 0.5f }
    };
    bool hit = false;
    grid.QueryAABB(pos, size, [&](int idx) -> bool {
        const WorldBlock& blk = world[idx];
        if (!CheckOverlapStrict(queryBox, blk.GetAABB())) return true;

        if (blk.IsSlope()) {
            float surfY = blk.GetMaxSurfaceUnderBox(queryBox);
            if (queryBox.min.y >= surfY - 0.02f) {
                return true; // Acima da superfície inclinada da rampa
            }
        }
        hit = true;
        return false;
    });
    return hit;
}

inline bool OverlapsAnyBlock(Vector3 pos, Vector3 size, const std::vector<WorldBlock>& world) {
    return OverlapsAnyBlock(pos, size, world, GetWorldSpatialGrid(world));
}

inline float GetClosestWallDistance(Ray ray, float maxDist, const std::vector<WorldBlock>& world, const SpatialHashGrid& grid) {
    float closest = maxDist;
    grid.TraverseRayXZ(ray.position, ray.direction, maxDist, [&](int idx) -> bool {
        RayCollision col = GetRayCollisionBox(ray, world[idx].GetAABB());
        if (col.hit && col.distance >= 0.0f && col.distance < closest) {
            closest = col.distance;
        }
        return true;
    });
    return closest;
}

inline float GetClosestWallDistance(Ray ray, float maxDist, const std::vector<WorldBlock>& world) {
    return GetClosestWallDistance(ray, maxDist, world, GetWorldSpatialGrid(world));
}

inline bool HasLineOfSight(Vector3 from, Vector3 to, const std::vector<WorldBlock>& world, const SpatialHashGrid& grid) {
    Vector3 diff = Vector3Subtract(to, from);
    float dist = Vector3Length(diff);
    if (dist <= 0.001f) return true;

    Ray ray = { from, Vector3Scale(diff, 1.0f / dist) };
    bool blocked = false;

    grid.TraverseRayXZ(from, ray.direction, dist, [&](int idx) -> bool {
        RayCollision col = GetRayCollisionBox(ray, world[idx].GetAABB());
        if (col.hit && col.distance > 0.001f && col.distance < dist - 0.02f) {
            blocked = true;
            return false;
        }
        return true;
    });

    return !blocked;
}

inline bool HasLineOfSight(Vector3 from, Vector3 to, const std::vector<WorldBlock>& world) {
    return HasLineOfSight(from, to, world, GetWorldSpatialGrid(world));
}

// =============================================================================
// CARREGADOR DE MAPAS & GERADOR DE PRESETS
// =============================================================================
class LevelLoader {
public:
    static std::vector<WorldBlock> LoadFromFile(const std::string& filename, MapPreset preset) {
        std::ifstream testFile(filename);
        if (!testFile.is_open()) {
            GeneratePresetFile(filename, preset);
        } else {
            testFile.close();
        }

        std::vector<WorldBlock> blocks;
        std::ifstream file(filename);
        if (!file.is_open()) return blocks;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            WorldBlock b{};
            int r = 120, g = 125, bl = 135, slope = 0;
            if (iss >> b.pos.x >> b.pos.y >> b.pos.z >> b.size.x >> b.size.y >> b.size.z >> r >> g >> bl) {
                if (iss >> slope) {
                    b.slopeDir = std::clamp(slope, 0, 4);
                }
                b.color = {
                    static_cast<unsigned char>(std::clamp(r, 0, 255)),
                    static_cast<unsigned char>(std::clamp(g, 0, 255)),
                    static_cast<unsigned char>(std::clamp(bl, 0, 255)),
                    255
                };
                b.bounds = b.ComputeBounds();
                blocks.push_back(b);
            }
        }
        return blocks;
    }

    static void GeneratePresetFile(const std::string& filename, MapPreset preset) {
        std::ofstream out(filename);
        if (!out.is_open()) return;

        out << "# posX posY posZ tamX tamY tamZ R G B [slopeDir: 0=Caixa, 1=+X, 2=-X, 3=+Z, 4=-Z]\n";
        out << "0.0 -0.5 0.0 52.0 1.0 52.0 58 64 74 0\n";
        out << "0.0 2.5 -26.0 52.0 5.0 1.0 75 82 95 0\n";
        out << "0.0 2.5 26.0 52.0 5.0 1.0 75 82 95 0\n";
        out << "-26.0 2.5 0.0 1.0 5.0 52.0 75 82 95 0\n";
        out << "26.0 2.5 0.0 1.0 5.0 52.0 75 82 95 0\n";

        if (preset == MapPreset::MOVEMENT_COURSE) {
            out << "-6.0 0.5 12.0 4.0 1.0 4.0 95 130 165 0\n";
            out << "0.0 1.1 6.0 4.0 2.2 4.0 95 150 185 0\n";
            out << "6.0 1.7 0.0 4.0 3.4 4.0 95 170 205 0\n";
            out << "0.0 2.2 -7.0 5.0 4.4 4.0 110 190 140 0\n";
            out << "-8.0 2.6 -14.0 5.0 5.2 5.0 210 165 80 0\n";
            // Rampa inclinada (-Z) e escadaria de degraus para teste de step-up
            out << "12.0 1.1 10.0 4.0 2.2 6.0 85 165 140 4\n";
            out << "12.0 1.1 4.5 4.0 2.2 5.0 90 135 160 0\n";
            out << "12.0 0.25 0.5 4.0 0.5 1.2 140 145 155 0\n";
            out << "12.0 0.50 -0.7 4.0 1.0 1.2 140 145 155 0\n";
            out << "12.0 0.75 -1.9 4.0 1.5 1.2 140 145 155 0\n";
            out << "12.0 1.00 -3.1 4.0 2.0 1.2 140 145 155 0\n";
        } else if (preset == MapPreset::FIRING_RANGE) {
            out << "0.0 0.6 9.5 16.0 1.2 0.7 110 118 130 0\n";
            out << "-5.5 1.1 -2.0 2.6 2.2 0.45 165 110 70 0\n";
            out << "5.5 1.1 -2.0 2.6 2.2 0.45 165 110 70 0\n";
            out << "0.0 1.3 -6.5 3.4 2.6 0.9 95 105 120 0\n";
        } else {
            out << "-4.5 0.75 7.5 3.2 1.5 0.8 100 110 125 0\n";
            out << "4.5 0.75 7.5 3.2 1.5 0.8 100 110 125 0\n";
            out << "0.0 1.2 1.5 4.2 2.4 1.1 90 100 115 0\n";
            out << "-9.5 1.35 0.0 2.8 2.7 0.55 155 105 70 0\n";
            out << "9.5 1.35 0.0 2.8 2.7 0.55 155 105 70 0\n";
            out << "-5.5 1.25 -6.0 3.4 2.5 0.9 95 105 120 0\n";
            out << "5.5 1.25 -6.0 3.4 2.5 0.9 95 105 120 0\n";
            out << "0.0 1.5 -12.5 5.5 3.0 1.2 82 92 108 0\n";
        }
    }
};