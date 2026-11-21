#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include "Audio.hpp"
#include <vector>
#include <string>
#include <array>
#include <fstream>
#include <algorithm>
#include <cmath>

enum class EditorToolMode {
    BUILD,
    EDIT
};

struct BlockMaterial {
    const char* name;
    Color color;
};

class InGameMapEditor {
public:
    Camera3D cam = { 0 };
    float yaw = PI;
    float pitch = -0.35f;

    EditorToolMode toolMode = EditorToolMode::BUILD;
    int selectedBlockIdx = -1;
    int hoveredBlockIdx = -1;

    // Configuração do bloco fantasma (modo BUILD)
    Vector3 brushSize = { 2.0f, 1.5f, 2.0f };
    Vector3 ghostPos = { 0.0f, 0.75f, 0.0f };
    bool hasValidPlacement = false;

    // Grade magnética (Snap-to-Grid)
    int gridPresetIdx = 1; // 0: 0.25m, 1: 0.50m, 2: 1.00m, 3: 2.00m
    float gridSnap = 0.50f;

    // Paleta de cores / materiais
    int selectedColorIdx = 0;

    // Arquivos alvo para salvar/carregar
    int targetFileIdx = 0;
    float statusBannerTimer = 0.0f;
    std::string statusBannerText = "";
    Color statusBannerColor = GREEN;

    // Histórico simples de desfazer (Undo)
    std::vector<std::vector<WorldBlock>> undoStack;

    static const std::array<BlockMaterial, 8>& GetPalette() {
        static const std::array<BlockMaterial, 8> palette = {{
            { "Concreto",       { 115, 122, 132, 255 } },
            { "Cobertura Azul", {  58, 105, 155, 255 } },
            { "Barreira Laranja",{ 195, 110,  45, 255 } },
            { "Areia Tatica",   { 168, 152, 118, 255 } },
            { "Metal Escuro",   {  52,  58,  68, 255 } },
            { "Madeira",        { 130,  92,  58, 255 } },
            { "Zona Verde",     {  55, 135,  75, 255 } },
            { "Alerta Vermelho",{ 175,  52,  52, 255 } }
        }};
        return palette;
    }

    static const std::array<const char*, 4>& GetTargetFiles() {
        static const std::array<const char*, 4> files = {{
            "mapa_squad.txt",
            "mapa_treino.txt",
            "mapa_movimento.txt",
            "mapa_custom.txt"
        }};
        return files;
    }

    void Enter(std::vector<WorldBlock>& world) {
        cam.position = { 0.0f, 11.0f, 18.0f };
        yaw = PI;
        pitch = -0.45f;
        cam.up = { 0.0f, 1.0f, 0.0f };
        cam.fovy = 75.0f;
        cam.projection = CAMERA_PERSPECTIVE;

        toolMode = EditorToolMode::BUILD;
        selectedBlockIdx = -1;
        hoveredBlockIdx = -1;
        undoStack.clear();

        targetFileIdx = 0;
        world = LevelLoader::LoadFromFile(GetTargetFiles()[targetFileIdx], MapPreset::SQUAD_ARENA);
        ShowBanner(TextFormat("Carregado: %s", GetTargetFiles()[targetFileIdx]), SKYBLUE);
    }

