#include <Arduino.h>

#include "Nextion.h"
#include "mp3player.h"

#define nexSerial   Serial2

#define NEX_RET_CMD_FINISHED                (0x01)
#define NEX_RET_EVENT_LAUNCHED              (0x88)
#define NEX_RET_EVENT_UPGRADED              (0x89)
#define NEX_RET_EVENT_TOUCH_HEAD            (0x65)     
#define NEX_RET_EVENT_POSITION_HEAD         (0x67)
#define NEX_RET_EVENT_SLEEP_POSITION_HEAD   (0x68)
#define NEX_RET_CURRENT_PAGE_ID_HEAD        (0x66)
#define NEX_RET_STRING_HEAD                 (0x70)
#define NEX_RET_NUMBER_HEAD                 (0x71)
#define NEX_RET_INVALID_CMD                 (0x00)
#define NEX_RET_INVALID_COMPONENT_ID        (0x02)
#define NEX_RET_INVALID_PAGE_ID             (0x03)
#define NEX_RET_INVALID_PICTURE_ID          (0x04)
#define NEX_RET_INVALID_FONT_ID             (0x05)
#define NEX_RET_INVALID_BAUD                (0x11)
#define NEX_RET_INVALID_VARIABLE            (0x1A)
#define NEX_RET_INVALID_OPERATION           (0x1B)

#define NEX_EVENT_PUSH  (0x01)
#define NEX_EVENT_POP   (0x00)  


NexPage pageStartup = NexPage(0, 0, "Startup");
NexPage pageClock   = NexPage(1, 0, "Clock");
NexPage pageSetup   = NexPage(2, 0, "Setup");
NexPage pageAlarm   = NexPage(3, 0, "Alarm");
NexPage pageUpload  = NexPage(4, 0, "Upload");
NexPage pageRadio   = NexPage(5, 0, "Radio");

// Page 0 - Startup
NexObject pStartup_Status1 = NexObject(&pageStartup, 1, "Status1");
NexObject pStartup_Status2 = NexObject(&pageStartup, 2, "Status2");
NexObject pStartup_Status3 = NexObject(&pageStartup, 4, "Status3");
NexObject pStartup_Status4 = NexObject(&pageStartup, 5, "Status4");
NexObject pStartup_Spinner = NexObject(&pageStartup, 3, "Spinner");

// Page 1 - Clock
NexObject pClock_AM     = NexObject(&pageClock,  1, "AM");
NexObject pClock_PM     = NexObject(&pageClock,  2, "PM");
NexObject pClock_THour  = NexObject(&pageClock,  3, "THour");
NexObject pClock_Hour   = NexObject(&pageClock,  4, "Hour");
NexObject pClock_TMin   = NexObject(&pageClock,  5, "TMin");
NexObject pClock_Min    = NexObject(&pageClock,  6, "Min");
NexObject pClock_Colon  = NexObject(&pageClock,  7, "Colon");
NexObject pClock_Date   = NexObject(&pageClock,  8, "Date");
NexObject pClock_bRadio = NexObject(&pageClock, 13, "bRadio");
NexObject pClock_Title  = NexObject(&pageClock, 15, "Title");

// Page 2 - Setup
NexObject pSetup_qFlashColon = NexObject(&pageSetup,  1, "qFlashColon");
NexObject pSetup_q24Hour     = NexObject(&pageSetup,  3, "q24Hour");
NexObject pSetup_qMetaData   = NexObject(&pageSetup,  8, "qMetaData");


// Page 4 - Upload
NexObject pUpload_Message  = NexObject(&pageUpload, 1, "t0");
NexObject pUpload_Status   = NexObject(&pageUpload, 3, "t1");
NexObject pUpload_Progress = NexObject(&pageUpload, 2, "j0");


