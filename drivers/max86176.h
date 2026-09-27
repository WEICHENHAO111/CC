#ifndef MAX86176_H__
#define MAX86176_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Verified board pin mapping (nRF52810):
 *   P0.05 -> AFE_POWER_EN
 *   P0.11 -> AFE_INT
 *   P0.12 -> VLED_EN
 *   P0.14 -> MAX86176 CS
 *   P0.15 -> MAX86176 SCLK
 *   P0.16 -> MAX86176 MOSI/SDI
 *   P0.17 -> MAX86176 MISO/SDO
 */
#ifndef MAX86176_AFE_POWER_EN_PIN
#define MAX86176_AFE_POWER_EN_PIN   5u
#endif

#ifndef MAX86176_AFE_INT_PIN
#define MAX86176_AFE_INT_PIN        11u
#endif

#ifndef MAX86176_VLED_EN_PIN
#define MAX86176_VLED_EN_PIN        12u
#endif

#ifndef MAX86176_SPI_CS_PIN
#define MAX86176_SPI_CS_PIN         14u
#endif

#ifndef MAX86176_SPI_SCK_PIN
#define MAX86176_SPI_SCK_PIN        15u
#endif

#ifndef MAX86176_SPI_MOSI_PIN
#define MAX86176_SPI_MOSI_PIN       16u
#endif

#ifndef MAX86176_SPI_MISO_PIN
#define MAX86176_SPI_MISO_PIN       17u
#endif

#ifndef MAX86176_SPIM_INSTANCE
#define MAX86176_SPIM_INSTANCE      0
#endif

#define MAX86176_PART_ID_VALUE      0x39u
#define MAX86176_TAG_PPG_MEAS1      0x00u
#define MAX86176_TAG_PPG_MEAS2      0x01u

/*
 * PPG frame rate = 32768 / MAX86176_PPG_FRAME_DIVIDER.
 * 328 gives 99.90 fps. Keep the divider >= 256 for this configuration.
 */
#ifndef MAX86176_PPG_FRAME_DIVIDER
#define MAX86176_PPG_FRAME_DIVIDER  328u
#endif

/* LED current codes on the selected 32 mA range (0.125 mA/LSB). */
#ifndef MAX86176_RED_LED_CURRENT_CODE
#define MAX86176_RED_LED_CURRENT_CODE  0x40u /* 8 mA */
#endif

#ifndef MAX86176_IR_LED_CURRENT_CODE
#define MAX86176_IR_LED_CURRENT_CODE   0x40u /* 8 mA */
#endif

typedef struct
{
    uint8_t  tag;       /* FIFO bits [23:20] */
    uint32_t raw20;     /* FIFO bits [19:0]  */
    int32_t  value;     /* sign-extended 20-bit PPG value */
} max86176_ppg_sample_t;

/* Power up AFE, initialize SPI, verify PART_ID, then software reset. */
bool max86176_init(void);

/* Start red (MEAS1/LED2) and IR (MEAS2/LED1) on PD1, about 100 fps each. */
bool max86176_ppg_start(void);

/* Stop PPG measurement and turn off the external VLED enable. */
void max86176_ppg_stop(void);

/* Read PART_ID. Returns 0 if SPI is not initialized or the read fails. */
uint8_t max86176_read_part_id(void);

/* Number of FIFO items currently available (0..256). */
uint16_t max86176_fifo_count(void);

/* Read one FIFO item if available. Returns true only for a newly read item. */
bool max86176_read_ppg(max86176_ppg_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif /* MAX86176_H__ */
