#ifndef TEXT_SYMBOLS_H
#define TEXT_SYMBOLS_H

#include <stddef.h>

// Initialize the symbol mapping from JSON file
int init_symbol_mapping(const char* json_path);

// Convert a symbol to its ID
int symbol_to_id(const char* symbol);

#endif  