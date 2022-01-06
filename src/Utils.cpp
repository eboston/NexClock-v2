#include <Arduino.h>

#include "Utils.h"


SemaphoreHandle_t SPIsem = NULL ;                        // For exclusive SPI usage



//**************************************************************************************************
//                                      C L A I M S P I                                            *
//**************************************************************************************************
// Claim the SPI bus.  Uses FreeRTOS semaphores.                                                   *
// If the semaphore cannot be claimed within the time-out period, the function continues without   *
// claiming the semaphore.  This is incorrect but allows debugging.                                *
//**************************************************************************************************
void claimSPI(const char* p)
{
    const              TickType_t ctry = 10 ;                 // Time to wait for semaphore
    uint32_t           count = 0 ;                            // Wait time in ticks
    static const char* old_id = "none" ;                      // ID that holds the bus

    while (xSemaphoreTake(SPIsem, ctry) != pdTRUE)            // Claim SPI bus
    {
        if ( count++ > 25 )
        {
            log_w("SPI semaphore not taken within %d ticks by CPU %d, id %s", count * ctry, xPortGetCoreID(), p);
            log_w("Semaphore is claimed by %s", old_id);
        }
        if (count >= 100)
        {
            return ;                                               // Continue without semaphore
        }
    }
    old_id = p ;                                               // Remember ID holding the semaphore
}


//**************************************************************************************************
//                                   R E L E A S E S P I                                           *
//**************************************************************************************************
// Free the the SPI bus.  Uses FreeRTOS semaphores.                                                *
//**************************************************************************************************
void releaseSPI()
{
  xSemaphoreGive(SPIsem) ;                            // Release SPI bus
}
