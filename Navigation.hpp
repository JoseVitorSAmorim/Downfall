#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <cstdint>

struct NavNode {
    int gx = 0;
    int gz = 0;
    Vector3 worldPos = { 0.0f, 0.0f, 0.0f }; // Posição nos pés sobre a superfície caminhável
    int neighbors[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    float neighborCost[8] = { 0 };
    uint8_t neighborCount = 0;
};

class NavGrid {
public:
    static constexpr float CELL_SIZE = 1.0f;
    static constexpr float AGENT_RADIUS = 0.36f;
    static constexpr float AGENT_CLEARANCE_H = 1.45f;
    static constexpr float MAX_CLIMB_STEP = 0.72f; // Suporta step-up e rampas entre células de 1m
    static constexpr float MAX_DROP_STEP  = 2.10f; // Permite descer plataformas/degraus

    void Clear() {
        nodes.clear();
        columnNodes.clear();
        gridW = 0;
        gridD = 0;
    }

    // Gera automaticamente a malha 3D caminhável multi-andar a partir dos WorldBlock
    void Build(const std::vector<WorldBlock>& world, const SpatialHashGrid& spatialGrid) {
        Clear();
        if (world.empty()) return;

        float minX = 1e30f, maxX = -1e30f;
        float minZ = 1e30f, maxZ = -1e30f;

        for (const auto& b : world) {
            BoundingBox bb = b.GetAABB();
            minX = std::min(minX, bb.min.x);
            maxX = std::max(maxX, bb.max.x);
            minZ = std::min(minZ, bb.min.z);
            maxZ = std::max(maxZ, bb.max.z);
        }

        // Limita a área navegável para evitar desperdício em planos infinitos
        minX = std::max(minX, -80.0f);
        maxX = std::min(maxX,  80.0f);
        minZ = std::max(minZ, -80.0f);
        maxZ = std::min(maxZ,  80.0f);

        originX = std::floor(minX);
        originZ = std::floor(minZ);
        gridW = std::max(1, static_cast<int>(std::ceil((maxX - originX) / CELL_SIZE)));
        gridD = std::max(1, static_cast<int>(std::ceil((maxZ - originZ) / CELL_SIZE)));

        columnNodes.resize(static_cast<size_t>(gridW * gridD));
        nodes.reserve(static_cast<size_t>(gridW * gridD));

        std::vector<float> candidateHeights;
        candidateHeights.reserve(8);

        // 1. Extrai todas as superfícies caminháveis válidas por coluna (suporta múltiplos andares)
        for (int gx = 0; gx < gridW; gx++) {
            float wx = originX + (static_cast<float>(gx) + 0.5f) * CELL_SIZE;
            for (int gz = 0; gz < gridD; gz++) {
                float wz = originZ + (static_cast<float>(gz) + 0.5f) * CELL_SIZE;
                candidateHeights.clear();

                Vector3 probeCenter = { wx, 0.0f, wz };
                Vector3 probeSize = { 0.45f, 200.0f, 0.45f };

                spatialGrid.QueryAABB(probeCenter, probeSize, [&](int idx) -> bool {
                    const WorldBlock& b = world[idx];
                    BoundingBox bb = b.GetAABB();
                    if (wx >= bb.min.x - 0.15f && wx <= bb.max.x + 0.15f &&
                        wz >= bb.min.z - 0.15f && wz <= bb.max.z + 0.15f) {
                        float surfY = b.GetSurfaceYAt(wx, wz);
                        candidateHeights.push_back(surfY);
                    }
                    return true;
                });

                if (candidateHeights.empty()) continue;

                std::sort(candidateHeights.begin(), candidateHeights.end());
                std::vector<float> uniqueHeights;
                for (float h : candidateHeights) {
                    if (uniqueHeights.empty() || std::fabs(h - uniqueHeights.back()) > 0.25f) {
                        uniqueHeights.push_back(h);
                    }
                }

                for (float surfY : uniqueHeights) {
                    if (IsSurfaceWalkable(wx, surfY, wz, world, spatialGrid)) {
                        int nodeIdx = static_cast<int>(nodes.size());
                        NavNode node;
                        node.gx = gx;
                        node.gz = gz;
                        node.worldPos = { wx, surfY, wz };
                        nodes.push_back(node);
                        columnNodes[static_cast<size_t>(gx * gridD + gz)].push_back(nodeIdx);
                    }
                }
            }
        }

        // 2. Conecta vizinhos 8-direcionais respeitando desníveis, rampas e quinas de paredes
        static const int dxTable[8] = {  1, -1,  0,  0,  1,  1, -1, -1 };
        static const int dzTable[8] = {  0,  0,  1, -1,  1, -1,  1, -1 };

        for (size_t i = 0; i < nodes.size(); i++) {
            NavNode& cur = nodes[i];

            for (int dir = 0; dir < 8; dir++) {
                int nx = cur.gx + dxTable[dir];
                int nz = cur.gz + dzTable[dir];
                if (nx < 0 || nx >= gridW || nz < 0 || nz >= gridD) continue;

                int targetNode = FindCompatibleNodeInColumn(nx, nz, cur.worldPos.y);
                if (targetNode == -1) continue;

                // Para diagonais, exige que ambos os eixos cardinais sejam transitáveis (evita travar em quinas)
                if (dxTable[dir] != 0 && dzTable[dir] != 0) {
                    int sideA = FindCompatibleNodeInColumn(cur.gx + dxTable[dir], cur.gz, cur.worldPos.y);
                    int sideB = FindCompatibleNodeInColumn(cur.gx, cur.gz + dzTable[dir], cur.worldPos.y);
                    if (sideA == -1 || sideB == -1) continue;
                }

                float dist3D = Vector3Distance(cur.worldPos, nodes[targetNode].worldPos);
                float dy = nodes[targetNode].worldPos.y - cur.worldPos.y;
                float slopePenalty = (dy > 0.05f) ? (dy * 0.6f) : 0.0f;

                cur.neighbors[cur.neighborCount] = targetNode;
                cur.neighborCost[cur.neighborCount] = dist3D + slopePenalty;
                cur.neighborCount++;
            }
        }
    }

