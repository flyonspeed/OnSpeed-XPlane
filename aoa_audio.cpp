#define XPLM_64 1  // Define XPLM_64 as 1 for 64-bit compatibility
#ifndef XPLM_API   
#define XPLM_API   // XPLM_API 
#endif

#ifdef _WIN32
    #define PLUGIN_API __declspec(dllexport)
    #include <al.h>
    #include <alc.h>
#else
    #define PLUGIN_API
    #include <OpenAL/al.h>
    #include <OpenAL/alc.h>
#endif

#include "SDK/CHeaders/XPLM/XPLMDisplay.h"
#include "SDK/CHeaders/XPLM/XPLMGraphics.h"
#include "SDK/CHeaders/XPLM/XPLMProcessing.h"
#include "SDK/CHeaders/XPLM/XPLMDataAccess.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <cstring>

// Function declarations
void cleanupAudio();

// AOA ranges and tone configuration
#define AOA_LOW_THRESHOLD    -2.0f   // Below this is "low AOA"
#define AOA_HIGH_THRESHOLD    2.0f   // Above this is "high AOA"
#define AOA_STALL_WARNING   15.0f    // Maximum AOA before stall

#define TONE_FREQUENCY     400.0f    // Base frequency in Hz
#define PULSE_MIN_RATE      1.5f     // Minimum pulses per second
#define PULSE_MAX_RATE_LOW  8.2f     // Maximum pulses per second for low AOA
#define PULSE_MAX_RATE_HIGH 6.2f     // Maximum pulses per second for high AOA

// OpenAL device and context
ALCdevice* device = nullptr;
ALCcontext* context = nullptr;
ALuint audioSource;
ALuint audioBuffer;

// Timing variables for pulse
float lastPulseTime = 0.0f;
bool pulseState = false;

// DataRef for AOA
XPLMDataRef aoaDataRef = nullptr;

// Function to generate a sine wave tone
std::vector<ALshort> generateTone(float frequency, float duration, int sampleRate = 44100) {
    std::vector<ALshort> buffer;
    int samples = static_cast<int>(duration * sampleRate);
    buffer.reserve(samples);
    
    for (int i = 0; i < samples; i++) {
        float t = static_cast<float>(i) / sampleRate;
        float value = 32767.0f * std::sin(2.0f * M_PI * frequency * t);
        buffer.push_back(static_cast<ALshort>(value));
    }
    
    return buffer;
}

// Function to calculate pulse rate based on AOA
float calculatePulseRate(float aoa) {
    if (aoa < AOA_LOW_THRESHOLD) {
        // Map low AOA to pulse rate (further negative = slower pulse)
        float ratio = (aoa - AOA_LOW_THRESHOLD) / (AOA_LOW_THRESHOLD - 2 * AOA_LOW_THRESHOLD);
        return PULSE_MIN_RATE + (PULSE_MAX_RATE_LOW - PULSE_MIN_RATE) * (1.0f - ratio);
    } else if (aoa > AOA_HIGH_THRESHOLD) {
        // Map high AOA to pulse rate (higher AOA = faster pulse)
        float ratio = (aoa - AOA_HIGH_THRESHOLD) / (AOA_STALL_WARNING - AOA_HIGH_THRESHOLD);
        ratio = std::min(1.0f, ratio);  // Clamp to maximum
        return PULSE_MIN_RATE + (PULSE_MAX_RATE_HIGH - PULSE_MIN_RATE) * ratio;
    }
    return 0.0f;  // No pulsing in normal range
}

// Function to initialize OpenAL and create tone
bool initializeAudio() {
    device = alcOpenDevice(nullptr);
    if (!device) return false;
    
    context = alcCreateContext(device, nullptr);
    if (!context) {
        alcCloseDevice(device);
        return false;
    }
    
    alcMakeContextCurrent(context);
    
    // Generate source and buffer
    alGenSources(1, &audioSource);
    alGenBuffers(1, &audioBuffer);
    
    // Create the 400 Hz tone
    auto tone = generateTone(TONE_FREQUENCY, 0.1f);  // 0.1s duration
    
    // Load the tone into OpenAL buffer
    alBufferData(audioBuffer, AL_FORMAT_MONO16, tone.data(), 
                 tone.size() * sizeof(ALshort), 44100);
    
    // Set the buffer on the source
    alSourcei(audioSource, AL_BUFFER, audioBuffer);
    
    // Configure source to loop
    alSourcei(audioSource, AL_LOOPING, AL_FALSE);
    
    return true;
}

// Updated PlayAOATone function
void PlayAOATone(float aoa, float elapsedTime) {
    static float timeSinceLastPulse = 0.0f;
    
    if (aoa >= AOA_LOW_THRESHOLD && aoa <= AOA_HIGH_THRESHOLD) {
        // Steady tone for normal AOA range
        alSourcei(audioSource, AL_LOOPING, AL_TRUE);
        alSourcePlay(audioSource);
    } else {
        // Pulsing tone for high or low AOA
        float pulseRate = calculatePulseRate(aoa);
        float pulsePeriod = 1.0f / pulseRate;
        
        timeSinceLastPulse += elapsedTime;
        
        if (timeSinceLastPulse >= pulsePeriod) {
            timeSinceLastPulse = 0.0f;
            alSourcePlay(audioSource);
        }
    }
}

// Updated flight loop callback
float CheckAOAAndPlayTone(float inElapsedSinceLastCall, 
                         float inElapsedTimeSinceLastFlightLoop, 
                         int inCounter, 
                         void *inRefcon) {
    float aoa = XPLMGetDataf(aoaDataRef);
    PlayAOATone(aoa, inElapsedSinceLastCall);
    return -1.0f;  // Negative value means "call me next frame"
}

// Plugin start
PLUGIN_API int XPluginStart(char *outName, char *outSig, char *outDesc) {
    strcpy(outName, "AOA-Tone-FlyOnSpeed");
    strcpy(outSig, "xplane.plugin.aoa-tone-flyon-speed");
    strcpy(outDesc, "A plugin that plays audio tones based on AOA");
    
    if (!initializeAudio()) {
        std::cerr << "Failed to initialize audio" << std::endl;
        return 0;
    }

    aoaDataRef = XPLMFindDataRef("sim/cockpit2/gauges/indicators/aoa_angle_degrees");
    if (aoaDataRef == nullptr) {
        std::cerr << "Failed to find AOA DataRef" << std::endl;
        return 0;
    }

    XPLMRegisterFlightLoopCallback(CheckAOAAndPlayTone, 1.0, nullptr);
    return 1;
}

// Plugin stop
PLUGIN_API void XPluginStop(void) {
    XPLMUnregisterFlightLoopCallback(CheckAOAAndPlayTone, nullptr);
    cleanupAudio();
}

// Plugin enable
PLUGIN_API int XPluginEnable(void) {
    return 1;
}

// Plugin disable
PLUGIN_API void XPluginDisable(void) {
}

// Plugin receive message
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFromWho, int inMessage, void *inParam) {
}

// Update cleanup function
void cleanupAudio() {
    if (context) {
        alSourceStop(audioSource);
        alDeleteSources(1, &audioSource);
        alDeleteBuffers(1, &audioBuffer);
        
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        context = nullptr;
    }
    
    if (device) {
        alcCloseDevice(device);
        device = nullptr;
    }
}