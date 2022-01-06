#if !defined(__SETTINGS_H__)
#define __SETTINGS_H__

typedef struct Settings_struct
{
    bool        b24Hour;
    bool        bFlashColon;
    uint8_t     lastRadioStation;
    bool        bRadioOn;
    char        szSSID[16];
    char        szPWD[16];
    uint8_t     volumeLevel;
    bool        bMetadata;
} Settings;


// host = "us2.internet-radio.com:8075/"
// name = "Classic Hits Global"

typedef struct radioStationHost_struct
{
	char friendlyName[64];
	char host[128];
	uint8_t useMetaData;
} radioStationHost;


class NexCrop;


// Setup values
extern std::vector<radioStationHost> radioStations;
extern bool  b24Hour;
extern bool  bFlashColon;
extern Settings ClockSettings;
extern NexCrop pSetup_q0;
extern bool saveSettings;

void listDir(const char * dirname);
bool StartLITTLEFS();

bool InitSetup();

bool writeSettings();
bool readSettings();
void dumpSettings();

bool writeRadioStations();
bool readRadioStations();
void dumpRadioStations();


void ApplySettingToPage();

void pSetup_FlashColon_PushCallback(void *ptr);
void pSetup_24Hour_PushCallback(void *ptr);
void pSetup_Metadata_PushCallback(void *ptr);


#endif  // __SETTINGS_H__
