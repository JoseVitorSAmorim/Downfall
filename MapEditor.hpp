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

enum class EditorBrushCategory {
    BLOCKS,
    ENTITIES
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
    EditorBrushCategory brushCategory = EditorBrushCategory::BLOCKS;

    // Seleção e Hover separados para Blocos e Marcadores Jogáveis
    int selectedBlockIdx = -1;
    int hoveredBlockIdx = -1;
    int selectedEntityIdx = -1;
    int hoveredEntityIdx = -1;

    // Marcadores de cena serializáveis
    std::vector<SceneEntityMarker> entities;

    // Configuração do pincel fantasma (modo BUILD)
    Vector3 brushSize = { 2.0f, 1.5f, 2.0f };
    int brushSlopeDir = 0; // 0 = Caixa, 1..4 = Rampa
    Vector3 ghostPos = { 0.0f, 0.75f, 0.0f };
    bool hasValidPlacement = false;

    // Configuração do pincel de entidade (modo BUILD -> ENTITIES)
    int selectedEntityTypeIdx = 0; // 0..6 correspondente a SceneEntityType
    int entityParamA = 0;
    int entityParamB = 0;
    Vector3 triggerBrushSize = { 8.0f, 3.5f, 3.5f };
    float extractBrushRadius = 3.2f;

    // Grade magnética (Snap-to-Grid)
    int gridPresetIdx = 1; // 0: 0.25m, 1: 0.50m, 2: 1.00m, 3: 2.00m
    float gridSnap = 0.50f;

    // Paleta de cores / materiais
    int selectedColorIdx = 0;

    // Arquivos alvo para salvar/carregar (.txt e .scene)
    int targetFileIdx = 0;
    float statusBannerTimer = 0.0f;
    std::string statusBannerText = "";
    Color statusBannerColor = GREEN;

    // Histórico de desfazer (Undo) sincronizado (blocos + entidades)
    std::vector<SceneData> undoStack;

    static const std::array<BlockMaterial, 8>& GetPalette() {
        static const std::array<BlockMaterial, 8> palette = {{
            { "Concreto",        { 115, 122, 132, 255 } },
            { "Cobertura Azul",  {  58, 105, 155, 255 } },
            { "Barreira Laranja",{ 195, 110,  45, 255 } },
            { "Areia Tatica",    { 168, 152, 118, 255 } },
            { "Metal Escuro",    {  52,  58,  68, 255 } },
            { "Madeira",         { 130,  92,  58, 255 } },
            { "Zona Verde",      {  55, 135,  75, 255 } },
            { "Alerta Vermelho", { 175,  52,  52, 255 } }
        }};
        return palette;
    }

