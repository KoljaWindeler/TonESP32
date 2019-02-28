/* Control with a touch pad playing MP3 files from SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_touch.h"
#include "audio_hal.h"
#include "board.h"

#include "freertos/queue.h"
#include "driver/gpio.h"

#define GPIO_INPUT_IO_0     36
#define GPIO_INPUT_IO_1     13
#define GPIO_INPUT_IO_2     19
#define GPIO_INPUT_IO_3     23
#define GPIO_INPUT_IO_4     18
#define GPIO_INPUT_IO_5     5
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1) | (1ULL<<GPIO_INPUT_IO_2) | (1ULL<<GPIO_INPUT_IO_3) | (1ULL<<GPIO_INPUT_IO_4) | (1ULL<<GPIO_INPUT_IO_5))
#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static const char *TAG = "SDCARD_MP3_CONTROL_EXAMPLE";

static const char *mp3_file[] = {
    "/sdcard/test.mp3",
    "/sdcard/test1.mp3",
    "/sdcard/test2.mp3",
};
// more files may be added and `MP3_FILE_COUNT` will reflect the actual count
#define MP3_FILE_COUNT sizeof(mp3_file)/sizeof(char*)


#define CURRENT 0
#define NEXT    1

static FILE *get_file(int next_file)
{
    static FILE *file;
    static int file_index = 0;

    if (next_file != CURRENT) {
        // advance to the next file
        if (++file_index > MP3_FILE_COUNT - 1) {
            file_index = 0;
        }
        if (file != NULL) {
            fclose(file);
            file = NULL;
        }
        ESP_LOGI(TAG, "[ * ] File index %d", file_index);
    }
    // return a handle to the current file
    if (file == NULL) {
        file = fopen(mp3_file[file_index], "r");
        if (!file) {
            ESP_LOGE(TAG, "Error opening file");
            return NULL;
        }
    }
    return file;
}

/*
 * Callback function to feed audio data stream from sdcard to mp3 decoder element
 */
static int my_sdcard_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int read_len = fread(buf, 1, len, get_file(CURRENT));
    if (read_len == 0) {
        read_len = AEL_IO_DONE;
    }
    return read_len;
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

void app_main(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_writer, mp3_decoder;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
/////////////////////
		ESP_LOGI(TAG, "[1.3] Initialize Touch peripheral");
		gpio_config_t io_conf;
		//interrupt of rising edge
		io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
		//bit mask of the pins, use GPIO4/5 here
		io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
		//set as input mode
		io_conf.mode = GPIO_MODE_INPUT;
		//enable pull-up mode
		io_conf.pull_up_en = 1;
		gpio_config(&io_conf);
		//create a queue to handle gpio event from isr
		gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
		//install gpio isr service
		ESP_LOGI(TAG, "[1.3] install isr gpio");
		gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
		ESP_LOGI(TAG, "[1.3] install isr gpio");
		//hook isr handler for specific gpio pin
		gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
		gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);
		gpio_isr_handler_add(GPIO_INPUT_IO_2, gpio_isr_handler, (void*) GPIO_INPUT_IO_2);
		gpio_isr_handler_add(GPIO_INPUT_IO_3, gpio_isr_handler, (void*) GPIO_INPUT_IO_3);
		gpio_isr_handler_add(GPIO_INPUT_IO_4, gpio_isr_handler, (void*) GPIO_INPUT_IO_4);
		gpio_isr_handler_add(GPIO_INPUT_IO_5, gpio_isr_handler, (void*) GPIO_INPUT_IO_5);

		//xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);
		/*int cnt = 0;
		while(1) {
				printf("cnt: %d\n", cnt++);
				vTaskDelay(1000 / portTICK_RATE_MS);
				//gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
				//gpio_set_level(GPIO_OUTPUT_IO_1, cnt % 2);
		}*/

