#ifndef DHT22_H
#define DHT22_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

/* -- Pin binding -------------------------------------------------------------
 * These must match the labels you gave in CubeMX.
 * CubeMX generates these defines in main.h automatically.
 * DHT22_DATA_Pin and DHT22_DATA_GPIO_Port are already defined there.
 * --------------------------------------------------------------------------- */
extern TIM_HandleTypeDef htim2;   /* µs timer, started in main() */

/* -- Return status --------------------------------------------------------- */
typedef enum {
    DHT22_OK        = 0,   /* valid reading */
    DHT22_ERROR_NO_RESPONSE,   /* sensor did not pull line low after start */
    DHT22_ERROR_TIMEOUT,       /* bit reading timed out */
    DHT22_ERROR_CHECKSUM       /* data received but checksum mismatch */
} DHT22_Status_t;

/* -- Sensor data ----------------------------------------------------------- */
typedef struct {
    uint16_t humidity_int;       /* humidity whole part    e.g. 65   */
    uint16_t humidity_dec;       /* humidity decimal part  e.g. 3 ? 65.3% */
    uint16_t temperature_int;    /* temperature whole part e.g. 27   */
    uint16_t temperature_dec;    /* temperature decimal    e.g. 5 ? 27.5°C */
    uint8_t  temp_negative;      /* 1 if temperature is below 0°C */
} DHT22_Data_t;

/* -- Public API ------------------------------------------------------------ */
DHT22_Status_t DHT22_Read(DHT22_Data_t *data);

#endif /* DHT22_H */