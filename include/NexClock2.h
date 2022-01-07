#if !defined(__NEXCLOCK2_H__)
#define __NEXCLOCK2_H__


void claimSPI(const char* p);
void releaseSPI();

extern NexObject pStartup_Status3;
extern NexObject pStartup_Spinner;

#endif  // __NEXCLOCK2_H__