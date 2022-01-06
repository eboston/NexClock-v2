#if !defined(__MP3PLAYER_H__)
#define __MP3PLAYER_H__


void mp3Start();
void mp3loop();
bool connectToStation(uint8_t stationNumber);
void playRadio(bool state, bool saveChange = true);

//extern String            host ;

extern TaskHandle_t      xplaytask ;                            // Task handle for playtask

#endif  // __MP3PLAYER_H__