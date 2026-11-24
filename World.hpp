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

enum class MapPreset {
    SQUAD_ARENA = 0,
    MOVEMENT_COURSE,
    FIRING_RANGE
};

// =============================================================================
// ENTIDADES JOGÁVEIS SERIALIZÁVEIS (SAVEPOINT_23 - DATA-DRIVEN SCENE)
// =============================================================================
enum class SceneEntityType : int {
    SPAWN_ALLY  = 0, // paramA: slot (0=Alpha, 1=Bravo, 2=Charlie, 3=Delta)
    SPAWN_ENEMY = 1, // paramA: minPhase (1..3), paramB: isReinforcement (0 ou 1)
    TERMINAL    = 2, // paramA: terminalId (1, 2, ...)
    BARREL      = 3, // Barril explosivo
    CRATE       = 4, // Caixa de suprimentos
    EXTRACT     = 5, // size.x = raio da zona de extração
    TRIGGER     = 6  // size = dimensões da caixa AABB, paramA = cutsceneId, paramB = requiresExtract (0/1)
};

struct SceneEntityMarker {
    SceneEntityType type = SceneEntityType::BARREL;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 size = { 1.0f, 1.0f, 1.0f };
    int paramA = 0;
    int paramB = 0;
    std::string tag = "";

    BoundingBox GetAABB() const noexcept {
        if (type == SceneEntityType::TRIGGER) {
            Vector3 half = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
            return {
                { pos.x - half.x, pos.y - half.y, pos.z - half.z },
                { pos.x + half.x, pos.y + half.y, pos.z + half.z }
            };
        }
        float r = (type == SceneEntityType::EXTRACT) ? std::max(0.6f, size.x) : 0.55f;
        float h = (type == SceneEntityType::EXTRACT) ? 0.35f : 1.35f;
        return {
            { pos.x - r, pos.y, pos.z - r },
            { pos.x + r, pos.y + h, pos.z + r }
        };
    }
};

inline const char* SceneEntityTypeToToken(SceneEntityType type) noexcept {
    switch (type) {
        case SceneEntityType::SPAWN_ALLY:  return "SPAWN_ALLY";
        case SceneEntityType::SPAWN_ENEMY: return "SPAWN_ENEMY";
        case SceneEntityType::TERMINAL:    return "TERMINAL";
        case SceneEntityType::BARREL:      return "BARREL";
        case SceneEntityType::CRATE:       return "CRATE";
        case SceneEntityType::EXTRACT:     return "EXTRACT";
        case SceneEntityType::TRIGGER:     return "TRIGGER";
        default:                           return "BARREL";
    }
}

inline bool TryParseSceneEntityToken(const std::string& token, SceneEntityType& outType) noexcept {
    if (token == "SPAWN_ALLY")  { outType = SceneEntityType::SPAWN_ALLY;  return true; }
    if (token == "SPAWN_ENEMY") { outType = SceneEntityType::SPAWN_ENEMY; return true; }
    if (token == "TERMINAL")    { outType = SceneEntityType::TERMINAL;    return true; }
    if (token == "BARREL")      { outType = SceneEntityType::BARREL;      return true; }
    if (token == "CRATE")       { outType = SceneEntityType::CRATE;       return true; }
    if (token == "EXTRACT")     { outType = SceneEntityType::EXTRACT;     return true; }
    if (token == "TRIGGER")     { outType = SceneEntityType::TRIGGER;     return true; }
    return false;
}

inline Color GetSceneEntityColor(SceneEntityType type) noexcept {
    switch (type) {
        case SceneEntityType::SPAWN_ALLY:  return SKYBLUE;
        case SceneEntityType::SPAWN_ENEMY: return RED;
        case SceneEntityType::TERMINAL:    return { 45, 185, 255, 255 };
        case SceneEntityType::BARREL:      return ORANGE;
        case SceneEntityType::CRATE:       return LIME;
        case SceneEntityType::EXTRACT:     return GREEN;
        case SceneEntityType::TRIGGER:     return GOLD;
        default:                           return WHITE;
    }
}

