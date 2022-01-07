#if !defined(__MP3PLAYER_H__)
#define __MP3PLAYER_H__


void mp3Start();
void mp3loop();
bool connectToStation(uint8_t stationNumber);
void playRadio(bool state, bool saveChange = true);
void showStreamTitle();

//extern String            host ;

extern TaskHandle_t      xplaytask ;                            // Task handle for playtask

extern NexPage pageRadio;
extern NexObject pRadio_Time;
extern NexObject pRadio_Title;
extern NexObject pRadio_STitle;
extern NexObject pRadio_Artist;
extern NexObject pRadio_StationName;    


#endif  // __MP3PLAYER_H__