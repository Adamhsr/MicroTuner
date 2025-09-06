#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <portaudio.h>
#include <math.h>
#include <pthread.h>

#define SHARED_BUFFER_SIZE 2048
int16_t shared_audio_buffer[SHARED_BUFFER_SIZE];
pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER SHARED_BUFFER_SIZE
#define NUM_CHANNELS 1
#define SAMPLE_TYPE paInt16
typedef int16_t SAMPLE;

#include "effects.h"

int select_input(PaStreamParameters *inputParams) {

    PaDeviceIndex bestDevice = paNoDevice;
    const PaDeviceInfo *bestInfo = NULL;

    int numDevices = Pa_GetDeviceCount();
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (info->maxInputChannels > 0 &&
            info->defaultSampleRate >= 44100) {
            if (!bestInfo || info->defaultSampleRate >= bestInfo->defaultSampleRate) {
                bestDevice = i;
                bestInfo = info;
            }
        }
    }

    if (bestDevice == paNoDevice) {
        fprintf(stderr, "No suitable input device found.\n");
        return 1;
    }

    printf("Using input device: %s (%.0f Hz)\n", bestInfo->name, bestInfo->defaultSampleRate);

    inputParams->device = bestDevice;
    inputParams->channelCount = NUM_CHANNELS;
    inputParams->sampleFormat = SAMPLE_TYPE;
    inputParams->suggestedLatency = bestInfo->defaultLowInputLatency;
    inputParams->hostApiSpecificStreamInfo = NULL;

    return 0;
}

void processAudio(SAMPLE *in, SAMPLE *out, size_t frames) {

    memcpy(out, in, frames * sizeof(SAMPLE));

}

static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* __attribute__((unused)) timeInfo,
                         PaStreamCallbackFlags __attribute__((unused)) statusFlags,
                         void *__attribute__((unused)) userData) {

                            
    if (!inputBuffer || !outputBuffer) return paContinue;

    SAMPLE *in = (SAMPLE *)inputBuffer;
    SAMPLE *out = (SAMPLE *)outputBuffer;

    processAudio(in, out, framesPerBuffer);

    if (out) {
        pthread_mutex_lock(&audio_mutex);
        size_t copy_size = framesPerBuffer < SHARED_BUFFER_SIZE ? framesPerBuffer : SHARED_BUFFER_SIZE;
        memcpy(shared_audio_buffer, out, copy_size * sizeof(int16_t));
        pthread_mutex_unlock(&audio_mutex);
    }

    return paContinue;
}

int setupAudioAndStart(PaStream *stream, int (*audioCallback)(const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*)) {
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) goto error;

    PaStreamParameters inputParams, outputParams;

    if (select_input(&inputParams) != 0) {
        goto error;
    }

    outputParams.device = Pa_GetDefaultOutputDevice();
    if (outputParams.device == paNoDevice) {
        fprintf(stderr, "No output device.\n");
        return 1;
    }
    const PaDeviceInfo *outputInfo = Pa_GetDeviceInfo(outputParams.device);
    outputParams.channelCount = NUM_CHANNELS;
    outputParams.sampleFormat = SAMPLE_TYPE;
    outputParams.suggestedLatency = outputInfo->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&stream,
        &inputParams,
        &outputParams,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,
        audioCallback,
        NULL);
    if (err != paNoError) goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error;

    return 0;

    error:
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        if (stream) Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
}

int main() {

    printf("Setting up PortAudio... ");
    PaStream *stream = NULL;
    if (setupAudioAndStart(stream, audioCallback) != 0) {
        return 1;
    }
    printf("Done\n");

    printf("Setting up window... ");
    init_waveform_visualizer(SAMPLE_RATE, FRAMES_PER_BUFFER);
    printf("done\n");


    printf("Starting main loop...\n");

    while (1) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto done;
        }
    
        // Lock and copy audio safely
        int16_t buffer_copy[SHARED_BUFFER_SIZE];
        pthread_mutex_lock(&audio_mutex);
        memcpy(buffer_copy, shared_audio_buffer, sizeof(buffer_copy));
        pthread_mutex_unlock(&audio_mutex);
    
        
        render_fundamental_graph(buffer_copy, SHARED_BUFFER_SIZE);
    
        SDL_Delay(16); // ~60 FPS
    }

    done:

    printf("Cleaning up SDL... ");
    cleanup_waveform_visualizer();
    printf("done\n");


    printf("Cleaning up PortAudio... ");
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    printf("done\n");


    return 0;
}