    int FindNearestNode(Vector3 pos) const {
        if (nodes.empty()) return -1;

        int centerGx = std::clamp(static_cast<int>(std::floor((pos.x - originX) / CELL_SIZE)), 0, gridW - 1);
        int centerGz = std::clamp(static_cast<int>(std::floor((pos.z - originZ) / CELL_SIZE)), 0, gridD - 1);

        int bestIdx = -1;
        float bestScore = 1e30f;

        for (int radius = 0; radius <= 4; radius++) {
            for (int dx = -radius; dx <= radius; dx++) {
                for (int dz = -radius; dz <= radius; dz++) {
                    if (radius > 0 && std::abs(dx) != radius && std::abs(dz) != radius) continue;
                    int gx = centerGx + dx;
                    int gz = centerGz + dz;
                    if (gx < 0 || gx >= gridW || gz < 0 || gz >= gridD) continue;

                    const auto& col = columnNodes[static_cast<size_t>(gx * gridD + gz)];
                    for (int nIdx : col) {
                        const Vector3& np = nodes[nIdx].worldPos;
                        float dxW = np.x - pos.x;
                        float dzW = np.z - pos.z;
                        float dyW = std::fabs(np.y - pos.y);
                        float score = std::sqrt(dxW * dxW + dzW * dzW) + dyW * 2.5f;
                        if (score < bestScore) {
                            bestScore = score;
                            bestIdx = nIdx;
                        }
                    }
                }
            }
            if (bestIdx != -1 && bestScore < 1.35f) return bestIdx;
        }
        return bestIdx;
    }

