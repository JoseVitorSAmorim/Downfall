#pragma once

#include "raylib.h"
#include "Weapon.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

class ProceduralAudio {
public:
    bool initialized = false;
    bool muted = false;

    Sound sfxShootNormal = { 0 };
    Sound sfxShootStealth = { 0 };
    Sound sfxShootShotgun = { 0 };
    Sound sfxShootHeavy = { 0 };
    Sound sfxMelee = { 0 };
    Sound sfxExplosion = { 0 };
    Sound sfxReload = { 0 };
    Sound sfxJump = { 0 };
    Sound sfxOrder = { 0 };
    Sound sfxDowned = { 0 };
    Sound sfxRevive = { 0 };

    void Init() {
        InitAudioDevice();
        if (!IsAudioDeviceReady()) {
            initialized = false;
            return;
        }

        sfxShootNormal  = GenerateShootSound(false);
        sfxShootStealth = GenerateShootSound(true);
        sfxShootShotgun = GenerateShotgunSound();
        sfxShootHeavy   = GenerateHeavyGunSound();
        sfxMelee        = GenerateMeleeSound();
        sfxExplosion    = GenerateExplosionSound();
        sfxReload       = GenerateReloadSound();
        sfxJump         = GenerateJumpSound();
        sfxOrder        = GenerateOrderSound();
        sfxDowned       = GenerateDownedSound();
        sfxRevive       = GenerateReviveSound();

        initialized = true;
    }

    void Shutdown() {
        if (!initialized) return;
        UnloadSound(sfxShootNormal);
        UnloadSound(sfxShootStealth);
        UnloadSound(sfxShootShotgun);
        UnloadSound(sfxShootHeavy);
        UnloadSound(sfxMelee);
        UnloadSound(sfxExplosion);
        UnloadSound(sfxReload);
        UnloadSound(sfxJump);
        UnloadSound(sfxOrder);
        UnloadSound(sfxDowned);
        UnloadSound(sfxRevive);
        CloseAudioDevice();
        initialized = false;
    }

    void ToggleMute() {
        muted = !muted;
    }

    void PlayWeaponAction(WeaponId wid, bool isStealth) {
        if (!initialized || muted) return;

        if (wid == WeaponId::MELEE_KNIFE) {
            SetSoundPitch(sfxMelee, (float)GetRandomValue(95, 108) / 100.0f);
            PlaySound(sfxMelee);
            return;
        }

        if (wid == WeaponId::GADGET_FRAG || wid == WeaponId::GADGET_FLASH) {
            SetSoundPitch(sfxMelee, 0.85f);
            PlaySound(sfxMelee);
            return;
        }

        if (isStealth && (wid == WeaponId::PISTOL || wid == WeaponId::RIFLE)) {
            SetSoundPitch(sfxShootStealth, (float)GetRandomValue(96, 105) / 100.0f);
            PlaySound(sfxShootStealth);
            return;
        }

        if (wid == WeaponId::SHOTGUN) {
            SetSoundPitch(sfxShootShotgun, (float)GetRandomValue(94, 103) / 100.0f);
            PlaySound(sfxShootShotgun);
        } else if (wid == WeaponId::LMG || wid == WeaponId::REVOLVER) {
            float basePitch = (wid == WeaponId::REVOLVER) ? 0.86f : 1.04f;
            SetSoundPitch(sfxShootHeavy, basePitch * ((float)GetRandomValue(96, 104) / 100.0f));
            PlaySound(sfxShootHeavy);
        } else {
            float basePitch = (wid == WeaponId::PISTOL) ? 1.22f : 1.0f;
            SetSoundPitch(sfxShootNormal, basePitch * ((float)GetRandomValue(95, 106) / 100.0f));
            PlaySound(sfxShootNormal);
        }
    }

    void PlayExplosion(bool isFlashbang) {
        if (!initialized || muted) return;
        SetSoundPitch(sfxExplosion, isFlashbang ? 1.55f : 0.92f);
        PlaySound(sfxExplosion);
    }

    void PlayReload() {
        if (!initialized || muted) return;
        PlaySound(sfxReload);
    }

    void PlayJump() {
        if (!initialized || muted) return;
        SetSoundPitch(sfxJump, (float)GetRandomValue(97, 104) / 100.0f);
        PlaySound(sfxJump);
    }

    void PlayOrder() {
        if (!initialized || muted) return;
        PlaySound(sfxOrder);
    }

    void PlayDownedAlert() {
        if (!initialized || muted) return;
        PlaySound(sfxDowned);
    }

    void PlayRevive() {
        if (!initialized || muted) return;
        PlaySound(sfxRevive);
    }

private:
    static constexpr int SAMPLE_RATE = 44100;

    static Sound BuildSoundFromSamples(short* samples, int frameCount, float volume) {
        Wave wave = { 0 };
        wave.frameCount = (unsigned int)frameCount;
        wave.sampleRate = SAMPLE_RATE;
        wave.sampleSize = 16;
        wave.channels   = 1;
        wave.data       = samples;

        Sound sound = LoadSoundFromWave(wave);
        SetSoundVolume(sound, volume);
        UnloadWave(wave);
        return sound;
    }

    static short ClampSample(float v) {
        float clamped = std::clamp(v, -1.0f, 1.0f);
        return (short)(clamped * 32767.0f);
    }

    static float WhiteNoise() {
        return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    }

