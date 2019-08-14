/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mbed_events.h>
#include <rtos.h>
#include "mbed.h"
#include "ble/BLE.h"
#include "ble/services/HealthThermometerService.h"
#include <Adafruit_ST7735.h>
#include "rtos.h"
#include "lvgl.h"

DigitalOut led1(LED1, 1);

const static char     DEVICE_NAME[]        = "PIOTherm";
static const uint16_t uuid16_list[]        = {GattService::UUID_HEALTH_THERMOMETER_SERVICE};

static float                     currentTemperature   = 39.6;
static HealthThermometerService *thermometerServicePtr;

static EventQueue eventQueue(/* event count */ 16 * EVENTS_EVENT_SIZE);

/* Restart Advertising on disconnection*/
void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *)
{
    BLE::Instance().gap().startAdvertising();
}

void updateSensorValue(void)
{
    /* Do blocking calls or whatever is necessary for sensor polling.
       In our case, we simply update the Temperature measurement. */
    currentTemperature = (currentTemperature + 0.1 > 43.0) ? 39.6 : currentTemperature + 0.1;
    thermometerServicePtr->updateTemperature(currentTemperature);
}

void periodicCallback(void)
{
    led1 = !led1; /* Do blinky on LED1 while we're waiting for BLE events */

    if (BLE::Instance().gap().getState().connected) {
        eventQueue.call(updateSensorValue);
    }
}

void onBleInitError(BLE &ble, ble_error_t error)
{
    /* Initialization error handling should go here */
}

void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE        &ble   = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        onBleInitError(ble, error);
        return;
    }

    if (ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        return;
    }

    ble.gap().onDisconnection(disconnectionCallback);

    /* Setup primary service. */
    thermometerServicePtr = new HealthThermometerService(ble, currentTemperature, HealthThermometerService::LOCATION_EAR);

    /* setup advertising */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::THERMOMETER_EAR);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(1000); /* 1000ms */
    ble.gap().startAdvertising();
}

void scheduleBleEventsProcessing(BLE::OnEventsToProcessCallbackContext *context)
{
    BLE &ble = BLE::Instance();
    eventQueue.call(Callback<void()>(&ble, &BLE::processEvents));
}
//!  ***********************************************

#define SPI_DC      p15
#define TFT_RSET    p16
#define TFT_BL      p17
// SPI device(SPI_PSELMOSI1, SPI_PSELMISO1, SPI_PSELSCK1);
// DigitalOut chip_select(SPI_PSELSS1);
// DigitalOut dc_select(SPI_DC);
// DigitalOut tft_reset(TFT_RSET);

Adafruit_ST7735 tft(SPI_PSELMOSI1, SPI_PSELMISO1, SPI_PSELSCK1, SPI_PSELSS1, SPI_DC, TFT_RSET);
Serial pc(USBTX, USBRX, "NRF", 9600); // tx, rx



static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) ;
    // pc.printf("xs:%d ys:%d xe:%d ye:%d\r\n", area->x1, area->y1, area->x2, area->y2);
    tft.setAddrWindow(area->x1, area->y1, area->x2, area->y2);
    tft.pushColors(( uint16_t *)color_p, size);
    lv_disp_flush_ready(disp_drv);
}


void setupLvgl()
{
    lv_init();

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    static lv_disp_buf_t disp_buf;
    static lv_color_t buf1[LV_HOR_RES_MAX * 10];                        /*A buffer for 10 rows*/
    static lv_color_t buf2[LV_HOR_RES_MAX * 10];                        /*An other buffer for 10 rows*/
    lv_disp_buf_init(&disp_buf, buf1, buf2, LV_HOR_RES_MAX * 10);   /*Initialize the display buffer*/

    disp_drv.hor_res = 135;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = disp_flush;
    /*Set a display buffer*/
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);
}

lv_obj_t *text ;

void lv_tick_handler()
{
    lv_tick_inc(5);
}

void runtask()
{
    // lv_label_set_text(text, "T-Watch");
}

int main()
{
    // eventQueue.call_every(500, periodicCallback);

    // BLE &ble = BLE::Instance();
    // ble.onEventsToProcess(scheduleBleEventsProcessing);
    // ble.init(bleInitComplete);

    // eventQueue.dispatch_forever();

    tft.initST7789();

    setupLvgl();

    text = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(text, "T-Watch");
    lv_obj_align(text, NULL, LV_ALIGN_CENTER, 0, 0);

    Ticker tick;
    tick.attach_us(callback(lv_tick_handler), 5000);

    Ticker tick1;
    tick1.attach_us(callback(runtask), 5000000);

    while (1) {
        lv_task_handler();
        wait_ms(5);
    }

    for (;;) {
        pc.printf("R\n\r");
        tft.fillScreen(TFT_RED);
        wait(1);
        pc.printf("G\n\r");
        tft.fillScreen(TFT_GREEN);
        wait(1);
        pc.printf("B\n\r");
        tft.fillScreen(TFT_BLUE);
        wait(1);
    }
    return 0;
}
