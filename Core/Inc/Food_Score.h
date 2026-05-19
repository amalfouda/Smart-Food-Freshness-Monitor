#ifndef FOOD_SCORE_H
#define FOOD_SCORE_H

#include <stdint.h>

/* ── Food modes ──────────────────────────────────────────────────────────── */
typedef enum {
    FOOD_MEAT  = 0,
    FOOD_BREAD = 1,
    FOOD_FRUIT = 2,
    FOOD_DAIRY = 3,
    FOOD_MODE_COUNT
} FoodMode_t;

/* ── Freshness status ────────────────────────────────────────────────────── */
typedef enum {
    STATUS_FRESH = 0,
    STATUS_WARN  = 1,
    STATUS_SPOIL = 2
} FreshnessStatus_t;

/* ── Sensor input ────────────────────────────────────────────────────────── */
typedef struct {
    uint16_t mq135_raw;
    uint16_t mq3_raw;
    int16_t  temperature;
    uint8_t  humidity;      /* relative humidity 0-100% */
} SensorInput_t;

/* ── Score result ────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t           score;
    FreshnessStatus_t status;
    uint8_t           mq135_pct;
    uint8_t           mq3_pct;
} ScoreResult_t;

/* ── Public API ──────────────────────────────────────────────────────────── */
const char*   FoodMode_Name(FoodMode_t mode);
const char*   FoodMode_ShortName(FoodMode_t mode);
ScoreResult_t ComputeFreshnessScore(FoodMode_t mode, SensorInput_t *input);

#endif /* FOOD_SCORE_H */