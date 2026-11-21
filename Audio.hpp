#pragma once

#include "raylib.h"
#include "raymath.h"
#include "Weapon.hpp"
#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdlib>

enum class SfxId {
    RIFLE = 0,
    SHOTGUN,
    LMG,
    PISTOL,
    REVOLVER,
    KNIFE,
    GRENADE_THROW,
    SILENCED,
    EXPLOSION,
    FLASHBANG,
    FOOTSTEP,
    RELOAD,
    JUMP,
    ORDER,
    DOWNED,
    REVIVE,
    COUNT
};

struct SfxBank {
    static constexpr int MAX_VOICES = 4;
    Sound baseSound = { 0 };
    std::array<Sound, MAX_VOICES> voices = { { { 0 } } };
    int nextVoice = 0;
    bool loaded = false;
    bool isExternal = false;
};

class ProceduralAudio {
public:
    bool initialized = false;
    bool muted = false;
    float masterVolume = 0.85f;
    float sfxVolume = 0.90f;
    float musicVolume = 0.42f;

    // Mantido público para compatibilidade com chamadas diretas de pitch
    Sound sfxOrder = { 0 };

    // Estado do ouvinte 3D (câmera do jogador)
    Vector3 listenerPos = { 0.0f, 1.7f, 0.0f };
    Vector3 listenerForward = { 0.0f, 0.0f, -1.0f };
    Vector3 listenerRight = { 1.0f, 0.0f, 0.0f };

    // Trilha Sonora Dinâmica (Ambiente <-> Combate)
    Music extAmbientMusic = { 0 };
    Music extCombatMusic = { 0 };
    bool hasExtAmbient = false;
    bool hasExtCombat = false;

    Sound procAmbientLoop = { 0 };
    Sound procCombatLoop = { 0 };
    bool hasProcMusic = false;

    float musicTension = 0.0f; // 0.0 = Tranquilo | 1.0 = Combate Intenso
    int externalSfxCount = 0;
    int externalMusicCount = 0;

    std::array<SfxBank, (size_t)SfxId::COUNT> banks;

    void Init() {
        if (!IsAudioDeviceReady()) {
            InitAudioDevice();
        }
        if (!IsAudioDeviceReady()) {
            initialized = false;
            return;
        }

        LoadAllBanks();
        LoadDynamicMusic();
        initialized = true;
    }

    void Shutdown() {
        if (!initialized) return;

        for (auto& b : banks) {
            if (!b.loaded) continue;
            for (int v = 0; v < SfxBank::MAX_VOICES; v++) {
                if (b.voices[v].frameCount > 0) {
                    UnloadSoundAlias(b.voices[v]);
                    b.voices[v] = { 0 };
                }
            }
            if (b.baseSound.frameCount > 0) {
                UnloadSound(b.baseSound);
                b.baseSound = { 0 };
            }
            b.loaded = false;
            b.isExternal = false;
        }

        if (hasExtAmbient) {
            UnloadMusicStream(extAmbientMusic);
            hasExtAmbient = false;
        }
        if (hasExtCombat) {
            UnloadMusicStream(extCombatMusic);
            hasExtCombat = false;
        }
        if (hasProcMusic) {
            if (procAmbientLoop.frameCount > 0) UnloadSound(procAmbientLoop);
            if (procCombatLoop.frameCount > 0) UnloadSound(procCombatLoop);
            procAmbientLoop = { 0 };
            procCombatLoop = { 0 };
            hasProcMusic = false;
        }

        sfxOrder = { 0 };
        initialized = false;

        if (IsAudioDeviceReady()) {
            CloseAudioDevice();
        }
    }

    void ReloadExternalAssets() {
        if (!initialized) {
            Init();
            return;
        }

        for (auto& b : banks) {
            if (!b.loaded) continue;
            for (int v = 0; v < SfxBank::MAX_VOICES; v++) {
                if (b.voices[v].frameCount > 0) {
                    UnloadSoundAlias(b.voices[v]);
                    b.voices[v] = { 0 };
                }
            }
            if (b.baseSound.frameCount > 0) {
                UnloadSound(b.baseSound);
                b.baseSound = { 0 };
            }
            b.loaded = false;
            b.isExternal = false;
        }

        if (hasExtAmbient) { UnloadMusicStream(extAmbientMusic); hasExtAmbient = false; }
        if (hasExtCombat)  { UnloadMusicStream(extCombatMusic);  hasExtCombat = false; }
        if (hasProcMusic) {
            if (procAmbientLoop.frameCount > 0) UnloadSound(procAmbientLoop);
            if (procCombatLoop.frameCount > 0) UnloadSound(procCombatLoop);
            procAmbientLoop = { 0 };
            procCombatLoop = { 0 };
            hasProcMusic = false;
        }

        LoadAllBanks();
        LoadDynamicMusic();
    }

