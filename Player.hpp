#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

class SourcePlayer {
public:
    static constexpr float GRAVITY          = 20.0f;
    static constexpr float JUMP_IMPULSE     = 7.6f;
    static constexpr float STAND_HEIGHT     = 1.80f;
    static constexpr float CROUCH_HEIGHT    = 1.00f;
    static constexpr float HEIGHT_DIFF      = STAND_HEIGHT - CROUCH_HEIGHT;
    static constexpr float HALF_WIDTH       = 0.38f;
    static constexpr float MAX_STEP_HEIGHT  = 0.55f; // Altura máxima de degrau automático

    static constexpr float MAX_WALK_SPD     = 6.2f;
    static constexpr float MAX_SPRINT_SPD   = 9.8f;
    static constexpr float MAX_CROUCH_SPD   = 3.1f;
    static constexpr float MAX_DRAG_SPD     = 3.4f;
    static constexpr float MAX_DOWNED_SPD   = 1.6f;

    static constexpr float GROUND_ACCEL     = 11.0f;
    static constexpr float AIR_ACCEL        = 14.0f;
    static constexpr float AIR_SPEED_CAP    = 0.75f;
    static constexpr float FRICTION         = 6.0f;
    static constexpr float STOP_SPEED       = 1.5f;

    Vector3 pos = { 0.0f, 0.5f, 14.0f }; // Posição na base dos pés
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float yaw = 0.0f;
    float pitch = 0.0f;
    float currentHeight = STAND_HEIGHT;

    bool isGrounded = false;
    bool isCrouched = false;
    bool isSprinting = false;

    // Estado de combate / esquadrão
    float hp = 100.0f;
    float aggro = 0.0f;
    bool isDowned = false;
    int draggingAllyId = -1;
    float reviveProgress = 0.0f;

    BoundingBox GetBoxAt(Vector3 feetPos, float h) const noexcept {
        return {
            { feetPos.x - HALF_WIDTH, feetPos.y,     feetPos.z - HALF_WIDTH },
            { feetPos.x + HALF_WIDTH, feetPos.y + h, feetPos.z + HALF_WIDTH }
        };
    }

    Vector3 GetEyePosition() const noexcept {
        return { pos.x, pos.y + currentHeight - 0.15f, pos.z };
    }

    Vector3 GetForward3D() const noexcept {
        float cosPitch = std::cos(pitch);
        return {
            std::sin(yaw) * cosPitch,
            std::sin(pitch),
            std::cos(yaw) * cosPitch
        };
    }

    void UpdateLook(Vector2 mouseDelta, float sensitivity) {
        yaw   -= mouseDelta.x * sensitivity;
        pitch -= mouseDelta.y * sensitivity;
        const float maxPitch = 89.0f * DEG2RAD;
        pitch = std::clamp(pitch, -maxPitch, maxPitch);
    }

    bool OverlapsAnyBlock(const BoundingBox& box, const std::vector<WorldBlock>& world) const {
        const SpatialHashGrid& grid = GetWorldSpatialGrid(world);
        Vector3 center = {
            (box.min.x + box.max.x) * 0.5f,
            (box.min.y + box.max.y) * 0.5f,
            (box.min.z + box.max.z) * 0.5f
        };
        Vector3 size = {
            box.max.x - box.min.x,
            box.max.y - box.min.y,
            box.max.z - box.min.z
        };

        bool hit = false;
        grid.QueryAABB(center, size, [&](int idx) -> bool {
            const WorldBlock& block = world[idx];
            if (!CheckOverlapStrict(box, block.GetAABB())) return true;

            if (block.IsSlope()) {
                float surfY = block.GetMaxSurfaceUnderBox(box);
                if (box.min.y >= surfY - 0.02f) return true;
            }
            hit = true;
            return false;
        });
        return hit;
    }

