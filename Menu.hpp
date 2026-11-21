#pragma once

#include "raylib.h"
#include <vector>
#include <string>

enum class GameMode {
    MAIN_MENU,
    TACTICAL_SQUAD,
    MOVEMENT_TEST,
    SUPPRESSION_RANGE,
    EXIT_GAME
};

struct MenuOption {
    GameMode mode;
    std::string title;
    std::string description;
    Color accent;
};

class MainMenu {
public:
    int selectedIndex = 0;

    std::vector<MenuOption> options = {
        {
            GameMode::TACTICAL_SQUAD,
            "1. COMBATE TATICO (ESQUADRAO 4x4 & AGGRO)",
            "Arena completa com 3 aliados, inimigos com cobertura, supressao, arrasto (E) e ordens (1/2/3).",
            ORANGE
        },
        {
            GameMode::MOVEMENT_TEST,
            "2. PISTA DE MOVIMENTACAO (SOURCE PHYSICS)",
            "Circuito de parkour para testar Bunny Hop, Air Strafe, Sprint (Shift) e Crouch-Jump (Espaco+Ctrl).",
            SKYBLUE
        },
        {
            GameMode::SUPPRESSION_RANGE,
            "3. ESTANDE DE TIRO & FLANCO FURTIVO",
            "Campo de treino com barreiras para testar recuo, dano furtivo (2x) e comportamento da IA.",
            LIME
        },
        {
            GameMode::EXIT_GAME,
            "4. SAIR DO PROTOTIPO",
            "Encerra a aplicacao e fecha a janela.",
            RED
        }
    };

    GameMode Update(int screenWidth, int screenHeight) {
        // Navegação por teclado
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            selectedIndex = (selectedIndex + 1) % (int)options.size();
        }
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            selectedIndex = (selectedIndex - 1 + (int)options.size()) % (int)options.size();
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            return options[selectedIndex].mode;
        }

        // Atalhos diretos numéricos (1 a 4)
        if (IsKeyPressed(KEY_ONE))   return options[0].mode;
        if (IsKeyPressed(KEY_TWO))   return options[1].mode;
        if (IsKeyPressed(KEY_THREE)) return options[2].mode;
        if (IsKeyPressed(KEY_FOUR))  return options[3].mode;

        // Navegação e clique com o Mouse
        Vector2 mouse = GetMousePosition();
        int cardW = 760;
        int cardH = 82;
        int startX = screenWidth / 2 - cardW / 2;
        int startY = 210;
        int spacing = 96;

        for (size_t i = 0; i < options.size(); i++) {
            Rectangle cardRect = { (float)startX, (float)(startY + (int)i * spacing), (float)cardW, (float)cardH };
            if (CheckCollisionPointRec(mouse, cardRect)) {
                selectedIndex = (int)i;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    return options[i].mode;
                }
            }
        }

        return GameMode::MAIN_MENU;
    }

    void Draw(int screenWidth, int screenHeight) const {
        ClearBackground({ 16, 19, 26, 255 });

        // Grade decorativa sutil no fundo
        for (int x = 0; x < screenWidth; x += 40) {
            DrawLine(x, 0, x, screenHeight, Fade(WHITE, 0.03f));
        }
        for (int y = 0; y < screenHeight; y += 40) {
            DrawLine(0, y, screenWidth, y, Fade(WHITE, 0.03f));
        }

        // Cabeçalho
        const char* title = "SQUAD TACTICAL ENGINE // SANDBOX";
        const char* subtitle = "Selecione um ambiente de teste abaixo (Mouse, Setas+Enter ou Teclas 1-4)";
        DrawText(title, screenWidth / 2 - MeasureText(title, 36) / 2, 70, 36, WHITE);
        DrawText(subtitle, screenWidth / 2 - MeasureText(subtitle, 18) / 2, 120, 18, LIGHTGRAY);

        DrawLine(screenWidth / 2 - 380, 160, screenWidth / 2 + 380, 160, Fade(SKYBLUE, 0.4f));

        // Cartões de seleção
        int cardW = 760;
        int cardH = 82;
        int startX = screenWidth / 2 - cardW / 2;
        int startY = 210;
        int spacing = 96;

        for (size_t i = 0; i < options.size(); i++) {
            bool isSelected = ((int)i == selectedIndex);
            int y = startY + (int)i * spacing;

            Color bgCol = isSelected ? Color{ 32, 38, 50, 255 } : Color{ 23, 27, 36, 255 };
            Color borderCol = isSelected ? options[i].accent : Fade(GRAY, 0.35f);

            DrawRectangle(startX, y, cardW, cardH, bgCol);
            DrawRectangleLinesEx({ (float)startX, (float)y, (float)cardW, (float)cardH }, isSelected ? 2.5f : 1.0f, borderCol);

            // Faixa lateral colorida
            DrawRectangle(startX, y, 8, cardH, options[i].accent);

            DrawText(options[i].title.c_str(), startX + 24, y + 16, 20, isSelected ? WHITE : LIGHTGRAY);
            DrawText(options[i].description.c_str(), startX + 24, y + 46, 15, isSelected ? SKYBLUE : GRAY);
        }

        // Rodapé informativo
        const char* footer = "SAVEPOINT_08 | Dentro de qualquer teste, pressione [ESC] para retornar a este menu";
        DrawText(footer, screenWidth / 2 - MeasureText(footer, 16) / 2, screenHeight - 40, 16, GRAY);
    }
};