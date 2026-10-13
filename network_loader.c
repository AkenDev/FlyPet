#include "network_loader.h"
#include <furi.h>
#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

static bool network_read_values(
    FlipperFormat* file,
    const char* key,
    int32_t* values,
    uint16_t count) {
    uint32_t actual_count;
    return flipper_format_rewind(file) &&
           flipper_format_get_value_count(file, key, &actual_count) &&
           actual_count == count &&
           flipper_format_rewind(file) &&
           flipper_format_read_int32(file, key, values, count);
}

bool network_load_file(const char* path, NetworkDefinition* network) {
    if(!path || !network) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    NetworkDefinition candidate = {0};
    int32_t values[BRAIN_MAX_SYNAPSES > BRAIN_MAX_NEURONS ?
                       BRAIN_MAX_SYNAPSES : BRAIN_MAX_NEURONS];
    uint32_t version;
    bool loaded = false;

    do {
        if(!flipper_format_file_open_existing(file, path) ||
           !flipper_format_read_header(file, filetype, &version) ||
           furi_string_cmp_str(filetype, "FlyPet Network") != 0 || version != 1) {
            break;
        }

        if(!network_read_values(file, "NeuronCount", values, 1) ||
           values[0] < 1 || values[0] > BRAIN_MAX_NEURONS) {
            break;
        }
        candidate.neuron_count = (uint16_t)values[0];

        if(!network_read_values(file, "SynapseCount", values, 1) ||
           values[0] < 0 || values[0] > BRAIN_MAX_SYNAPSES) {
            break;
        }
        candidate.synapse_count = (uint16_t)values[0];

        if(!network_read_values(file, "Thresholds", values, candidate.neuron_count)) {
            break;
        }
        bool valid = true;
        for(uint16_t i = 0; i < candidate.neuron_count; i++) {
            if(values[i] < 1 || values[i] > 100) {
                valid = false;
                break;
            }
            candidate.thresholds[i] = (uint16_t)values[i];
        }
        if(!valid) break;

        if(candidate.synapse_count > 0) {
            if(!network_read_values(file, "Sources", values, candidate.synapse_count)) break;
            for(uint16_t s = 0; s < candidate.synapse_count; s++) {
                if(values[s] < 0 || values[s] >= candidate.neuron_count) {
                    valid = false;
                    break;
                }
                candidate.synapses[s].source = (uint8_t)values[s];
            }
            if(!valid) break;

            if(!network_read_values(file, "Targets", values, candidate.synapse_count)) break;
            for(uint16_t s = 0; s < candidate.synapse_count; s++) {
                if(values[s] < 0 || values[s] >= candidate.neuron_count) {
                    valid = false;
                    break;
                }
                candidate.synapses[s].target = (uint8_t)values[s];
            }
            if(!valid) break;

            if(!network_read_values(file, "Weights", values, candidate.synapse_count)) break;
            for(uint16_t s = 0; s < candidate.synapse_count; s++) {
                if(values[s] < INT16_MIN || values[s] > INT16_MAX) {
                    valid = false;
                    break;
                }
                candidate.synapses[s].weight = (int16_t)values[s];
            }
            if(!valid) break;
        }

        *network = candidate;
        loaded = true;
    } while(false);

    furi_string_free(filetype);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return loaded;
}