// Page 5 - Radio
NexObject pRadio_Artist      = NexObject(&pageRadio,  6, "t1");
NexObject pRadio_StationName = NexObject(&pageRadio,  7, "t2");
NexObject pRadio_QUsage      = NexObject(&pageRadio,  9, "t3");
NexObject pRadio_Time        = NexObject(&pageRadio, 16, "t4");
NexObject pRadio_VolDisplay  = NexObject(&pageRadio, 10, "n0");
NexObject pRadio_STitle      = NexObject(&pageRadio, 17, "g0");
NexObject pRadio_Title       = NexObject(&pageRadio, 18, "t0");

NexObject pRadio_VolDn = NexObject(&pageRadio, 1, "b0");
NexObject pRadio_VolUp = NexObject(&pageRadio, 2, "b1");
NexObject pRadio_StaDn = NexObject(&pageRadio, 3, "b2");
NexObject pRadio_StaUp = NexObject(&pageRadio, 4, "b3");
NexObject pRadio_Play  = NexObject(&pageRadio, 8, "b5");

NexObject pRadio_state  = NexObject(&pageRadio, 11, "state");
NexObject pRadio_vTitle = NexObject(&pageRadio, 18, "Title");

TaskHandle_t NexClockLoopTaskHandle;



NexObject *nex_listen_list[] =
{
  &pRadio_Play,
  &pRadio_VolDn,
  &pRadio_VolUp,
  &pRadio_StaDn,
  &pRadio_StaUp,
  &pClock_bRadio,
  &pSetup_qFlashColon,
  &pSetup_q24Hour,
  &pSetup_qMetaData,
  NULL
};


void NexClockLoopTask(void *parameter)
{
    static uint8_t __buffer[10];

    uint16_t i;
    uint8_t c;  

    while (true)
    {
        while (nexSerial.available() > 0)
        {   
            delay(10);
            c = nexSerial.read();

            switch (c)
            {
            case 0x65:  // Button event
                if (nexSerial.available() >= 6)
                {
                    __buffer[0] = c;  
                    for (i = 1; i < 7; i++) __buffer[i] = nexSerial.read();
                    __buffer[i] = 0x00;

                    if (0xFF == __buffer[4] && 0xFF == __buffer[5] && 0xFF == __buffer[6])
                        NexObject::iterate(nex_listen_list, __buffer[1], __buffer[2], (int32_t)__buffer[3]);
                }

            case 0x66:  // sendme - current page being displayed
                if (nexSerial.available() >= 4)
                {
                    __buffer[0] = c;
                    for (i = 1; i < 5; i++) __buffer[i] = nexSerial.read();
                    __buffer[i] = 0x00;

                    if (0xFF == __buffer[2] && 0xFF == __buffer[3] && 0xFF == __buffer[4])
                    {
                        uint8_t currentPage = __buffer[1];
                        log_w("Current page is %d", currentPage);
                    }
                }
            }
        }

        vTaskDelay(1);
    }
}


/*
 * Receive uint32_t data. 
 * 
 * @param number - save uint32_t data. 
 * @param timeout - set timeout time. 
 *
 * @retval true - success. 
 * @retval false - failed.
 *
 */
bool recvRetNumber(uint32_t *number, uint32_t timeout)
{
    bool ret = false;
    uint8_t temp[8] = {0};

    if (!number)
    {
        goto __return;
    }
    
    nexSerial.setTimeout(timeout);
    if (sizeof(temp) != nexSerial.readBytes((char *)temp, sizeof(temp)))
    {
        goto __return;
    }

    if (temp[0] == NEX_RET_NUMBER_HEAD
        && temp[5] == 0xFF
        && temp[6] == 0xFF
        && temp[7] == 0xFF
        )
    {
        *number = ((uint32_t)temp[4] << 24) | ((uint32_t)temp[3] << 16) | (temp[2] << 8) | (temp[1]);
        ret = true;
    }

__return:

    if (ret) 
    {
        log_w("recvRetNumber: %d", *number);
    }
    else
    {
        log_w("recvRetNumber err");
    }
    
    return ret;
}


