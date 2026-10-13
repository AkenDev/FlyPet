#include "brain.h"

bool brain_init(Brain* brain, const NetworkDefinition* network) {
    if(!brain || !network || network->neuron_count == 0 ||
       network->neuron_count > BRAIN_MAX_NEURONS ||
       network->synapse_count > BRAIN_MAX_SYNAPSES) {
        return false;
    }
    for(uint16_t i = 0; i < network->neuron_count; i++) {
        if(network->thresholds[i] == 0 || network->thresholds[i] > 100) {
            return false;
        }
    }
    for(uint16_t s = 0; s < network->synapse_count; s++) {
        if(network->synapses[s].source >= network->neuron_count ||
           network->synapses[s].target >= network->neuron_count) {
            return false;
        }
    }

    *brain = (Brain){0};
    brain->neuron_count = network->neuron_count;
    brain->synapse_count = network->synapse_count;
    for(uint16_t i = 0; i < brain->neuron_count; i++) {
        brain->neurons[i].threshold = network->thresholds[i];
    }
    for(uint16_t s = 0; s < brain->synapse_count; s++) {
        brain->synapses[s] = network->synapses[s];
    }
    return true;
}

void brain_stimulate(Brain* brain, uint8_t neuron_index, int16_t amount) {
    if(neuron_index >= brain->neuron_count) {
        return;
    }
    Neuron* neuron = &brain->neurons[neuron_index];
    if(neuron->refractory_remaining > 0) {
        return;
    }
    int32_t potential = (int32_t)neuron->potential + amount;
    if(potential > 100) {
        potential = 100;
    } else if(potential < 0) {
        potential = 0;
    }
    neuron->potential = (int16_t)potential;
}

void brain_step(Brain* brain) {
    int32_t incoming[BRAIN_MAX_NEURONS] = {0};
    for(uint16_t i = 0; i < brain->neuron_count; i++) {
        brain->neurons[i].fired =
            brain->neurons[i].refractory_remaining == 0 &&
            brain->neurons[i].potential >= brain->neurons[i].threshold;
    }
    for(uint16_t s = 0; s < brain->synapse_count; s++) {
        uint8_t source = brain->synapses[s].source;
        uint8_t target = brain->synapses[s].target;
        if(brain->neurons[source].fired) {
            incoming[target] += brain->synapses[s].weight;
        }
    }
    for(uint16_t i = 0; i < brain->neuron_count; i++) {
        if(brain->neurons[i].fired) {
            brain->neurons[i].spike_count++;
            brain->neurons[i].potential = 0;
            brain->neurons[i].refractory_remaining = REFRACTORY_TICKS;
            continue;
        }
        if(brain->neurons[i].refractory_remaining > 0) {
            brain->neurons[i].refractory_remaining--;
            brain->neurons[i].potential = 0;
            continue;
        }
        int32_t potential = (brain->neurons[i].potential * 8) / 10 + incoming[i];
        if(potential > 100) {
            potential = 100;
        } else if(potential < 0) {
            potential = 0;
        }
        brain->neurons[i].potential = (int16_t)potential;
    }
    brain->tick++;
}