    void Update(float dt, std::vector<WorldBlock>& world, ProceduralAudio& audio) {
        if (statusBannerTimer > 0.0f) statusBannerTimer -= dt;

        // 1. Rotação e Movimentação Livre da Câmera (Noclip)
        Vector2 mouseDelta = GetMouseDelta();
        yaw -= mouseDelta.x * 0.0022f;
        pitch = std::clamp(pitch - mouseDelta.y * 0.0022f, -1.52f, 1.52f);

        Vector3 forward = {
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };
        forward = Vector3Normalize(forward);
        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, { 0.0f, 1.0f, 0.0f }));

        float moveSpeed = IsKeyDown(KEY_LEFT_SHIFT) ? 18.0f : 8.5f;
        Vector3 move = { 0.0f, 0.0f, 0.0f };
        if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
        if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
        if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
        if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
        if (IsKeyDown(KEY_SPACE)) move.y += 1.0f;
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C)) move.y -= 1.0f;

        if (Vector3Length(move) > 0.001f) {
            move = Vector3Scale(Vector3Normalize(move), moveSpeed * dt);
            cam.position = Vector3Add(cam.position, move);
        }
        cam.target = Vector3Add(cam.position, forward);

        // 2. Alternar Modo (Construir vs Editar) e Grade (G)
        if (IsKeyPressed(KEY_TAB)) {
            toolMode = (toolMode == EditorToolMode::BUILD) ? EditorToolMode::EDIT : EditorToolMode::BUILD;
            if (toolMode == EditorToolMode::BUILD) selectedBlockIdx = -1;
            audio.PlayOrder();
        }

        if (IsKeyPressed(KEY_G)) {
            const float snaps[4] = { 0.25f, 0.50f, 1.00f, 2.00f };
            gridPresetIdx = (gridPresetIdx + 1) % 4;
            gridSnap = snaps[gridPresetIdx];
            ShowBanner(TextFormat("Grade (Snap): %.2fm", gridSnap), GOLD);
        }

        // 3. Seleção de Cor/Material (1 a 8)
        for (int k = 0; k < 8; k++) {
            if (IsKeyPressed(KEY_ONE + k)) {
                selectedColorIdx = k;
                if (toolMode == EditorToolMode::EDIT && IsValidIdx(selectedBlockIdx, world)) {
                    PushUndo(world);
                    world[selectedBlockIdx].color = GetPalette()[selectedColorIdx].color;
                }
            }
        }

        // 4. Desfazer última ação (Z)
        if (IsKeyPressed(KEY_Z) && !undoStack.empty()) {
            world = undoStack.back();
            undoStack.pop_back();
            selectedBlockIdx = -1;
            ShowBanner("Acao desfeita (Undo)", YELLOW);
            audio.PlayOrder();
        }

        // 5. Presets Base (F1-F4), Troca de Arquivo (F6) e Salvar (F5)
        if (IsKeyPressed(KEY_F1)) {
            PushUndo(world);
            targetFileIdx = 0;
            world = LevelLoader::LoadFromFile("mapa_squad.txt", MapPreset::SQUAD_ARENA);
            selectedBlockIdx = -1;
            ShowBanner("Base carregada: mapa_squad.txt", SKYBLUE);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F2)) {
            PushUndo(world);
            targetFileIdx = 1;
            world = LevelLoader::LoadFromFile("mapa_treino.txt", MapPreset::FIRING_RANGE);
            selectedBlockIdx = -1;
            ShowBanner("Base carregada: mapa_treino.txt", SKYBLUE);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F3)) {
            PushUndo(world);
            targetFileIdx = 2;
            world = LevelLoader::LoadFromFile("mapa_movimento.txt", MapPreset::MOVEMENT_COURSE);
            selectedBlockIdx = -1;
            ShowBanner("Base carregada: mapa_movimento.txt", SKYBLUE);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F4)) {
            PushUndo(world);
            BuildCleanArena(world);
            selectedBlockIdx = -1;
            ShowBanner("Nova Arena Limpa criada!", LIME);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F6)) {
            targetFileIdx = (targetFileIdx + 1) % (int)GetTargetFiles().size();
            ShowBanner(TextFormat("Arquivo destino: %s", GetTargetFiles()[targetFileIdx]), GOLD);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F5)) {
            const char* fname = GetTargetFiles()[targetFileIdx];
            if (SaveWorldToFile(fname, world)) {
                ShowBanner(TextFormat("MAPA SALVO COM SUCESSO EM: %s", fname), GREEN);
                audio.PlayRevive();
            } else {
                ShowBanner(TextFormat("ERRO AO SALVAR: %s", fname), RED);
                audio.PlayDownedAlert();
            }
        }

        // 6. Raycast da Mira para detectar bloco sob o cursor
        Ray ray = { cam.position, forward };
        hoveredBlockIdx = -1;
        RayCollision closestHit = { 0 };
        closestHit.distance = 80.0f;

        for (size_t i = 0; i < world.size(); i++) {
            RayCollision col = GetRayCollisionBox(ray, world[i].bounds);
            if (col.hit && col.distance < closestHit.distance) {
                closestHit = col;
                hoveredBlockIdx = (int)i;
            }
        }

        // Pintar bloco diretamente sob a mira com P
        if (IsKeyPressed(KEY_P) && IsValidIdx(hoveredBlockIdx, world)) {
            PushUndo(world);
            world[hoveredBlockIdx].color = GetPalette()[selectedColorIdx].color;
            ShowBanner(TextFormat("Bloco pintado: %s", GetPalette()[selectedColorIdx].name), GetPalette()[selectedColorIdx].color);
        }

        // 7. Redimensionamento (J/L = X, U/O = Y, I/K = Z)
        Vector3 sizeDelta = { 0.0f, 0.0f, 0.0f };
        if (IsKeyPressed(KEY_L)) sizeDelta.x += gridSnap;
        if (IsKeyPressed(KEY_J)) sizeDelta.x -= gridSnap;
        if (IsKeyPressed(KEY_O)) sizeDelta.y += gridSnap;
        if (IsKeyPressed(KEY_U)) sizeDelta.y -= gridSnap;
        if (IsKeyPressed(KEY_I)) sizeDelta.z += gridSnap;
        if (IsKeyPressed(KEY_K)) sizeDelta.z -= gridSnap;

        if (sizeDelta.x != 0.0f || sizeDelta.y != 0.0f || sizeDelta.z != 0.0f) {
            if (toolMode == EditorToolMode::EDIT && IsValidIdx(selectedBlockIdx, world)) {
                PushUndo(world);
                WorldBlock& b = world[selectedBlockIdx];
                b.size.x = std::max(0.25f, b.size.x + sizeDelta.x);
                b.size.y = std::max(0.25f, b.size.y + sizeDelta.y);
                b.size.z = std::max(0.25f, b.size.z + sizeDelta.z);
                RecalculateBounds(b);
            } else {
                brushSize.x = std::max(0.25f, brushSize.x + sizeDelta.x);
                brushSize.y = std::max(0.25f, brushSize.y + sizeDelta.y);
                brushSize.z = std::max(0.25f, brushSize.z + sizeDelta.z);
            }
        }

        // 8. Lógica Específica por Modo (BUILD vs EDIT)
        if (toolMode == EditorToolMode::BUILD) {
            hasValidPlacement = false;
            if (hoveredBlockIdx != -1) {
                Vector3 rawPos = {
                    closestHit.point.x + closestHit.normal.x * (brushSize.x * 0.5f),
                    closestHit.point.y + closestHit.normal.y * (brushSize.y * 0.5f),
                    closestHit.point.z + closestHit.normal.z * (brushSize.z * 0.5f)
                };
                ghostPos = {
                    SnapValue(rawPos.x, gridSnap),
                    std::max(brushSize.y * 0.5f, SnapValue(rawPos.y, gridSnap)),
                    SnapValue(rawPos.z, gridSnap)
                };
                hasValidPlacement = true;
            } else if (fabsf(forward.y) > 0.05f) {
                // Projeta no plano Y = 0 caso esteja olhando para fora dos blocos
                float t = -cam.position.y / forward.y;
                if (t > 1.0f && t < 50.0f) {
                    Vector3 groundPt = Vector3Add(cam.position, Vector3Scale(forward, t));
                    ghostPos = {
                        SnapValue(groundPt.x, gridSnap),
                        brushSize.y * 0.5f,
                        SnapValue(groundPt.z, gridSnap)
                    };
                    hasValidPlacement = true;
                }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hasValidPlacement) {
                PushUndo(world);
                WorldBlock nb = MakeBlock(ghostPos, brushSize, GetPalette()[selectedColorIdx].color);
                world.push_back(nb);
                audio.PlayReload();
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && IsValidIdx(hoveredBlockIdx, world)) {
                // Evita apagar o piso principal (índice 0) sem querer, a menos que segure Shift
                if (hoveredBlockIdx > 0 || IsKeyDown(KEY_LEFT_SHIFT)) {
                    PushUndo(world);
                    world.erase(world.begin() + hoveredBlockIdx);
                    hoveredBlockIdx = -1;
                    audio.PlayOrder();
                } else {
                    ShowBanner("Segure SHIFT + Clique Direito para apagar o piso base", ORANGE);
                }
            }
        } else {
            // Modo EDIT: Selecionar, Mover, Duplicar e Remover
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                selectedBlockIdx = hoveredBlockIdx;
                if (selectedBlockIdx != -1) {
                    audio.PlayOrder();
                }
            }

            if (IsValidIdx(selectedBlockIdx, world)) {
                Vector3 posDelta = { 0.0f, 0.0f, 0.0f };
                if (IsKeyPressed(KEY_RIGHT))     posDelta.x += gridSnap;
                if (IsKeyPressed(KEY_LEFT))      posDelta.x -= gridSnap;
                if (IsKeyPressed(KEY_DOWN))      posDelta.z += gridSnap;
                if (IsKeyPressed(KEY_UP))        posDelta.z -= gridSnap;
                if (IsKeyPressed(KEY_PAGE_UP))   posDelta.y += gridSnap;
                if (IsKeyPressed(KEY_PAGE_DOWN)) posDelta.y -= gridSnap;

                if (posDelta.x != 0.0f || posDelta.y != 0.0f || posDelta.z != 0.0f) {
                    PushUndo(world);
                    WorldBlock& b = world[selectedBlockIdx];
                    b.pos = Vector3Add(b.pos, posDelta);
                    RecalculateBounds(b);
                }

                // Duplicar bloco selecionado com V
                if (IsKeyPressed(KEY_V)) {
                    PushUndo(world);
                    WorldBlock copy = world[selectedBlockIdx];
                    copy.pos.x += gridSnap;
                    RecalculateBounds(copy);
                    world.push_back(copy);
                    selectedBlockIdx = (int)world.size() - 1;
                    ShowBanner("Bloco duplicado!", SKYBLUE);
                    audio.PlayReload();
                }

                // Remover bloco selecionado com DELETE ou Clique Direito
                if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    if (selectedBlockIdx > 0 || IsKeyDown(KEY_LEFT_SHIFT)) {
                        PushUndo(world);
                        world.erase(world.begin() + selectedBlockIdx);
                        selectedBlockIdx = -1;
                        audio.PlayOrder();
                    }
                }
            }
        }
    }

    void Draw3D(const std::vector<WorldBlock>& world) const {
        DrawGrid(50, 1.0f);

        for (size_t i = 0; i < world.size(); i++) {
            const auto& b = world[i];
            DrawCubeV(b.pos, b.size, b.color);

            bool isSelected = (toolMode == EditorToolMode::EDIT && (int)i == selectedBlockIdx);
            bool isHovered = ((int)i == hoveredBlockIdx);

            Color wireCol = BLACK;
            if (isSelected) wireCol = YELLOW;
            else if (isHovered) wireCol = SKYBLUE;

            DrawCubeWiresV(b.pos, b.size, wireCol);

            if (isSelected) {
                Vector3 expSize = { b.size.x + 0.06f, b.size.y + 0.06f, b.size.z + 0.06f };
                DrawCubeWiresV(b.pos, expSize, GOLD);
            }
        }

        // Bloco Fantasma (Ghost) no modo BUILD
        if (toolMode == EditorToolMode::BUILD && hasValidPlacement) {
            Color c = GetPalette()[selectedColorIdx].color;
            DrawCubeV(ghostPos, brushSize, Fade(c, 0.55f));
            DrawCubeWiresV(ghostPos, brushSize, LIME);
        }
    }

    void DrawHUD(int screenWidth, int screenHeight, const std::vector<WorldBlock>& world) const {
        int cx = screenWidth / 2;
        int cy = screenHeight / 2;

        // Mira de precisão do editor
        DrawLine(cx - 8, cy, cx + 8, cy, WHITE);
        DrawLine(cx, cy - 8, cx, cy + 8, WHITE);

        // Cabeçalho Superior Esquerdo
        DrawRectangle(16, 16, 430, 128, Color{ 16, 20, 28, 220 });
        DrawRectangleLinesEx({ 16, 16, 430, 128 }, 1.5f, SKYBLUE);

        const char* modeStr = (toolMode == EditorToolMode::BUILD)
            ? "MODO: [BUILD] CONSTRUIR BLOCOS"
            : "MODO: [EDIT] SELECIONAR E REDIMENSIONAR";
        DrawText(modeStr, 28, 26, 17, (toolMode == EditorToolMode::BUILD) ? LIME : GOLD);

        DrawText(TextFormat("Arquivo Destino [F6]: %s  |  Blocos: %d", GetTargetFiles()[targetFileIdx], (int)world.size()),
                 28, 52, 14, WHITE);
        DrawText(TextFormat("Grade [G]: %.2fm  |  Pincel: %.2f x %.2f x %.2f", gridSnap, brushSize.x, brushSize.y, brushSize.z),
                 28, 74, 14, LIGHTGRAY);

        if (toolMode == EditorToolMode::EDIT && IsValidIdx(selectedBlockIdx, world)) {
            const WorldBlock& sb = world[selectedBlockIdx];
            DrawText(TextFormat("Selecionado #%d: Pos(%.1f, %.1f, %.1f) Tam(%.2f, %.2f, %.2f)",
                                selectedBlockIdx, sb.pos.x, sb.pos.y, sb.pos.z, sb.size.x, sb.size.y, sb.size.z),
                     28, 98, 13, YELLOW);
        } else {
            DrawText("Aperte [TAB] para alternar entre Construir e Editar/Mover", 28, 98, 13, SKYBLUE);
        }

        // Painel Superior Direito: Presets e Salvamento
        int rx = screenWidth - 340;
        DrawRectangle(rx, 16, 324, 128, Color{ 16, 20, 28, 220 });
        DrawRectangleLinesEx({ (float)rx, 16, 324, 128 }, 1.5f, GOLD);
        DrawText("ARQUIVOS E PRESETS:", rx + 12, 26, 15, GOLD);
        DrawText("[F1] Arena Squad   |  [F2] Estande", rx + 12, 48, 13, LIGHTGRAY);
        DrawText("[F3] Pista Bhop    |  [F4] Arena Limpa", rx + 12, 68, 13, LIGHTGRAY);
        DrawText("[F5] SALVAR MAPA   |  [Z] Desfazer", rx + 12, 92, 14, GREEN);
        DrawText("[ESC] Voltar ao Menu Principal", rx + 12, 114, 13, ORANGE);

        // Banner de Notificação Central
        if (statusBannerTimer > 0.0f) {
            int bw = MeasureText(statusBannerText.c_str(), 18) + 32;
            DrawRectangle(cx - bw / 2, 160, bw, 34, Color{ 12, 16, 22, 235 });
            DrawRectangleLinesEx({ (float)(cx - bw / 2), 160, (float)bw, 34 }, 1.5f, statusBannerColor);
            DrawText(statusBannerText.c_str(), cx - MeasureText(statusBannerText.c_str(), 18) / 2, 168, 18, statusBannerColor);
        }

        // Paleta de Cores Inferior (Teclas 1 a 8)
        const auto& palette = GetPalette();
        int palW = 680;
        int palX = cx - palW / 2;
        int palY = screenHeight - 92;
        DrawRectangle(palX, palY, palW, 54, Color{ 16, 20, 28, 220 });
        DrawRectangleLinesEx({ (float)palX, (float)palY, (float)palW, 54 }, 1.2f, GRAY);

        for (int i = 0; i < 8; i++) {
            int itemX = palX + 10 + i * 83;
            bool active = (selectedColorIdx == i);
            DrawRectangle(itemX, palY + 8, 76, 24, palette[i].color);
            DrawRectangleLinesEx({ (float)itemX, (float)(palY + 8), 76, 24 }, active ? 2.5f : 1.0f, active ? YELLOW : BLACK);
            DrawText(TextFormat("[%d]", i + 1), itemX + 4, palY + 36, 12, active ? YELLOW : LIGHTGRAY);
        }

        // Barra de Controles na Base
        const char* helpLine = (toolMode == EditorToolMode::BUILD)
            ? "WASD+Espaco/Ctrl: Voar | Click Esq: Colocar | Click Dir: Apagar | J/L U/O I/K: Tamanho | P: Pintar | TAB: Modo Editar"
            : "Click Esq: Selecionar | Setas+PgUp/PgDn: Mover | J/L U/O I/K: Redimensionar | V: Duplicar | DEL: Apagar | TAB: Construir";
        DrawText(helpLine, cx - MeasureText(helpLine, 13) / 2, screenHeight - 24, 13, LIGHTGRAY);
    }