/*
 * Receive string data. 
 * 
 * @param buffer - save string data. 
 * @param len - string buffer length. 
 * @param timeout - set timeout time. 
 *
 * @return the length of string buffer.
 *
 */
uint16_t recvRetString(char *buffer, uint16_t len, uint32_t timeout)
{
    uint16_t ret = 0;
    bool str_start_flag = false;
    uint8_t cnt_0xff = 0;
    String temp = String("");
    uint8_t c = 0;
    long start;

    if (!buffer || len == 0)
    {
        goto __return;
    }
    
    start = millis();
    while (millis() - start <= timeout)
    {
        while (nexSerial.available())
        {
            c = nexSerial.read();
            if (str_start_flag)
            {
                if (0xFF == c)
                {
                    cnt_0xff++;                    
                    if (cnt_0xff >= 3)
                    {
                        break;
                    }
                }
                else
                {
                    temp += (char)c;
                }
            }
            else if (NEX_RET_STRING_HEAD == c)
            {
                str_start_flag = true;
            }
        }
        
        if (cnt_0xff >= 3)
        {
            break;
        }
    }

    ret = temp.length();
    ret = ret > len ? len : ret;
    strncpy(buffer, temp.c_str(), ret);
    
__return:

    log_w("recvRetString[%d,'%s']", temp.length(), temp.c_str());

    return ret;
}


/*
 * Command is executed successfully. 
 *
 * @param timeout - set timeout time.
 *
 * @retval true - success.
 * @retval false - failed. 
 *
 */
bool recvRetCommandFinished(uint32_t timeout)
{    
    uint8_t temp[4] = {0};
    
    nexSerial.setTimeout(timeout);
    if (sizeof(temp) != nexSerial.readBytes((char *)temp, sizeof(temp)))
    {
        return false;
    }

    if (temp[0] == NEX_RET_CMD_FINISHED
        && temp[1] == 0xFF
        && temp[2] == 0xFF
        && temp[3] == 0xFF
        )
    {
        return true;
    }
    
    return false;
}


/*
 * Send command to Nextion.
 *
 * @param cmd - the string of command.
 */
void sendCommand(const char* buffer, ...)
{
    // clear any data waiting to be read
    while (nexSerial.available())
        nexSerial.read();

    static char cmd[128];
    va_list varArgs;

    va_start(varArgs, buffer);
    vsnprintf(cmd, sizeof(cmd), buffer, varArgs);
    va_end(varArgs);


    nexSerial.print(cmd);
    nexSerial.write(0xFF);
    nexSerial.write(0xFF);
    nexSerial.write(0xFF);
}


bool NexClockInit(int8_t rxPin, int8_t txPin)
{
    bool ret1 = false;
    bool ret2 = false;
    
    nexSerial.begin(9600, SERIAL_8N1, rxPin, txPin);
    while (!nexSerial)
      delay(10);

 	xTaskCreatePinnedToCore(
		NexClockLoopTask,		    /* Function to implement the task */
		"NexClockLoopTask",		    /* Name of the task */
		3000,				        /* Stack size in words */
		NULL,				        /* Task input parameter */
		1,					        /* Priority of the task - must be higher than 0 (idle)*/
		&NexClockLoopTaskHandle,    /* Task handle. */
		1);					        /* Core where the task should run */


    sendCommand("");
    sendCommand("bkcmd=1");
    ret1 = recvRetCommandFinished(100);
    sendCommand("page 0");
    ret2 = recvRetCommandFinished(100);
    return (ret1 && ret2);

    if (ret1 && ret2)
    {
      // Change the baud to 115200
      sendCommand("baud 115200");

      // Set the new speed
      nexSerial.updateBaudRate(115200);

      sendCommand("");
      sendCommand("bkcmd=1");
      ret1 = recvRetCommandFinished(100);
      sendCommand("page 0");
      ret2 = recvRetCommandFinished(100);

      return (ret1 && ret2);
    }

    return 0;
}


