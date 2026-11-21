#pragma once

#include "raylib.h"
#include <array>

enum class GameMode {
    MAIN_MENU,
    MOVEMENT_TEST,
    SUPPRESSION_RANGE,
    SQUAD_ARENA,
    MAP_EDITOR,
    EXIT_GAME
};

class MainMenu {
public:
    int selectedIndex = 0;

    GameMode Update(int screenWidth, int screenHeight) {
        const int optionCount = 5;

        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            selectedIndex = (selectedIndex + 1) % optionCount;
        }
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            selectedIndex = (selectedIndex - 1 + optionCount) % optionCount;
        }

        Vector2 mouse = GetMousePosition();
        int cardW = 540;
        int cardH = 66;
        int startX = screenWidth / 2 - cardW / 2;
        int startY = 175;

        for (int i = 0; i < optionCount; i++) {
            Rectangle rect = { (float)startX, (float)(startY + i * 78), (float)cardW, (float)cardH };
            if (CheckCollisionPointRec(mouse, rect)) {
                selectedIndex = i;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    return IndexToMode(i);
                }
            }
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            return IndexToMode(selectedIndex);
        }

        if (IsKeyPressed(KEY_ONE))   return GameMode::SQUAD_ARENA;
        if (IsKeyPressed(KEY_TWO))   return GameMode::SUPPRESSION_RANGE;
        if (IsKeyPressed(KEY_THREE)) return GameMode::MOVEMENT_TEST;
        if (IsKeyPressed(KEY_FOUR))  return GameMode::MAP_EDITOR;
        if (IsKeyPressed(KEY_FIVE))  return GameMode::EXIT_GAME;

        return GameMode::MAIN_MENU;
    }

    void Draw(int screenWidth, int screenHeight) const {
        ClearBackground({ 16, 19, 26, 255 });

        const char* title = "SQUAD TACTICAL SHOOTER";
        const char* subtitle = "Prototipo Modular - SAVEPOINT_14 (Editor de Mapas In-Game)";
        DrawText(title, screenWidth / 2 - MeasureText(title, 38) / 2, 65, 38, WHITE);
        DrawText(subtitle, screenWidth / 2 - MeasureText(subtitle, 17) / 2, 112, 17, SKYBLUE);

        struct MenuOption {
            const char* label;
            const char* desc;
            Color accent;
        };

        const std::array<MenuOption, 5> options = {{
            { "[1] MISSAO TATICA DE ESQUADRAO", "Hackear terminais, suprimir, flanquear e extrair (mapa_squad.txt)", GREEN },
            { "[2] ESTANDE DE TIRO & SUPRESSAO", "Sandbox livre com barris explosivos e caixas de municao", ORANGE },
            { "[3] PISTA DE MOVIMENTACAO (SOURCE)", "Teste de Bunny Hop, Air Strafe e Crouch-Jump", SKYBLUE },
            { "[4] EDITOR DE MAPAS IN-GAME", "Construir, redimensionar, pintar e salvar mapas 3D em .txt", GOLD },
            { "[5] SAIR DO JOGO", "Encerrar aplicacao", RED }
        }};

        int cardW = 540;
        int cardH = 66;
        int startX = screenWidth / 2 - cardW / 2;
        int startY = 175;

        for (int i = 0; i < 5; i++) {
            bool active = (selectedIndex == i);
            int y = startY + i * 78;

            Color bg = active ? Color{ 32, 38, 52, 255 } : Color{ 22, 26, 35, 255 };
            Color border = active ? options[i].accent : Color{ 60, 68, 82, 255 };

            DrawRectangle(startX, y, cardW, cardH, bg);
            DrawRectangleLinesEx({ (float)startX, (float)y, (float)cardW, (float)cardH }, active ? 2.5f : 1.0f, border);

            DrawText(options[i].label, startX + 18, y + 12, 19, active ? options[i].accent : WHITE);
            DrawText(options[i].desc, startX + 18, y + 38, 14, LIGHTGRAY);
        }

        const char* footer = "Use Setas / W-S ou Mouse para navegar  |  ENTER ou Clique para confirmar";
        DrawText(footer, screenWidth / 2 - MeasureText(footer, 15) / 2, screenHeight - 36, 15, GRAY);
    }

private:
    static GameMode IndexToMode(int idx) {
        switch (idx) {
            case 0: return GameMode::SQUAD_ARENA;
            case 1: return GameMode::SUPPRESSION_RANGE;
            case 2: return GameMode::MOVEMENT_TEST;
            case 3: return GameMode::MAP_EDITOR;
            default: return GameMode::EXIT_GAME;
        }
    }
};