    void ToggleMute() {
        muted = !muted;
        if (initialized) {
            SetMasterVolume(muted ? 0.0f : masterVolume);
        }
    }

    void UpdateListener(Vector3 pos, Vector3 forward, Vector3 right) {
        listenerPos = pos;
        listenerForward = (Vector3Length(forward) > 0.001f) ? Vector3Normalize(forward) : Vector3{ 0.0f, 0.0f, -1.0f };
        listenerRight = (Vector3Length(right) > 0.001f) ? Vector3Normalize(right) : Vector3{ 1.0f, 0.0f, 0.0f };
    }

    void UpdateDynamicMusic(float dt, float targetTension, bool shouldPlay) {
        if (!initialized) return;

        float clampedTarget = std::clamp(targetTension, 0.0f, 1.0f);
        float blendSpeed = (clampedTarget > musicTension) ? 1.8f : 0.65f;
        musicTension += (clampedTarget - musicTension) * std::min(blendSpeed * dt, 1.0f);

        float baseVol = (muted || !shouldPlay) ? 0.0f : musicVolume;
        float ambVol = baseVol * (1.0f - musicTension * 0.85f);
        float cmbVol = baseVol * musicTension;

        if (hasExtAmbient) {
            if (!IsMusicStreamPlaying(extAmbientMusic) && shouldPlay) PlayMusicStream(extAmbientMusic);
            UpdateMusicStream(extAmbientMusic);
            SetMusicVolume(extAmbientMusic, ambVol);
        }
        if (hasExtCombat) {
            if (!IsMusicStreamPlaying(extCombatMusic) && shouldPlay) PlayMusicStream(extCombatMusic);
            UpdateMusicStream(extCombatMusic);
            SetMusicVolume(extCombatMusic, cmbVol);
        }

        if (hasProcMusic) {
            if (!hasExtAmbient && procAmbientLoop.frameCount > 0) {
                if (shouldPlay && !IsSoundPlaying(procAmbientLoop)) PlaySound(procAmbientLoop);
                SetSoundVolume(procAmbientLoop, ambVol);
                SetSoundPan(procAmbientLoop, 0.5f);
            }
            if (!hasExtCombat && procCombatLoop.frameCount > 0) {
                if (shouldPlay && !IsSoundPlaying(procCombatLoop)) PlaySound(procCombatLoop);
                SetSoundVolume(procCombatLoop, cmbVol);
                SetSoundPan(procCombatLoop, 0.5f);
            }
        }
    }

    // Reprodução 2D centrada (Interface, ordens de rádio, arma do próprio jogador)
    void Play2D(SfxId id, float volume = 1.0f, float pitch = 1.0f) {
        if (!initialized || muted) return;
        SfxBank& b = banks[(size_t)id];
        if (!b.loaded) return;

        Sound& voice = b.voices[b.nextVoice];
        b.nextVoice = (b.nextVoice + 1) % SfxBank::MAX_VOICES;

        SetSoundVolume(voice, std::clamp(volume * sfxVolume, 0.0f, 1.0f));
        SetSoundPan(voice, 0.5f);
        SetSoundPitch(voice, pitch);
        PlaySound(voice);
    }

