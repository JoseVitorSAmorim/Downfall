#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

// Constantes de física e movimentação
inline constexpr float GRAVITY         = 18.0f;
inline constexpr float JUMP_FORCE      = 6.8f;
inline constexpr float MAX_WALK_SPD    = 6.4f;
inline constexpr float MAX_SPRINT_SPD  = 9.8f;
inline constexpr float MAX_CROUCH_SPD  = 2.6f;
inline constexpr float MAX_DRAG_SPD    = 3.4f; // Velocidade ao arrastar aliado
inline constexpr float MAX_DOWNED_SPD  = 1.6f; // Velocidade rastejando caído
inline constexpr float GROUND_ACCEL    = 10.0f;
inline constexpr float FRICTION        = 6.0f;
inline constexpr float AIR_ACCEL       = 14.0f;
inline constexpr float AIR_SPEED_CAP   = 0.75f;

// Dimensões do jogador
inline constexpr float STAND_HEIGHT    = 1.80f;
inline constexpr float CROUCH_HEIGHT   = 1.00f;
inline constexpr float HEIGHT_DIFF     = STAND_HEIGHT - CROUCH_HEIGHT;
inline constexpr float STAND_EYE       = 1.65f;
inline constexpr float CROUCH_EYE      = 0.85f;

class SourcePlayer {
public:
    Vector3 pos = { 0.0f, 0.5f, 14.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float halfWidth = 0.4f;
    float height = STAND_HEIGHT;
    float currentEyeHeight = STAND_EYE;
    float yaw = PI;
    float pitch = 0.0f;
    bool isGrounded = false;
    bool isCrouched = false;
    bool isSprinting = false;

    // Estados de combate e cooperação
    float hp = 100.0f;
    float aggro = 15.0f;
    bool isDowned = false;
    int draggingAllyId = -1;
    float reviveProgress = 0.0f;

    BoundingBox GetAABBAt(Vector3 basePos, float boxHeight) const {
        return {
            { basePos.x - halfWidth, basePos.y,             basePos.z - halfWidth },
            { basePos.x + halfWidth, basePos.y + boxHeight, basePos.z + halfWidth }
        };
    }

    BoundingBox GetAABB() const { return GetAABBAt(pos, height); }
    Vector3 GetEyePosition() const { return { pos.x, pos.y + currentEyeHeight, pos.z }; }

    Vector3 GetForward3D() const {
        return {
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };
    }

    void UpdateLook(Vector2 delta, float sensitivity) {
        yaw -= delta.x * sensitivity;
        pitch -= delta.y * sensitivity;
        pitch = std::clamp(pitch, -89.0f * DEG2RAD, 89.0f * DEG2RAD);
    }

    void Update(float dt, const std::vector<WorldBlock>& world) {
        if (isDowned) {
            // Força postura baixa no chão e zera ameaça
            isCrouched = true;
            height = CROUCH_HEIGHT;
            isSprinting = false;
            draggingAllyId = -1;
            aggro = 0.0f;
            currentEyeHeight += (0.55f - currentEyeHeight) * std::min(12.0f * dt, 1.0f);
        } else {
            HandleCrouch(IsKeyDown(KEY_LEFT_CONTROL), dt, world);
        }

        Vector3 forward = { sinf(yaw), 0.0f, cosf(yaw) };
        Vector3 right   = { -cosf(yaw), 0.0f, sinf(yaw) };

        Vector3 wishDir = { 0.0f, 0.0f, 0.0f };
        if (IsKeyDown(KEY_W)) wishDir = Vector3Add(wishDir, forward);
        if (IsKeyDown(KEY_S)) wishDir = Vector3Subtract(wishDir, forward);
        if (IsKeyDown(KEY_D)) wishDir = Vector3Add(wishDir, right);
        if (IsKeyDown(KEY_A)) wishDir = Vector3Subtract(wishDir, right);

        if (Vector3LengthSqr(wishDir) > 0.0f) wishDir = Vector3Normalize(wishDir);

        bool isDragging = (draggingAllyId != -1);
        isSprinting = !isDowned && !isDragging && IsKeyDown(KEY_LEFT_SHIFT) && !isCrouched && IsKeyDown(KEY_W);

        float targetSpeed = MAX_WALK_SPD;
        if (isDowned)         targetSpeed = MAX_DOWNED_SPD;
        else if (isDragging)  targetSpeed = MAX_DRAG_SPD;
        else if (isCrouched)  targetSpeed = MAX_CROUCH_SPD;
        else if (isSprinting) targetSpeed = MAX_SPRINT_SPD;

        if (!isDowned && isGrounded && IsKeyDown(KEY_SPACE)) {
            vel.y = JUMP_FORCE;
            isGrounded = false;
        }

        if (isGrounded) {
            ApplyFriction(dt);
            Accelerate(wishDir, targetSpeed, GROUND_ACCEL, dt);
        } else {
            AirAccelerate(wishDir, targetSpeed, AIR_ACCEL, dt);
            vel.y -= GRAVITY * dt;
        }

        MoveAndCollide(dt, world);

        // Regenera HP apenas se estiver de pé
        if (!isDowned && hp < 100.0f) {
            hp = std::min(100.0f, hp + 3.5f * dt);
        }
    }

private:
    bool OverlapsAnyBlock(const BoundingBox& box, const std::vector<WorldBlock>& world) const {
        for (const auto& block : world) {
            if (CheckOverlapStrict(box, block.bounds)) return true;
        }
        return false;
    }

    void HandleCrouch(bool wantCrouch, float dt, const std::vector<WorldBlock>& world) {
        if (wantCrouch && !isCrouched) {
            isCrouched = true;
            height = CROUCH_HEIGHT;
            if (!isGrounded) {
                pos.y += HEIGHT_DIFF;
                currentEyeHeight = CROUCH_EYE;
            }
        } else if (!wantCrouch && isCrouched) {
            if (isGrounded) {
                if (!OverlapsAnyBlock(GetAABBAt(pos, STAND_HEIGHT), world)) {
                    isCrouched = false;
                    height = STAND_HEIGHT;
                }
            } else {
                Vector3 lowerPos = { pos.x, pos.y - HEIGHT_DIFF, pos.z };
                if (!OverlapsAnyBlock(GetAABBAt(lowerPos, STAND_HEIGHT), world)) {
                    pos.y -= HEIGHT_DIFF;
                    isCrouched = false;
                    height = STAND_HEIGHT;
                    currentEyeHeight = STAND_EYE;
                }
            }
        }
        float targetEye = isCrouched ? CROUCH_EYE : STAND_EYE;
        currentEyeHeight += (targetEye - currentEyeHeight) * std::min(16.0f * dt, 1.0f);
    }

    void MoveAndCollide(float dt, const std::vector<WorldBlock>& world) {
        // 1. Eixo X
        pos.x += vel.x * dt;
        BoundingBox playerBox = GetAABB();
        for (const auto& block : world) {
            if (CheckOverlapStrict(playerBox, block.bounds)) {
                if (vel.x > 0.0f) pos.x = block.bounds.min.x - halfWidth;
                else if (vel.x < 0.0f) pos.x = block.bounds.max.x + halfWidth;
                vel.x = 0.0f;
                playerBox = GetAABB();
            }
        }

        // 2. Eixo Z
        pos.z += vel.z * dt;
        playerBox = GetAABB();
        for (const auto& block : world) {
            if (CheckOverlapStrict(playerBox, block.bounds)) {
                if (vel.z > 0.0f) pos.z = block.bounds.min.z - halfWidth;
                else if (vel.z < 0.0f) pos.z = block.bounds.max.z + halfWidth;
                vel.z = 0.0f;
                playerBox = GetAABB();
            }
        }

        // 3. Eixo Y
        pos.y += vel.y * dt;
        isGrounded = false;
        playerBox = GetAABB();
        for (const auto& block : world) {
            if (CheckOverlapStrict(playerBox, block.bounds)) {
                if (vel.y <= 0.0f) {
                    pos.y = block.bounds.max.y;
                    isGrounded = true;
                } else if (vel.y > 0.0f) {
                    pos.y = block.bounds.min.y - height;
                }
                vel.y = 0.0f;
                playerBox = GetAABB();
            }
        }
    }

    void ApplyFriction(float dt) {
        float speed = sqrtf(vel.x * vel.x + vel.z * vel.z);
        if (speed < 0.01f) { vel.x = 0.0f; vel.z = 0.0f; return; }
        float drop = speed * FRICTION * dt;
        float newSpeed = std::max(speed - drop, 0.0f) / speed;
        vel.x *= newSpeed;
        vel.z *= newSpeed;
    }

    void Accelerate(Vector3 wishDir, float wishSpeed, float accel, float dt) {
        float currentSpeed = Vector3DotProduct(vel, wishDir);
        float addSpeed = wishSpeed - currentSpeed;
        if (addSpeed <= 0.0f) return;
        float accelSpeed = std::min(accel * wishSpeed * dt, addSpeed);
        vel.x += accelSpeed * wishDir.x;
        vel.z += accelSpeed * wishDir.z;
    }

    void AirAccelerate(Vector3 wishDir, float wishSpeed, float accel, float dt) {
        float wishSpd = std::min(wishSpeed, AIR_SPEED_CAP);
        float currentSpeed = Vector3DotProduct(vel, wishDir);
        float addSpeed = wishSpd - currentSpeed;
        if (addSpeed <= 0.0f) return;
        float accelSpeed = std::min(accel * wishSpeed * dt, addSpeed);
        vel.x += accelSpeed * wishDir.x;
        vel.z += accelSpeed * wishDir.z;
    }
};