    void Update(float dt, const std::vector<WorldBlock>& world) {
        HandleCrouch(world);

        Vector3 forwardFlat = { std::sin(yaw), 0.0f, std::cos(yaw) };
        Vector3 rightFlat   = { -forwardFlat.z, 0.0f, forwardFlat.x };

        float moveForward = 0.0f;
        float moveRight   = 0.0f;
        if (IsKeyDown(KEY_W)) moveForward += 1.0f;
        if (IsKeyDown(KEY_S)) moveForward -= 1.0f;
        if (IsKeyDown(KEY_D)) moveRight   += 1.0f;
        if (IsKeyDown(KEY_A)) moveRight   -= 1.0f;

        Vector3 wishDir = Vector3Add(Vector3Scale(forwardFlat, moveForward), Vector3Scale(rightFlat, moveRight));
        if (Vector3LengthSqr(wishDir) > 0.0001f) {
            wishDir = Vector3Normalize(wishDir);
        }

        isSprinting = (!isDowned && !isCrouched && draggingAllyId == -1 &&
                       isGrounded && IsKeyDown(KEY_LEFT_SHIFT) && moveForward > 0.0f);

        float maxSpeed = MAX_WALK_SPD;
        if (isDowned)                maxSpeed = MAX_DOWNED_SPD;
        else if (draggingAllyId != -1) maxSpeed = MAX_DRAG_SPD;
        else if (isCrouched)         maxSpeed = MAX_CROUCH_SPD;
        else if (isSprinting)        maxSpeed = MAX_SPRINT_SPD;

        if (isGrounded) {
            ApplyFriction(dt);
            Accelerate(wishDir, maxSpeed, GROUND_ACCEL, dt);

            // Auto-Bhop segurando Espaço
            if (!isDowned && IsKeyDown(KEY_SPACE)) {
                vel.y = JUMP_IMPULSE;
                isGrounded = false;
            }
        } else {
            AirAccelerate(wishDir, maxSpeed, AIR_ACCEL, dt);
            vel.y -= GRAVITY * dt;
        }

        MoveAndCollide(dt, world);
    }