    static Sound GenerateShootSound(bool stealth) {
        float duration = stealth ? 0.13f : 0.18f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        float lastNoise = 0.0f;

        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            float env = expf(-t * (stealth ? 36.0f : 24.0f));

            float startFreq = stealth ? 660.0f : 310.0f;
            float endFreq   = stealth ? 180.0f : 52.0f;
            float freq = endFreq + (startFreq - endFreq) * expf(-t * 32.0f);

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float tone = sinf(phase);

            float rawNoise = WhiteNoise();
            lastNoise = lastNoise * 0.65f + rawNoise * 0.35f;

            float mix = stealth
                ? (tone * 0.45f + rawNoise * 0.35f) * env
                : (tone * 0.55f + lastNoise * 0.65f) * env;

            data[i] = ClampSample(mix);
        }

        return BuildSoundFromSamples(data, frames, stealth ? 0.42f : 0.52f);
    }

    static Sound GenerateShotgunSound() {
        float duration = 0.30f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        float lastNoise = 0.0f;

        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            float env = expf(-t * 14.0f);
            float freq = 42.0f + 220.0f * expf(-t * 25.0f);

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float rawNoise = WhiteNoise();
            lastNoise = lastNoise * 0.5f + rawNoise * 0.5f;

            float mix = (sinf(phase) * 0.65f + lastNoise * 0.85f) * env;
            data[i] = ClampSample(mix);
        }

        return BuildSoundFromSamples(data, frames, 0.62f);
    }

    static Sound GenerateHeavyGunSound() {
        float duration = 0.22f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        float lastNoise = 0.0f;

        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            float env = expf(-t * 19.0f);
            float freq = 48.0f + 260.0f * expf(-t * 28.0f);

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float rawNoise = WhiteNoise();
            lastNoise = lastNoise * 0.6f + rawNoise * 0.4f;

            float mix = (sinf(phase) * 0.7f + lastNoise * 0.7f) * env;
            data[i] = ClampSample(mix);
        }

        return BuildSoundFromSamples(data, frames, 0.58f);
    }

    static Sound GenerateMeleeSound() {
        float duration = 0.14f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float lastNoise = 0.0f;
        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            float norm = t / duration;
            float env = sinf(norm * PI);

            float raw = WhiteNoise();
            lastNoise = lastNoise * 0.75f + raw * 0.25f;
            data[i] = ClampSample(lastNoise * 0.65f * env);
        }

        return BuildSoundFromSamples(data, frames, 0.42f);
    }

    static Sound GenerateExplosionSound() {
        float duration = 0.65f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        float lastNoise = 0.0f;
        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            float env = expf(-t * 6.5f);
            float freq = 35.0f + 95.0f * expf(-t * 12.0f);

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float raw = WhiteNoise();
            lastNoise = lastNoise * 0.82f + raw * 0.18f;

            float mix = (sinf(phase) * 0.65f + lastNoise * 0.90f) * env;
            data[i] = ClampSample(mix);
        }

        return BuildSoundFromSamples(data, frames, 0.68f);
    }

    static Sound GenerateReloadSound() {
        float duration = 0.20f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            bool secondClick = (t > 0.11f);
            float localT = secondClick ? (t - 0.11f) : t;
            float env = (localT < 0.035f) ? expf(-localT * 95.0f) : 0.0f;

            float sample = WhiteNoise() * 0.55f * env;
            data[i] = ClampSample(sample);
        }

        return BuildSoundFromSamples(data, frames, 0.42f);
    }

    static Sound GenerateJumpSound() {
        float duration = 0.14f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            float progress = t / duration;
            float attack = std::min(progress * 8.0f, 1.0f);
            float env = attack * (1.0f - progress);

            float freq = 140.0f + 230.0f * progress;
            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;

            float sample = sinf(phase) * 0.6f * env;
            data[i] = ClampSample(sample);
        }

        return BuildSoundFromSamples(data, frames, 0.35f);
    }

    static Sound GenerateOrderSound() {
        float duration = 0.16f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            bool secondPulse = (t > 0.075f);
            float localT = secondPulse ? (t - 0.075f) : t;

            float freq = secondPulse ? 1320.0f : 880.0f;
            float env = expf(-localT * 28.0f);
            if (t > 0.055f && t < 0.075f) env = 0.0f;

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float wave = (sinf(phase) + 0.25f * sinf(phase * 2.0f)) * 0.45f * env;
            data[i] = ClampSample(wave);
        }

        return BuildSoundFromSamples(data, frames, 0.40f);
    }

    static Sound GenerateDownedSound() {
        float duration = 0.42f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            bool secondPulse = (t > 0.21f);
            float localT = secondPulse ? (t - 0.21f) : t;

            float freq = 620.0f - localT * 1100.0f;
            float env = (localT < 0.17f) ? (1.0f - localT / 0.17f) : 0.0f;

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float saw = 2.0f * (phase / (2.0f * PI) - floorf(0.5f + phase / (2.0f * PI)));
            float sample = (sinf(phase) * 0.6f + saw * 0.3f) * 0.5f * env;
            data[i] = ClampSample(sample);
        }

        return BuildSoundFromSamples(data, frames, 0.45f);
    }

    static Sound GenerateReviveSound() {
        float duration = 0.36f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        const float notes[3] = { 523.25f, 659.25f, 783.99f };
        float phase = 0.0f;

        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            int step = std::min((int)(t / 0.12f), 2);
            float localT = t - step * 0.12f;

            float freq = notes[step];
            float env = expf(-localT * 12.0f);

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float sample = sinf(phase) * 0.5f * env;
            data[i] = ClampSample(sample);
        }

        return BuildSoundFromSamples(data, frames, 0.42f);
    }
};