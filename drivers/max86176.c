#include "max86176.h"

#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrfx_spim.h"

/* =========================
 * MAX86176 registers
 * ========================= */
#define REG_FIFO_CNT1           0x0Au
#define REG_FIFO_CNT2           0x0Bu
#define REG_FIFO_DATA           0x0Cu
#define REG_FIFO_CFG2           0x0Eu

#define REG_SYS_CFG1            0x10u
#define REG_SYS_CFG2            0x11u
#define REG_PD_BIAS             0x14u

#define REG_FR_CLK_SEL          0x1Cu
#define REG_FR_DIV_MSB          0x1Du
#define REG_FR_DIV_LSB          0x1Eu

#define REG_MEAS1_SELECT        0x20u
#define REG_MEAS1_CFG1          0x21u
#define REG_MEAS1_CFG2          0x22u
#define REG_MEAS1_CFG3          0x23u
#define REG_MEAS1_CFG4          0x24u
#define REG_MEAS1_LEDA_PA       0x25u
#define REG_MEAS1_LEDB_PA       0x26u

#define REG_MEAS2_SELECT        0x28u
#define REG_MEAS2_CFG1          0x29u
#define REG_MEAS2_CFG2          0x2Au
#define REG_MEAS2_CFG3          0x2Bu
#define REG_MEAS2_CFG4          0x2Cu
#define REG_MEAS2_LEDA_PA       0x2Du
#define REG_MEAS2_LEDB_PA       0x2Eu

#define REG_PART_ID             0xFFu

#define SYS_CFG1_RESET_BIT      0x01u
#define FIFO_CFG2_FLUSH_BIT     0x10u

static const nrfx_spim_t m_spim =
    NRFX_SPIM_INSTANCE(MAX86176_SPIM_INSTANCE);

static bool m_spi_initialized = false;

static void power_gpio_init(void)
{
    nrf_gpio_pin_clear(MAX86176_VLED_EN_PIN);
    nrf_gpio_cfg_output(MAX86176_VLED_EN_PIN);

    nrf_gpio_pin_clear(MAX86176_AFE_POWER_EN_PIN);
    nrf_gpio_cfg_output(MAX86176_AFE_POWER_EN_PIN);

    nrf_gpio_cfg_input(MAX86176_AFE_INT_PIN, NRF_GPIO_PIN_NOPULL);
}

static void afe_power_on(void)
{
    nrf_gpio_pin_set(MAX86176_AFE_POWER_EN_PIN);
    nrf_delay_ms(10);
}

static bool spi_init(void)
{
    nrfx_spim_config_t config;
    nrfx_err_t err;

    if (m_spi_initialized)
    {
        return true;
    }

    config = (nrfx_spim_config_t)NRFX_SPIM_DEFAULT_CONFIG;

    config.sck_pin   = MAX86176_SPI_SCK_PIN;
    config.mosi_pin  = MAX86176_SPI_MOSI_PIN;
    config.miso_pin  = MAX86176_SPI_MISO_PIN;
    config.ss_pin    = NRFX_SPIM_PIN_NOT_USED;
    config.frequency = NRF_SPIM_FREQ_1M;
    config.mode      = NRF_SPIM_MODE_0;
    config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;

    nrf_gpio_pin_set(MAX86176_SPI_CS_PIN);
    nrf_gpio_cfg_output(MAX86176_SPI_CS_PIN);

    err = nrfx_spim_init(&m_spim, &config, NULL, NULL);
    if (err != NRFX_SUCCESS)
    {
        return false;
    }

    m_spi_initialized = true;
    return true;
}

static bool write_reg(uint8_t reg, uint8_t data)
{
    uint8_t tx[3];
    uint8_t rx[3];
    nrfx_spim_xfer_desc_t xfer;
    nrfx_err_t err;

    tx[0] = reg;
    tx[1] = 0x00u;
    tx[2] = data;

    rx[0] = 0u;
    rx[1] = 0u;
    rx[2] = 0u;

    xfer = (nrfx_spim_xfer_desc_t)
           NRFX_SPIM_XFER_TRX(tx, sizeof(tx), rx, sizeof(rx));

    nrf_gpio_pin_clear(MAX86176_SPI_CS_PIN);
    err = nrfx_spim_xfer(&m_spim, &xfer, 0);
    nrf_gpio_pin_set(MAX86176_SPI_CS_PIN);

    return (err == NRFX_SUCCESS);
}

