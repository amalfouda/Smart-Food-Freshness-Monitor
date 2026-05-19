#include "food_score.h"
#include <math.h>

/* ADC baselines measured after 1h warmup in clean air */
#define MQ135_CLEAN_AIR  63
#define MQ3_CLEAN_AIR    274
#define MQ135_BASELINE   57
#define MQ3_BASELINE     247
#define ADC_MAX          4095
#define MQ135_ADC_MAX    400
#define MQ3_ADC_MAX      ADC_MAX

#define MQ135_RANGE      (MQ135_ADC_MAX - MQ135_BASELINE)   /* 343 counts */
#define MQ3_RANGE        (MQ3_ADC_MAX   - MQ3_BASELINE)     /* 3848 counts */

#define MQ135_PPM_FULLSCALE   200u
#define MQ3_PPM_FULLSCALE     400u

/* PPM thresholds per food type, from food science reference table */
typedef struct {
    uint16_t mq135_fresh;
    uint16_t mq135_borderline;
    uint16_t mq3_fresh;
    uint16_t mq3_borderline;
    int16_t  temp_safe_max;      /* above this = spoilage risk */
    uint8_t  rh_safe_max;
    uint8_t  rh_danger_max;
    uint8_t  w_mq135;
    uint8_t  w_mq3;
    uint8_t  w_temp;
    uint8_t  w_rh;
} FoodProfile_t;

static const FoodProfile_t profiles[FOOD_MODE_COUNT] = {
    [FOOD_MEAT] = {
        .mq135_fresh      = 25,
        .mq135_borderline = 100,
        .mq3_fresh        = 10,
        .mq3_borderline   = 50,
        .temp_safe_max    = 4,
        .rh_safe_max      = 85,
        .rh_danger_max    = 95,
        .w_mq135          = 56,
        .w_mq3            = 17,
        .w_temp           = 27,
        .w_rh             = 10
    },
    [FOOD_BREAD] = {
        .mq135_fresh      = 50,
        .mq135_borderline = 150,
        .mq3_fresh        = 20,
        .mq3_borderline   = 100,
        .temp_safe_max    = 25,
        .rh_safe_max      = 75,
        .rh_danger_max    = 85,
        .w_mq135          = 25,
        .w_mq3            = 42,
        .w_temp           = 33,
        .w_rh             = 40
    },
    [FOOD_FRUIT] = {
        .mq135_fresh      = 50,
        .mq135_borderline = 150,
        .mq3_fresh        = 30,
        .mq3_borderline   = 150,
        .temp_safe_max    = 20,
        .rh_safe_max      = 95,
        .rh_danger_max    = 100,
        .w_mq135          = 22,
        .w_mq3            = 50,
        .w_temp           = 28,
        .w_rh             = 10
    },
    [FOOD_DAIRY] = {
        .mq135_fresh      = 30,
        .mq135_borderline = 100,
        .mq3_fresh        = 15,
        .mq3_borderline   = 80,
        .temp_safe_max    = 7,
        .rh_safe_max      = 90,
        .rh_danger_max    = 97,
        .w_mq135          = 32,
        .w_mq3            = 37,
        .w_temp           = 31,
        .w_rh             = 5
    }
};

static const char* mode_names[]       = {"Meat",  "Bread", "Fruit", "Dairy"};
static const char* mode_short_names[] = {"MEAT",  "BREAD", "FRUIT", "DAIRY"};

const char* FoodMode_Name(FoodMode_t mode)      { return mode_names[mode]; }
const char* FoodMode_ShortName(FoodMode_t mode) { return mode_short_names[mode]; }

/* Sigmoid curve: returns 0-100% spoilage.
 * ~5% at fresh_adc, 50% at midpoint, ~95% at border_adc.
 * k controls the steepness, derived from ln(19) / half_band. */
static uint8_t adc_to_spoilage(uint16_t raw,
                                uint16_t fresh_adc,
                                uint16_t border_adc)
{
    float half_band = (border_adc - fresh_adc) / 2.0f;
    if (half_band < 1.0f) half_band = 1.0f;

    float mid      = (fresh_adc + border_adc) / 2.0f;
    float k        = 2.944f / half_band;
    float spoilage = 100.0f / (1.0f + expf(-k * ((float)raw - mid)));

    if (spoilage <  0.5f) return 0;
    if (spoilage > 99.5f) return 100;
    return (uint8_t)spoilage;
}

