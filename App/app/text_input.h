#ifndef APP_TEXT_INPUT_H
#define APP_TEXT_INPUT_H
#include <stdbool.h>
#include <stdint.h>
#include "driver/keyboard.h"

typedef struct {
    char *buffer;
    uint8_t max_len;
    uint8_t len;
    bool upper;
    uint8_t mode; /* 0=upper(B), 1=lower(b), 2=numeric(2) */
    KEY_Code_t pending_key;
    uint8_t cycle_index;
    uint16_t pending_ticks;
    bool has_pending;
} TEXT_INPUT_Editor_t;

void TEXT_INPUT_Start(TEXT_INPUT_Editor_t *ed, char *buf, uint8_t max_len);
bool TEXT_INPUT_HandleKey(TEXT_INPUT_Editor_t *ed, KEY_Code_t key);
bool TEXT_INPUT_HandleLongKey(TEXT_INPUT_Editor_t *ed, KEY_Code_t key);
void TEXT_INPUT_Tick(TEXT_INPUT_Editor_t *ed);
void TEXT_INPUT_Commit(TEXT_INPUT_Editor_t *ed);

#endif