    void MoveAndCollide(float dt, const std::vector<WorldBlock>& world) {
        const SpatialHashGrid& grid = GetWorldSpatialGrid(world);

        // =====================================================================
        // 1. RESOLUÇÃO HORIZONTAL X (COM STEP-UP E SUBIDA DE RAMPAS)
        // =====================================================================
        float moveX = vel.x * dt;
        if (std::fabs(moveX) > 1e-6f) {
            float targetX = pos.x + moveX;
            BoundingBox testBox = GetBoxAt({ targetX, pos.y, pos.z }, currentHeight);
            Vector3 qCenter = { targetX, pos.y + currentHeight * 0.5f, pos.z };
            Vector3 qSize   = { HALF_WIDTH * 2.0f + 0.1f, currentHeight + MAX_STEP_HEIGHT, HALF_WIDTH * 2.0f + 0.1f };

            bool blocked = false;
            float requiredLiftY = pos.y;
            float clampedX = targetX;

            grid.QueryAABB(qCenter, qSize, [&](int idx) -> bool {
                const WorldBlock& block = world[idx];
                BoundingBox bb = block.GetAABB();
                if (!CheckOverlapStrict(testBox, bb)) return true;

                if (block.IsSlope()) {
                    float surfY = block.GetMaxSurfaceUnderBox(testBox);
                    if (pos.y >= surfY - MAX_STEP_HEIGHT) {
                        requiredLiftY = std::max(requiredLiftY, surfY);
                        return true;
                    }
                } else {
                    float stepDelta = bb.max.y - pos.y;
                    if (isGrounded && stepDelta > 0.0f && stepDelta <= MAX_STEP_HEIGHT) {
                        requiredLiftY = std::max(requiredLiftY, bb.max.y);
                        return true;
                    }
                }

                blocked = true;
                if (moveX > 0.0f) clampedX = std::min(clampedX, bb.min.x - HALF_WIDTH);
                else              clampedX = std::max(clampedX, bb.max.x + HALF_WIDTH);
                return true;
            });

            if (!blocked) {
                if (requiredLiftY > pos.y + 1e-4f) {
                    BoundingBox steppedBox = GetBoxAt({ targetX, requiredLiftY, pos.z }, currentHeight);
                    if (!OverlapsAnyBlock(steppedBox, world)) {
                        pos.x = targetX;
                        pos.y = requiredLiftY;
                        isGrounded = true;
                    } else {
                        vel.x = 0.0f;
                    }
                } else {
                    pos.x = targetX;
                }
            } else {
                pos.x = clampedX;
                vel.x = 0.0f;
            }
        }

        // =====================================================================
        // 2. RESOLUÇÃO HORIZONTAL Z (COM STEP-UP E SUBIDA DE RAMPAS)
        // =====================================================================
        float moveZ = vel.z * dt;
        if (std::fabs(moveZ) > 1e-6f) {
            float targetZ = pos.z + moveZ;
            BoundingBox testBox = GetBoxAt({ pos.x, pos.y, targetZ }, currentHeight);
            Vector3 qCenter = { pos.x, pos.y + currentHeight * 0.5f, targetZ };
            Vector3 qSize   = { HALF_WIDTH * 2.0f + 0.1f, currentHeight + MAX_STEP_HEIGHT, HALF_WIDTH * 2.0f + 0.1f };

            bool blocked = false;
            float requiredLiftY = pos.y;
            float clampedZ = targetZ;

            grid.QueryAABB(qCenter, qSize, [&](int idx) -> bool {
                const WorldBlock& block = world[idx];
                BoundingBox bb = block.GetAABB();
                if (!CheckOverlapStrict(testBox, bb)) return true;

                if (block.IsSlope()) {
                    float surfY = block.GetMaxSurfaceUnderBox(testBox);
                    if (pos.y >= surfY - MAX_STEP_HEIGHT) {
                        requiredLiftY = std::max(requiredLiftY, surfY);
                        return true;
                    }
                } else {
                    float stepDelta = bb.max.y - pos.y;
                    if (isGrounded && stepDelta > 0.0f && stepDelta <= MAX_STEP_HEIGHT) {
                        requiredLiftY = std::max(requiredLiftY, bb.max.y);
                        return true;
                    }
                }

                blocked = true;
                if (moveZ > 0.0f) clampedZ = std::min(clampedZ, bb.min.z - HALF_WIDTH);
                else              clampedZ = std::max(clampedZ, bb.max.z + HALF_WIDTH);
                return true;
            });

            if (!blocked) {
                if (requiredLiftY > pos.y + 1e-4f) {
                    BoundingBox steppedBox = GetBoxAt({ pos.x, requiredLiftY, targetZ }, currentHeight);
                    if (!OverlapsAnyBlock(steppedBox, world)) {
                        pos.z = targetZ;
                        pos.y = requiredLiftY;
                        isGrounded = true;
                    } else {
                        vel.z = 0.0f;
                    }
                } else {
                    pos.z = targetZ;
                }
            } else {
                pos.z = clampedZ;
                vel.z = 0.0f;
            }
        }

        // =====================================================================
        // 3. RESOLUÇÃO VERTICAL Y + SNAPPING EM RAMPAS AO DESCER
        // =====================================================================
        bool wasGrounded = isGrounded;
        isGrounded = false;
        pos.y += vel.y * dt;

        BoundingBox playerBox = GetBoxAt(pos, currentHeight);
        Vector3 qCenter = { pos.x, pos.y + currentHeight * 0.5f, pos.z };
        Vector3 qSize   = { HALF_WIDTH * 2.0f + 0.1f, currentHeight + 0.6f, HALF_WIDTH * 2.0f + 0.1f };

        grid.QueryAABB(qCenter, qSize, [&](int idx) -> bool {
            const WorldBlock& block = world[idx];
            BoundingBox bb = block.GetAABB();
            if (!CheckOverlapStrict(playerBox, bb)) return true;

            if (block.IsSlope()) {
                float surfY = block.GetMaxSurfaceUnderBox(playerBox);
                if (vel.y <= 0.0f && pos.y <= surfY + 0.05f) {
                    pos.y = surfY;
                    vel.y = 0.0f;
                    isGrounded = true;
                    playerBox = GetBoxAt(pos, currentHeight);
                } else if (vel.y > 0.0f && pos.y + currentHeight > bb.min.y && pos.y < bb.min.y) {
                    pos.y = bb.min.y - currentHeight;
                    vel.y = 0.0f;
                    playerBox = GetBoxAt(pos, currentHeight);
                }
            } else {
                if (vel.y <= 0.0f) {
                    pos.y = bb.max.y;
                    vel.y = 0.0f;
                    isGrounded = true;
                } else {
                    pos.y = bb.min.y - currentHeight;
                    vel.y = 0.0f;
                }
                playerBox = GetBoxAt(pos, currentHeight);
            }
            return true;
        });

        // Cola o jogador à superfície ao descer rampas suaves sem quicar
        if (!isGrounded && wasGrounded && vel.y <= 0.0f) {
            BoundingBox snapBox = GetBoxAt({ pos.x, pos.y - 0.28f, pos.z }, currentHeight);
            float bestFloorY = -1e30f;
            bool foundFloor = false;

            grid.QueryAABB({ pos.x, pos.y, pos.z }, { HALF_WIDTH * 2.0f, 0.8f, HALF_WIDTH * 2.0f }, [&](int idx) -> bool {
                const WorldBlock& block = world[idx];
                BoundingBox bb = block.GetAABB();
                if (!CheckOverlapStrict(snapBox, bb)) return true;

                float floorY = block.IsSlope() ? block.GetMaxSurfaceUnderBox(snapBox) : bb.max.y;
                if (floorY <= pos.y + 0.04f && floorY >= pos.y - 0.28f && floorY > bestFloorY) {
                    bestFloorY = floorY;
                    foundFloor = true;
                }
                return true;
            });

            if (foundFloor) {
                pos.y = bestFloorY;
                vel.y = 0.0f;
                isGrounded = true;
            }
        }
    }

private:
    void HandleCrouch(const std::vector<WorldBlock>& world) {
        bool wantCrouch = IsKeyDown(KEY_LEFT_CONTROL) || isDowned;

        if (wantCrouch && !isCrouched) {
            isCrouched = true;
            currentHeight = CROUCH_HEIGHT;
            // Crouch-Jump no ar: puxa os pés para cima mantendo a cabeça/câmera fixa
            if (!isGrounded) {
                pos.y += HEIGHT_DIFF;
            }
        } else if (!wantCrouch && isCrouched) {
            Vector3 testPos = pos;
            if (!isGrounded) {
                testPos.y -= HEIGHT_DIFF;
            }
            BoundingBox standBox = GetBoxAt(testPos, STAND_HEIGHT);
            if (!OverlapsAnyBlock(standBox, world)) {
                isCrouched = false;
                currentHeight = STAND_HEIGHT;
                pos = testPos;
            }
        }
    }

