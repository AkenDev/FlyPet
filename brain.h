#ifndef FLYPET_BRAIN_H
#define FLYPET_BRAIN_H

#include <stdbool.h>
#include <stdint.h>

#define BRAIN_MAX_NEURONS 8
#define BRAIN_MAX_SYNAPSES 32
#define REFRACTORY_TICKS 1

typedef struct {
    int16_t potential;
    uint16_t threshold;
    bool fired;
    uint32_t spike_count;
    uint8_t refractory_remaining;
} Neuron;

typedef struct {
    uint8_t source;
    uint8_t target;
    int16_t weight;
} Synapse;

typedef struct {
    uint16_t neuron_count;
    uint16_t synapse_count;
    uint16_t thresholds[BRAIN_MAX_NEURONS];
    Synapse synapses[BRAIN_MAX_SYNAPSES];
} NetworkDefinition;

typedef struct {
    uint16_t neuron_count;
    uint16_t synapse_count;
    Neuron neurons[BRAIN_MAX_NEURONS];
    Synapse synapses[BRAIN_MAX_SYNAPSES];
    uint32_t tick;
} Brain;

bool brain_init(Brain* brain, const NetworkDefinition* network);
void brain_stimulate(Brain* brain, uint8_t neuron_index, int16_t amount);
void brain_step(Brain* brain);

#endif
