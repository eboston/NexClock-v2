#include <Arduino.h>
#include <SPI.h>
#include <ArduinoOTA.h>
#include <LITTLEFS.h>
#include <esp_task_wdt.h>

#include "NexClock2.h"
#include "mp3player.h"
#include "FSUtils.h"
#include "WiFiUtils.h"
#include "Nextion.h"
#include "settings.h"
#include "Clock.h"


void setup() 
{
    Serial.begin(115200);
    delay(500);
    log_w("\n\n\n*** Starting *************************************************************************\n\n");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    ////////////////////////////////////////////////////////
    // Start Nextion display

    if (!NexClockInit())
    {
        log_w("Nextion display initialization failed");
    }

    pRadio_Title.setText("");
    pRadio_STitle.setText("");
    pRadio_Artist.setText("");
    pRadio_StationName.setText("");

    pStartup_Status1.setText("");
    pStartup_Status2.setText("");
    pStartup_Status3.setText("");
    pStartup_Status4.setText("");

    pageStartup.show();

    ////////////////////////////////////////////////////////
    // Start Filesystem and read settings
    SPI.begin();

    if (!StartLITTLEFS())
    {
        log_w("Unable to starrt LITTLEFS");
        while (true) delay(10000);
    }

    //LITTLEFS.remove("/conf");
    //LITTLEFS.remove("/stations");

    //listDir("/");
    //LITTLEFS.remove("/conf");
    //listDir("/");

    if (!InitSetup())
        log_w("Error reading/applying settings");

    //listDir("/");

    dumpSettings();
    dumpRadioStations();

    pStartup_Status1.setText("WiFi Connecting to Oxford");
    while (!connectToWiFi())
        delay(1000);

    pStartup_Status1.setText("Connected to %s", "Oxford - HASP");
    pStartup_Status2.setText("%s", WiFi.localIP().toString().c_str());

    ////////////////////////////////////////////////////////
    // Start Over-The-Air update system
    ArduinoOTA.onStart([]() 
    {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "firmware";
        else // U_SPIFFS
        {
            type = "filesystem";
            StopLITTLEFS();
        }

        vTaskSuspend(xplaytask);

        pageUpload.show();
        pUpload_Message.setText("Start updating %s", type.c_str());
        pUpload_Status.setText("");

        log_w("ArduinoOTA: %s", type);
    });
    ArduinoOTA.onEnd([]() 
    {
        log_w("ArduinoOTA: End");
        pUpload_Status.setText("Update finished");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
    {
        Serial.printf("ArduinoOTA: Progress: %u%%\r", (progress / (total / 100)));
        pUpload_Progress.setValue((progress / (total / 100)));
        esp_task_wdt_reset() ;                                        // Protect against idle cpu
    });
    ArduinoOTA.onError([](ota_error_t error) 
    {
        String smsg;

        smsg = "ArduinoOTA: Error[" + error;
        smsg +=  "]: ";
        if (error == OTA_AUTH_ERROR)    smsg +=  "Auth Failed";
        else if (error == OTA_BEGIN_ERROR)   smsg +=  "Begin Failed";
        else if (error == OTA_CONNECT_ERROR) smsg +=  "Connect Failed";
        else if (error == OTA_RECEIVE_ERROR) smsg +=  "Receive Failed";
        else if (error == OTA_END_ERROR)     smsg +=  "End Failed";

        log_w("%s", smsg.c_str());
        pUpload_Status.setText(smsg.c_str());
        delay(5000);
        ESP.restart();
    });

    ArduinoOTA.begin();

    ////////////////////////////////////////////////////////
    // Start Timekeeping system
    if (!startClock())
    {
        log_w("NTP failed to start");
        while (1)
            yield();
    }

    log_w("%02d:%02d %cm", 
        ClockSettings.b24Hour ? (tmTime.tm_hour + tmTime.tm_isdst) % 24 : ((tmTime.tm_hour + tmTime.tm_isdst) % 12) ? ((tmTime.tm_hour + tmTime.tm_isdst) % 12) : 12, 
        tmTime.tm_min,
        ((tmTime.tm_hour + tmTime.tm_isdst) % 24) >= 12 ? 'p' : 'a');

    log_w("%s %s %d, %4d", chDaysOfWeek[tmTime.tm_wday], chMonth[tmTime.tm_mon], tmTime.tm_mday, tmTime.tm_year);

    pStartup_Status3.setFont(0);
    pStartup_Status3.setText("%s, %s %d%s, %4d, %02d:%02d:%02d%s",
        chDaysOfWeek[tmTime.tm_wday],
        chMonth[tmTime.tm_mon],
        tmTime.tm_mday,
        (tmTime.tm_mday == 1) ? "st" : (tmTime.tm_mday == 2) ? "nd" : (tmTime.tm_mday == 3) ? "rd" : "th",
        tmTime.tm_year,
        ((tmTime.tm_hour + tmTime.tm_isdst) % 12) ? ((tmTime.tm_hour + tmTime.tm_isdst) % 12) : 12,
        tmTime.tm_min,
        tmTime.tm_sec,
        (((tmTime.tm_hour + tmTime.tm_isdst) % 24) >= 12) ? "pm" : "am");

    displayTime();
    displayDate();

    
    ////////////////////////////////////////////////////////
    // Start MP3 player
    pStartup_Status4.setText("Starting radio");
    mp3Start();

    pageClock.show();

    // Connect to the station to start the data stream
    playRadio(ClockSettings.bRadioOn, false);

    pStartup_Status1.setText("");
    pStartup_Status2.setText("");
    pStartup_Status3.setText("");
    pStartup_Status4.setText("");

    log_w("Ready");
}


void loop() 
{
    static unsigned long prevMillis10S = millis();


    ArduinoOTA.handle();
    
    mp3loop();

    // Wait for alarm tick (set to 1hz. in setup()).  Keep looping until we get the signal.
    if(xSemaphoreTake(timerLoopSemaphore, ( TickType_t ) 1) == pdTRUE)
    {
        // Update time.
        UpdateTime(&tmTime);
    }

    vTaskDelay(1);

    if (millis() - prevMillis10S > 10000)
    {
        prevMillis10S = millis();
        if (saveSettings)
        {
            writeSettings();
            saveSettings = false;
        }
    }
}

