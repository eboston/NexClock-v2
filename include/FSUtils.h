#if !defined(__FSUTILS_H__)
#define __FSUTILS_H__

bool StartLITTLEFS();
void StopLITTLEFS();
void listDir(const char * dirname);

extern bool bFSStarted;


#endif  // __FSUTILS_H__