    // Verifica se há linha caminhável limpa entre dois pontos (sem paredes e sem buracos)
    bool HasClearWalkLine(Vector3 from, Vector3 to, const std::vector<WorldBlock>& world, const SpatialHashGrid& spatialGrid) const {
        Vector3 diff = { to.x - from.x, 0.0f, to.z - from.z };
        float horizDist = std::sqrt(diff.x * diff.x + diff.z * diff.z);
        if (horizDist < 0.25f) return std::fabs(to.y - from.y) <= MAX_CLIMB_STEP;

        Vector3 dir = { diff.x / horizDist, 0.0f, diff.z / horizDist };
        Vector3 perp = { -dir.z * 0.34f, 0.0f, dir.x * 0.34f };

        int steps = std::max(2, static_cast<int>(std::ceil(horizDist / 0.55f)));
        float prevY = from.y;

        for (int s = 1; s <= steps; s++) {
            float t = static_cast<float>(s) / static_cast<float>(steps);
            Vector3 p = {
                from.x + diff.x * t,
                from.y + (to.y - from.y) * t,
                from.z + diff.z * t
            };

            int nodeIdx = FindNearestNode(p);
            if (nodeIdx == -1) return false;
            float floorY = nodes[nodeIdx].worldPos.y;
            if (floorY - prevY > MAX_CLIMB_STEP || prevY - floorY > MAX_DROP_STEP) return false;
            prevY = floorY;

            // Verifica folga lateral do corpo (ombro esquerdo, centro, ombro direito)
            Vector3 torsoSize = { 0.42f, 1.05f, 0.42f };
            Vector3 cMid   = { p.x, floorY + 0.95f, p.z };
            Vector3 cLeft  = { p.x + perp.x, floorY + 0.95f, p.z + perp.z };
            Vector3 cRight = { p.x - perp.x, floorY + 0.95f, p.z - perp.z };

            if (OverlapsAnyBlock(cMid, torsoSize, world, spatialGrid) ||
                OverlapsAnyBlock(cLeft, torsoSize, world, spatialGrid) ||
                OverlapsAnyBlock(cRight, torsoSize, world, spatialGrid)) {
                return false;
            }
        }
        return true;
    }

    // Busca A* + Suavização String-Pulling
    bool FindPath(Vector3 startPos, Vector3 goalPos,
                  const std::vector<WorldBlock>& world,
                  const SpatialHashGrid& spatialGrid,
                  std::vector<Vector3>& outPath) const {
        outPath.clear();
        if (nodes.empty()) {
            outPath.push_back(goalPos);
            return false;
        }

        // Atalho direto se o caminho estiver livre sem obstáculos ou desníveis abruptos
        if (HasClearWalkLine(startPos, goalPos, world, spatialGrid)) {
            outPath.push_back(goalPos);
            return true;
        }

        int startNode = FindNearestNode(startPos);
        int goalNode = FindNearestNode(goalPos);
        if (startNode == -1 || goalNode == -1) {
            outPath.push_back(goalPos);
            return false;
        }

        if (startNode == goalNode) {
            outPath.push_back(goalPos);
            return true;
        }

        const size_t nCount = nodes.size();
        if (gScore.size() < nCount) {
            gScore.resize(nCount);
            cameFrom.resize(nCount);
            closedStamp.resize(nCount, 0);
        }

        searchStamp++;
        if (searchStamp == 0) {
            std::fill(closedStamp.begin(), closedStamp.end(), 0);
            searchStamp = 1;
        }
        const uint32_t stamp = searchStamp;

        struct OpenEntry {
            int node;
            float fScore;
            bool operator>(const OpenEntry& other) const noexcept {
                return fScore > other.fScore;
            }
        };

        std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>> openSet;

        gScore[startNode] = 0.0f;
        cameFrom[startNode] = -1;
        closedStamp[startNode] = stamp; // Marcado como inicializado
        openSet.push({ startNode, Heuristic(nodes[startNode].worldPos, nodes[goalNode].worldPos) });

        int bestFallbackNode = startNode;
        float bestFallbackH = Heuristic(nodes[startNode].worldPos, nodes[goalNode].worldPos);
        bool reachedGoal = false;
        int expansions = 0;
        const int maxExpansions = 4500;

        while (!openSet.empty() && expansions < maxExpansions) {
            OpenEntry cur = openSet.top();
            openSet.pop();
            int u = cur.node;

            if (u == goalNode) {
                reachedGoal = true;
                break;
            }

            expansions++;
            const NavNode& uNode = nodes[u];
            float uG = gScore[u];

            for (uint8_t k = 0; k < uNode.neighborCount; k++) {
                int v = uNode.neighbors[k];
                float tentativeG = uG + uNode.neighborCost[k];

                if (closedStamp[v] != stamp || tentativeG < gScore[v]) {
                    closedStamp[v] = stamp;
                    gScore[v] = tentativeG;
                    cameFrom[v] = u;
                    float h = Heuristic(nodes[v].worldPos, nodes[goalNode].worldPos);
                    if (h < bestFallbackH) {
                        bestFallbackH = h;
                        bestFallbackNode = v;
                    }
                    openSet.push({ v, tentativeG + h });
                }
            }
        }

        int targetEndNode = reachedGoal ? goalNode : bestFallbackNode;
        if (targetEndNode == startNode) {
            outPath.push_back(goalPos);
            return false;
        }

        // Reconstrói cadeia bruta
        std::vector<Vector3> rawPath;
        int curr = targetEndNode;
        while (curr != -1 && curr != startNode) {
            rawPath.push_back(nodes[curr].worldPos);
            curr = cameFrom[curr];
        }
        std::reverse(rawPath.begin(), rawPath.end());
        if (reachedGoal) {
            rawPath.push_back(goalPos);
        }

        // Suavização String-Pulling: remove pontos intermediários redundantes com linha de visão caminhável
        Vector3 anchor = startPos;
        size_t idx = 0;
        while (idx < rawPath.size()) {
            size_t furthest = idx;
            for (size_t probe = rawPath.size() - 1; probe > idx; probe--) {
                if (HasClearWalkLine(anchor, rawPath[probe], world, spatialGrid)) {
                    furthest = probe;
                    break;
                }
            }
            outPath.push_back(rawPath[furthest]);
            anchor = rawPath[furthest];
            idx = furthest + 1;
        }

        return reachedGoal;
    }

