/*****************************************************************************
* | File      	:   dev_config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2025-08-29
* | Info        :   Basic version
*
******************************************************************************/
#include "dev_config.h"

TDL_BUTTON_HANDLE button_hdl = NULL;

OPERATE_RET dev_gpio_init(uint8_t pin, uint8_t mode)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    if(mode == 0 || mode == TUYA_GPIO_INPUT) {
        pin_cfg.mode = TUYA_GPIO_PULLUP;
        pin_cfg.direct = TUYA_GPIO_INPUT;
    } else {
        pin_cfg.mode = TUYA_GPIO_PULLUP;
        pin_cfg.direct = TUYA_GPIO_OUTPUT;
        pin_cfg.level = TUYA_GPIO_LEVEL_LOW;
    }
    return tkl_gpio_init(pin, &pin_cfg);
}

OPERATE_RET dev_digital_write(uint8_t pin, uint8_t value)
{
    return tkl_gpio_write(pin, value);
}

OPERATE_RET dev_digital_read(uint8_t pin, uint8_t *value)
{
    return tkl_gpio_read(pin, value);
}

OPERATE_RET dev_button_init(uint8_t pin)
{
    OPERATE_RET rt = OPRT_OK;

    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = pin,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(EXAMPLE_PWR_BUTTON_NAME, &button_hw_cfg));
    
    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    

    TUYA_CALL_ERR_RETURN(tdl_button_create(EXAMPLE_PWR_BUTTON_NAME, &button_cfg, &button_hdl));

    return rt;

}

void dev_button_event_register(TDL_BUTTON_TOUCH_EVENT_E event, TDL_BUTTON_EVENT_CB cb)
{
    tdl_button_event_register(button_hdl, event, cb);
}

void dev_sys_init()
{
    dev_gpio_init(EXAMPLE_GPIO_47_PIN, TUYA_GPIO_OUTPUT);
    dev_gpio_init(EXAMPLE_GPIO_17_PIN, TUYA_GPIO_OUTPUT);
    dev_gpio_init(EXAMPLE_GPIO_16_PIN, TUYA_GPIO_OUTPUT);
    dev_gpio_init(EXAMPLE_GPIO_15_PIN, TUYA_GPIO_OUTPUT);
    dev_gpio_init(EXAMPLE_GPIO_14_PIN, TUYA_GPIO_OUTPUT);

    dev_digital_write(EXAMPLE_GPIO_14_PIN, 1);
    dev_digital_write(EXAMPLE_GPIO_47_PIN, 1);
    dev_digital_write(EXAMPLE_GPIO_17_PIN, 1);
    dev_digital_write(EXAMPLE_GPIO_16_PIN, 1);
    dev_digital_write(EXAMPLE_GPIO_15_PIN, 1);
}

OPERATE_RET dev_adc_init()
{
    static TUYA_ADC_BASE_CFG_T sg_adc_cfg = {
        .ch_list.data = 1 << ADC_CHANNEL,
        .ch_nums = 1, // adc Number of channel lists
        .width = 12,
        .mode = TUYA_ADC_CONTINUOUS,
        .type = TUYA_ADC_INNER_SAMPLE_VOL,
        .conv_cnt = 8,
    };
    return tkl_adc_init(TUYA_ADC_NUM_0, &sg_adc_cfg);
}

float dev_adc_read()
{
    int32_t adc_value[8] = {0};
    memset(adc_value, 0, sizeof(adc_value));
    /* ADC Read */
    tkl_adc_read_voltage(TUYA_ADC_NUM_0, adc_value, 8);
    // PR_DEBUG("ADC%d value(mv) = %d", TUYA_ADC_NUM_0, adc_value[0]);
    // PR_DEBUG("ADC%d value(mv) = %f", TUYA_ADC_NUM_0, adc_value[0] * ADC_Ratio_Voltage);
    return adc_value[0] * ADC_Ratio_Voltage;
}

//I2C
OPERATE_RET dev_i2c_init()
{
    TUYA_IIC_BASE_CFG_T cfg;

    tkl_io_pinmux_config(EXAMPLE_I2C_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(EXAMPLE_I2C_SDA_PIN, TUYA_IIC0_SDA);

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    OPERATE_RET ret = tkl_i2c_init(TUYA_I2C_NUM_0, &cfg);
    if (OPRT_OK != ret) {
        PR_ERR("i2c init fail, err<%d>!", ret);
    }
    return ret;
}

OPERATE_RET dev_i2c_write(uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, data, 2, FALSE);
}

OPERATE_RET dev_i2c_write_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len)
{
    return tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, pdata, len, FALSE);
}

OPERATE_RET dev_i2c_read_nbytes(uint8_t addr, uint8_t reg, uint8_t *pdata, uint32_t len)
{
    tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, &reg, 1, FALSE);
    return tkl_i2c_master_receive(TUYA_I2C_NUM_0, addr, pdata, len, FALSE);
}

OPERATE_RET dev_i2c_read_only_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len)
{
    return tkl_i2c_master_receive(TUYA_I2C_NUM_0, addr, pdata, len, FALSE);
}

//UART
OPERATE_RET dev_uart_init(TUYA_UART_NUM_E port, uint32_t baudrate)
{
    OPERATE_RET ret;

    // Configure UART parameters using TAL (Tuya Abstraction Layer)
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = baudrate;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.flowctrl = TUYA_UART_FLOWCTRL_NONE;
    cfg.rx_buffer_size = 1024;  // Buffer size for receiving
    cfg.open_mode = O_BLOCK;    // Blocking mode

    ret = tal_uart_init(port, &cfg);
    if (OPRT_OK != ret) {
        PR_ERR("UART init fail, err<%d>!", ret);
    } else {
        PR_INFO("UART port %d initialized at baudrate %d", port, baudrate);
    }
    
    return ret;
}

OPERATE_RET dev_uart_deinit(TUYA_UART_NUM_E port)
{
    return tal_uart_deinit(port);
}

int dev_uart_write(TUYA_UART_NUM_E port, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) {
        return -1;
    }
    return tal_uart_write(port, data, len);
}

int dev_uart_read(TUYA_UART_NUM_E port, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (!data || len == 0) {
        return -1;
    }
    
    // Use TAL UART read with timeout via polling
    uint32_t start_time = tal_system_get_millisecond();
    int total_read = 0;
    
    while (total_read < len) {
        int bytes = tal_uart_read(port, data + total_read, len - total_read);
        if (bytes > 0) {
            total_read += bytes;
            // If we got some data, return immediately (non-greedy read)
            break;
        }
        
        // Check timeout
        uint32_t elapsed = tal_system_get_millisecond() - start_time;
        if (elapsed >= timeout_ms) {
            break;
        }
        
        // Short delay to avoid busy waiting
        tal_system_sleep(10);
    }
    
    return total_read;
}

OPERATE_RET dev_uart_wait_for_data(TUYA_UART_NUM_E port, uint32_t timeout_ms)
{
    // Poll for data availability using TAL API
    uint32_t start_time = tal_system_get_millisecond();
    uint8_t test_byte;
    
    while (1) {
        int bytes = tal_uart_read(port, &test_byte, 1);
        if (bytes > 0) {
            return OPRT_OK;
        }
        
        uint32_t elapsed = tal_system_get_millisecond() - start_time;
        if (elapsed >= timeout_ms) {
            return OPRT_TIMEOUT;
        }
        
        tal_system_sleep(10);
    }
}