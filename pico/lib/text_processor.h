#ifndef TEXT_PROCESSOR_H
#define TEXT_PROCESSOR_H

#include <stddef.h>
#include "text_symbols.h"

// Initialize the text processor with the symbol mapping
int init_text_processor(const char* json_path);

// Convert text to a sequence of symbol IDs
int* text_to_sequence(char* text, size_t* length);

// To free the sequence pointer
void cleanup_text_processor(void);

// Helper functions
char* text_cleaner(char* text, const char* lang);
char* to_lower_case(char* text);

#endif 