#include "waveform_visualizer.h"
#include "kiss_fft.h"
#include "math.h"
#include <stdio.h>

#define win_width 800
#define win_height 512

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static int max_samples = 0;
static SDL_Texture *__attribute__((unused)) texture = NULL;
static int16_t *wave_buffer = NULL;

static bool transform(const int16_t *samples, int num_samples, const int fft_size, kiss_fft_cpx *out) {
    if (num_samples < fft_size) return false;

    kiss_fft_cfg cfg = kiss_fft_alloc(fft_size, 0, NULL, NULL);
    if (!cfg) return false;

    kiss_fft_cpx in[fft_size];

    for (int i = 0; i < fft_size; i++) {
        in[i].r = samples[i] / 32768.0f; // Normalize to [-1, 1]
        in[i].i = 0;
    }

    kiss_fft(cfg, in, out);
    free(cfg);
    return true;
}

float magnitude(kiss_fft_cpx val) {
    return sqrtf(val.r * val.r + val.i * val.i);
}

bool init_waveform_visualizer(int __attribute__((unused)) sample_rate, int buffer_size) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("Waveform Visualizer",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              win_width, win_height,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("CreateWindow Error: %s\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("CreateRenderer Error: %s\n", SDL_GetError());
        return false;
    }

    max_samples = buffer_size;
    wave_buffer = malloc(sizeof(int16_t) * max_samples);
    if (!wave_buffer) return (cleanup_waveform_visualizer(), false);

    return true;
}

void render_waveform(const int16_t *samples, int num_samples) {

    // memcpy(wave_buffer, samples, sizeof(int16_t) * num_samples);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    int mid = win_height / 2;

    for (int x = 0; x < win_width && x < num_samples; x++) {
        int sample_index = (x * num_samples) / win_width;
        int16_t sample = samples[sample_index];
        int y = mid - (sample * mid / 32768); // Scale to window height
        SDL_RenderDrawPoint(renderer, x, y);
    }

    SDL_RenderPresent(renderer);
}

static void set_intensity_color(float frequency_intensity) {
    // log magnitude
    float num = log10f(frequency_intensity) * 200;

    SDL_SetRenderDrawColor(renderer, num, num, num, 255);
}

void render_frequency_table(const int16_t *samples, int num_samples) {

    // memcpy(wave_buffer, samples, sizeof(int16_t) * num_samples);
    
    static int16_t levels[win_width][win_height] = {0};
    static int index = 0;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    kiss_fft_cpx freq_spectrum[512];
    if (!transform(samples, num_samples, 512, freq_spectrum)) return;

    for (int i = 0; i < win_height; ++i) {
        levels[index][i] = magnitude(freq_spectrum[i]);
    }
    ++index;

    for (int x = 0; x < win_width; ++x) {
        for (int y = 0; y < win_height; ++y) {
            if (levels[x][y] > 0) {
                set_intensity_color(levels[x][y]);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

void met(const int16_t *samples, int num_samples) {
    // memcpy(wave_buffer, samples, sizeof(int16_t) * num_samples);

    static int16_t maxes[win_width] = {0};
    static int index = 0;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    int max = 0;
    for (int i = 0; i < num_samples; ++i) {
        if (abs(samples[i]) > max) {
            max = abs(samples[i]);
        }
    }

    maxes[index] = max;
    index = (index + 1) % win_width;

    int mid = win_height / 2;

    for (int i = 0; i < win_width; ++i) {
        int y = mid - (maxes[(i + index) % win_width] * mid / 32768);

        SDL_RenderDrawPoint(renderer, i, y);
    }

    SDL_RenderPresent(renderer);
}

float get_fundamental(kiss_fft_cpx *freq_spectrum, int fft_size, int sample_rate) {
    int max_bin = 0;
    float max_mag = 0;

    for (int bin = 0; bin < fft_size; ++bin) {
        float mag = magnitude(freq_spectrum[bin]);
        if (mag > max_mag) {
            max_mag = mag;
            max_bin = bin;
        }
    }

    float fundamental = (float) max_bin * sample_rate / fft_size;
    return fundamental;
}

void render_fundamental_graph(const int16_t *samples, int num_samples) {
    // memcpy(wave_buffer, samples, sizeof(int16_t) * num_samples);

    // setup for graph buffer

    static float graph_buffer[win_width] = {0};
    static int index = 0;

    // get current fundamental

    kiss_fft_cpx freq_spectrum[num_samples];
    if (!transform(samples, num_samples, num_samples, freq_spectrum)) return;

    float fundamental = get_fundamental(freq_spectrum, num_samples, 44100);

    // update graph data

    graph_buffer[index] = fundamental;
    index = (index + 1) % win_width;

    // render graph

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (int x = 0; x < win_width; ++x) {
        SDL_RenderDrawPoint(renderer, x, win_height - 10 * (int) log2f(graph_buffer[x]));
    }

    SDL_RenderPresent(renderer);

    // debug
    printf("%.4f, ", fundamental);
}

void render_frequencies(const int16_t *samples, int num_samples) {

    // memcpy(wave_buffer, samples, sizeof(int16_t) * num_samples);

    const int fft_size = 2048;
    const int sample_rate = 44100;
    const int num_bars = 80;
    const float min_freq = 50.0f;
    const float max_freq = 80000.0f;

    // fft -> freq_spectrum
    kiss_fft_cpx freq_spectrum[fft_size];
    if (!transform(samples, num_samples, fft_size, freq_spectrum)) return;


    // Frequency band setup
    float log_min = log10f(min_freq);
    float log_max = log10f(max_freq);
    int bar_width = win_width / num_bars;

    // Clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); // Orange bars

    for (int b = 0; b < num_bars; b++) {
        float t1 = (float)b / num_bars;
        float t2 = (float)(b + 1) / num_bars;

        float f1 = powf(10, log_min + t1 * (log_max - log_min));
        float f2 = powf(10, log_min + t2 * (log_max - log_min));

        int bin1 = (int)(f1 * fft_size / sample_rate);
        int bin2 = (int)(f2 * fft_size / sample_rate);
        if (bin2 >= fft_size / 2) bin2 = fft_size / 2 - 1;
        if (bin1 > bin2) bin1 = bin2;

        // Average magnitude
        float mag_sum = 0;
        int count = 0;
        for (int i = bin1; i <= bin2; i++) {
            mag_sum += magnitude(freq_spectrum[i]);
            count++;
        }

        float avg_mag = (count > 0) ? (mag_sum / count) : 0;

        // Apply log1p for smoother visual dynamics
        float db = log1pf(avg_mag) * 20.0f;

        // Scale to screen height
        int bar_height = (int)(db * (win_height / 30.0f));
        if (bar_height < 0) bar_height = 0;
        if (bar_height > win_height) bar_height = win_height;

        // Draw bar

        SDL_Rect rect = {
            .x = b * bar_width,
            .y = win_height - bar_height,
            .w = bar_width - 1,
            .h = bar_height
        };

        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_RenderPresent(renderer);
}



void cleanup_waveform_visualizer() {
    if (wave_buffer) free(wave_buffer);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}