#define XPLM_64 1  // Define XPLM_64 as 1 for 64-bit compatibility
#ifndef XPLM_API   
#define XPLM_API   // XPLM_API 
#endif

#ifdef _WIN32
    #define PLUGIN_API __declspec(dllexport)
    #include <al.h>
    #include <alc.h>
    #define IBM 1
    #define XPWIDGETS 1
    #define XPMENUS 1
    #define XPLM_64 1

#else
    #if defined(__APPLE__)
        #define APL 1
        #define IBM 0
        #define LIN 0
        #define XPMENUS 1
        #define XPLM_64 1
    #endif
    #include <OpenAL/al.h>
    #include <OpenAL/alc.h>
#endif

#include "SDK/CHeaders/XPLM/XPLMDisplay.h"
#include "SDK/CHeaders/XPLM/XPLMGraphics.h"
#include "SDK/CHeaders/XPLM/XPLMProcessing.h"
#include "SDK/CHeaders/XPLM/XPLMDataAccess.h"
#include "SDK/CHeaders/XPLM/XPLMPlugin.h"
#include "SDK/CHeaders/XPLM/XPLMUtilities.h"
#include "SDK/CHeaders/Widgets/XPWidgets.h"
#include "SDK/CHeaders/Widgets/XPStandardWidgets.h"
#include "SDK/CHeaders/Widgets/XPWidgetUtils.h"
#include "SDK/CHeaders/XPLM/XPLMMenus.h"

#include <iostream>
#include <cmath>
#include <vector>
#include <cstring>

// Function declarations
void cleanupAudio();

// AOA ranges and tone configuration
#define AOA_LOW_THRESHOLD       -2.0f   // Below this is "low AOA"
#define AOA_HIGH_THRESHOLD      2.0f    // Above this is "high AOA"
#define AOA_STALL_WARNING       15.0f   // Maximum AOA before stall

#define TONE_FREQUENCY          400.0f  // Base frequency in Hz
#define PULSE_MIN_RATE          1.5f    // Minimum pulses per second
#define PULSE_MAX_RATE_LOW      8.2f    // Maximum pulses per second for low AOA
#define PULSE_MAX_RATE_HIGH     6.2f    // Maximum pulses per second for high AOA

#define DEFAULT_VOLUME          1.0f    // Default volume level (0.0 to 1.0)

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

