#pragma once

#include "driver/gpio.h"

/* I2C */
#define I2C_SCL            (GPIO_NUM_7)
#define I2C_SDA            (GPIO_NUM_8)

#define I2C_NUM            (0)

#define I2S_SCLK           (GPIO_NUM_20)
#define I2S_MCLK           (GPIO_NUM_19)
#define I2S_LCLK           (GPIO_NUM_22)
#define I2S_DOUT           (GPIO_NUM_23)
#define I2S_DSIN           (GPIO_NUM_21)
#define POWER_AMP_IO       (GPIO_NUM_6)

/* Display */
#define LCD_CS             (GPIO_NUM_5)
#define LCD_PCLK           (GPIO_NUM_0)
#define LCD_DATA0          (GPIO_NUM_1)
#define LCD_DATA1          (GPIO_NUM_2)
#define LCD_DATA2          (GPIO_NUM_3)
#define LCD_DATA3          (GPIO_NUM_4)

#define LCD_BACKLIGHT      (GPIO_NUM_NC)
#define LCD_RST            (GPIO_NUM_11)
#define LCD_TOUCH_RST      (GPIO_NUM_10)
#define LCD_TOUCH_INT      (GPIO_NUM_15)

#define LCD_SPI_NUM        (SPI2_HOST)

/* Buttons */
#define BTN_BOOT           (GPIO_NUM_9)
#define BTN_POWER          (GPIO_NUM_18)