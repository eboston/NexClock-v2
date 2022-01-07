#include <Arduino.h>
#include <FS.h>
#include <LITTLEFS.h>

#include <vector>

#include "Nextion.h"
#include "settings.h"
#include "FSUtils.h"
#include "mp3player.h"
#include "Clock.h"

#define CONFIG_FILE     "/conf"
#define STATIONS_FILE   "/stations"


NexPage pageSetup    = NexPage(4, 0, "Setup");
NexObject pSetup_qFlashColon = NexObject(&pageSetup,  1, "qFlashColon");
NexObject pSetup_q24Hour     = NexObject(&pageSetup,  3, "q24Hour");
NexObject pSetup_qMetaData   = NexObject(&pageSetup,  8, "qMetaData");

bool saveSettings = false;

// Default setup values
Settings ClockSettings =
{
    false,          // 24 Hour
    true,           // Flash Colon
    2,              // Last Radio Station
    false,          // Radio on
    "Oxford-HASP",  // SSID
    "HermanHASP",   // Password
    80,             // Volume Level
    true,           // Metadata
};

radioStationHost defaultRadioStations[] =
{
	"Classic Rock Florida", "us4.internet-radio.com:8258/", true,
	"Beatles 128k", "listen.181fm.com:80/181-beatles_128k.mp3", true,
	"Classic Hits Global", "us2.internet-radio.com:8075/", true,
};

//    "KOOL 96.1", "ice42.securenetsystems.net:443/KCWD", true,


std::vector<radioStationHost> radioStations;


void dumpSettings()
{
    log_w("Settings:");
    log_w("       24 Hour = %d", ClockSettings.b24Hour);
    log_w("   Flash Colon = %d", ClockSettings.bFlashColon);
    log_w("  Last Station = %d", ClockSettings.lastRadioStation);
    log_w("      Radio On = %d", ClockSettings.bRadioOn);
    log_w("          SSID = '%s'", ClockSettings.szSSID);
    log_w("        Volume = %d", ClockSettings.volumeLevel);
    log_w("      Metadata = %d", ClockSettings.bMetadata);
}


bool writeSettings()
{
    if (!bFSStarted)
    {
        log_w("File System has not been started yet.");
        return false;
    }

    File f = LITTLEFS.open(CONFIG_FILE, "w");
    if (!f) 
    {
        log_w("writeSettings: File open failed!");
        return false;
    } 

    if (f.write((const uint8_t *)&ClockSettings, sizeof(ClockSettings)) != sizeof(ClockSettings))
    {
        log_w("Error writing settings");
        return false;
    }

    return true;
}


bool readSettings()
{
    if (!bFSStarted)
    {
        log_w("File System has not been started yet.");
        return false;
    }

    if (LITTLEFS.exists(CONFIG_FILE) == false) 
    {
        log_w("readSettings: Settings File does not yet exists.");
        return writeSettings();
    }

    File f = LITTLEFS.open(CONFIG_FILE, "r");
    f.readBytes((char *)&ClockSettings, sizeof(ClockSettings));

    return true;
}

//*****************************************************************************************************************************

void dumpRadioStations()
{
    log_w("Stations:");
    log_w("  Number of stations: %d", radioStations.size());

    uint8_t nCnt = 0;
    for (std::vector<radioStationHost>::iterator it = radioStations.begin(); it != radioStations.end(); ++it, ++nCnt)
    {
        log_w("    %d: '%s' '%s' %d", nCnt, ((radioStationHost) *it).friendlyName, ((radioStationHost) *it).host, ((radioStationHost) *it).useMetaData);
    }
    log_w("end of list");
}


bool writeRadioStations()
{
    if (!bFSStarted)
    {
        log_w("File System has not been started yet.");
        return false;
    }

    File f = LITTLEFS.open(STATIONS_FILE, "w");
    if (!f) 
    {
        log_w("File open failed!");
        return false;
    } 

    for (uint8_t i = 0; i < radioStations.size(); ++i)
    {
        if (f.write((const uint8_t *)&radioStations[i], sizeof(radioStationHost)) != sizeof(radioStationHost))
        {
            log_w("Error writing stations");
            LITTLEFS.remove(STATIONS_FILE);
            return false;
        }
    }

    return true;
}


bool readRadioStations()
{
    if (!bFSStarted)
    {
        log_w("File System has not been started yet.");
        return false;
    }

    if (LITTLEFS.exists(STATIONS_FILE) == false) 
    {
        log_w("Stations File does not yet exists.  Creating default list");

        radioStations.clear();

        uint8_t defaultStationCount = sizeof(defaultRadioStations) / sizeof(radioStationHost);
        log_w("number of default stations: %d", defaultStationCount);
        for (uint8_t i = 0; i < defaultStationCount; i++)
            radioStations.push_back(defaultRadioStations[i]);

        log_w("Calling writeStations");
        return writeRadioStations();
    }

    File f = LITTLEFS.open(STATIONS_FILE, "r");
    if (!f)
    {
        log_w("Unable to open stations file");
        return false;
    }

    radioStations.clear();

    while (f.available())
    {
        radioStationHost rsh;

        if (f.readBytes((char *)&rsh, sizeof(radioStationHost)) != sizeof(radioStationHost))
        {
            log_w("Error reading stations file");
            radioStations.clear();
            return false;
        }
        radioStations.push_back(rsh);
    }

    log_w("Number of stations read: %d", radioStations.size());

    return true;
}

//*****************************************************************************************************************************


void ApplySettings()
{
    pSetup_qFlashColon.Set_Pic(ClockSettings.bFlashColon);
    pSetup_q24Hour.Set_Pic(ClockSettings.b24Hour);
    pSetup_qMetaData.Set_Pic(ClockSettings.bMetadata);
}


void pSetup_qFlashColon_PushCallback(void *ptr)
{
  uint32_t number = 0;

  pSetup_qFlashColon.Get_background_crop_picc(&number);

  number += 1;
  number %= 2;

  pSetup_qFlashColon.Set_background_crop_picc(number);

  ClockSettings.bFlashColon = number;
  writeSettings();
}


void pSetup_q24Hour_PushCallback(void *ptr)
{
log_w("pSetup_24Hour_PushCallback");
  uint32_t number = 0;

  pSetup_q24Hour.Get_background_crop_picc(&number);

  number += 1;
  number %= 2;

  pSetup_q24Hour.Set_background_crop_picc(number);

  ClockSettings.b24Hour = number;
  writeSettings();

  displayTime();
}


void pSetup_qMetaData_PushCallback(void *ptr)
{
log_w("pSetup_Metadata_PushCallback");
  uint32_t number = 0;

  pSetup_qMetaData.Get_background_crop_picc(&number);

  number += 1;
  number %= 2;

  pSetup_qMetaData.Set_background_crop_picc(number);

  ClockSettings.bMetadata = number;
  writeSettings();

  if (ClockSettings.bRadioOn)
    showStreamTitle();
}


bool InitSetup()
{

    if (!readSettings())
    {
        log_w("Error reading settings");
        return false;
    }

    if (!readRadioStations())
    {
        log_w("Error reading stations");
        return false;
    }

    ApplySettings();

    pSetup_qFlashColon.attachPush(pSetup_qFlashColon_PushCallback);
    pSetup_q24Hour.attachPush(pSetup_q24Hour_PushCallback);
    pSetup_qMetaData.attachPush(pSetup_qMetaData_PushCallback);

    nexListen.push_back(&pSetup_qFlashColon);
    nexListen.push_back(&pSetup_q24Hour);
    nexListen.push_back(&pSetup_qMetaData);

    return true;
}