/////////////////////
    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Start SD card peripheral");
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = SD_CARD_INTR_GPIO, //GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    ESP_LOGI(TAG, "[1.2] Start SD card peripheral");
    esp_periph_start(sdcard_handle);

    // Wait until sdcard was mounted
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    /*periph_touch_cfg_t touch_cfg = {
        .touch_mask = TOUCH_PAD_SEL9 | TOUCH_PAD_SEL8 | TOUCH_PAD_SEL7 | TOUCH_PAD_SEL4,
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_periph = periph_touch_init(&touch_cfg);

    ESP_LOGI(TAG, "[1.4] Start touch peripheral");
    esp_periph_start(touch_periph);
		*/

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    //audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
		audio_hal_codec_config_t audio_hal_codec_cfg = AUDIO_HAL_AC101_DEFAULT();
    audio_hal_codec_cfg.i2s_iface.samples = AUDIO_HAL_44K_SAMPLES;
    audio_hal_handle_t hal = audio_hal_init(&audio_hal_codec_cfg, 0);
    audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    int player_volume;
    audio_hal_get_volume(hal, &player_volume);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.2] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);
    audio_element_set_read_cb(mp3_decoder, my_sdcard_read_cb, NULL);

    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[3.4] Link it together [my_sdcard_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline, (const char *[]) {"mp3", "i2s"}, 2);

    ESP_LOGI(TAG, "[4.0] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listen for all pipeline events");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_get_event_iface(), evt);

    ESP_LOGW(TAG, "[ 5 ] Tap touch buttons to control music player:");
    ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] next song.");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");

		uint32_t io_num;
    while (1) {
        /* Handle event interface messages from pipeline
           to set music info and to advance to the next song
        */
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, 0);
        if (ret == ESP_OK) {
	        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {
	            // Set music info for a new song to be played
	            if (msg.source == (void *) mp3_decoder
	                && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
	                audio_element_info_t music_info = {0};
	                audio_element_getinfo(mp3_decoder, &music_info);
	                ESP_LOGI(TAG, "[ * ] Received music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
	                         music_info.sample_rates, music_info.bits, music_info.channels);
	                audio_element_setinfo(i2s_stream_writer, &music_info);
	                i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
	                continue;
	            }
	            // Advance to the next song when previous finishes
	            if (msg.source == (void *) i2s_stream_writer
	                && msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
	                audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
	                if (el_state == AEL_STATE_FINISHED) {
	                    ESP_LOGI(TAG, "[ * ] Finished, advancing to the next song");
	                    audio_pipeline_stop(pipeline);
	                    audio_pipeline_wait_for_stop(pipeline);
	                    get_file(NEXT);
	                    audio_pipeline_run(pipeline);
	                }
	                continue;
	            }
	        }
				}

				if(xQueueReceive(gpio_evt_queue, &io_num, 0)) {
						printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
				}
        /* Handle touch pad events
           to start, pause, resume, finish current song and adjust volume
        */
        /*if (msg.source_type == PERIPH_ID_TOUCH
            && msg.cmd == PERIPH_TOUCH_TAP
            && msg.source == (void *)touch_periph) {
            if ((int) msg.data == TOUCH_PLAY) {
                ESP_LOGI(TAG, "[ * ] [Play] touch tap event");
                audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
                switch (el_state) {
                    case AEL_STATE_INIT :
                        ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
                        audio_pipeline_run(pipeline);
                        break;
                    case AEL_STATE_RUNNING :
                        ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
                        audio_pipeline_pause(pipeline);
                        break;
                    case AEL_STATE_PAUSED :
                        ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
                        audio_pipeline_resume(pipeline);
                        break;
                    default :
                        ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
                }
            } else if ((int) msg.data == TOUCH_SET) {
                ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
                audio_pipeline_terminate(pipeline);
                ESP_LOGI(TAG, "[ * ] Stopped, advancing to the next song");
                get_file(NEXT);
                audio_pipeline_run(pipeline);
            } else if ((int) msg.data == TOUCH_VOLUP) {
                ESP_LOGI(TAG, "[ * ] [Vol+] touch tap event");
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                audio_hal_set_volume(hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            } else if ((int) msg.data == TOUCH_VOLDWN) {
                ESP_LOGI(TAG, "[ * ] [Vol-] touch tap event");
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                audio_hal_set_volume(hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
            }
        }*/
    }

    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, mp3_decoder);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_stop_all();
    audio_event_iface_remove_listener(esp_periph_get_event_iface(), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);
    esp_periph_destroy();
}
