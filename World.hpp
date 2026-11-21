#pragma once

#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

enum class MapPreset {
    SQUAD_ARENA,
    MOVEMENT_COURSE,
    FIRING_RANGE
};

struct WorldBlock {
    Vector3 pos;
    Vector3 size;
    Color color;
    BoundingBox bounds;

    void UpdateBounds() {
        Vector3 half = Vector3Scale(size, 0.5f);
        bounds.min = Vector3Subtract(pos, half);
        bounds.max = Vector3Add(pos, half);
    }
};

struct Tracer {
    Vector3 start;
    Vector3 end;
    Color color;
    float timer;
};

// Checagem estrita (< e >) evita que tocar o chão trave o movimento lateral em X/Z
inline bool CheckOverlapStrict(const BoundingBox& a, const BoundingBox& b) {
    return (a.min.x < b.max.x && a.max.x > b.min.x) &&
           (a.min.y < b.max.y && a.max.y > b.min.y) &&
           (a.min.z < b.max.z && a.max.z > b.min.z);
}

// Verifica se existe linha de visão limpa entre dois pontos sem paredes bloqueando
inline bool HasLineOfSight(Vector3 from, Vector3 to, const std::vector<WorldBlock>& world) {
    Vector3 diff = Vector3Subtract(to, from);
    float dist = Vector3Length(diff);
    if (dist < 0.01f) return true;

    Ray ray = { from, Vector3Scale(diff, 1.0f / dist) };
    for (const auto& block : world) {
        RayCollision col = GetRayCollisionBox(ray, block.bounds);
        if (col.hit && col.distance < dist) {
            return false;
        }
    }
    return true;
}

inline float GetClosestWallDistance(Ray ray, const std::vector<WorldBlock>& world, float maxDist) {
    float closest = maxDist;
    for (const auto& block : world) {
        RayCollision col = GetRayCollisionBox(ray, block.bounds);
        if (col.hit && col.distance < closest) {
            closest = col.distance;
        }
    }
    return closest;
}

class LevelLoader {
public:
    static void EnsurePresetMap(const std::string& filename, MapPreset preset) {
        std::ifstream test(filename);
        if (test.good()) return;

        std::ofstream out(filename);
        out << "# posX posY posZ tamX tamY tamZ R G B\n";

        if (preset == MapPreset::SQUAD_ARENA) {
            out << "0.0 -0.5 0.0 70.0 1.0 70.0 52 57 66\n"
                << "0.0 2.5 -25.0 50.0 5.0 1.0 100 105 115\n"
                << "0.0 2.5 25.0 50.0 5.0 1.0 100 105 115\n"
                << "-25.0 2.5 0.0 1.0 5.0 50.0 100 105 115\n"
                << "25.0 2.5 0.0 1.0 5.0 50.0 100 105 115\n"
                << "-6.0 1.1 2.0 3.5 2.2 1.2 85 110 130\n"
                << "6.0 1.1 2.0 3.5 2.2 1.2 85 110 130\n"
                << "0.0 1.2 -5.0 4.0 2.4 1.5 180 95 65\n"
                << "-13.0 1.0 -8.0 5.0 2.0 5.0 75 145 115\n"
                << "13.0 1.0 -8.0 5.0 2.0 5.0 75 145 115\n";
        } else if (preset == MapPreset::MOVEMENT_COURSE) {
            // Pista dedicada para Bunny Hop, Air Strafe, Crouch-Jump e Túnel
            out << "0.0 -0.5 0.0 80.0 1.0 80.0 46 52 62\n"
                << "0.0 3.0 -35.0 70.0 6.0 1.0 90 95 105\n"
                << "0.0 3.0 35.0 70.0 6.0 1.0 90 95 105\n"
                << "-35.0 3.0 0.0 1.0 6.0 70.0 90 95 105\n"
                << "35.0 3.0 0.0 1.0 6.0 70.0 90 95 105\n"
                // Degrau 1: Pulo simples (altura 0.95m)
                << "0.0 0.475 10.0 5.0 0.95 4.0 70 160 210\n"
                // Degrau 2: Crouch-Jump obrigatorio (altura 1.65m acima do chão)
                << "0.0 0.825 2.0 5.0 1.65 4.0 215 110 60\n"
                // Degrau 3: Sprint + Crouch-Jump (topo em 2.5m)
                << "0.0 1.25 -7.0 5.0 2.50 4.5 95 185 115\n"
                // Ilhas sequenciais para treino de Bunny Hop / Air Strafe
                << "0.0 1.25 -16.5 4.0 2.50 4.0 220 180 60\n"
                << "4.5 1.25 -25.0 4.5 2.50 4.5 175 105 215\n"
                // Tunel baixo na lateral direita (vao livre de 1.25m -> exige Ctrl)
                << "14.0 2.25 4.0 6.0 2.0 10.0 85 155 120\n"
                << "11.5 1.5 4.0 1.0 3.0 10.0 65 125 95\n"
                << "16.5 1.5 4.0 1.0 3.0 10.0 65 125 95\n";
        } else if (preset == MapPreset::FIRING_RANGE) {
            // Estande aberto com coberturas escalonadas para testar IA e flanco furtivo
            out << "0.0 -0.5 0.0 70.0 1.0 70.0 48 54 64\n"
                << "0.0 2.5 -25.0 50.0 5.0 1.0 100 105 115\n"
                << "0.0 2.5 25.0 50.0 5.0 1.0 100 105 115\n"
                << "-25.0 2.5 0.0 1.0 5.0 50.0 100 105 115\n"
                << "25.0 2.5 0.0 1.0 5.0 50.0 100 105 115\n"
                // Linha de protecao dos aliados
                << "-4.5 1.0 5.0 3.0 2.0 1.0 70 130 180\n"
                << "4.5 1.0 5.0 3.0 2.0 1.0 70 130 180\n"
                // Coberturas intermediarias para flanqueamento
                << "-9.5 1.1 -4.0 3.5 2.2 1.2 190 120 60\n"
                << "9.5 1.1 -4.0 3.5 2.2 1.2 190 120 60\n"
                // Coberturas proximas aos inimigos
                << "-5.0 1.1 -12.0 3.5 2.2 1.2 165 75 75\n"
                << "5.0 1.1 -12.0 3.5 2.2 1.2 165 75 75\n";
        }
    }

    static std::vector<WorldBlock> LoadFromFile(const std::string& filename, MapPreset preset = MapPreset::SQUAD_ARENA) {
        EnsurePresetMap(filename, preset);
        std::vector<WorldBlock> blocks;
        std::ifstream file(filename);
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            WorldBlock b;
            int r, g, bl;
            if (iss >> b.pos.x >> b.pos.y >> b.pos.z
                    >> b.size.x >> b.size.y >> b.size.z
                    >> r >> g >> bl) {
                b.color = { (unsigned char)r, (unsigned char)g, (unsigned char)bl, 255 };
                b.UpdateBounds();
                blocks.push_back(b);
            }
        }
        return blocks;
    }
};