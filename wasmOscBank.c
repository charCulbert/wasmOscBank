#include <emscripten/webaudio.h>
#include <emscripten/em_math.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 48000
#define PI 3.14159265359

// Per-oscillator state structure. Each instance gets its own copy of these variables
typedef struct {
    float phase;           // Current position in the sine wave cycle (0 to 2π)
    float phaseIncrement;  // How much to advance phase each sample (based on frequency)
    float currentVolume;   // Current smoothed volume value
} OscillatorState;

// Global stack for the audio worklet thread (shared across all instances)
uint8_t wasmAudioWorkletStack[4096];

// Forward declarations
bool ProcessAudio(int numInputs, const AudioSampleFrame *inputs, 
                 int numOutputs, AudioSampleFrame *outputs,
                 int numParams, const AudioParamFrame *params, 
                 void *userData);
void WebAudioWorkletThreadInitialized(EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void *userData);
void AudioWorkletProcessorCreated(EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void *userData);
void createAudioWorkletProcessor(EMSCRIPTEN_WEBAUDIO_T audioContext);

// Called from JavaScript to initialize the audio worklet system
EMSCRIPTEN_KEEPALIVE
void startAudioWorkletThread(EMSCRIPTEN_WEBAUDIO_T context) {
    emscripten_start_wasm_audio_worklet_thread_async(
        context,
        wasmAudioWorkletStack,
        sizeof(wasmAudioWorkletStack),
        WebAudioWorkletThreadInitialized,
        0
    );
}

// Callback: Notifies when worklet thread is ready
void WebAudioWorkletThreadInitialized(EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void *userData) {
    if (!success) return;
    printf("web audio worklet thread successfully initialized\n");
}

// Called from JavaScript to set up the audio processor blueprint
void createAudioWorkletProcessor(EMSCRIPTEN_WEBAUDIO_T audioContext) {
    static WebAudioParamDescriptor paramDescriptors[2] = {
        // Frequency parameter (Hz)
        {
            .defaultValue = 440.0f,    
            .minValue = 20.0f,         
            .maxValue = 20000.0f,      
            .automationRate = WEBAUDIO_PARAM_A_RATE
        },
        // Volume parameter (0-1)
        {
            .defaultValue = 0.3f,      
            .minValue = 0.0f,          
            .maxValue = 1.0f,          
            .automationRate = WEBAUDIO_PARAM_A_RATE
        }
    };

    WebAudioWorkletProcessorCreateOptions opts = {
        .name = "toneGenerator",
        .numAudioParams = 2,
        .audioParamDescriptors = paramDescriptors
    };

    emscripten_create_wasm_audio_worklet_processor_async(
        audioContext,
        &opts,
        AudioWorkletProcessorCreated,
        0
    );
}

// Callback: Notifies when processor is registered
void AudioWorkletProcessorCreated(EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void *userData) {
    if (!success) return;
    printf("audio worklet processor created\n");
}

// Called from JavaScript to create a new oscillator instance
EMSCRIPTEN_KEEPALIVE
EMSCRIPTEN_AUDIO_WORKLET_NODE_T connectAudioWorkletProcessor(EMSCRIPTEN_WEBAUDIO_T audioContext) {
    // Create state for this specific oscillator instance
    OscillatorState* state = (OscillatorState*)malloc(sizeof(OscillatorState));
    state->phase = 0.0f;
    state->phaseIncrement = 440 * 2.f * PI / SAMPLE_RATE;
    state->currentVolume = 0.3f;

    int outputChannelCounts[1] = { 1 };
    EmscriptenAudioWorkletNodeCreateOptions options = {
        .numberOfInputs = 0,
        .numberOfOutputs = 1,
        .outputChannelCounts = outputChannelCounts,
    };

    // Create and connect the node
    EMSCRIPTEN_AUDIO_WORKLET_NODE_T wasmAudioWorklet = emscripten_create_wasm_audio_worklet_node(
        audioContext,
        "toneGenerator",
        &options,
        &ProcessAudio,
        state  // Pass instance-specific state
    );

    emscripten_audio_node_connect(wasmAudioWorklet, audioContext, 0, 0);
    return wasmAudioWorklet;
}

// Audio processing callback: Called per-block for each oscillator instance
bool ProcessAudio(int numInputs, const AudioSampleFrame *inputs, 
                 int numOutputs, AudioSampleFrame *outputs,
                 int numParams, const AudioParamFrame *params, 
                 void *userData) {
    
    OscillatorState* state = (OscillatorState*)userData;
    float targetToneFrequency = params[0].data[0];
    float targetVolume = params[1].data[0];

    // Smooth parameter changes
    float targetPhaseIncrement = targetToneFrequency * 2.f * PI / SAMPLE_RATE;
    state->phaseIncrement = state->phaseIncrement * 0.95f + 0.05f * targetPhaseIncrement;
    state->currentVolume = state->currentVolume * 0.95f + 0.05f * targetVolume;

    // Generate sine wave
    for(int o = 0; o < numOutputs; ++o) {
        for(int i = 0; i < outputs[o].samplesPerChannel; ++i) {
            float s = emscripten_math_sin(state->phase);
            state->phase += state->phaseIncrement;
            for(int ch = 0; ch < outputs[o].numberOfChannels; ++ch) {
                outputs[o].data[ch*outputs[o].samplesPerChannel + i] = s * state->currentVolume;
            }
        }
    }

    state->phase = emscripten_math_fmod(state->phase, 2.f * PI);
    return true;
}