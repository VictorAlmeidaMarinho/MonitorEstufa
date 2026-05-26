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

// Set points: temperature, humidity, luminosity
#define SP_TEMP_DEFAULT 27
#define SP_HUMID_DEFAULT 80
#define SP_LUMIN_DEFAULT 80

// -- Index to temperature, humidity and luminosity
#define INDX_MIN 0
#define INDX_MAX 2

const gpio_num_t buttons[] = {BT_RIGHT, BT_LEFT, BT_UP, BT_DOWN, BT_CONFIRM, BT_BACK};

// Display positions
typedef struct {
    int8_t center_line;
    int8_t top;
    int8_t bottom;
    int8_t header;
    int8_t center_column;
    int8_t down;
} displayPosition_t;

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

typedef struct {
    int8_t temp;
    uint8_t  humid;
    uint8_t lumin;
} setPoint_t;

setPoint_t set_point = {
    .temp = SP_TEMP_DEFAULT,
    .humid = SP_HUMID_DEFAULT,
    .lumin = SP_LUMIN_DEFAULT,
};

// -- DHT11 configure
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT11;
static const gpio_num_t dht_gpio = 15;


// -- TAG's
static const char *tag = "DHT11: ";
static const char *TAG = "BUTTON: ";


// -- Auxiliar global variables 
char linechar[30] = {0}; // write in oled
SSD1306_t oled; // code in oled
bool decide_sets; // set points


// -- Queue create - NULL address
static QueueHandle_t gpio_evt_queue = NULL;

// -- ISR handler
static void IRAM_ATTR gpio_isr_handler(void* arg){
    uint32_t gpio_num = (uint32_t) arg; // converte o numero do pino para numero
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL); // envia o gpio_num a fila
}

// -- Functions
void taskSetPoints(void *pvparameters);
void hardwareInit(void);
void initializateOled(void);
void configureButtons(const gpio_num_t *pins, size_t count);
void configureLeds(void);
void displayStartScreen(void);
void runSetPoints(void);
void displaySetPoints(void);
void displayMonitor(int16_t temperature, int16_t humidiy);


// -- Button task for set points
void taskSetPoints(void* pvparameters){

    int index = 0;

    uint32_t gpio_num;
    TickType_t last_button_press = 0;

    while(1)
    {
        xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY); 
        ESP_LOGI(TAG,"GPIO[%li] intr \n",gpio_num);

        TickType_t current_time = xTaskGetTickCount();

        // diferenca entre o tempo em que o botao foi apertado
        if(current_time - last_button_press >= pdMS_TO_TICKS(250))
        {
            last_button_press = current_time;

            switch (gpio_num)
            {
                case BT_LEFT:
                    switch(index) 
                    {
                        case 0: set_point.temp--; break;
                        case 1: set_point.humid--; break;
                        case 2: set_point.lumin--; break;
                    }
                    break;
                case BT_RIGHT:                   
                    switch(index) 
                    {
                        case 0: set_point.temp++; break;
                        case 1: set_point.humid++; break;
                        case 2: set_point.lumin++; break;
                    }
                    break;

                case BT_DOWN:                   
                    index = (index < INDX_MAX) ? index + 1 : INDX_MIN;
                    break;
                case BT_UP:
                    index = (index > INDX_MIN) ? index - 1 : INDX_MAX;
                    break;
                case BT_CONFIRM:                   
                    decide_sets = true;
                    break;
            }
        }
    }
}

void hardwareInit(void) {

    // -- Configure control buttons
    configureButtons(buttons, sizeof(buttons) / sizeof(buttons[0]));

    // -- Initializate I2C and Oled Logging
    initializateOled();
}

void initializateOled(void) {

    // -- Initialize the I2C master driver
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
}

void configureButtons(const gpio_num_t *pins, size_t count) {

    gpio_config_t io_config = {};
    for (size_t i = 0; i < count; i++) {
        io_config.pin_bit_mask |= (1ULL << pins[i]);
    }

    io_config.mode = GPIO_MODE_INPUT;
    io_config.pull_up_en = GPIO_PULLUP_ENABLE;
    io_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_config.intr_type = GPIO_INTR_NEGEDGE;
    
    gpio_config(&io_config);
}