/* Linear ramp: 0% at safe_max, 100% at safe_max+15C */
static uint8_t temp_to_spoilage(int16_t temp_c, int16_t safe_max)
{
    if (temp_c <= safe_max)       return 0;
    if (temp_c >= safe_max + 15)  return 100;
    return (uint8_t)((temp_c - safe_max) * 100 / 15);
}

/* Linear ramp: 0% at rh_safe_max, 100% at rh_danger_max */
static uint8_t rh_to_spoilage(uint8_t rh, uint8_t rh_safe_max, uint8_t rh_danger_max)
{
    if (rh <= rh_safe_max)  return 0;
    if (rh >= rh_danger_max) return 100;
    return (uint8_t)((rh - rh_safe_max) * 100 / (rh_danger_max - rh_safe_max));
}

ScoreResult_t ComputeFreshnessScore(FoodMode_t mode, SensorInput_t *input)
{
    ScoreResult_t result = {0};
    const FoodProfile_t *p = &profiles[mode];

    /* Convert PPM thresholds to ADC counts for this hardware */
    uint16_t mq135_fresh_adc = (uint16_t)(MQ135_BASELINE
        + ((uint32_t)MQ135_RANGE * p->mq135_fresh / MQ135_PPM_FULLSCALE));

    uint16_t mq135_border_adc = (uint16_t)(MQ135_BASELINE
        + ((uint32_t)MQ135_RANGE * p->mq135_borderline / MQ135_PPM_FULLSCALE));

    uint16_t mq3_fresh_adc = (uint16_t)(MQ3_BASELINE
        + ((uint32_t)MQ3_RANGE * p->mq3_fresh / MQ3_PPM_FULLSCALE));

    uint16_t mq3_border_adc = (uint16_t)(MQ3_BASELINE
        + ((uint32_t)MQ3_RANGE * p->mq3_borderline / MQ3_PPM_FULLSCALE));

    uint8_t mq135_spoilage = adc_to_spoilage(input->mq135_raw,
                                              mq135_fresh_adc,
                                              mq135_border_adc);

    uint8_t mq3_spoilage   = adc_to_spoilage(input->mq3_raw,
                                              mq3_fresh_adc,
                                              mq3_border_adc);

    uint8_t temp_spoilage  = temp_to_spoilage(input->temperature,
                                               p->temp_safe_max);

    uint8_t rh_spoilage    = rh_to_spoilage(input->humidity,
                                             p->rh_safe_max,
                                             p->rh_danger_max);

    uint32_t total_weight = (uint32_t)p->w_mq135 + p->w_mq3 + p->w_temp + p->w_rh;
    uint32_t weighted_sum = ((uint32_t)mq135_spoilage * p->w_mq135)
                          + ((uint32_t)mq3_spoilage   * p->w_mq3)
                          + ((uint32_t)temp_spoilage  * p->w_temp)
                          + ((uint32_t)rh_spoilage    * p->w_rh);
    uint8_t spoilage_index = (uint8_t)(weighted_sum / total_weight);

    result.score = (spoilage_index >= 100) ? 0 : (uint8_t)(100 - spoilage_index);

    /* Snap to 100 in clean air — sigmoid never fully reaches 0 so without
     * this the display would show 97-98% even with nothing near the sensors */
    if (input->mq135_raw <= (MQ135_CLEAN_AIR + 10) &&
        input->mq3_raw   <= (MQ3_CLEAN_AIR   + 10))
    {
        result.score     = 100;
        result.status    = STATUS_FRESH;
        result.mq135_pct = 0;
        result.mq3_pct   = 0;
        return result;
    }
    result.mq135_pct = mq135_spoilage;
    result.mq3_pct   = mq3_spoilage;

    if      (result.score >= 70) result.status = STATUS_FRESH;
    else if (result.score >= 40) result.status = STATUS_WARN;
    else                         result.status = STATUS_SPOIL;

    return result;
}
