#ifndef FLYPET_NETWORK_LOADER_H
#define FLYPET_NETWORK_LOADER_H

#include "brain.h"

#define FLYPET_NETWORK_PATH "/ext/apps_data/flypet/network.ff"

bool network_load_file(const char* path, NetworkDefinition* network);

#endif