    void ApplyFriction(float dt) {
        float speed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
        if (speed < 0.001f) {
            vel.x = 0.0f;
            vel.z = 0.0f;
            return;
        }
        float control = (speed < STOP_SPEED) ? STOP_SPEED : speed;
        float drop = control * FRICTION * dt;
        float newSpeed = std::max(0.0f, speed - drop);
        float scale = newSpeed / speed;
        vel.x *= scale;
        vel.z *= scale;
    }

    void Accelerate(Vector3 wishDir, float wishSpeed, float accel, float dt) {
        float currentSpeed = vel.x * wishDir.x + vel.z * wishDir.z;
        float addSpeed = wishSpeed - currentSpeed;
        if (addSpeed <= 0.0f) return;

        float accelSpeed = std::min(accel * wishSpeed * dt, addSpeed);
        vel.x += accelSpeed * wishDir.x;
        vel.z += accelSpeed * wishDir.z;
    }

    void AirAccelerate(Vector3 wishDir, float wishSpeed, float accel, float dt) {
        float cappedSpeed = std::min(wishSpeed, AIR_SPEED_CAP);
        float currentSpeed = vel.x * wishDir.x + vel.z * wishDir.z;
        float addSpeed = cappedSpeed - currentSpeed;
        if (addSpeed <= 0.0f) return;

        float accelSpeed = std::min(accel * wishSpeed * dt, addSpeed);
        vel.x += accelSpeed * wishDir.x;
        vel.z += accelSpeed * wishDir.z;
    }
};