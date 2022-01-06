#if !defined(__UTILS_H__)
#define __UTILS_H__

#include <freertos/queue.h>
#include <freertos/task.h>

void claimSPI(const char* p);
void releaseSPI();

extern SemaphoreHandle_t SPIsem;


#endif  // __UTILS_H__