static bool read_reg(uint8_t reg, uint8_t *data)
{
    uint8_t tx[3];
    uint8_t rx[3];
    nrfx_spim_xfer_desc_t xfer;
    nrfx_err_t err;

    if (data == NULL)
    {
        return false;
    }

    tx[0] = reg;
    tx[1] = 0x80u;
    tx[2] = 0x00u;

    rx[0] = 0u;
    rx[1] = 0u;
    rx[2] = 0u;

    xfer = (nrfx_spim_xfer_desc_t)
           NRFX_SPIM_XFER_TRX(tx, sizeof(tx), rx, sizeof(rx));

    nrf_gpio_pin_clear(MAX86176_SPI_CS_PIN);
    err = nrfx_spim_xfer(&m_spim, &xfer, 0);
    nrf_gpio_pin_set(MAX86176_SPI_CS_PIN);

    if (err != NRFX_SUCCESS)
    {
        return false;
    }

    *data = rx[2];
    return true;
}

static bool software_reset(void)
{
    if (!write_reg(REG_SYS_CFG1, SYS_CFG1_RESET_BIT))
    {
        return false;
    }

    nrf_delay_ms(1);
    return true;
}

static bool fifo_read_word(uint32_t *word)
{
    uint8_t tx[5];
    uint8_t rx[5];
    nrfx_spim_xfer_desc_t xfer;
    nrfx_err_t err;

    if (word == NULL)
    {
        return false;
    }

    /* FIFO_DATA must be read with SPI burst mode. */
    tx[0] = REG_FIFO_DATA;
    tx[1] = 0x80u;
    tx[2] = 0x00u;
    tx[3] = 0x00u;
    tx[4] = 0x00u;

    rx[0] = 0u;
    rx[1] = 0u;
    rx[2] = 0u;
    rx[3] = 0u;
    rx[4] = 0u;

    xfer = (nrfx_spim_xfer_desc_t)
           NRFX_SPIM_XFER_TRX(tx, sizeof(tx), rx, sizeof(rx));

    nrf_gpio_pin_clear(MAX86176_SPI_CS_PIN);
    err = nrfx_spim_xfer(&m_spim, &xfer, 0);
    nrf_gpio_pin_set(MAX86176_SPI_CS_PIN);

    if (err != NRFX_SUCCESS)
    {
        return false;
    }

    *word = ((uint32_t)rx[2] << 16) |
            ((uint32_t)rx[3] << 8)  |
            ((uint32_t)rx[4]);

    return true;
}

bool max86176_init(void)
{
    uint8_t part_id;

    power_gpio_init();
    nrf_delay_ms(10);
    afe_power_on();

    if (!spi_init())
    {
        return false;
    }

    if (!read_reg(REG_PART_ID, &part_id))
    {
        return false;
    }

    if (part_id != MAX86176_PART_ID_VALUE)
    {
        return false;
    }

    if (!software_reset())
    {
        return false;
    }

    if (!read_reg(REG_PART_ID, &part_id))
    {
        return false;
    }

    return (part_id == MAX86176_PART_ID_VALUE);
}