    // Reprodução Espacial 3D com atenuação por distância e panning via produto escalar
    void Play3D(SfxId id, Vector3 worldPos, float volume = 1.0f, float pitch = 1.0f, float maxDistance = 42.0f) {
        if (!initialized || muted) return;
        SfxBank& b = banks[(size_t)id];
        if (!b.loaded) return;

        float dist = Vector3Distance(listenerPos, worldPos);
        if (dist >= maxDistance) return;

        // Atenuação suave por distância
        float normDist = dist / maxDistance;
        float attenuation = (1.0f - normDist) * (1.0f - normDist);
        float finalVol = std::clamp(volume * sfxVolume * attenuation, 0.0f, 1.0f);
        if (finalVol <= 0.01f) return;

        // Panning estéreo: Raylib usa 0.5 = centro, 0.0 = direita, 1.0 = esquerda
        float pan = 0.5f;
        if (dist > 0.25f) {
            Vector3 toSource = Vector3Scale(Vector3Subtract(worldPos, listenerPos), 1.0f / dist);
            float rightDot = std::clamp(Vector3DotProduct(listenerRight, toSource), -1.0f, 1.0f);
            pan = 0.5f - rightDot * 0.46f;
        }

        // Leve queda de brilho/pitch em longas distâncias
        float distPitch = pitch * (1.0f - normDist * 0.07f);

        Sound& voice = b.voices[b.nextVoice];
        b.nextVoice = (b.nextVoice + 1) % SfxBank::MAX_VOICES;

        SetSoundVolume(voice, finalVol);
        SetSoundPan(voice, pan);
        SetSoundPitch(voice, distPitch);
        PlaySound(voice);
    }

    // Wrappers compatíveis com todo o projeto
    void PlayOrder() {
        if (!initialized || muted) return;
        SetSoundVolume(sfxOrder, std::clamp(0.75f * sfxVolume, 0.0f, 1.0f));
        SetSoundPan(sfxOrder, 0.5f);
        PlaySound(sfxOrder);
        SetSoundPitch(sfxOrder, 1.0f);
    }

    void PlayReload()      { Play2D(SfxId::RELOAD, 0.72f, 1.0f); }
    void PlayJump()        { Play2D(SfxId::JUMP, 0.48f, 1.0f); }
    void PlayRevive()      { Play2D(SfxId::REVIVE, 0.85f, 1.0f); }
    void PlayDownedAlert() { Play2D(SfxId::DOWNED, 0.90f, 1.0f); }

    void PlayWeaponAction(WeaponId id, bool isStealthFlank) {
        if (id == WeaponId::MELEE_KNIFE) {
            Play2D(SfxId::KNIFE, 0.75f, 1.0f);
            return;
        }
        if (id == WeaponId::GADGET_FRAG || id == WeaponId::GADGET_FLASH) {
            Play2D(SfxId::GRENADE_THROW, 0.70f, 1.0f);
            return;
        }
        if (isStealthFlank) {
            Play2D(SfxId::SILENCED, 0.78f, 0.98f + (float)GetRandomValue(-4, 4) * 0.01f);
            return;
        }

        float jitter = 1.0f + (float)GetRandomValue(-4, 4) * 0.01f;
        switch (id) {
            case WeaponId::RIFLE:    Play2D(SfxId::RIFLE,    0.76f, jitter); break;
            case WeaponId::SHOTGUN:  Play2D(SfxId::SHOTGUN,  0.88f, jitter); break;
            case WeaponId::LMG:      Play2D(SfxId::LMG,      0.78f, jitter); break;
            case WeaponId::PISTOL:   Play2D(SfxId::PISTOL,   0.70f, jitter); break;
            case WeaponId::REVOLVER: Play2D(SfxId::REVOLVER, 0.86f, jitter); break;
            default:                 Play2D(SfxId::RIFLE,    0.75f, jitter); break;
        }
    }

    void PlayExplosion(bool isFlashbang) {
        Play2D(isFlashbang ? SfxId::FLASHBANG : SfxId::EXPLOSION, 0.92f, 1.0f);
    }

    void PlayExplosion3D(Vector3 pos, bool isFlashbang) {
        Play3D(isFlashbang ? SfxId::FLASHBANG : SfxId::EXPLOSION, pos, 1.0f, 1.0f, 55.0f);
    }

