#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

// -- Components
#include "dht.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

// -- OLED GPIO maping
#define OLED_SDA_GPIO 8
#define OLED_SCL_GPIO 9
#define OLED_RESET_GPIO -1

// -- Buttons
#define BT_RIGHT 5
#define BT_LEFT 6
#define BT_UP 7
#define BT_DOWN 4
#define BT_CONFIRM 2
#define BT_BACK 3

// Display positions
typedef struct {
    int8_t center_line;
    int8_t top;
    int8_t bottom;
    int8_t header;
    int8_t center_column;
    int8_t down;
} displayPosition_t;

// Set points: temperature, humidity, luminosity
typedef struct {
    int8_t temp;
    uint8_t  humid;
    uint8_t lumin;
} setPoint_t;

// -- DHT11 configure
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT11;
static const gpio_num_t dht_gpio = 15;

// -- TAG's
static const char *tag = "DHT11: ";

// -- Auxiliar global variable to write in oled
char linechar[30] = {0};

// -- Message start display
void startDisplay(displayPosition_t position, SSD1306_t oled ) {

    // -- Horizontal Scroll
	ssd1306_clear_screen(&oled, false);
	ssd1306_contrast(&oled, 0xff);
	ssd1306_display_text(&oled, position.center_line, "Monitor: Estufa", 16, false);
	ssd1306_hardware_scroll(&oled, SCROLL_RIGHT);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(&oled, SCROLL_STOP);
}

void configureButtons(const gpio_num_t *pins, size_t count) {

    gpio_config_t io_config = {};
    for (size_t i = 0; i < count; i++) {
        io_config.pin_bit_mask |= (1ULL << pins[i]);
    }

    io_config.mode = GPIO_MODE_INPUT;
    io_config.pull_up_en = GPIO_PULLUP_ENABLE;
    io_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_config.intr_type = GPIO_INTR_NEGEDGE; //tipo de interrpução - negativa
    
    gpio_config(&io_config);
}

// -- Set temperature, humidity, luminosity points
void setPoints(displayPosition_t position, SSD1306_t oled) {
    setPoint_t set_point = {
        .temp = 27,
        .humid = 80,
        .lumin = 80,
    };

    ssd1306_clear_screen(&oled, false);
	ssd1306_contrast(&oled, 0xff);
	ssd1306_display_text(&oled, position.top, " - SetPoints - ", 16, false);

    // show setpoints
    sprintf(linechar, "Temp: %d C", set_point.temp);
    ssd1306_display_text(&oled, position.center_line + 1, linechar, sizeof(linechar),false);
    sprintf(linechar, "Humi: %d %%", set_point.humid);
    ssd1306_display_text(&oled, position.center_line + 2, linechar, sizeof(linechar),false);
    sprintf(linechar, "Lumin: %d cd", set_point.lumin);
    ssd1306_display_text(&oled, position.center_line + 3, linechar, sizeof(linechar),false);

    // configure setpoints

}


void app_main(void)
{
    // -- Auxiliar variables
    int16_t temperature = 0;
    int16_t humidity = 0;

    // -- Display positions
    #if CONFIG_SSD1306_128x64
    displayPosition_t posit = {
        .header = 0,
        .top = 2,
        .center_line = 3,
        .down = 7,
        .bottom = 8,
        .center_column = 55,
    };
    #endif 

    // -- Configure control buttons
    const gpio_num_t buttons[] = {
    BT_RIGHT, BT_LEFT, BT_UP, BT_DOWN, BT_CONFIRM, BT_BACK
    };
    configure_buttons(buttons, sizeof(buttons) / sizeof(buttons[0]));

    // -- Initialize the I2C master driver
    SSD1306_t oled;
    i2c_master_init(&oled,OLED_SDA_GPIO,OLED_SCL_GPIO,OLED_RESET_GPIO);     

    // -- Initializate OLED
    ssd1306_init(&oled, 128, 64);
    ssd1306_clear_screen(&oled, false);
    
    // -- Logging OLED
    ESP_LOGI(tag, "Panel is 128x64");                   
    ESP_LOGI(tag, "INTERFACE is i2c");
    ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",OLED_SDA_GPIO);
    ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",OLED_SCL_GPIO);
    ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",OLED_RESET_GPIO);

    // -- Start Monitor Estufa
    startDisplay(posit, oled);

    // -- SetPoints
	setPoints(posit, oled);

    // -- Footer
    ssd1306_display_text(&oled, posit.down, "   (Monitor)   ", 16, false);

    int8_t count = 1;

    while(true)
    {
        // check if the dht11 sensor is ok
        if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
        {
            sprintf(linechar, "     - %d -     ", count);
            ssd1306_display_text(&oled, posit.header, linechar, 16, false);

            ESP_LOGI(tag, "Temperature %d C", temperature/10);
            ESP_LOGI(tag, "Humidity %d %%", humidity/10);
            
            /*memset(linechar, 0, sizeof(linechar));*/
            sprintf(linechar, "Temp: %d C", temperature/10);
            ssd1306_display_text(&oled, posit.center_line, linechar, sizeof(linechar),false);
            sprintf(linechar, "Humi: %d %%", humidity/10);
            ssd1306_display_text(&oled, posit.center_line + 1, linechar, sizeof(linechar),false);
        }
        else
        {
            ESP_LOGE(tag, "Could not possible read data from sensor");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
        count++;
    }
}