    size_t GetNodeCount() const noexcept { return nodes.size(); }

private:
    float originX = 0.0f;
    float originZ = 0.0f;
    int gridW = 0;
    int gridD = 0;

    std::vector<NavNode> nodes;
    std::vector<std::vector<int>> columnNodes;

    mutable std::vector<float> gScore;
    mutable std::vector<int> cameFrom;
    mutable std::vector<uint32_t> closedStamp;
    mutable uint32_t searchStamp = 1;

    static float Heuristic(Vector3 a, Vector3 b) noexcept {
        float dx = std::fabs(a.x - b.x);
        float dz = std::fabs(a.z - b.z);
        float dy = std::fabs(a.y - b.y);
        float octile = (dx + dz) + (1.4142135f - 2.0f) * std::min(dx, dz);
        return octile + dy * 1.5f;
    }

    int FindCompatibleNodeInColumn(int gx, int gz, float refY) const {
        const auto& col = columnNodes[static_cast<size_t>(gx * gridD + gz)];
        int bestNode = -1;
        float bestDy = 1e30f;
        for (int idx : col) {
            float dy = nodes[idx].worldPos.y - refY;
            if (dy <= MAX_CLIMB_STEP && dy >= -MAX_DROP_STEP) {
                float absDy = std::fabs(dy);
                if (absDy < bestDy) {
                    bestDy = absDy;
                    bestNode = idx;
                }
            }
        }
        return bestNode;
    }

    static bool IsSurfaceWalkable(float wx, float surfY, float wz,
                                  const std::vector<WorldBlock>& world,
                                  const SpatialHashGrid& spatialGrid) {
        // Verifica se há espaço livre acima da superfície (altura mínima agachada/em pé de 1.45m)
        Vector3 bodyCenter = { wx, surfY + 0.14f + AGENT_CLEARANCE_H * 0.5f, wz };
        Vector3 bodySize   = { AGENT_RADIUS * 1.85f, AGENT_CLEARANCE_H, AGENT_RADIUS * 1.85f };
        BoundingBox bodyBox = {
            { bodyCenter.x - bodySize.x * 0.5f, bodyCenter.y - bodySize.y * 0.5f, bodyCenter.z - bodySize.z * 0.5f },
            { bodyCenter.x + bodySize.x * 0.5f, bodyCenter.y + bodySize.y * 0.5f, bodyCenter.z + bodySize.z * 0.5f }
        };

        bool blocked = false;
        spatialGrid.QueryAABB(bodyCenter, bodySize, [&](int idx) -> bool {
            const WorldBlock& b = world[idx];
            BoundingBox bb = b.GetAABB();
            if (!CheckOverlapStrict(bodyBox, bb)) return true;

            if (b.IsSlope()) {
                float rampTop = b.GetMaxSurfaceUnderBox(bodyBox);
                if (surfY >= rampTop - 0.12f) return true;
            }
            blocked = true;
            return false;
        });

        return !blocked;
    }
};

// Cache sincronizado automaticamente com o mapa ativo
inline const NavGrid& GetWorldNavGrid(const std::vector<WorldBlock>& world) {
    static NavGrid cachedNav;
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
        const SpatialHashGrid& sGrid = GetWorldSpatialGrid(world);
        cachedNav.Build(world, sGrid);
        lastPtr = curPtr;
        lastSize = world.size();
        lastFingerprint = fp;
    }

    return cachedNav;
}