// Must define SAMPLE_RATE and FRAMES_PER_BUFFER before including
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef ECHO_BUFFER_FRAMES
#define ECHO_BUFFER_FRAMES 100000
#endif

#include "waveform_visualizer.h"