    static const std::array<const char*, 5>& GetTargetFiles() {
        static const std::array<const char*, 5> files = {{
            "mapa_squad.txt",
            "mapa_treino.txt",
            "mapa_movimento.txt",
            "mapa_custom.scene",
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
        brushCategory = EditorBrushCategory::BLOCKS;
        selectedBlockIdx = -1;
        hoveredBlockIdx = -1;
        selectedEntityIdx = -1;
        hoveredEntityIdx = -1;
        brushSlopeDir = 0;
        undoStack.clear();

        targetFileIdx = 0;
        LoadSceneIntoEditor(GetTargetFiles()[targetFileIdx], MapPreset::SQUAD_ARENA, world);
        ShowBanner(TextFormat("Cena carregada: %s (%d blocos, %d marcadores)",
                              GetTargetFiles()[targetFileIdx], (int)world.size(), (int)entities.size()), SKYBLUE);
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

        // 2. Alternar Categoria (B: Blocos vs Entidades), Modo (TAB) e Grade (G)
        if (IsKeyPressed(KEY_B)) {
            brushCategory = (brushCategory == EditorBrushCategory::BLOCKS)
                ? EditorBrushCategory::ENTITIES
                : EditorBrushCategory::BLOCKS;
            ShowBanner(brushCategory == EditorBrushCategory::BLOCKS
                ? "Pincel Ativo: BLOCOS GEOMETRICOS"
                : "Pincel Ativo: ENTIDADES JOGAVEIS (.SCENE)",
                brushCategory == EditorBrushCategory::BLOCKS ? SKYBLUE : GOLD);
            audio.PlayOrder();
        }

        if (IsKeyPressed(KEY_TAB)) {
            toolMode = (toolMode == EditorToolMode::BUILD) ? EditorToolMode::EDIT : EditorToolMode::BUILD;
            if (toolMode == EditorToolMode::BUILD) {
                selectedBlockIdx = -1;
                selectedEntityIdx = -1;
            }
            audio.PlayOrder();
        }

        if (IsKeyPressed(KEY_G)) {
            const float snaps[4] = { 0.25f, 0.50f, 1.00f, 2.00f };
            gridPresetIdx = (gridPresetIdx + 1) % 4;
            gridSnap = snaps[gridPresetIdx];
            ShowBanner(TextFormat("Grade (Snap): %.2fm", gridSnap), GOLD);
        }

        // Rotacionar Rampa (R)
        if (IsKeyPressed(KEY_R)) {
            if (toolMode == EditorToolMode::EDIT && IsValidIdx(selectedBlockIdx, world)) {
                PushUndo(world);
                world[selectedBlockIdx].slopeDir = (world[selectedBlockIdx].slopeDir + 1) % 5;
                ShowBanner(TextFormat("Rampa Bloco #%d: %s", selectedBlockIdx, GetSlopeLabel(world[selectedBlockIdx].slopeDir)), LIME);
            } else {
                brushSlopeDir = (brushSlopeDir + 1) % 5;
                ShowBanner(TextFormat("Formato Pincel: %s", GetSlopeLabel(brushSlopeDir)), LIME);
            }
            audio.PlayOrder();
        }

        // 3. Seleção de Cor (1..8) ou Tipo de Entidade (1..7)
        if (brushCategory == EditorBrushCategory::BLOCKS) {
            for (int k = 0; k < 8; k++) {
                if (IsKeyPressed(KEY_ONE + k)) {
                    selectedColorIdx = k;
                    if (toolMode == EditorToolMode::EDIT && IsValidIdx(selectedBlockIdx, world)) {
                        PushUndo(world);
                        world[selectedBlockIdx].color = GetPalette()[selectedColorIdx].color;
                    }
                }
            }
        } else {
            for (int k = 0; k < 7; k++) {
                if (IsKeyPressed(KEY_ONE + k)) {
                    selectedEntityTypeIdx = k;
                    SetDefaultParamsForEntity(static_cast<SceneEntityType>(k));
                    if (toolMode == EditorToolMode::EDIT && IsValidEntityIdx(selectedEntityIdx)) {
                        PushUndo(world);
                        SceneEntityType nt = static_cast<SceneEntityType>(k);
                        entities[selectedEntityIdx].type = nt;
                        entities[selectedEntityIdx].paramA = entityParamA;
                        entities[selectedEntityIdx].paramB = entityParamB;
                        entities[selectedEntityIdx].tag = GenerateDefaultTag(nt, entityParamA, entityParamB);
                        if (nt == SceneEntityType::TRIGGER) entities[selectedEntityIdx].size = triggerBrushSize;
                        else if (nt == SceneEntityType::EXTRACT) entities[selectedEntityIdx].size = { extractBrushRadius, 0.2f, extractBrushRadius };
                    }
                    audio.PlayOrder();
                }
            }
        }

        // Ajuste de parâmetros da entidade com '[' e ']'
        if (IsKeyPressed(KEY_LEFT_BRACKET) || IsKeyPressed(KEY_RIGHT_BRACKET)) {
            int delta = IsKeyPressed(KEY_RIGHT_BRACKET) ? 1 : -1;
            if (toolMode == EditorToolMode::EDIT && IsValidEntityIdx(selectedEntityIdx)) {
                PushUndo(world);
                AdjustEntityParams(entities[selectedEntityIdx], delta);
                ShowBanner(TextFormat("Marcador #%d: paramA=%d paramB=%d (%s)",
                                      selectedEntityIdx, entities[selectedEntityIdx].paramA,
                                      entities[selectedEntityIdx].paramB, entities[selectedEntityIdx].tag.c_str()), GOLD);
            } else if (brushCategory == EditorBrushCategory::ENTITIES) {
                SceneEntityMarker tmp{};
                tmp.type = static_cast<SceneEntityType>(selectedEntityTypeIdx);
                tmp.paramA = entityParamA;
                tmp.paramB = entityParamB;
                AdjustEntityParams(tmp, delta);
                entityParamA = tmp.paramA;
                entityParamB = tmp.paramB;
                ShowBanner(TextFormat("Pincel %s: paramA=%d paramB=%d",
                                      SceneEntityTypeToToken(tmp.type), entityParamA, entityParamB), GOLD);
            }
            audio.PlayOrder();
        }

        // 4. Desfazer última ação (Z)
        if (IsKeyPressed(KEY_Z) && !undoStack.empty()) {
            world = undoStack.back().blocks;
            entities = undoStack.back().entities;
            undoStack.pop_back();
            selectedBlockIdx = -1;
            selectedEntityIdx = -1;
            ShowBanner("Acao desfeita (Undo)", YELLOW);
            audio.PlayOrder();
        }

        // 5. Presets Base (F1-F4), Troca de Arquivo (F6) e Salvar (F5)
        if (IsKeyPressed(KEY_F1)) {
            PushUndo(world);
            targetFileIdx = 0;
            LoadSceneIntoEditor("mapa_squad.txt", MapPreset::SQUAD_ARENA, world);
            ShowBanner("Cena carregada: mapa_squad.txt", SKYBLUE);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F2)) {
            PushUndo(world);
            targetFileIdx = 1;
            LoadSceneIntoEditor("mapa_treino.txt", MapPreset::FIRING_RANGE, world);
            ShowBanner("Cena carregada: mapa_treino.txt", SKYBLUE);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F3)) {
            PushUndo(world);
            targetFileIdx = 2;
            LoadSceneIntoEditor("mapa_movimento.txt", MapPreset::MOVEMENT_COURSE, world);
            ShowBanner("Cena carregada: mapa_movimento.txt", SKYBLUE);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F4)) {
            PushUndo(world);
            BuildCleanArena(world);
            selectedBlockIdx = -1;
            selectedEntityIdx = -1;
            ShowBanner("Nova Arena Limpa + Spawns base criados!", LIME);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F6)) {
            targetFileIdx = (targetFileIdx + 1) % (int)GetTargetFiles().size();
            ShowBanner(TextFormat("Arquivo destino: %s", GetTargetFiles()[targetFileIdx]), GOLD);
            audio.PlayOrder();
        } else if (IsKeyPressed(KEY_F5)) {
            const char* fname = GetTargetFiles()[targetFileIdx];
            SceneData outScene{ world, entities };
            if (LevelLoader::SaveSceneToFile(fname, outScene)) {
                ShowBanner(TextFormat("CENA SALVA: %s (%d blocos, %d entidades)", fname, (int)world.size(), (int)entities.size()), GREEN);
                audio.PlayRevive();
            } else {
                ShowBanner(TextFormat("ERRO AO SALVAR: %s", fname), RED);
                audio.PlayDownedAlert();
            }
        }

        // 6. Raycast da Mira para detectar bloco e entidade sob o cursor
        Ray ray = { cam.position, forward };
        hoveredBlockIdx = -1;
        RayCollision closestBlockHit = { 0 };
        closestBlockHit.distance = 80.0f;

        for (size_t i = 0; i < world.size(); i++) {
            RayCollision col = GetRayCollisionBox(ray, world[i].GetAABB());
            if (col.hit && col.distance < closestBlockHit.distance) {
                closestBlockHit = col;
                hoveredBlockIdx = (int)i;
            }
        }

        hoveredEntityIdx = -1;
        float closestEntityDist = 80.0f;
        for (size_t i = 0; i < entities.size(); i++) {
            RayCollision col = GetRayCollisionBox(ray, entities[i].GetAABB());
            if (col.hit && col.distance < closestEntityDist) {
                closestEntityDist = col.distance;
                hoveredEntityIdx = (int)i;
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
            if (toolMode == EditorToolMode::EDIT && IsValidEntityIdx(selectedEntityIdx)) {
                PushUndo(world);
                SceneEntityMarker& em = entities[selectedEntityIdx];
                if (em.type == SceneEntityType::TRIGGER) {
                    em.size.x = std::max(1.0f, em.size.x + sizeDelta.x);
                    em.size.y = std::max(1.0f, em.size.y + sizeDelta.y);
                    em.size.z = std::max(1.0f, em.size.z + sizeDelta.z);
                } else if (em.type == SceneEntityType::EXTRACT) {
                    em.size.x = std::max(1.2f, em.size.x + sizeDelta.x + sizeDelta.z);
                    em.size.z = em.size.x;
                }
            } else if (toolMode == EditorToolMode::EDIT && IsValidIdx(selectedBlockIdx, world)) {
                PushUndo(world);
                WorldBlock& b = world[selectedBlockIdx];
                b.size.x = std::max(0.25f, b.size.x + sizeDelta.x);
                b.size.y = std::max(0.25f, b.size.y + sizeDelta.y);
                b.size.z = std::max(0.25f, b.size.z + sizeDelta.z);
                RecalculateBounds(b);
            } else if (brushCategory == EditorBrushCategory::ENTITIES) {
                SceneEntityType curType = static_cast<SceneEntityType>(selectedEntityTypeIdx);
                if (curType == SceneEntityType::TRIGGER) {
                    triggerBrushSize.x = std::max(1.0f, triggerBrushSize.x + sizeDelta.x);
                    triggerBrushSize.y = std::max(1.0f, triggerBrushSize.y + sizeDelta.y);
                    triggerBrushSize.z = std::max(1.0f, triggerBrushSize.z + sizeDelta.z);
                } else if (curType == SceneEntityType::EXTRACT) {
                    extractBrushRadius = std::max(1.2f, extractBrushRadius + sizeDelta.x + sizeDelta.z);
                }
            } else {
                brushSize.x = std::max(0.25f, brushSize.x + sizeDelta.x);
                brushSize.y = std::max(0.25f, brushSize.y + sizeDelta.y);
                brushSize.z = std::max(0.25f, brushSize.z + sizeDelta.z);
            }
        }

        // 8. Lógica Específica por Modo (BUILD vs EDIT)
        if (toolMode == EditorToolMode::BUILD) {
            hasValidPlacement = false;
            Vector3 surfacePoint = { 0.0f, 0.0f, 0.0f };
            Vector3 surfaceNormal = { 0.0f, 1.0f, 0.0f };

            if (hoveredBlockIdx != -1) {
                surfacePoint = closestBlockHit.point;
                surfaceNormal = closestBlockHit.normal;
                hasValidPlacement = true;
            } else if (fabsf(forward.y) > 0.05f) {
                float t = -cam.position.y / forward.y;
                if (t > 1.0f && t < 50.0f) {
                    surfacePoint = Vector3Add(cam.position, Vector3Scale(forward, t));
                    surfaceNormal = { 0.0f, 1.0f, 0.0f };
                    hasValidPlacement = true;
                }
            }

            if (hasValidPlacement) {
                if (brushCategory == EditorBrushCategory::BLOCKS) {
                    Vector3 rawPos = {
                        surfacePoint.x + surfaceNormal.x * (brushSize.x * 0.5f),
                        surfacePoint.y + surfaceNormal.y * (brushSize.y * 0.5f),
                        surfacePoint.z + surfaceNormal.z * (brushSize.z * 0.5f)
                    };
                    ghostPos = {
                        SnapValue(rawPos.x, gridSnap),
                        std::max(brushSize.y * 0.5f, SnapValue(rawPos.y, gridSnap)),
                        SnapValue(rawPos.z, gridSnap)
                    };
                } else {
                    SceneEntityType curType = static_cast<SceneEntityType>(selectedEntityTypeIdx);
                    float yOffset = (curType == SceneEntityType::TRIGGER) ? (triggerBrushSize.y * 0.5f)
                                  : (curType == SceneEntityType::SPAWN_ALLY && entityParamA == 0 ? 0.5f : 0.0f);
                    ghostPos = {
                        SnapValue(surfacePoint.x, gridSnap),
                        std::max(0.0f, SnapValue(surfacePoint.y, gridSnap)) + yOffset,
                        SnapValue(surfacePoint.z, gridSnap)
                    };
                }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hasValidPlacement) {
                PushUndo(world);
                if (brushCategory == EditorBrushCategory::BLOCKS) {
                    WorldBlock nb = MakeBlock(ghostPos, brushSize, GetPalette()[selectedColorIdx].color, brushSlopeDir);
                    world.push_back(nb);
                    audio.PlayReload();
                } else {
                    SceneEntityType curType = static_cast<SceneEntityType>(selectedEntityTypeIdx);
                    SceneEntityMarker em{};
                    em.type = curType;
                    em.pos = ghostPos;
                    em.paramA = entityParamA;
                    em.paramB = entityParamB;
                    if (curType == SceneEntityType::TRIGGER) em.size = triggerBrushSize;
                    else if (curType == SceneEntityType::EXTRACT) em.size = { extractBrushRadius, 0.2f, extractBrushRadius };
                    else em.size = { 1.0f, 1.0f, 1.0f };
                    em.tag = GenerateDefaultTag(curType, entityParamA, entityParamB);

                    // Garante apenas 1 SPAWN_ALLY por slot e 1 zona EXTRACT principal
                    if (curType == SceneEntityType::SPAWN_ALLY) {
                        entities.erase(std::remove_if(entities.begin(), entities.end(), [&](const SceneEntityMarker& m) {
                            return m.type == SceneEntityType::SPAWN_ALLY && m.paramA == entityParamA;
                        }), entities.end());
                    } else if (curType == SceneEntityType::EXTRACT) {
                        entities.erase(std::remove_if(entities.begin(), entities.end(), [](const SceneEntityMarker& m) {
                            return m.type == SceneEntityType::EXTRACT;
                        }), entities.end());
                    }

                    entities.push_back(em);
                    ShowBanner(TextFormat("Marcador adicionado: %s (%s)", SceneEntityTypeToToken(em.type), em.tag.c_str()),
                               GetSceneEntityColor(em.type));
                    audio.PlayRevive();
                }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                if (hoveredEntityIdx != -1) {
                    PushUndo(world);
                    entities.erase(entities.begin() + hoveredEntityIdx);
                    hoveredEntityIdx = -1;
                    ShowBanner("Marcador removido!", ORANGE);
                    audio.PlayOrder();
                } else if (IsValidIdx(hoveredBlockIdx, world)) {
                    if (hoveredBlockIdx > 0 || IsKeyDown(KEY_LEFT_SHIFT)) {
                        PushUndo(world);
                        world.erase(world.begin() + hoveredBlockIdx);
                        hoveredBlockIdx = -1;
                        audio.PlayOrder();
                    } else {
                        ShowBanner("Segure SHIFT + Clique Direito para apagar o piso base", ORANGE);
                    }
                }
            }
        } else {
            // Modo EDIT: Selecionar, Mover, Duplicar e Remover (Entidades têm prioridade se estiverem na mira)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (hoveredEntityIdx != -1) {
                    selectedEntityIdx = hoveredEntityIdx;
                    selectedBlockIdx = -1;
                    audio.PlayOrder();
                } else {
                    selectedBlockIdx = hoveredBlockIdx;
                    selectedEntityIdx = -1;
                    if (selectedBlockIdx != -1) audio.PlayOrder();
                }
            }

            Vector3 posDelta = { 0.0f, 0.0f, 0.0f };
            if (IsKeyPressed(KEY_RIGHT))     posDelta.x += gridSnap;
            if (IsKeyPressed(KEY_LEFT))      posDelta.x -= gridSnap;
            if (IsKeyPressed(KEY_DOWN))      posDelta.z += gridSnap;
            if (IsKeyPressed(KEY_UP))        posDelta.z -= gridSnap;
            if (IsKeyPressed(KEY_PAGE_UP))   posDelta.y += gridSnap;
            if (IsKeyPressed(KEY_PAGE_DOWN)) posDelta.y -= gridSnap;

            if (IsValidEntityIdx(selectedEntityIdx)) {
                if (posDelta.x != 0.0f || posDelta.y != 0.0f || posDelta.z != 0.0f) {
                    PushUndo(world);
                    entities[selectedEntityIdx].pos = Vector3Add(entities[selectedEntityIdx].pos, posDelta);
                }

                if (IsKeyPressed(KEY_V)) {
                    PushUndo(world);
                    SceneEntityMarker copy = entities[selectedEntityIdx];
                    copy.pos.x += gridSnap;
                    entities.push_back(copy);
                    selectedEntityIdx = (int)entities.size() - 1;
                    ShowBanner("Marcador duplicado!", GOLD);
                    audio.PlayReload();
                }

                if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    PushUndo(world);
                    entities.erase(entities.begin() + selectedEntityIdx);
                    selectedEntityIdx = -1;
                    ShowBanner("Marcador apagado!", ORANGE);
                    audio.PlayOrder();
                }
            } else if (IsValidIdx(selectedBlockIdx, world)) {
                if (posDelta.x != 0.0f || posDelta.y != 0.0f || posDelta.z != 0.0f) {
                    PushUndo(world);
                    WorldBlock& b = world[selectedBlockIdx];
                    b.pos = Vector3Add(b.pos, posDelta);
                    RecalculateBounds(b);
                }

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
            DrawWorldBlock(b);

            bool isSelected = (toolMode == EditorToolMode::EDIT && (int)i == selectedBlockIdx);
            bool isHovered = ((int)i == hoveredBlockIdx && hoveredEntityIdx == -1);

            if (isSelected) {
                Vector3 expSize = { b.size.x + 0.06f, b.size.y + 0.06f, b.size.z + 0.06f };
                DrawCubeWiresV(b.pos, expSize, YELLOW);
            } else if (isHovered) {
                DrawCubeWiresV(b.pos, b.size, SKYBLUE);
            }
        }

        // Renderiza todos os marcadores de entidades jogáveis
        for (size_t i = 0; i < entities.size(); i++) {
            const auto& e = entities[i];
            bool isSelected = (toolMode == EditorToolMode::EDIT && (int)i == selectedEntityIdx);
            bool isHovered = ((int)i == hoveredEntityIdx);
            DrawEntityMarker3D(e, isSelected, isHovered, false);
        }

        // Fantasma no modo BUILD
        if (toolMode == EditorToolMode::BUILD && hasValidPlacement) {
            if (brushCategory == EditorBrushCategory::BLOCKS) {
                WorldBlock ghostBlock = MakeBlock(ghostPos, brushSize, Fade(GetPalette()[selectedColorIdx].color, 0.55f), brushSlopeDir);
                DrawWorldBlock(ghostBlock);
                DrawCubeWiresV(ghostPos, brushSize, LIME);
            } else {
                SceneEntityMarker ghostEnt{};
                ghostEnt.type = static_cast<SceneEntityType>(selectedEntityTypeIdx);
                ghostEnt.pos = ghostPos;
                ghostEnt.paramA = entityParamA;
                ghostEnt.paramB = entityParamB;
                ghostEnt.size = (ghostEnt.type == SceneEntityType::TRIGGER) ? triggerBrushSize
                              : (ghostEnt.type == SceneEntityType::EXTRACT ? Vector3{ extractBrushRadius, 0.2f, extractBrushRadius } : Vector3{ 1.0f, 1.0f, 1.0f });
                DrawEntityMarker3D(ghostEnt, false, false, true);
            }
        }
    }

    void DrawHUD(int screenWidth, int screenHeight, const std::vector<WorldBlock>& world) const {
        int cx = screenWidth / 2;
        int cy = screenHeight / 2;

        DrawLine(cx - 8, cy, cx + 8, cy, WHITE);
        DrawLine(cx, cy - 8, cx, cy + 8, WHITE);

        // Cabeçalho Superior Esquerdo
        DrawRectangle(16, 16, 480, 146, Color{ 16, 20, 28, 225 });
        DrawRectangleLinesEx({ 16, 16, 480, 146 }, 1.5f, SKYBLUE);

        const char* modeStr = (toolMode == EditorToolMode::BUILD)
            ? "MODO [TAB]: CONSTRUIR (BUILD)"
            : "MODO [TAB]: SELECIONAR / MOVER (EDIT)";
        DrawText(modeStr, 28, 24, 16, (toolMode == EditorToolMode::BUILD) ? LIME : GOLD);

        const char* catStr = (brushCategory == EditorBrushCategory::BLOCKS)
            ? TextFormat("PINCEL [B]: BLOCOS (%s)", GetSlopeLabel(brushSlopeDir))
            : TextFormat("PINCEL [B]: ENTIDADE (%s)", SceneEntityTypeToToken(static_cast<SceneEntityType>(selectedEntityTypeIdx)));
        DrawText(catStr, 28, 46, 15, (brushCategory == EditorBrushCategory::BLOCKS) ? SKYBLUE : ORANGE);

        DrawText(TextFormat("Destino [F6]: %s  |  Blocos: %d  |  Entidades: %d",
                            GetTargetFiles()[targetFileIdx], (int)world.size(), (int)entities.size()),
                 28, 68, 13, WHITE);

        DrawText(TextFormat("Grade [G]: %.2fm  |  [R] Rampa  |  [ / ] Parametros", gridSnap),
                 28, 88, 13, LIGHTGRAY);

        if (toolMode == EditorToolMode::EDIT && IsValidEntityIdx(selectedEntityIdx)) {
            const SceneEntityMarker& se = entities[selectedEntityIdx];
            DrawText(TextFormat("Entidade #%d [%s]: Pos(%.1f, %.1f, %.1f) pA=%d pB=%d (%s)",
                                selectedEntityIdx, SceneEntityTypeToToken(se.type),
                                se.pos.x, se.pos.y, se.pos.z, se.paramA, se.paramB, se.tag.c_str()),
                     28, 112, 13, GOLD);
            DrawText("Use Setas/PgUp/PgDn p/ mover, [ / ] p/ mudar slot/fase/cutscene, DEL p/ apagar",
                     28, 132, 12, YELLOW);
        } else if (toolMode == EditorToolMode::EDIT && IsValidIdx(selectedBlockIdx, world)) {
            const WorldBlock& sb = world[selectedBlockIdx];
            DrawText(TextFormat("Bloco #%d (%s): Pos(%.1f, %.1f, %.1f) Tam(%.2f, %.2f, %.2f)",
                                selectedBlockIdx, GetSlopeLabel(sb.slopeDir),
                                sb.pos.x, sb.pos.y, sb.pos.z, sb.size.x, sb.size.y, sb.size.z),
                     28, 112, 13, YELLOW);
            DrawText("Use Setas/PgUp/PgDn p/ mover, J/L U/O I/K p/ escala, R p/ rampa, V p/ duplicar",
                     28, 132, 12, LIGHTGRAY);
        } else {
            DrawText(brushCategory == EditorBrushCategory::BLOCKS
                ? TextFormat("Tamanho Pincel: %.2f x %.2f x %.2f (J/L U/O I/K)", brushSize.x, brushSize.y, brushSize.z)
                : TextFormat("Config Entidade: paramA=%d  paramB=%d  (Ajuste com [ e ])", entityParamA, entityParamB),
                28, 114, 13, SKYBLUE);
        }

        // Painel Superior Direito: Presets e Salvamento
        int rx = screenWidth - 340;
        DrawRectangle(rx, 16, 324, 146, Color{ 16, 20, 28, 225 });
        DrawRectangleLinesEx({ (float)rx, 16, 324, 146 }, 1.5f, GOLD);
        DrawText("ARQUIVOS E CENA DATA-DRIVEN:", rx + 12, 24, 14, GOLD);
        DrawText("[F1] Arena Squad   |  [F2] Estande", rx + 12, 46, 13, LIGHTGRAY);
        DrawText("[F3] Pista Bhop    |  [F4] Arena Limpa", rx + 12, 66, 13, LIGHTGRAY);
        DrawText("[F5] EXPORTAR .SCENE / .TXT", rx + 12, 90, 14, GREEN);
        DrawText("[Z] Desfazer       |  [ESC] Menu", rx + 12, 114, 13, ORANGE);

        // Banner de Notificação Central
        if (statusBannerTimer > 0.0f) {
            int bw = MeasureText(statusBannerText.c_str(), 17) + 32;
            DrawRectangle(cx - bw / 2, 174, bw, 32, Color{ 12, 16, 22, 240 });
            DrawRectangleLinesEx({ (float)(cx - bw / 2), 174, (float)bw, 32 }, 1.5f, statusBannerColor);
            DrawText(statusBannerText.c_str(), cx - MeasureText(statusBannerText.c_str(), 17) / 2, 181, 17, statusBannerColor);
        }

        // Paleta Inferior Dinâmica (Blocos 1..8 vs Entidades 1..7)
        int palW = 740;
        int palX = cx - palW / 2;
        int palY = screenHeight - 94;
        DrawRectangle(palX, palY, palW, 56, Color{ 16, 20, 28, 225 });
        DrawRectangleLinesEx({ (float)palX, (float)palY, (float)palW, 56 }, 1.2f, GRAY);

        if (brushCategory == EditorBrushCategory::BLOCKS) {
            const auto& palette = GetPalette();
            for (int i = 0; i < 8; i++) {
                int itemX = palX + 12 + i * 90;
                bool active = (selectedColorIdx == i);
                DrawRectangle(itemX, palY + 8, 82, 24, palette[i].color);
                DrawRectangleLinesEx({ (float)itemX, (float)(palY + 8), 82, 24 }, active ? 2.5f : 1.0f, active ? YELLOW : BLACK);
                DrawText(TextFormat("[%d]", i + 1), itemX + 4, palY + 37, 12, active ? YELLOW : LIGHTGRAY);
            }
        } else {
            const char* entLabels[7] = { "ALLY", "ENEMY", "TERM", "BARREL", "CRATE", "EXTRACT", "TRIGGER" };
            for (int i = 0; i < 7; i++) {
                int itemX = palX + 12 + i * 103;
                SceneEntityType et = static_cast<SceneEntityType>(i);
                Color ec = GetSceneEntityColor(et);
                bool active = (selectedEntityTypeIdx == i);
                DrawRectangle(itemX, palY + 8, 95, 24, Fade(ec, active ? 0.85f : 0.35f));
                DrawRectangleLinesEx({ (float)itemX, (float)(palY + 8), 95, 24 }, active ? 2.5f : 1.0f, active ? WHITE : ec);
                DrawText(entLabels[i], itemX + 48 - MeasureText(entLabels[i], 12) / 2, palY + 14, 12, WHITE);
                DrawText(TextFormat("[%d]", i + 1), itemX + 4, palY + 37, 12, active ? YELLOW : LIGHTGRAY);
            }
        }

        const char* helpLine = "WASD+Espaco/Ctrl: Voar | B: Alternar Blocos/Entidades | TAB: Construir/Editar | Click Esq/Dir: Inserir/Apagar | F5: Salvar";
        DrawText(helpLine, cx - MeasureText(helpLine, 13) / 2, screenHeight - 24, 13, LIGHTGRAY);
    }

