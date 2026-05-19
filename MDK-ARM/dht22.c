#include "dht22.h"
#include "main.h"

/* -- Timing constants (us) ------------------------------------------------- */
#define DHT22_START_LOW_US      1200    /* pull low >1ms to wake sensor       */
#define DHT22_BIT_THRESHOLD_US  50      /* >50us HIGH = bit '1', else bit '0' */
#define DHT22_TIMEOUT_US        1000    /* max wait for any single edge        */

/* -- Private: us delay using TIM2 free-running counter -------------------- */
static void delay_us(uint32_t us)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}

/* -- Private: wait for pin to reach desired state, return elapsed us ------- */
/* Returns elapsed time in us, or 0xFFFF on timeout                          */
static uint16_t wait_for_pin(GPIO_PinState state, uint32_t timeout_us)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (HAL_GPIO_ReadPin(DHT22_DATA_GPIO_Port, DHT22_DATA_Pin) != state)
    {
        if (__HAL_TIM_GET_COUNTER(&htim2) > timeout_us)
            return 0xFFFF;
    }
    return (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
}

/* -- Private: set PA6 as output open-drain --------------------------------- */
static void DHT22_SetOutput(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = DHT22_DATA_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(DHT22_DATA_GPIO_Port, &g);
}

/* -- Private: set PA6 as input (pull-up on breakout holds line HIGH at rest) -- */
static void DHT22_SetInput(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = DHT22_DATA_Pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT22_DATA_GPIO_Port, &g);
}

/* -- Public: read 40 bits from DHT22, parse into data struct -------------- */
DHT22_Status_t DHT22_Read(DHT22_Data_t *data)
{
    uint8_t  raw[5] = {0};
    uint16_t duration;

    /* -- Steps 1-3: timing-critical section (~5ms) -------------------------
     * FreeRTOS task preemption disrupts us-level bit-bang timing.
     * Disable the scheduler for the entire start-pulse + 40-bit read.     */
    __disable_irq();

    /* -- Step 1: MCU sends start pulse ------------------------------------- */
    DHT22_SetOutput();
    HAL_GPIO_WritePin(DHT22_DATA_GPIO_Port, DHT22_DATA_Pin, GPIO_PIN_RESET);
    delay_us(DHT22_START_LOW_US);
    HAL_GPIO_WritePin(DHT22_DATA_GPIO_Port, DHT22_DATA_Pin, GPIO_PIN_SET);
    delay_us(40);   /* hold HIGH 40us so sensor detects rising edge before we switch to input */
    DHT22_SetInput();

    /* -- Step 2: Sensor acknowledgement ------------------------------------ */
    /* Sensor pulls line LOW ~20-40us after we release, then HIGH ~80us     */
    duration = wait_for_pin(GPIO_PIN_RESET, DHT22_TIMEOUT_US);
    if (duration == 0xFFFF) { __enable_irq(); return DHT22_ERROR_NO_RESPONSE; }

    duration = wait_for_pin(GPIO_PIN_SET, DHT22_TIMEOUT_US);
    if (duration == 0xFFFF) { __enable_irq(); return DHT22_ERROR_TIMEOUT; }

    duration = wait_for_pin(GPIO_PIN_RESET, DHT22_TIMEOUT_US);
    if (duration == 0xFFFF) { __enable_irq(); return DHT22_ERROR_TIMEOUT; }

    /* -- Step 3: Read 40 bits ---------------------------------------------- */
    /* Per bit: 50us LOW preamble, then 26-28us HIGH = '0', 70us HIGH = '1' */
    for (int byte_idx = 0; byte_idx < 5; byte_idx++)
    {
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--)
        {
            duration = wait_for_pin(GPIO_PIN_SET, DHT22_TIMEOUT_US);
            if (duration == 0xFFFF) { __enable_irq(); return DHT22_ERROR_TIMEOUT; }

            __HAL_TIM_SET_COUNTER(&htim2, 0);
            wait_for_pin(GPIO_PIN_RESET, DHT22_TIMEOUT_US);
            uint16_t high_time = __HAL_TIM_GET_COUNTER(&htim2);

            if (high_time > DHT22_BIT_THRESHOLD_US)
                raw[byte_idx] |= (1 << bit_idx);
        }
    }

    __enable_irq();

    /* -- Step 4: Checksum -------------------------------------------------- */
    uint8_t checksum = raw[0] + raw[1] + raw[2] + raw[3];
    if (checksum != raw[4])
        return DHT22_ERROR_CHECKSUM;

    /* -- Step 5: Parse humidity (bytes 0-1, value x10) -------------------- */
    uint16_t humidity_raw = ((uint16_t)raw[0] << 8) | raw[1];
    data->humidity_int = humidity_raw / 10;
    data->humidity_dec = humidity_raw % 10;

    /* -- Step 6: Parse temperature (bytes 2-3, value x10) ----------------- */
    data->temp_negative = (raw[2] & 0x80) ? 1 : 0;
    raw[2] &= 0x7F;
    uint16_t temp_raw = ((uint16_t)raw[2] << 8) | raw[3];
    data->temperature_int = temp_raw / 10;
    data->temperature_dec = temp_raw % 10;

    /* -- Step 7: Return line to idle HIGH ---------------------------------- */
    DHT22_SetOutput();
    HAL_GPIO_WritePin(DHT22_DATA_GPIO_Port, DHT22_DATA_Pin, GPIO_PIN_SET);

    return DHT22_OK;
}