struct WorldBlock {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 size = { 1.0f, 1.0f, 1.0f };
    Color color = GRAY;
    BoundingBox bounds = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    int slopeDir = 0; // 0 = Caixa retangular padrão, 1..4 = Rampa inclinada

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

struct SceneData {
    std::vector<WorldBlock> blocks;
    std::vector<SceneEntityMarker> entities;
};

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
        case SlopeDir::POS_X:
            tA = { x1, y1, z0 }; tB = { x1, y1, z1 };
            DrawTriangle3D(b00, b01, tB, block.color);
            DrawTriangle3D(b00, tB, tA, block.color);
            DrawTriangle3D(b10, tA, tB, sideCol);
            DrawTriangle3D(b10, tB, b11, sideCol);
            DrawTriangle3D(b00, tA, b10, sideCol);
            DrawTriangle3D(b01, b11, tB, sideCol);
            break;
        case SlopeDir::NEG_X:
            tA = { x0, y1, z0 }; tB = { x0, y1, z1 };
            DrawTriangle3D(tA, tB, b11, block.color);
            DrawTriangle3D(tA, b11, b10, block.color);
            DrawTriangle3D(b00, b01, tB, sideCol);
            DrawTriangle3D(b00, tB, tA, sideCol);
            DrawTriangle3D(b00, tA, b10, sideCol);
            DrawTriangle3D(b01, b11, tB, sideCol);
            break;
        case SlopeDir::POS_Z:
            tA = { x0, y1, z1 }; tB = { x1, y1, z1 };
            DrawTriangle3D(b00, tA, tB, block.color);
            DrawTriangle3D(b00, tB, b10, block.color);
            DrawTriangle3D(b01, b11, tB, sideCol);
            DrawTriangle3D(b01, tB, tA, sideCol);
            DrawTriangle3D(b00, b01, tA, sideCol);
            DrawTriangle3D(b10, tB, b11, sideCol);
            break;
        case SlopeDir::NEG_Z:
            tA = { x0, y1, z0 }; tB = { x1, y1, z0 };
            DrawTriangle3D(tA, b01, b11, block.color);
            DrawTriangle3D(tA, b11, tB, block.color);
            DrawTriangle3D(b00, tA, tB, sideCol);
            DrawTriangle3D(b00, tB, b10, sideCol);
            DrawTriangle3D(b00, b01, tA, sideCol);
            DrawTriangle3D(b10, tB, b11, sideCol);
            break;
        default:
            break;
    }

    DrawTriangle3D(b00, b10, b11, sideCol);
    DrawTriangle3D(b00, b11, b01, sideCol);

    DrawLine3D(b00, b10, BLACK);
    DrawLine3D(b10, b11, BLACK);
    DrawLine3D(b11, b01, BLACK);
    DrawLine3D(b01, b00, BLACK);
    DrawLine3D(tA, tB, BLACK);
}

