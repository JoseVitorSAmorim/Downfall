#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

class ViewmodelWeapon {
public:
    Vector2 swayOffset = { 0.0f, 0.0f };
    float bobTimer = 0.0f;
    float recoilZ = 0.0f;
    float recoilPitch = 0.0f;
    float muzzleFlashTimer = 0.0f;

    void TriggerFire() {
        recoilZ = 0.13f;
        recoilPitch = 10.0f;
        muzzleFlashTimer = 0.05f;
    }

    void Update(float dt, Vector2 mouseDelta, float horizSpeed, bool isGrounded) {
        float targetSwayX = std::clamp(-mouseDelta.x * 0.0022f, -0.06f, 0.06f);
        float targetSwayY = std::clamp(mouseDelta.y * 0.0022f, -0.06f, 0.06f);
        swayOffset.x += (targetSwayX - swayOffset.x) * std::min(14.0f * dt, 1.0f);
        swayOffset.y += (targetSwayY - swayOffset.y) * std::min(14.0f * dt, 1.0f);

        if (isGrounded && horizSpeed > 0.5f) {
            bobTimer += dt * (7.0f + horizSpeed * 0.9f);
        } else {
            bobTimer = 0.0f;
        }

        recoilZ += (0.0f - recoilZ) * std::min(16.0f * dt, 1.0f);
        recoilPitch += (0.0f - recoilPitch) * std::min(14.0f * dt, 1.0f);
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;
    }

    void Draw(const Camera3D& camera, bool isStealthFlank) const {
        Matrix invView = MatrixInvert(GetCameraMatrix(camera));
        float bobX = cosf(bobTimer) * 0.012f;
        float bobY = fabsf(sinf(bobTimer)) * -0.014f;

        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(invView));

        rlTranslatef(0.27f + swayOffset.x + bobX, -0.23f + swayOffset.y + bobY, -0.52f + recoilZ);
        rlRotatef(recoilPitch, 1.0f, 0.0f, 0.0f);
        rlRotatef(swayOffset.x * 120.0f, 0.0f, 1.0f, 0.0f);

        // Corpo da arma
        DrawCube({ 0.0f, 0.0f, 0.0f }, 0.10f, 0.12f, 0.44f, { 42, 46, 54, 255 });
        DrawCubeWires({ 0.0f, 0.0f, 0.0f }, 0.10f, 0.12f, 0.44f, BLACK);
        DrawCube({ 0.0f, 0.015f, -0.32f }, 0.045f, 0.045f, 0.24f, { 28, 30, 35, 255 });

        // Visor traseiro indica o estado de Aggro (Ciano = Furtivo 2x Dano | Laranja = Aggro Alto)
        Color statusLed = isStealthFlank ? SKYBLUE : ORANGE;
        DrawCube({ 0.0f, 0.075f, 0.08f }, 0.06f, 0.025f, 0.12f, statusLed);

        if (muzzleFlashTimer > 0.0f) {
            DrawSphere({ 0.0f, 0.015f, -0.47f }, 0.065f, GOLD);
        }

        rlPopMatrix();
    }
};