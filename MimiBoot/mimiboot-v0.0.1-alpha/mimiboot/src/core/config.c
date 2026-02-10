/**
 * MimiBoot - Minimal Second-Stage Bootloader for ARM Cortex-M
 * 
 * config.c - Boot Configuration Parser
 */

#include "config.h"
#include <stddef.h>

/*============================================================================
 * String Helpers (no libc)
 *============================================================================*/

static int str_len(const char* s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a++ != *b++) return false;
    }
    return *a == *b;
}

static bool str_start(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return false;
    }
    return true;
}

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static const char* skip_space(const char* s) {
    while (*s && is_space(*s)) s++;
    return s;
}

static uint32_t parse_uint(const char* s) {
    uint32_t val = 0;
    while (is_digit(*s)) {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

static bool parse_bool(const char* s) {
    if (str_equal(s, "1") || str_equal(s, "true") || 
        str_equal(s, "yes") || str_equal(s, "on")) {
        return true;
    }
    return false;
}

/*============================================================================
 * Configuration Initialization
 *============================================================================*/

void mimi_config_init(mimi_config_t* config) {
    /* Zero everything */
    uint8_t* p = (uint8_t*)config;
    for (uint32_t i = 0; i < sizeof(mimi_config_t); i++) {
        p[i] = 0;
    }
    
    /* Set defaults */
    str_copy(config->image_path, MIMI_DEFAULT_IMAGE, CONFIG_MAX_PATH);
    str_copy(config->fallback_path, MIMI_DEFAULT_FALLBACK, CONFIG_MAX_PATH);
    config->has_fallback = true;
    
    config->timeout_ms = MIMI_DEFAULT_TIMEOUT;
    config->boot_delay_ms = 0;
    
    config->console_baud = MIMI_DEFAULT_BAUD;
    config->verbose = MIMI_DEFAULT_VERBOSE;
    config->quiet = false;
    
    config->verify = false;
    config->reset_on_fail = true;
    config->max_retries = 3;
    
    config->boot_count = 0;
    config->config_loaded = false;
}

/*============================================================================
 * Configuration Parsing
 *============================================================================*/

/**
 * Parse a single configuration line.
 */
static void parse_line(mimi_config_t* config, const char* line) {
    /* Skip leading whitespace */
    line = skip_space(line);
    
    /* Skip empty lines and comments */
    if (*line == '\0' || *line == '#') {
        return;
    }
    
    /* Find '=' separator */
    const char* eq = line;
    while (*eq && *eq != '=') eq++;
    
    if (*eq != '=') {
        return;  /* No '=' found, skip line */
    }
    
    /* Extract key (trim trailing whitespace) */
    char key[64];
    int key_len = 0;
    const char* k = line;
    while (k < eq && key_len < 63) {
        if (!is_space(*k)) {
            key[key_len++] = *k;
        } else if (key_len > 0) {
            /* Stop at first space after key starts */
            break;
        }
        k++;
    }
    key[key_len] = '\0';
    
    /* Extract value (skip leading whitespace, trim trailing) */
    const char* val_start = skip_space(eq + 1);
    char value[CONFIG_MAX_PATH];
    int val_len = 0;
    
    while (*val_start && *val_start != '\n' && *val_start != '\r' && 
           *val_start != '#' && val_len < CONFIG_MAX_PATH - 1) {
        value[val_len++] = *val_start++;
    }
    
    /* Trim trailing whitespace from value */
    while (val_len > 0 && is_space(value[val_len - 1])) {
        val_len--;
    }
    value[val_len] = '\0';
    
    /* Match known keys */
    if (str_equal(key, "image")) {
        str_copy(config->image_path, value, CONFIG_MAX_PATH);
    }
    else if (str_equal(key, "fallback")) {
        str_copy(config->fallback_path, value, CONFIG_MAX_PATH);
        config->has_fallback = true;
    }
    else if (str_equal(key, "timeout")) {
        config->timeout_ms = parse_uint(value);
    }
    else if (str_equal(key, "delay")) {
        config->boot_delay_ms = parse_uint(value);
    }
    else if (str_equal(key, "baudrate") || str_equal(key, "baud")) {
        config->console_baud = parse_uint(value);
    }
    else if (str_equal(key, "verbose")) {
        config->verbose = parse_bool(value);
    }
    else if (str_equal(key, "quiet")) {
        config->quiet = parse_bool(value);
        if (config->quiet) config->verbose = false;
    }
    else if (str_equal(key, "verify")) {
        config->verify = parse_bool(value);
    }
    else if (str_equal(key, "reset_on_fail")) {
        config->reset_on_fail = parse_bool(value);
    }
    else if (str_equal(key, "max_retries") || str_equal(key, "retries")) {
        config->max_retries = parse_uint(value);
    }
    /* Ignore unknown keys */
}

int mimi_config_parse(mimi_config_t* config, const char* buffer) {
    const char* line_start = buffer;
    
    while (*line_start) {
        /* Find end of line */
        const char* line_end = line_start;
        while (*line_end && *line_end != '\n') {
            line_end++;
        }
        
        /* Copy line to temporary buffer */
        char line[CONFIG_MAX_LINE];
        int line_len = line_end - line_start;
        if (line_len >= CONFIG_MAX_LINE) {
            line_len = CONFIG_MAX_LINE - 1;
        }
        
        for (int i = 0; i < line_len; i++) {
            line[i] = line_start[i];
        }
        line[line_len] = '\0';
        
        /* Parse line */
        parse_line(config, line);
        
        /* Move to next line */
        line_start = line_end;
        if (*line_start == '\n') line_start++;
    }
    
    config->config_loaded = true;
    return 0;
}

int mimi_config_load(
    mimi_config_t* config,
    int (*read_file)(const char* path, char* buffer, uint32_t max_size),
    const char* path
) {
    char buffer[2048];  /* Max config file size */
    
    int result = read_file(path, buffer, sizeof(buffer) - 1);
    if (result < 0) {
        return result;
    }
    
    buffer[result] = '\0';
    return mimi_config_parse(config, buffer);
}

/*============================================================================
 * Boot Image Selection
 *============================================================================*/

const char* mimi_config_get_image(mimi_config_t* config) {
    /* If too many retries, try fallback */
    if (config->boot_count >= config->max_retries) {
        if (config->has_fallback && config->fallback_path[0] != '\0') {
            return config->fallback_path;
        }
    }
    
    /* Return primary image */
    if (config->image_path[0] != '\0') {
        return config->image_path;
    }
    
    return NULL;
}

void mimi_config_boot_attempt(mimi_config_t* config) {
    config->boot_count++;
}

void mimi_config_boot_success(mimi_config_t* config) {
    config->boot_count = 0;
}