struct Tracer {
    Vector3 start;
    Vector3 end;
    Color color;
    float timer;
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
// SPATIAL HASH GRID (4m x 4m XZ)
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
                return true;
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
// SERIALIZADOR & CARREGADOR DE CENÁRIOS DATA-DRIVEN (.scene / .txt)
// =============================================================================
class LevelLoader {
public:
    static std::vector<SceneEntityMarker> BuildDefaultEntitiesForPreset(MapPreset preset) {
        std::vector<SceneEntityMarker> ents;

        if (preset == MapPreset::MOVEMENT_COURSE) {
            ents.push_back({ SceneEntityType::SPAWN_ALLY, { 0.0f, 0.5f, 20.0f }, { 1.0f, 1.0f, 1.0f }, 0, 0, "Alpha_Start" });
            return ents;
        }

        if (preset == MapPreset::FIRING_RANGE) {
            ents.push_back({ SceneEntityType::SPAWN_ALLY,  {  0.0f, 0.5f, 14.0f }, { 1.0f, 1.0f, 1.0f }, 0, 0, "Alpha" });
            ents.push_back({ SceneEntityType::SPAWN_ALLY,  { -4.5f, 0.0f,  6.5f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Bravo" });
            ents.push_back({ SceneEntityType::SPAWN_ALLY,  {  4.5f, 0.0f,  6.5f }, { 1.0f, 1.0f, 1.0f }, 2, 0, "Charlie" });

            ents.push_back({ SceneEntityType::SPAWN_ENEMY, { -8.0f, 0.0f, -12.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Alvo_Oeste" });
            ents.push_back({ SceneEntityType::SPAWN_ENEMY, { -2.8f, 0.0f, -15.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Alvo_Centro_Oeste" });
            ents.push_back({ SceneEntityType::SPAWN_ENEMY, {  2.8f, 0.0f, -15.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Alvo_Centro_Leste" });
            ents.push_back({ SceneEntityType::SPAWN_ENEMY, {  8.0f, 0.0f, -12.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Alvo_Leste" });

            ents.push_back({ SceneEntityType::CRATE,  { -7.5f, 0.0f,   9.5f }, { 1.15f, 0.80f, 0.75f }, 0, 0, "Suprimento_Oeste" });
            ents.push_back({ SceneEntityType::CRATE,  {  7.5f, 0.0f,   9.5f }, { 1.15f, 0.80f, 0.75f }, 0, 0, "Suprimento_Leste" });
            ents.push_back({ SceneEntityType::BARREL, { -3.2f, 0.0f, -13.5f }, { 0.78f, 1.25f, 0.78f }, 0, 0, "Barril_01" });
            ents.push_back({ SceneEntityType::BARREL, {  4.2f, 0.0f, -13.5f }, { 0.78f, 1.25f, 0.78f }, 0, 0, "Barril_02" });
            ents.push_back({ SceneEntityType::BARREL, { -9.5f, 0.0f,  -8.0f }, { 0.78f, 1.25f, 0.78f }, 0, 0, "Barril_03" });
            ents.push_back({ SceneEntityType::BARREL, {  9.5f, 0.0f,  -8.0f }, { 0.78f, 1.25f, 0.78f }, 0, 0, "Barril_04" });
            return ents;
        }

        // MapPreset::SQUAD_ARENA (Campanha Tática Completa)
        ents.push_back({ SceneEntityType::SPAWN_ALLY,  {  0.0f, 0.5f, 14.0f }, { 1.0f, 1.0f, 1.0f }, 0, 0, "Alpha" });
        ents.push_back({ SceneEntityType::SPAWN_ALLY,  { -5.5f, 0.0f,  4.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Bravo" });
        ents.push_back({ SceneEntityType::SPAWN_ALLY,  {  5.5f, 0.0f,  4.0f }, { 1.0f, 1.0f, 1.0f }, 2, 0, "Charlie" });
        ents.push_back({ SceneEntityType::SPAWN_ALLY,  {  9.0f, 0.0f, -2.0f }, { 1.0f, 1.0f, 1.0f }, 3, 0, "Delta" });

        // Hostis iniciais (Fase 1+)
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, { -8.0f, 0.0f, -12.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Sentinela_Oeste" });
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, { -2.8f, 0.0f, -15.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Guarda_Centro_A" });
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, {  2.8f, 0.0f, -15.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Guarda_Centro_B" });
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, {  8.0f, 0.0f, -12.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Sentinela_Leste" });

        // Hostis escalonados (Fases 2 e 3)
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, { -11.5f, 0.0f,  -6.5f }, { 1.0f, 1.0f, 1.0f }, 2, 0, "Flanco_Oeste_F2" });
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, {  11.5f, 0.0f,  -6.5f }, { 1.0f, 1.0f, 1.0f }, 2, 0, "Flanco_Leste_F2" });
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, {   0.0f, 0.0f, -15.5f }, { 1.0f, 1.0f, 1.0f }, 3, 0, "Comandante_F3" });

        // Reforços pós-hack (paramB = 1)
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, { -11.0f, 0.0f, -19.0f }, { 1.0f, 1.0f, 1.0f }, 1, 1, "Reforco_Oeste" });
        ents.push_back({ SceneEntityType::SPAWN_ENEMY, {  11.0f, 0.0f, -19.0f }, { 1.0f, 1.0f, 1.0f }, 1, 1, "Reforco_Leste" });

        // Objetivos e Suprimentos
        ents.push_back({ SceneEntityType::TERMINAL, { -13.0f, 0.0f, -4.8f }, { 0.75f, 1.30f, 0.55f }, 1, 0, "Terminal_Alpha_(Oeste)" });
        ents.push_back({ SceneEntityType::TERMINAL, {  13.0f, 0.0f, -4.8f }, { 0.75f, 1.30f, 0.55f }, 2, 0, "Terminal_Bravo_(Leste)" });

        ents.push_back({ SceneEntityType::CRATE,  { -7.5f, 0.0f,   9.5f }, { 1.15f, 0.80f, 0.75f }, 0, 0, "Suprimento_Oeste" });
        ents.push_back({ SceneEntityType::CRATE,  {  7.5f, 0.0f,   9.5f }, { 1.15f, 0.80f, 0.75f }, 0, 0, "Suprimento_Leste" });

        ents.push_back({ SceneEntityType::BARREL, { -3.2f, 0.0f, -13.5f }, { 0.78f, 1.25f, 0.78f }, 0, 0, "Barril_Central_A" });
        ents.push_back({ SceneEntityType::BARREL, {  4.2f, 0.0f, -13.5f }, { 0.78f, 1.25f, 0.78f }, 0, 0, "Barril_Central_B" });
        ents.push_back({ SceneEntityType::BARREL, { -9.5f, 0.0f,  -8.0f }, { 0.78f, 1.25f, 0.78f }, 0, 0, "Barril_Flanco_Oeste" });
        ents.push_back({ SceneEntityType::BARREL, {  9.5f, 0.0f,  -8.0f }, { 0.78f, 1.25f, 0.78f }, 0, 0, "Barril_Flanco_Leste" });

        ents.push_back({ SceneEntityType::EXTRACT, { 0.0f, 0.0f, 16.5f }, { 3.2f, 0.2f, 3.2f }, 0, 0, "Zona_Extracao" });

        // Gatilhos de Cutscene (pos = centro, size = dimensões, paramA = cutsceneId, paramB = requiresExtractionActive)
        ents.push_back({ SceneEntityType::TRIGGER, { 0.0f, 2.0f,  3.5f }, { 28.0f, 4.0f, 4.0f }, 1, 0, "TRIGGER:_PATIO_CENTRAL" });
        ents.push_back({ SceneEntityType::TRIGGER, { 0.0f, 2.0f, 12.6f }, { 20.0f, 4.0f, 3.2f }, 2, 1, "TRIGGER:_EXTRACAO" });

        return ents;
    }

    static bool SaveSceneToFile(const std::string& filename, const SceneData& scene) {
        std::ofstream out(filename);
        if (!out.is_open()) return false;

        out << "# ==========================================================================\n";
        out << "# CENARIO DATA-DRIVEN (SAVEPOINT_23)\n";
        out << "# BLOCK posX posY posZ tamX tamY tamZ R G B [SLOPE 1..4]\n";
        out << "# ENTITY_TYPE posX posY posZ sizeX sizeY sizeZ paramA paramB tag_sem_espacos\n";
        out << "# ==========================================================================\n\n";

        for (const auto& b : scene.blocks) {
            out << "BLOCK "
                << b.pos.x << " " << b.pos.y << " " << b.pos.z << " "
                << b.size.x << " " << b.size.y << " " << b.size.z << " "
                << (int)b.color.r << " " << (int)b.color.g << " " << (int)b.color.b;
            if (b.IsSlope()) {
                out << " SLOPE " << b.slopeDir;
            }
            out << "\n";
        }

        if (!scene.entities.empty()) {
            out << "\n# ENTIDADES JOGAVEIS E GATILHOS\n";
        }

        for (const auto& e : scene.entities) {
            std::string safeTag = e.tag.empty() ? "Entidade" : e.tag;
            std::replace(safeTag.begin(), safeTag.end(), ' ', '_');
            out << SceneEntityTypeToToken(e.type) << " "
                << e.pos.x << " " << e.pos.y << " " << e.pos.z << " "
                << e.size.x << " " << e.size.y << " " << e.size.z << " "
                << e.paramA << " " << e.paramB << " "
                << safeTag << "\n";
        }

        return true;
    }

    static SceneData LoadSceneFromFile(const std::string& filename, MapPreset preset) {
        std::ifstream testFile(filename);
        if (!testFile.is_open()) {
            GeneratePresetFile(filename, preset);
        } else {
            testFile.close();
        }

        SceneData scene;
        std::ifstream file(filename);
        if (!file.is_open()) return scene;

        bool hasAnyTypedEntityLine = false;
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            std::string firstToken;
            if (!(iss >> firstToken)) continue;

            SceneEntityType entType;
            if (TryParseSceneEntityToken(firstToken, entType)) {
                hasAnyTypedEntityLine = true;
                SceneEntityMarker m{};
                m.type = entType;
                if (iss >> m.pos.x >> m.pos.y >> m.pos.z >> m.size.x >> m.size.y >> m.size.z) {
                    if (!(iss >> m.paramA)) m.paramA = 0;
                    if (!(iss >> m.paramB)) m.paramB = 0;
                    std::string rawTag;
                    if (iss >> rawTag) {
                        std::replace(rawTag.begin(), rawTag.end(), '_', ' ');
                        m.tag = rawTag;
                    }
                    scene.entities.push_back(m);
                }
                continue;
            }

            // Suporta tanto formato novo ("BLOCK x y z ...") quanto legado ("x y z ...")
            std::istringstream blockStream;
            if (firstToken == "BLOCK") {
                blockStream.str(line.substr(5));
            } else {
                blockStream.str(line);
            }

            WorldBlock b{};
            b.slopeDir = 0;
            int r = 120, g = 125, bl = 135;
            if (blockStream >> b.pos.x >> b.pos.y >> b.pos.z >> b.size.x >> b.size.y >> b.size.z >> r >> g >> bl) {
                std::string extraTag;
                int slopeVal = 0;
                if ((blockStream >> extraTag) && (extraTag == "SLOPE" || extraTag == "RAMP") && (blockStream >> slopeVal)) {
                    b.slopeDir = std::clamp(slopeVal, 1, 4);
                }
                b.color = {
                    static_cast<unsigned char>(std::clamp(r, 0, 255)),
                    static_cast<unsigned char>(std::clamp(g, 0, 255)),
                    static_cast<unsigned char>(std::clamp(bl, 0, 255)),
                    255
                };
                b.bounds = b.ComputeBounds();
                scene.blocks.push_back(b);
            }
        }

        // Retrocompatibilidade: se o arquivo de mapa era antigo (só blocos), injeta as entidades padrão do preset
        if (!hasAnyTypedEntityLine && scene.entities.empty()) {
            scene.entities = BuildDefaultEntitiesForPreset(preset);
        }

        return scene;
    }

    static std::vector<WorldBlock> LoadFromFile(const std::string& filename, MapPreset preset) {
        return LoadSceneFromFile(filename, preset).blocks;
    }

    static void GeneratePresetFile(const std::string& filename, MapPreset preset) {
        SceneData scene;
        auto addBlk = [&](Vector3 pos, Vector3 size, Color col, int slope = 0) {
            WorldBlock b{};
            b.pos = pos;
            b.size = size;
            b.color = col;
            b.slopeDir = slope;
            b.bounds = b.ComputeBounds();
            scene.blocks.push_back(b);
        };

        addBlk({   0.0f, -0.5f,   0.0f }, { 52.0f, 1.0f, 52.0f }, { 58, 64, 74, 255 });
        addBlk({   0.0f,  2.5f, -26.0f }, { 52.0f, 5.0f,  1.0f }, { 75, 82, 95, 255 });
        addBlk({   0.0f,  2.5f,  26.0f }, { 52.0f, 5.0f,  1.0f }, { 75, 82, 95, 255 });
        addBlk({ -26.0f,  2.5f,   0.0f }, {  1.0f, 5.0f, 52.0f }, { 75, 82, 95, 255 });
        addBlk({  26.0f,  2.5f,   0.0f }, {  1.0f, 5.0f, 52.0f }, { 75, 82, 95, 255 });

        if (preset == MapPreset::MOVEMENT_COURSE) {
            addBlk({ -6.0f, 0.5f,  12.0f }, { 4.0f, 1.0f, 4.0f }, {  95, 130, 165, 255 });
            addBlk({  0.0f, 1.1f,   6.0f }, { 4.0f, 2.2f, 4.0f }, {  95, 150, 185, 255 });
            addBlk({  6.0f, 1.7f,   0.0f }, { 4.0f, 3.4f, 4.0f }, {  95, 170, 205, 255 });
            addBlk({  0.0f, 2.2f,  -7.0f }, { 5.0f, 4.4f, 4.0f }, { 110, 190, 140, 255 });
            addBlk({ -8.0f, 2.6f, -14.0f }, { 5.0f, 5.2f, 5.0f }, { 210, 165,  80, 255 });
            addBlk({ 12.0f, 1.1f,  10.0f }, { 4.0f, 2.2f, 6.0f }, {  85, 165, 140, 255 }, 4);
            addBlk({ 12.0f, 1.1f,   4.5f }, { 4.0f, 2.2f, 5.0f }, {  90, 135, 160, 255 });
            addBlk({ 12.0f, 0.25f,  0.5f }, { 4.0f, 0.5f, 1.2f }, { 140, 145, 155, 255 });
            addBlk({ 12.0f, 0.50f, -0.7f }, { 4.0f, 1.0f, 1.2f }, { 140, 145, 155, 255 });
            addBlk({ 12.0f, 0.75f, -1.9f }, { 4.0f, 1.5f, 1.2f }, { 140, 145, 155, 255 });
            addBlk({ 12.0f, 1.00f, -3.1f }, { 4.0f, 2.0f, 1.2f }, { 140, 145, 155, 255 });
        } else if (preset == MapPreset::FIRING_RANGE) {
            addBlk({  0.0f, 0.6f,  9.5f }, { 16.0f, 1.2f, 0.70f }, { 110, 118, 130, 255 });
            addBlk({ -5.5f, 1.1f, -2.0f }, {  2.6f, 2.2f, 0.45f }, { 165, 110,  70, 255 });
            addBlk({  5.5f, 1.1f, -2.0f }, {  2.6f, 2.2f, 0.45f }, { 165, 110,  70, 255 });
            addBlk({  0.0f, 1.3f, -6.5f }, {  3.4f, 2.6f, 0.90f }, {  95, 105, 120, 255 });
        } else {
            addBlk({ -4.5f, 0.75f,   7.5f }, { 3.2f, 1.5f, 0.80f }, { 100, 110, 125, 255 });
            addBlk({  4.5f, 0.75f,   7.5f }, { 3.2f, 1.5f, 0.80f }, { 100, 110, 125, 255 });
            addBlk({  0.0f, 1.20f,   1.5f }, { 4.2f, 2.4f, 1.10f }, {  90, 100, 115, 255 });
            addBlk({ -9.5f, 1.35f,   0.0f }, { 2.8f, 2.7f, 0.55f }, { 155, 105,  70, 255 });
            addBlk({  9.5f, 1.35f,   0.0f }, { 2.8f, 2.7f, 0.55f }, { 155, 105,  70, 255 });
            addBlk({ -5.5f, 1.25f,  -6.0f }, { 3.4f, 2.5f, 0.90f }, {  95, 105, 120, 255 });
            addBlk({  5.5f, 1.25f,  -6.0f }, { 3.4f, 2.5f, 0.90f }, {  95, 105, 120, 255 });
            addBlk({  0.0f, 1.50f, -12.5f }, { 5.5f, 3.0f, 1.20f }, {  82,  92, 108, 255 });
        }

        scene.entities = BuildDefaultEntitiesForPreset(preset);
        SaveSceneToFile(filename, scene);
    }
};