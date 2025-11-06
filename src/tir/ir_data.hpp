#pragma once
#include <stdint.h>

enum irproto : uint8_t {
    UNK,
    NEC,
    NECEXT,
    SONY,
    SAM,
    RC5,
    PROTO_COUNT
};

typedef struct ir_data {
    irproto protocol;
    uint64_t data;
    uint8_t repeat;
} ir_data_t;