    void PlayFootstep3D(Vector3 pos, float volume = 0.35f) {
        float jitter = 0.92f + (float)GetRandomValue(0, 16) * 0.01f;
        Play3D(SfxId::FOOTSTEP, pos, volume, jitter, 24.0f);
    }

private:
    static float RandFloat() {
        return ((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    }

    static const char* GetSfxBaseName(SfxId id) {
        switch (id) {
            case SfxId::RIFLE:         return "rifle";
            case SfxId::SHOTGUN:       return "shotgun";
            case SfxId::LMG:           return "lmg";
            case SfxId::PISTOL:        return "pistol";
            case SfxId::REVOLVER:      return "revolver";
            case SfxId::KNIFE:         return "knife";
            case SfxId::GRENADE_THROW: return "grenade_throw";
            case SfxId::SILENCED:      return "silenced";
            case SfxId::EXPLOSION:     return "explosion";
            case SfxId::FLASHBANG:     return "flashbang";
            case SfxId::FOOTSTEP:      return "footstep";
            case SfxId::RELOAD:        return "reload";
            case SfxId::JUMP:          return "jump";
            case SfxId::ORDER:         return "order";
            case SfxId::DOWNED:        return "downed";
            case SfxId::REVIVE:        return "revive";
            default:                   return "rifle";
        }
    }

    void LoadAllBanks() {
        externalSfxCount = 0;

        for (size_t i = 0; i < (size_t)SfxId::COUNT; i++) {
            SfxId id = (SfxId)i;
            SfxBank& bank = banks[i];
            const char* name = GetSfxBaseName(id);

            const char* oggPath = TextFormat("assets/sfx/%s.ogg", name);
            const char* wavPath = TextFormat("assets/sfx/%s.wav", name);

            if (FileExists(oggPath)) {
                bank.baseSound = LoadSound(oggPath);
                bank.isExternal = true;
            } else if (FileExists(wavPath)) {
                bank.baseSound = LoadSound(wavPath);
                bank.isExternal = true;
            } else {
                Wave w = GenerateProceduralWave(id);
                bank.baseSound = LoadSoundFromWave(w);
                UnloadWave(w);
                bank.isExternal = false;
            }

            if (bank.baseSound.frameCount > 0) {
                bank.loaded = true;
                if (bank.isExternal) externalSfxCount++;
                for (int v = 0; v < SfxBank::MAX_VOICES; v++) {
                    bank.voices[v] = LoadSoundAlias(bank.baseSound);
                }
            }
        }

        if (banks[(size_t)SfxId::ORDER].loaded) {
            sfxOrder = banks[(size_t)SfxId::ORDER].voices[0];
        }
    }

    void LoadDynamicMusic() {
        externalMusicCount = 0;

        if (FileExists("assets/music/ambient.ogg")) {
            extAmbientMusic = LoadMusicStream("assets/music/ambient.ogg");
            hasExtAmbient = (extAmbientMusic.frameCount > 0);
        } else if (FileExists("assets/music/ambient.wav")) {
            extAmbientMusic = LoadMusicStream("assets/music/ambient.wav");
            hasExtAmbient = (extAmbientMusic.frameCount > 0);
        }

        if (FileExists("assets/music/combat.ogg")) {
            extCombatMusic = LoadMusicStream("assets/music/combat.ogg");
            hasExtCombat = (extCombatMusic.frameCount > 0);
        } else if (FileExists("assets/music/combat.wav")) {
            extCombatMusic = LoadMusicStream("assets/music/combat.wav");
            hasExtCombat = (extCombatMusic.frameCount > 0);
        }

        if (hasExtAmbient) externalMusicCount++;
        if (hasExtCombat)  externalMusicCount++;

        // Fallback procedural para trilha dinâmica caso não haja arquivos em assets/music/
        if (!hasExtAmbient) {
            Wave ambW = GenerateAmbientMusicWave();
            procAmbientLoop = LoadSoundFromWave(ambW);
            UnloadWave(ambW);
            hasProcMusic = true;
        }
        if (!hasExtCombat) {
            Wave cmbW = GenerateCombatMusicWave();
            procCombatLoop = LoadSoundFromWave(cmbW);
            UnloadWave(cmbW);
            hasProcMusic = true;
        }
    }

    static Wave AllocMonoWave(unsigned int sampleRate, float durationSec) {
        unsigned int frames = (unsigned int)(sampleRate * durationSec);
        Wave w = { 0 };
        w.frameCount = frames;
        w.sampleRate = sampleRate;
        w.sampleSize = 16;
        w.channels = 1;
        w.data = RL_CALLOC(frames, sizeof(short));
        return w;
    }

    static Wave GenerateProceduralWave(SfxId id) {
        const unsigned int sr = 22050;
        float dur = 0.16f;
        switch (id) {
            case SfxId::RIFLE:         dur = 0.15f; break;
            case SfxId::SHOTGUN:       dur = 0.28f; break;
            case SfxId::LMG:           dur = 0.16f; break;
            case SfxId::PISTOL:        dur = 0.12f; break;
            case SfxId::REVOLVER:      dur = 0.25f; break;
            case SfxId::KNIFE:         dur = 0.13f; break;
            case SfxId::GRENADE_THROW: dur = 0.16f; break;
            case SfxId::SILENCED:      dur = 0.09f; break;
            case SfxId::EXPLOSION:     dur = 0.68f; break;
            case SfxId::FLASHBANG:     dur = 0.45f; break;
            case SfxId::FOOTSTEP:      dur = 0.09f; break;
            case SfxId::RELOAD:        dur = 0.26f; break;
            case SfxId::JUMP:          dur = 0.11f; break;
            case SfxId::ORDER:         dur = 0.18f; break;
            case SfxId::DOWNED:        dur = 0.34f; break;
            case SfxId::REVIVE:        dur = 0.30f; break;
            default:                   dur = 0.15f; break;
        }

        Wave w = AllocMonoWave(sr, dur);
        short* samples = (short*)w.data;
        float lp = 0.0f;

        for (unsigned int i = 0; i < w.frameCount; i++) {
            float t = (float)i / (float)sr;
            float norm = (float)i / (float)w.frameCount;
            float env = 1.0f - norm;
            float sig = 0.0f;

            switch (id) {
                case SfxId::RIFLE: {
                    float punch = sinf(2.0f * PI * (155.0f - 105.0f * norm) * t) * expf(-t * 28.0f);
                    float crack = RandFloat() * expf(-t * 36.0f);
                    sig = punch * 0.55f + crack * 0.65f;
                    break;
                }
                case SfxId::SHOTGUN: {
                    float boom = sinf(2.0f * PI * (110.0f - 75.0f * norm) * t) * expf(-t * 14.0f);
                    float blast = RandFloat() * expf(-t * 16.0f);
                    sig = boom * 0.75f + blast * 0.80f;
                    break;
                }
                case SfxId::LMG: {
                    float punch = sinf(2.0f * PI * (140.0f - 85.0f * norm) * t) * expf(-t * 24.0f);
                    float metallic = sinf(2.0f * PI * 920.0f * t) * expf(-t * 45.0f);
                    sig = punch * 0.60f + RandFloat() * expf(-t * 30.0f) * 0.55f + metallic * 0.25f;
                    break;
                }
                case SfxId::PISTOL: {
                    float snap = sinf(2.0f * PI * (230.0f - 150.0f * norm) * t) * expf(-t * 38.0f);
                    sig = snap * 0.55f + RandFloat() * expf(-t * 48.0f) * 0.55f;
                    break;
                }
                case SfxId::REVOLVER: {
                    float cannon = sinf(2.0f * PI * (165.0f - 115.0f * norm) * t) * expf(-t * 16.0f);
                    float ring = sinf(2.0f * PI * 1450.0f * t) * expf(-t * 22.0f);
                    sig = cannon * 0.75f + RandFloat() * expf(-t * 20.0f) * 0.70f + ring * 0.20f;
                    break;
                }
                case SfxId::KNIFE: {
                    float whoosh = RandFloat() * sinf(PI * norm);
                    lp += 0.25f * (whoosh - lp);
                    sig = lp * 0.65f;
                    break;
                }
                case SfxId::GRENADE_THROW: {
                    float swish = RandFloat() * sinf(PI * norm);
                    lp += 0.18f * (swish - lp);
                    sig = lp * 0.60f;
                    break;
                }
                case SfxId::SILENCED: {
                    float puff = RandFloat() * expf(-t * 55.0f);
                    lp += 0.22f * (puff - lp);
                    float tick = sinf(2.0f * PI * 680.0f * t) * expf(-t * 70.0f);
                    sig = lp * 0.85f + tick * 0.25f;
                    break;
                }
                case SfxId::EXPLOSION: {
                    float raw = RandFloat() * expf(-t * 5.2f);
                    lp += 0.14f * (raw - lp);
                    float sub = sinf(2.0f * PI * (72.0f - 48.0f * norm) * t) * expf(-t * 4.5f);
                    sig = lp * 0.95f + sub * 0.85f;
                    break;
                }
                case SfxId::FLASHBANG: {
                    float pop = RandFloat() * expf(-t * 22.0f);
                    float whine = sinf(2.0f * PI * 2850.0f * t) * expf(-t * 6.5f);
                    sig = pop * 0.75f + whine * 0.42f;
                    break;
                }
                case SfxId::FOOTSTEP: {
                    float thud = sinf(2.0f * PI * (95.0f - 55.0f * norm) * t) * expf(-t * 42.0f);
                    float grit = RandFloat() * expf(-t * 55.0f);
                    lp += 0.30f * (grit - lp);
                    sig = thud * 0.55f + lp * 0.40f;
                    break;
                }
                case SfxId::RELOAD: {
                    float click1 = (t < 0.08f) ? sinf(2.0f * PI * 1100.0f * t) * expf(-t * 55.0f) : 0.0f;
                    float click2 = (t >= 0.14f) ? sinf(2.0f * PI * 1450.0f * (t - 0.14f)) * expf(-(t - 0.14f) * 50.0f) : 0.0f;
                    sig = (click1 + click2) * 0.65f;
                    break;
                }
                case SfxId::JUMP: {
                    float air = RandFloat() * sinf(PI * norm);
                    lp += 0.15f * (air - lp);
                    sig = lp * 0.45f;
                    break;
                }
                case SfxId::ORDER: {
                    float freq = (t < 0.08f) ? 880.0f : 1175.0f;
                    sig = sinf(2.0f * PI * freq * t) * env * 0.45f;
                    break;
                }
                case SfxId::DOWNED: {
                    float freq = (t < 0.16f) ? 520.0f : 340.0f;
                    sig = (sinf(2.0f * PI * freq * t) + 0.3f * sinf(4.0f * PI * freq * t)) * env * 0.50f;
                    break;
                }
                case SfxId::REVIVE: {
                    float freq = (t < 0.14f) ? 523.25f : 783.99f;
                    sig = sinf(2.0f * PI * freq * t) * env * 0.50f;
                    break;
                }
                default: break;
            }

            sig = std::clamp(sig, -0.98f, 0.98f);
            samples[i] = (short)(sig * 32000.0f);
        }

        return w;
    }

    // Loop de 4.0s de atmosfera tranquila (acorde menor pad suave)
    static Wave GenerateAmbientMusicWave() {
        const unsigned int sr = 22050;
        const float dur = 4.0f;
        Wave w = AllocMonoWave(sr, dur);
        short* samples = (short*)w.data;

        for (unsigned int i = 0; i < w.frameCount; i++) {
            float t = (float)i / (float)sr;
            float lfo = 0.75f + 0.25f * sinf(2.0f * PI * 0.25f * t);
            // Frequências múltiplas de 0.25 Hz para loop 100% contínuo em 4.0s
            float pad = sinf(2.0f * PI * 110.0f * t) * 0.35f
                      + sinf(2.0f * PI * 130.75f * t) * 0.25f
                      + sinf(2.0f * PI * 165.0f * t) * 0.22f
                      + sinf(2.0f * PI * 220.0f * t) * 0.12f;
            float sig = std::clamp(pad * lfo * 0.45f, -0.95f, 0.95f);
            samples[i] = (short)(sig * 32000.0f);
        }
        return w;
    }

    // Loop de 4.0s de percussão e baixo tático para combate intenso (120 BPM)
    static Wave GenerateCombatMusicWave() {
        const unsigned int sr = 22050;
        const float dur = 4.0f;
        Wave w = AllocMonoWave(sr, dur);
        short* samples = (short*)w.data;

        for (unsigned int i = 0; i < w.frameCount; i++) {
            float t = (float)i / (float)sr;
            // Batida a cada 0.5s (120 BPM) + pulso de semicolcheia a cada 0.25s
            float beatPhase = fmodf(t, 0.5f);
            float subPhase = fmodf(t, 0.25f);

            float kick = sinf(2.0f * PI * (92.0f - 55.0f * (beatPhase * 2.0f)) * beatPhase) * expf(-beatPhase * 14.0f);
            float bass = sinf(2.0f * PI * 55.0f * t) * expf(-subPhase * 8.0f);
            float hihat = RandFloat() * expf(-subPhase * 45.0f) * 0.22f;

            float sig = std::clamp(kick * 0.60f + bass * 0.42f + hihat, -0.95f, 0.95f);
            samples[i] = (short)(sig * 32000.0f);
        }
        return w;
    }
};