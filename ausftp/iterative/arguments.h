#pragma once

#include "config.h"
#include <netinet/in.h>  // for INET_ADDRSTRLEN


// Arguments struct to hold parsed options
struct arguments {
  int port;
  int port_set;
  char address[INET_ADDRSTRLEN];
  int address_set;
};

// Parses command line arguments into the provided 'args' struct.
// Returns 0 on success, or non-zero on error.
int parse_arguments(int argc, char **argv, struct arguments *args);