private:
    static float SnapValue(float val, float step) {
        return roundf(val / step) * step;
    }

    static bool IsValidIdx(int idx, const std::vector<WorldBlock>& world) {
        return idx >= 0 && idx < (int)world.size();
    }

    bool IsValidEntityIdx(int idx) const {
        return idx >= 0 && idx < (int)entities.size();
    }

    static const char* GetSlopeLabel(int slopeDir) {
        switch (slopeDir) {
            case 1: return "Rampa +X";
            case 2: return "Rampa -X";
            case 3: return "Rampa +Z";
            case 4: return "Rampa -Z";
            default: return "Caixa";
        }
    }

    static void RecalculateBounds(WorldBlock& b) {
        b.bounds = b.ComputeBounds();
    }

    static WorldBlock MakeBlock(Vector3 pos, Vector3 size, Color color, int slopeDir = 0) {
        WorldBlock b{};
        b.pos = pos;
        b.size = size;
        b.color = color;
        b.slopeDir = slopeDir;
        RecalculateBounds(b);
        return b;
    }

    void SetDefaultParamsForEntity(SceneEntityType type) {
        switch (type) {
            case SceneEntityType::SPAWN_ALLY:
                entityParamA = 0; // 0=Alpha, 1=Bravo, 2=Charlie, 3=Delta
                entityParamB = 0;
                break;
            case SceneEntityType::SPAWN_ENEMY:
                entityParamA = 1; // minPhase (1..3)
                entityParamB = 0; // 0=inicial, 1=reforço pós-hack
                break;
            case SceneEntityType::TERMINAL:
                entityParamA = 1;
                for (const auto& e : entities) {
                    if (e.type == SceneEntityType::TERMINAL && e.paramA >= entityParamA) {
                        entityParamA = e.paramA + 1;
                    }
                }
                entityParamB = 0;
                break;
            case SceneEntityType::TRIGGER:
                entityParamA = 1; // cutsceneId
                entityParamB = 0; // requiresExtractionActive
                break;
            default:
                entityParamA = 0;
                entityParamB = 0;
                break;
        }
    }

    static void AdjustEntityParams(SceneEntityMarker& e, int delta) {
        switch (e.type) {
            case SceneEntityType::SPAWN_ALLY:
                e.paramA = (e.paramA + delta + 4) % 4;
                break;
            case SceneEntityType::SPAWN_ENEMY:
                if (delta > 0) {
                    if (e.paramB == 0 && e.paramA < 3) e.paramA++;
                    else if (e.paramB == 0) { e.paramA = 1; e.paramB = 1; }
                    else { e.paramA = 1; e.paramB = 0; }
                } else {
                    if (e.paramB == 1) { e.paramB = 0; e.paramA = 3; }
                    else if (e.paramA > 1) e.paramA--;
                    else { e.paramA = 1; e.paramB = 1; }
                }
                break;
            case SceneEntityType::TERMINAL:
                e.paramA = std::max(1, e.paramA + delta);
                break;
            case SceneEntityType::TRIGGER:
                e.paramA = (e.paramA + delta + 3) % 3;
                e.paramB = (e.paramA == 2) ? 1 : 0;
                break;
            default:
                break;
        }
        e.tag = GenerateDefaultTag(e.type, e.paramA, e.paramB);
    }

    static std::string GenerateDefaultTag(SceneEntityType type, int paramA, int paramB) {
        switch (type) {
            case SceneEntityType::SPAWN_ALLY: {
                const char* names[4] = { "Alpha", "Bravo", "Charlie", "Delta" };
                return names[std::clamp(paramA, 0, 3)];
            }
            case SceneEntityType::SPAWN_ENEMY:
                return (paramB == 1)
                    ? std::string("Reforco_Pos_Hack")
                    : std::string(TextFormat("Inimigo_Fase_%d", std::clamp(paramA, 1, 3)));
            case SceneEntityType::TERMINAL:
                return std::string(TextFormat("Terminal_0%d", std::max(1, paramA)));
            case SceneEntityType::BARREL:
                return "Barril_Explosivo";
            case SceneEntityType::CRATE:
                return "Caixa_Suprimentos";
            case SceneEntityType::EXTRACT:
                return "Zona_Extracao";
            case SceneEntityType::TRIGGER:
                return std::string(TextFormat("Trigger_Cutscene_%d", paramA));
            default:
                return "Marcador";
        }
    }

    static void DrawEntityMarker3D(const SceneEntityMarker& e, bool isSelected, bool isHovered, bool isGhost) {
        Color base = GetSceneEntityColor(e.type);
        float alpha = isGhost ? 0.55f : 0.90f;
        Color fill = Fade(base, alpha);
        Color wire = isSelected ? YELLOW : (isHovered ? WHITE : BLACK);

        if (e.type == SceneEntityType::SPAWN_ALLY || e.type == SceneEntityType::SPAWN_ENEMY) {
            DrawCylinder({ e.pos.x, e.pos.y, e.pos.z }, 0.36f, 0.36f, 1.65f, 12, fill);
            DrawCylinderWires({ e.pos.x, e.pos.y, e.pos.z }, 0.36f, 0.36f, 1.65f, 12, wire);
            DrawSphere({ e.pos.x, e.pos.y + 1.85f, e.pos.z }, 0.24f, fill);
        } else if (e.type == SceneEntityType::TERMINAL) {
            DrawCube({ e.pos.x, e.pos.y + 0.65f, e.pos.z }, 0.75f, 1.30f, 0.55f, fill);
            DrawCubeWires({ e.pos.x, e.pos.y + 0.65f, e.pos.z }, 0.75f, 1.30f, 0.55f, wire);
        } else if (e.type == SceneEntityType::BARREL) {
            DrawCylinder({ e.pos.x, e.pos.y, e.pos.z }, 0.39f, 0.39f, 1.25f, 12, fill);
            DrawCylinderWires({ e.pos.x, e.pos.y, e.pos.z }, 0.39f, 0.39f, 1.25f, 12, wire);
        } else if (e.type == SceneEntityType::CRATE) {
            DrawCube({ e.pos.x, e.pos.y + 0.40f, e.pos.z }, 1.15f, 0.80f, 0.75f, fill);
            DrawCubeWires({ e.pos.x, e.pos.y + 0.40f, e.pos.z }, 1.15f, 0.80f, 0.75f, wire);
        } else if (e.type == SceneEntityType::EXTRACT) {
            float rad = std::max(1.2f, e.size.x);
            DrawCircle3D({ e.pos.x, e.pos.y + 0.06f, e.pos.z }, rad, { 1.0f, 0.0f, 0.0f }, 90.0f, isSelected ? YELLOW : GREEN);
            DrawCircle3D({ e.pos.x, e.pos.y + 0.07f, e.pos.z }, rad * 0.65f, { 1.0f, 0.0f, 0.0f }, 90.0f, LIME);
            DrawLine3D({ e.pos.x, e.pos.y, e.pos.z }, { e.pos.x, e.pos.y + 3.5f, e.pos.z }, Fade(GREEN, 0.7f));
        } else if (e.type == SceneEntityType::TRIGGER) {
            DrawCubeV(e.pos, e.size, Fade(base, isGhost ? 0.25f : 0.18f));
            DrawCubeWiresV(e.pos, e.size, isSelected ? YELLOW : (isHovered ? WHITE : GOLD));
        }
    }

    void PushUndo(const std::vector<WorldBlock>& world) {
        if (undoStack.size() >= 20) {
            undoStack.erase(undoStack.begin());
        }
        undoStack.push_back({ world, entities });
    }

    void ShowBanner(const std::string& msg, Color col) {
        statusBannerText = msg;
        statusBannerColor = col;
        statusBannerTimer = 2.6f;
    }

    void LoadSceneIntoEditor(const std::string& filename, MapPreset preset, std::vector<WorldBlock>& world) {
        SceneData scene = LevelLoader::LoadSceneFromFile(filename, preset);
        world = scene.blocks;
        entities = scene.entities;
        selectedBlockIdx = -1;
        selectedEntityIdx = -1;
    }

    void BuildCleanArena(std::vector<WorldBlock>& world) {
        world.clear();
        entities.clear();
        world.push_back(MakeBlock({   0.0f, -0.5f,   0.0f }, { 50.0f, 1.0f, 50.0f }, { 58, 63,  72, 255 }));
        world.push_back(MakeBlock({   0.0f,  2.0f, -25.0f }, { 50.0f, 4.0f,  1.0f }, { 85, 92, 102, 255 }));
        world.push_back(MakeBlock({   0.0f,  2.0f,  25.0f }, { 50.0f, 4.0f,  1.0f }, { 85, 92, 102, 255 }));
        world.push_back(MakeBlock({ -25.0f,  2.0f,   0.0f }, {  1.0f, 4.0f, 50.0f }, { 85, 92, 102, 255 }));
        world.push_back(MakeBlock({  25.0f,  2.0f,   0.0f }, {  1.0f, 4.0f, 50.0f }, { 85, 92, 102, 255 }));

        entities.push_back({ SceneEntityType::SPAWN_ALLY,  {  0.0f, 0.5f,  14.0f }, { 1.0f, 1.0f, 1.0f }, 0, 0, "Alpha" });
        entities.push_back({ SceneEntityType::SPAWN_ENEMY, {  0.0f, 0.0f, -12.0f }, { 1.0f, 1.0f, 1.0f }, 1, 0, "Inimigo_Fase_1" });
        entities.push_back({ SceneEntityType::EXTRACT,     {  0.0f, 0.0f,  18.0f }, { 3.2f, 0.2f, 3.2f }, 0, 0, "Zona_Extracao" });
    }
};