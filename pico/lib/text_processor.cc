#include "text_processor.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "text_symbols.h"

#define MAX_SEQUENCE_LENGTH 2048
#define MAX_SYMBOL_LENGTH 4

static int* sequence_buffer = NULL;
static size_t sequence_length = 0;

int init_text_processor(const char* json_path) {
    int res = init_symbol_mapping(json_path);
    if (res != 0) return -1;

    sequence_buffer = (int*)malloc(MAX_SEQUENCE_LENGTH * sizeof(int));
    if (!sequence_buffer) return -1;

    return 0;
}

// Clean up processor resources.
void cleanup_text_processor(void) {
    if (sequence_buffer) {
        free(sequence_buffer);
        sequence_buffer = NULL;
    }
}

// Finding a single symbol and add it's ID to sequence.
static int process_symbol(const char* symbol) {
    int id = symbol_to_id(symbol);
    if (id >= 0 && sequence_length < MAX_SEQUENCE_LENGTH) {
        sequence_buffer[sequence_length++] = id;
        return 0;
    }
    return -1;
}

// Making every letter lower case
char* to_lower_case(char* text) {
    for (int i = 0; text[i] != '\0'; i++) {
        text[i] = tolower((unsigned char)text[i]);
    }
    return text;
}

/*
    - Text preprocessing.
    - TODO: Applying different text preprocessing based on language.
*/
char* text_cleaner(char* text, const char* lang) {
    text = to_lower_case(text);
    
    return text;
}

// Converting input text to sequence of IDs.
int* text_to_sequence(char* text, size_t* length) {
    if (!text || !length || !sequence_buffer) {
        return NULL;
    }

    sequence_length = 0;

    text = text_cleaner(text, "en-US");
    
    char symbol[MAX_SYMBOL_LENGTH];
    size_t text_len = strlen(text);
    
    for (size_t i = 0; i < text_len;) {
        if (text[i] == '@' && i + 2 < text_len) {
            size_t j = 0;
            symbol[j++] = text[i++];
            while (i < text_len && text[i] != ' ' && j < MAX_SYMBOL_LENGTH - 1) {
                symbol[j++] = text[i++];
            }
            symbol[j] = '\0';
        } else {
            symbol[0] = text[i++];
            symbol[1] = '\0';
        }
        
        if (process_symbol(symbol) != 0) {
            return NULL;
        }
    }
    
    // Adding EOS token at the end
    if (process_symbol("eos") != 0) {
        return NULL;
    }

    *length = sequence_length;
    
    int* result = (int*)malloc(sequence_length * sizeof(int));
    if (!result) return NULL;
    
    memcpy(result, sequence_buffer, sequence_length * sizeof(int));
    return result;
}