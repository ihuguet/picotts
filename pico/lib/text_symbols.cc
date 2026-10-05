#include "text_symbols.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_SYMBOLS 148      // Maximume symbols that model can process - TODO: Must chenage in different language.
#define MAX_SYMBOL_LENGTH 4  // Maximume characters of symbols.

// A structure to hold symbol and IDs
typedef struct {
    char symbol[MAX_SYMBOL_LENGTH];
    int id;
} SymbolMapping;

static SymbolMapping* symbol_map = NULL;  // Holds symbol-ID pairs.
static size_t symbol_count = 0;           // Holds number of symbols

// To parse the JSON file
static int parse_json_mapping(const char* json_path) {
    FILE* fp = fopen(json_path, "r");
    if (!fp) return -1;

    symbol_map = (SymbolMapping*)malloc(MAX_SYMBOLS * sizeof(SymbolMapping));
    
    if (!symbol_map) {
        fclose(fp);
        return -1;
    }
    
    char buffer[4096];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);
    
    char* symbol_section = strstr(buffer, "\"symbol_to_id\": {");
    if (!symbol_section) {
        free(symbol_map);
        symbol_map = NULL;
        return -1;
    }
    
    char* ptr = symbol_section;
    while ((ptr = strstr(ptr, "\""))) {
        ptr++; // Skip opening quote
        if (*ptr == '}') break;
        
        char symbol[MAX_SYMBOL_LENGTH];
        size_t i = 0;
        while (*ptr != '"' && i < MAX_SYMBOL_LENGTH - 1) {
            symbol[i++] = *ptr++;
        }
        symbol[i] = '\0';

        ptr = strchr(ptr, ':');
        if (!ptr) break;
        ptr++;
        
        int id = atoi(ptr);
        
        if (symbol_count <= MAX_SYMBOLS) {
            strcpy(symbol_map[symbol_count].symbol, symbol);
            symbol_map[symbol_count].id = id;
            symbol_count++;
        }
        
        ptr = strchr(ptr, ',');
        if (!ptr) break;
    }

    return 0;
}


int init_symbol_mapping(const char* json_path) {
    if (symbol_map) return 0;
    return parse_json_mapping(json_path);
}

// Gives a symbol and returns the ID of that symbol.
int symbol_to_id(const char* symbol) {
    if (!symbol_map) return -1;

    for (size_t i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_map[i].symbol, symbol) == 0) {
            return symbol_map[i].id;
        }
    }
    return -1;
}