private:
    static float SnapValue(float val, float step) {
        return roundf(val / step) * step;
    }

    static bool IsValidIdx(int idx, const std::vector<WorldBlock>& world) {
        return idx >= 0 && idx < (int)world.size();
    }

    static void RecalculateBounds(WorldBlock& b) {
        b.bounds = {
            { b.pos.x - b.size.x * 0.5f, b.pos.y - b.size.y * 0.5f, b.pos.z - b.size.z * 0.5f },
            { b.pos.x + b.size.x * 0.5f, b.pos.y + b.size.y * 0.5f, b.pos.z + b.size.z * 0.5f }
        };
    }

    static WorldBlock MakeBlock(Vector3 pos, Vector3 size, Color color) {
        WorldBlock b;
        b.pos = pos;
        b.size = size;
        b.color = color;
        RecalculateBounds(b);
        return b;
    }

    void PushUndo(const std::vector<WorldBlock>& world) {
        if (undoStack.size() >= 20) {
            undoStack.erase(undoStack.begin());
        }
        undoStack.push_back(world);
    }

    void ShowBanner(const std::string& msg, Color col) {
        statusBannerText = msg;
        statusBannerColor = col;
        statusBannerTimer = 2.6f;
    }

    void BuildCleanArena(std::vector<WorldBlock>& world) {
        world.clear();
        // Piso base 50x50
        world.push_back(MakeBlock({ 0.0f, -0.5f, 0.0f }, { 50.0f, 1.0f, 50.0f }, { 58, 63, 72, 255 }));
        // Muros externos
        world.push_back(MakeBlock({   0.0f, 2.0f, -25.0f }, { 50.0f, 4.0f, 1.0f }, { 85, 92, 102, 255 }));
        world.push_back(MakeBlock({   0.0f, 2.0f,  25.0f }, { 50.0f, 4.0f, 1.0f }, { 85, 92, 102, 255 }));
        world.push_back(MakeBlock({ -25.0f, 2.0f,   0.0f }, {  1.0f, 4.0f, 50.0f }, { 85, 92, 102, 255 }));
        world.push_back(MakeBlock({  25.0f, 2.0f,   0.0f }, {  1.0f, 4.0f, 50.0f }, { 85, 92, 102, 255 }));
    }

    static bool SaveWorldToFile(const char* filename, const std::vector<WorldBlock>& world) {
        std::ofstream out(filename);
        if (!out.is_open()) return false;

        for (const auto& b : world) {
            out << b.pos.x << " " << b.pos.y << " " << b.pos.z << " "
                << b.size.x << " " << b.size.y << " " << b.size.z << " "
                << (int)b.color.r << " " << (int)b.color.g << " "
                << (int)b.color.b << " " << (int)b.color.a << "\n";
        }
        return true;
    }
};