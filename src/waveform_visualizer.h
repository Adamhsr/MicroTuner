#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

bool init_waveform_visualizer(int sample_rate, int buffer_size);

void render_waveform(const int16_t *samples, int num_samples);

void render_frequencies(const int16_t *samples, int num_samples);

void cleanup_waveform_visualizer(void);

void met(const int16_t *samples, int num_samples);

void render_fundamental_graph(const int16_t *samples, int num_samples);

void render_frequency_table(const int16_t *samples, int num_samples);