// Add these globals for the UI
static XPWidgetID audioControlWidget = nullptr;
static XPWidgetID audioToggleCheckbox = nullptr;
static XPWidgetID widgetSoundStatus = nullptr;
static bool audioEnabled = false;
static XPLMMenuID menuId;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function to calculate pulse rate based on AOA
// aoa value is between range
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function to initialize OpenAL and create tone
static float init_sound(float elapsed, float elapsed_sim, int counter, void * ref)
{
    device = alcOpenDevice(nullptr);
    XPLMDebugString("FlyOnSpeed: Initializing audio device\n");  // Single string only
    if (!device)  {
        XPLMDebugString("FlyOnSpeed: Failed to open device\n");
        return false;
    }
    
    context = alcCreateContext(device, nullptr);
    XPLMDebugString("FlyOnSpeed: Creating audio context\n");  // Single string only
    if (!context) {
        XPLMDebugString("FlyOnSpeed: Failed to create context\n");
        alcCloseDevice(device);
        return false;
    }
    
    alcMakeContextCurrent(context);

    // Generate source and buffer
    alGenSources(1, &audioSource);
    alGenBuffers(1, &audioBuffer);
    
    // Set the initial volume (gain)
    alSourcef(audioSource, AL_GAIN, DEFAULT_VOLUME);
    
    // Create the 400 Hz tone
    auto tone = generateTone(TONE_FREQUENCY, 0.1f);  // 0.1s duration
    
    // Load the tone into OpenAL buffer
    alBufferData(audioBuffer, AL_FORMAT_MONO16, tone.data(), 
                 tone.size() * sizeof(ALshort), 44100);
    
    // Set the buffer on the source
    alSourcei(audioSource, AL_BUFFER, audioBuffer);
    
    // Configure source to loop
    alSourcei(audioSource, AL_LOOPING, AL_FALSE);
    
    return 0.0f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void printMessageDescription(XPWidgetMessage msg) {
    switch(msg) {
        case xpMessage_CloseButtonPushed:
            XPLMDebugString("FlyOnSpeed: xpMessage_CloseButtonPushed\n");
            break;
        case xpMsg_PushButtonPressed:
            XPLMDebugString("FlyOnSpeed: xpMsg_PushButtonPressed\n");
            break;
        case xpMsg_ButtonStateChanged:
            XPLMDebugString("FlyOnSpeed: xpMsg_ButtonStateChanged\n");
            break;
        case xpMsg_TextFieldChanged:
            XPLMDebugString("FlyOnSpeed: xpMsg_TextFieldChanged\n");
            break;
        case xpMsg_ScrollBarSliderPositionChanged:
            XPLMDebugString("FlyOnSpeed: xpMsg_ScrollBarSliderPositionChanged\n");
            break;
        default:
            //XPLMDebugString(("FlyOnSpeed: Unknown message type: " + std::to_string(msg) + "\n").c_str());
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Widget handler function
static int AudioControlHandler(
    XPWidgetMessage inMessage,
    XPWidgetID inWidget,
    intptr_t inParam1,
    intptr_t inParam2)
{
    printMessageDescription(inMessage);
    //XPLMDebugString(("FlyOnSpeed: AudioControlHandler. Message: " + std::to_string(inMessage) + "\n").c_str());
    if (inMessage == xpMessage_CloseButtonPushed) {
        XPHideWidget(audioControlWidget);
        return 1;
    }

    if (inMessage == xpMsg_PushButtonPressed) {
        XPLMDebugString(("FlyOnSpeed: AudioControlHandler. Button state changed " + std::to_string(inParam1) + "\n").c_str());
        if (inParam1 == (intptr_t)audioToggleCheckbox) {
            audioEnabled = !audioEnabled;
            //audioEnabled = XPGetWidgetProperty(audioToggleCheckbox, xpProperty_ButtonState, nullptr);
            //XPSetWidgetProperty(audioToggleCheckbox, xpProperty_ButtonState, audioEnabled);
            if(audioEnabled) XPSetWidgetDescriptor(audioToggleCheckbox, "Sound: On");
            else XPSetWidgetDescriptor(audioToggleCheckbox, "Sound: Off");

            XPLMDebugString(("FlyOnSpeed: AudioControlHandler. Button state: " + std::to_string(audioEnabled) + "\n").c_str());
            return 1;
        }
    }

    return 0;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// create the control window
static void CreateAudioControlWindow(int x, int y, int w, int h)
{
    int x2 = x + w;
    int y2 = y - h;

    audioControlWidget = XPCreateWidget(x, y, x2, y2,
        1,  // Visible
        "Fly On Speed", // desc
        1,  // root
        nullptr, // no container
        xpWidgetClass_MainWindow);

    XPSetWidgetProperty(audioControlWidget, xpProperty_MainWindowHasCloseBoxes, 1);

    // Create checkbox
    audioToggleCheckbox = XPCreateWidget(x + 20, y - 30, x + 120, y - 50,
        1, // Visible
        "Sound: Off", // desc
        0, // not root
        audioControlWidget,
        xpWidgetClass_Button);

    //XPSetWidgetProperty(audioToggleCheckbox, xpProperty_ButtonType, xpButtonBehaviorCheckBox);
    //XPSetWidgetProperty(audioToggleCheckbox, xpProperty_ButtonState, 1);

    // // create a caption next to the checkbox
    // widgetSoundStatus = XPCreateWidget(x + 100, y - 30, x + 120, y - 50,
    //     1, // Visible
    //     "Sound: Off", // desc
    //     0, // not root
    //     audioControlWidget,
    //     xpWidgetClass_Caption);


    XPAddWidgetCallback(audioControlWidget, AudioControlHandler);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add menu handler
static void AudioMenuHandler(void * mRef, void * iRef)
{
    if (!strcmp((char *)iRef, "Show")) {
        if (!audioControlWidget) {
            CreateAudioControlWindow(300, 450, 200, 100);
        } else if (!XPIsWidgetVisible(audioControlWidget)) {
            XPShowWidget(audioControlWidget);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Modify PlayAOATone to respect enabled state
void PlayAOATone(float aoa, float elapsedTime) {
    if (!audioEnabled) {
        alSourceStop(audioSource);
        return;
    }

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Updated flight loop callback
float CheckAOAAndPlayTone(float inElapsedSinceLastCall, 
                         float inElapsedTimeSinceLastFlightLoop, 
                         int inCounter, 
                         void *inRefcon) {

    // use XPLMGetDataf to get the AOA value.  https://developer.x-plane.com/sdk/XPLMDataAccess/#XPLMDataRef

    float aoa = XPLMGetDataf(aoaDataRef);
    PlayAOATone(aoa, inElapsedSinceLastCall);
    return -1.0f;  // Negative value means "call me next frame"
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Modify XPluginStart
PLUGIN_API int XPluginStart(char *outName, char *outSig, char *outDesc) {
    strcpy(outName, "AOA-Tone-FlyOnSpeed");
    strcpy(outSig, "xplane.plugin.aoa-tone-flyon-speed");
    strcpy(outDesc, "A plugin that plays audio tones based on AOA");
    
	if( sizeof(unsigned int) != 4 ||
		sizeof(unsigned short) != 2)
	{
		XPLMDebugString("FlyOnSpeed: This example plugin was compiled with a compiler with weird type sizes.\n");
		return 0;
	}

    // if (!initializeAudio()) {
    //     XPLMDebugString("Failed to initialize audio");
    //     return 0;
    // }

	XPLMRegisterFlightLoopCallback(init_sound,-1.0,NULL);	


    // find the AOA DataRef 
    // https://developer.x-plane.com/sdk/XPLMDataAccess/#XPLMDataRef

    // here is a site that shows a list of DataRefs:
    // https://siminnovations.com/xplane/dataref/index.php

    // find the AOA DataRef.
    // use sim/cockpit2/gauges/indicators/AoA_pilot ??
    // or sim/cockpit2/gauges/indicators/aoa_angle_degrees ??

    aoaDataRef = XPLMFindDataRef("sim/cockpit2/gauges/indicators/AoA_pilot");
    if (aoaDataRef == nullptr) {
        XPLMDebugString("FlyOnSpeed: Failed to find AOA DataRef");
        return 0;
    }

    XPLMRegisterFlightLoopCallback(CheckAOAAndPlayTone, 1.0, nullptr);

    // Add menu item
    int item = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Fly On Speed", nullptr, 1);
    menuId = XPLMCreateMenu("Fly On Speed", XPLMFindPluginsMenu(), item, AudioMenuHandler, nullptr);
    XPLMAppendMenuItem(menuId, "Show", (void*)"Show", 1);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Modify XPluginStop to cleanup UI
PLUGIN_API void XPluginStop(void) {
    if (audioControlWidget) {
        XPDestroyWidget(audioControlWidget, 1);
        audioControlWidget = nullptr;
    }
    XPLMDestroyMenu(menuId);
    XPLMUnregisterFlightLoopCallback(CheckAOAAndPlayTone, nullptr);
    cleanupAudio();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Plugin enable
PLUGIN_API int XPluginEnable(void) {
    return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Plugin disable
PLUGIN_API void XPluginDisable(void) {
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Plugin receive message
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFromWho, int inMessage, void *inParam) {
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
