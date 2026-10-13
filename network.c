#include "network.h"

const NetworkDefinition flypet_default_network = {
    .neuron_count = 8,
    .synapse_count = 11,
    .thresholds = {50, 50, 50, 50, 50, 50, 50, 50},
    .synapses = {
        {0, 1, 35},
        {0, 2, 25},
        {1, 3, 35},
        {2, 3, -20},
        {2, 4, 40},
        {3, 5, 30},
        {4, 5, 25},
        {5, 6, 35},
        {6, 7, 30},
        {1, 4, 15},
        {4, 0, 60},
    },
};

const NetworkDefinition flypet_small_network = {
    .neuron_count = 5,
    .synapse_count = 2,
    .thresholds = {50, 50, 50, 50, 50},
    .synapses = {
        {2, 4, 60},
        {4, 0, 60},
    },
};
