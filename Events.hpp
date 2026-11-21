#pragma once

#include "raylib.h"
#include <functional>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <utility>

// =============================================================================
// DEFINIÇÃO DOS EVENTOS DO NÚCLEO DA ENGINE (SAVEPOINT_22)
// =============================================================================

struct DamageDealtEvent {
    int attackerSlot = -1;       // 0: Alpha, 1..3: Bravo/Charlie/Delta, 99: Inimigo, -1: Ambiente
    int targetId = -1;           // Slot do aliado/jogador ou ID do inimigo
    bool targetIsEnemy = false;
    float damage = 0.0f;
    bool isHeadshot = false;
    bool isWallbang = false;
    bool isExplosion = false;
    bool isStealthFlank = false;
    Vector3 hitPos = { 0.0f, 0.0f, 0.0f };
    Vector3 attackerPos = { 0.0f, 0.0f, 0.0f };
};

struct EnemyKilledEvent {
    int killerSlot = -1;
    int enemyId = -1;
    Vector3 deathPos = { 0.0f, 0.0f, 0.0f };
    bool isHeadshot = false;
    bool isWallbang = false;
    bool isExplosion = false;
};

struct WeaponFiredEvent {
    int shooterSlot = 0;         // 0..3: Esquadrão, 99: Inimigo
    int weaponId = 0;            // Cast de WeaponId
    Vector3 muzzlePos = { 0.0f, 0.0f, 0.0f };
    Vector3 endPos = { 0.0f, 0.0f, 0.0f };
    bool isMelee = false;
    bool isStealthFlank = false;
    bool isEnemy = false;
    Color tracerColor = YELLOW;
};

struct ExplosionTriggeredEvent {
    int sourceSlot = 0;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    float maxRadius = 6.0f;
    bool isFlashbang = false;
};

struct CombatantDownedEvent {
    int combatantSlot = 0;
    std::string callsign;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
};

struct CombatantRevivedEvent {
    int reviverSlot = 0;
    int targetSlot = 0;
    std::string callsign;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
};

enum class ObjectiveEventType {
    TERMINAL_HACKED = 0,
    HOSTILES_CLEARED,
    EXTRACTION_COMPLETE,
    MISSION_VICTORY,
    MISSION_DEFEAT
};

struct ObjectiveCompletedEvent {
    ObjectiveEventType type = ObjectiveEventType::MISSION_VICTORY;
    int objectiveIndex = 0;
    int phase = 1;
    int totalKills = 0;
    int totalHeadshots = 0;
    Vector3 location = { 0.0f, 0.0f, 0.0f };
};

// =============================================================================
// EVENT MANAGER (BARRAMENTO PUBLISH / SUBSCRIBE SEM ACOPLAMENTO)
// =============================================================================
class EventManager {
public:
    using ListenerId = uint64_t;

    template <typename EventT>
    using Handler = std::function<void(const EventT&)>;

    template <typename EventT>
    static ListenerId Subscribe(Handler<EventT> handler) {
        ListenerId id = ++nextListenerId;
        GetChannel<EventT>().push_back({ id, std::move(handler) });
        return id;
    }

    template <typename EventT>
    static void Unsubscribe(ListenerId id) {
        auto& list = GetChannel<EventT>();
        list.erase(
            std::remove_if(list.begin(), list.end(),
                           [id](const Entry<EventT>& e) { return e.id == id; }),
            list.end()
        );
    }

    template <typename EventT>
    static void Emit(const EventT& event) {
        // Copia defensiva caso um listener inscreva/remova handlers durante o disparo
        auto snapshot = GetChannel<EventT>();
        for (const auto& entry : snapshot) {
            if (entry.fn) {
                entry.fn(event);
            }
        }
    }

    template <typename EventT>
    static void ClearChannel() {
        GetChannel<EventT>().clear();
    }

    static void ResetAllChannels() {
        ClearChannel<DamageDealtEvent>();
        ClearChannel<EnemyKilledEvent>();
        ClearChannel<WeaponFiredEvent>();
        ClearChannel<ExplosionTriggeredEvent>();
        ClearChannel<CombatantDownedEvent>();
        ClearChannel<CombatantRevivedEvent>();
        ClearChannel<ObjectiveCompletedEvent>();
    }

private:
    template <typename EventT>
    struct Entry {
        ListenerId id;
        Handler<EventT> fn;
    };

    template <typename EventT>
    static std::vector<Entry<EventT>>& GetChannel() {
        static std::vector<Entry<EventT>> channel;
        return channel;
    }

    static inline ListenerId nextListenerId = 0;
};