NexPage::NexPage(uint8_t pid, uint8_t cid, const char *name)
    : __pid(pid)
    , __name(name)
{
}


bool NexPage::show(void)
{
    const char *name = getObjName();
    if (!name)
        return false;

    sendCommand("page %s", name);

    return recvRetCommandFinished();
}


NexObject::NexObject(NexPage* page, uint8_t cid, const char *name)
{
    this->__pid = page->getObjPid();
    this->__cid = cid;
    this->__name = name;
    this->__page = page;
}


bool NexObject::enable(bool state)
{
    sendCommand("%s.%s.en=%d", __page->getObjName(), getObjName(), state);
    return recvRetCommandFinished();
}


void NexObject::attachPush(NexObjectEventCb push, void *ptr)
{
    this->__cb_push = push;
    this->__cbpush_ptr = ptr;
}


void NexObject::detachPush(void)
{
    this->__cb_push = NULL;
    this->__cbpush_ptr = NULL;
}


void NexObject::attachPop(NexObjectEventCb pop, void *ptr)
{
    this->__cb_pop = pop;
    this->__cbpop_ptr = ptr;
}


void NexObject::detachPop(void)
{
    this->__cb_pop = NULL;    
    this->__cbpop_ptr = NULL;
}


void NexObject::push(void)
{
    if (__cb_push)
    {
        __cb_push(__cbpush_ptr);
    }
}


void NexObject::pop(void)
{
    if (__cb_pop)
    {
        __cb_pop(__cbpop_ptr);
    }
}


void NexObject::iterate(NexObject **list, uint8_t pid, uint8_t cid, int32_t event)
{
    NexObject *e = NULL;
    uint16_t i = 0;

    if (NULL == list)
    {
        return;
    }
    
    for(i = 0; (e = list[i]) != NULL; i++)
    {
        if (e->getObjPid() == pid && e->getObjCid() == cid)
        {
            e->printObjInfo();
            if (NEX_EVENT_PUSH == event)
            {
                e->push();
            }
            else if (NEX_EVENT_POP == event)
            {
                e->pop();
            }
            
            break;
        }
    }
}


uint16_t NexObject::getAttrText(const char* attr, char* buffer, uint16_t len)
{
    sendCommand("get %s.%s.%s", __page->getObjName(), getObjName(), attr);
    return recvRetString(buffer, len);
}


bool NexObject::setText(const char *buffer, ...)
{
    static char cmd[128];
    va_list varArgs;

    va_start(varArgs, buffer);
    vsnprintf(cmd, sizeof(cmd), buffer, varArgs);
    va_end(varArgs);

    return setAttrText("txt", cmd);
}


bool NexObject::setAttrText(const char* attr, const char *buffer)
{
    sendCommand("%s.%s.%s=\"%s\"", __page->getObjName(), getObjName(), attr, buffer);
    return recvRetCommandFinished();
}


uint32_t NexObject::getAttrNumber(const char* attr, uint32_t* number)
{
    sendCommand("get %s.%s.%s", __page->getObjName(), getObjName(), attr);
    return recvRetNumber(number);
}


bool NexObject::setAttrNumber(const char* attr, uint32_t number)
{
    sendCommand("%s.%s.%s=%d", __page->getObjName(), getObjName(), attr, number);
    sendCommand("ref %s.%s", __page->getObjName(), getObjName());
    return recvRetCommandFinished();
}



uint8_t NexObject::getObjPid(void)
{
    return __pid;
}


uint8_t NexObject::getObjCid(void)
{
    return __cid;
}


const char* NexObject::getObjName(void)
{
    return __name;
}


const char* NexObject::getPageName(void)
{
    if (__page)
        return __page->getObjName();

    return NULL;
}


void NexObject::printObjInfo(void)
{
    log_i("[0x%08X:%d,%d,'%s']", this, __pid, __cid, __name);
}