bool max86176_ppg_start(void)
{
    uint8_t fifo_cfg;
    uint16_t frame_divider;

    frame_divider = (uint16_t)MAX86176_PPG_FRAME_DIVIDER;

    /* Stop all measurements before reconfiguration. */
    if (!write_reg(REG_SYS_CFG2, 0x00u))
    {
        return false;
    }

    nrf_gpio_pin_set(MAX86176_VLED_EN_PIN);
    nrf_delay_ms(10);

    /* Power up both optical measurement channels. */
    if (!write_reg(REG_SYS_CFG1, 0x00u))
    {
        return false;
    }

    if (!write_reg(REG_PD_BIAS, 0x55u))
    {
        return false;
    }

    /* Internal 32.768 kHz frame clock. */
    if (!write_reg(REG_FR_CLK_SEL, 0x20u))
    {
        return false;
    }

    /* Frame rate = 32768 / divider; default 328 -> 99.90 fps. */
    if (!write_reg(REG_FR_DIV_MSB, (uint8_t)(frame_divider >> 8)))
    {
        return false;
    }

    if (!write_reg(REG_FR_DIV_LSB, (uint8_t)frame_divider))
    {
        return false;
    }

    /* Verified board setting: Driver A -> LED2_DRV (visible red LED). */
    if (!write_reg(REG_MEAS1_SELECT, 0x01u))
    {
        return false;
    }

    /* TINT = 117.1 us, averaging off. */
    if (!write_reg(REG_MEAS1_CFG1, 0x18u))
    {
        return false;
    }

    /* CDM, 32 mA LED range, PPG ADC ranges = 16 uA. */
    if (!write_reg(REG_MEAS1_CFG2, 0x0Au))
    {
        return false;
    }

    if (!write_reg(REG_MEAS1_CFG3, 0x50u))
    {
        return false;
    }

    /* Active path: PPG1 -> PD1. */
    if (!write_reg(REG_MEAS1_CFG4, 0x04u))
    {
        return false;
    }

    /* 0x40 * 0.125 mA ~= 8 mA on the 32 mA full-scale range. */
    if (!write_reg(REG_MEAS1_LEDA_PA, MAX86176_RED_LED_CURRENT_CODE))
    {
        return false;
    }

    if (!write_reg(REG_MEAS1_LEDB_PA, 0x00u))
    {
        return false;
    }

    /*
     * Netlist mapping:
     *   LED2_DRV -> U6 pin 2 (verified visible red)
     *   LED1_DRV -> U6 pin 4 (IR)
     *   PD1_IN   -> U7 SFH2704 photodiode
     * MEAS2 uses LED1 through driver A and the same PD1 receive path.
     */
    if (!write_reg(REG_MEAS2_SELECT, 0x00u))
    {
        return false;
    }

    if (!write_reg(REG_MEAS2_CFG1, 0x18u))
    {
        return false;
    }

    if (!write_reg(REG_MEAS2_CFG2, 0x0Au))
    {
        return false;
    }

    if (!write_reg(REG_MEAS2_CFG3, 0x50u))
    {
        return false;
    }

    if (!write_reg(REG_MEAS2_CFG4, 0x04u))
    {
        return false;
    }

    if (!write_reg(REG_MEAS2_LEDA_PA, MAX86176_IR_LED_CURRENT_CODE))
    {
        return false;
    }

    if (!write_reg(REG_MEAS2_LEDB_PA, 0x00u))
    {
        return false;
    }

    if (!read_reg(REG_FIFO_CFG2, &fifo_cfg))
    {
        return false;
    }

    if (!write_reg(REG_FIFO_CFG2, (uint8_t)(fifo_cfg | FIFO_CFG2_FLUSH_BIT)))
    {
        return false;
    }

    nrf_delay_ms(1);

    /* Enable MEAS1 (red) and MEAS2 (IR). */
    if (!write_reg(REG_SYS_CFG2, 0x03u))
    {
        return false;
    }

    return true;
}

void max86176_ppg_stop(void)
{
    if (m_spi_initialized)
    {
        (void)write_reg(REG_SYS_CFG2, 0x00u);
    }

    nrf_gpio_pin_clear(MAX86176_VLED_EN_PIN);
}

uint8_t max86176_read_part_id(void)
{
    uint8_t part_id;

    part_id = 0u;

    if (!m_spi_initialized)
    {
        return 0u;
    }

    if (!read_reg(REG_PART_ID, &part_id))
    {
        return 0u;
    }

    return part_id;
}

uint16_t max86176_fifo_count(void)
{
    uint8_t cnt1;
    uint8_t cnt2;

    cnt1 = 0u;
    cnt2 = 0u;

    if (!m_spi_initialized)
    {
        return 0u;
    }

    if (!read_reg(REG_FIFO_CNT1, &cnt1))
    {
        return 0u;
    }

    if (!read_reg(REG_FIFO_CNT2, &cnt2))
    {
        return 0u;
    }

    return (uint16_t)(((uint16_t)(cnt1 & 0x80u) << 1) |
                      (uint16_t)cnt2);
}

bool max86176_read_ppg(max86176_ppg_sample_t *sample)
{
    uint32_t word;
    uint32_t raw20;

    if (sample == NULL)
    {
        return false;
    }

    if (max86176_fifo_count() == 0u)
    {
        return false;
    }

    if (!fifo_read_word(&word))
    {
        return false;
    }

    sample->tag = (uint8_t)((word >> 20) & 0x0Fu);

    raw20 = word & 0x000FFFFFu;
    sample->raw20 = raw20;

    if ((raw20 & 0x00080000u) != 0u)
    {
        sample->value = (int32_t)(raw20 | 0xFFF00000u);
    }
    else
    {
        sample->value = (int32_t)raw20;
    }

    return true;
}
