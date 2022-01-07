#include <Arduino.h>

#include "Nextion.h"
#include "settings.h"

NexPage pageStations = NexPage(8, 0, "Stations");

NexObject pStations_ID1  = NexObject(&pageStations,  2, "tID1");
NexObject pStations_ID2  = NexObject(&pageStations,  3, "tID2");
NexObject pStations_ID3  = NexObject(&pageStations,  4, "tID3");
NexObject pStations_ID4  = NexObject(&pageStations,  5, "tID4");
NexObject pStations_ID5  = NexObject(&pageStations,  6, "tID5");
NexObject pStations_ID6  = NexObject(&pageStations,  7, "tID6");
NexObject pStations_Sta1 = NexObject(&pageStations,  8, "bSta1");
NexObject pStations_Sta2 = NexObject(&pageStations,  9, "bSta2");
NexObject pStations_Sta3 = NexObject(&pageStations, 10, "bSta3");
NexObject pStations_Sta4 = NexObject(&pageStations, 11, "bSta4");
NexObject pStations_Sta5 = NexObject(&pageStations, 12, "bSta5");
NexObject pStations_Sta6 = NexObject(&pageStations, 13, "bSta6");
NexObject pStations_Prev = NexObject(&pageStations, 14, "bPrev");
NexObject pStations_Next = NexObject(&pageStations, 15, "bNext");



void pStations_StaPopCallback(void *ptr)
{
    uint32_t index = reinterpret_cast<uint32_t>(ptr);

    log_w("Edit station at index %d", index);
}


void StationsInit()
{
    pStations_Sta1.attachPop(pStations_StaPopCallback, (void*) 1);
    pStations_Sta2.attachPop(pStations_StaPopCallback, (void*) 2);
    pStations_Sta3.attachPop(pStations_StaPopCallback, (void*) 3);
    pStations_Sta4.attachPop(pStations_StaPopCallback, (void*) 4);
    pStations_Sta5.attachPop(pStations_StaPopCallback, (void*) 5);
    pStations_Sta6.attachPop(pStations_StaPopCallback, (void*) 6);

}