// -- Message start display
void displayStartScreen() {

    // -- Horizontal Scroll
	ssd1306_clear_screen(&oled, false);
	ssd1306_contrast(&oled, 0xff);
	ssd1306_display_text(&oled, posit.center_line, "Monitor: Estufa", 16, false);
	ssd1306_hardware_scroll(&oled, SCROLL_RIGHT);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	ssd1306_hardware_scroll(&oled, SCROLL_STOP);

    ssd1306_clear_screen(&oled, false);

}

void displaySetPoints (void) {
	ssd1306_contrast(&oled, 0xff);
	ssd1306_display_text(&oled, posit.top, " - SetPoints - ", 16, false);

    // show setpoints
    sprintf(linechar, "Temp: %d C", set_point.temp);
    ssd1306_display_text(&oled, posit.center_line + 1, linechar, sizeof(linechar),false);
    sprintf(linechar, "Humi: %d %%", set_point.humid);
    ssd1306_display_text(&oled, posit.center_line + 2, linechar, sizeof(linechar),false);
    sprintf(linechar, "Lumin: %d cd", set_point.lumin);
    ssd1306_display_text(&oled, posit.center_line + 3, linechar, sizeof(linechar),false);
}

void displayMonitor(int16_t temperature, int16_t humidity) {
    ESP_LOGI(tag, "Temperature %d C", temperature/10);
    ESP_LOGI(tag, "Humidity %d %%", humidity/10);
    
    ssd1306_display_text(&oled, posit.down, "   (Monitor)   ", 16, false);

    memset(linechar, 0, sizeof(linechar));
    sprintf(linechar, "Temp: %d C", temperature/10);
    ssd1306_display_text(&oled, posit.center_line, linechar, sizeof(linechar),false);
    sprintf(linechar, "Humid: %d %%", humidity/10);
    ssd1306_display_text(&oled, posit.center_line + 1, linechar, sizeof(linechar),false);
}


// -- Set temperature, humidity, luminosity points
void runSetPoints(void) {
    decide_sets = false;

    // -- Queue and Tasks
    gpio_evt_queue = xQueueCreate(1,sizeof(uint32_t));
    xTaskCreate(taskSetPoints,"taskSetPoints",2048,NULL, 2, NULL);

    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(BT_LEFT,gpio_isr_handler,(void*) BT_LEFT);
    gpio_isr_handler_add(BT_RIGHT,gpio_isr_handler,(void*) BT_RIGHT);
    gpio_isr_handler_add(BT_UP,gpio_isr_handler,(void*) BT_UP);
    gpio_isr_handler_add(BT_DOWN,gpio_isr_handler,(void*) BT_DOWN);
    gpio_isr_handler_add(BT_CONFIRM,gpio_isr_handler,(void*) BT_CONFIRM);

    while(!decide_sets) {
        displaySetPoints();
    }

    // -- clear ISR
    gpio_isr_handler_remove(BT_LEFT);
    gpio_isr_handler_remove(BT_RIGHT);
    gpio_isr_handler_remove(BT_UP);
    gpio_isr_handler_remove(BT_DOWN);
    gpio_isr_handler_remove(BT_CONFIRM);
}


void app_main(void)
{
    // -- Auxiliar variables
    int16_t temperature = 0;
    int16_t humidity = 0;

    // -- buttons + OLED + I2C initialization
    hardwareInit();

    // -- Start "Monitor: Estufa"
    displayStartScreen();

    // -- SetPoints
	runSetPoints();

    // -- Clear to show in real time monitor sensor
    ssd1306_clear_screen(&oled, false);

    while (true)
    {
        // check if the dht11 sensor is ok
        if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK) {
           displayMonitor(temperature, humidity);
        }
        else {
            ESP_LOGE(tag, "Could not possible read datas from sensor");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


