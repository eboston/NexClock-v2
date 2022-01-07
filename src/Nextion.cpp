#include <Arduino.h>

#include "Nextion.h"
#include "mp3player.h"

#include "Clock.h"

#define nexSerial   Serial2

#define NEX_RET_INVALID_CMD                 (0x00)
#define NEX_RET_CMD_FINISHED                (0x01)
#define NEX_RET_INVALID_PICTURE_ID          (0x04)
#define NEX_RET_INVALID_FONT_ID             (0x05)
#define NEX_RET_INVALID_BAUD                (0x11)
#define NEX_RET_EVENT_TOUCH_HEAD            (0x65)     
#define NEX_RET_CURRENT_PAGE_ID_HEAD        (0x66)
#define NEX_RET_EVENT_POSITION_HEAD         (0x67)
#define NEX_RET_EVENT_SLEEP_POSITION_HEAD   (0x68)
#define NEX_RET_STRING_HEAD                 (0x70)
#define NEX_RET_NUMBER_HEAD                 (0x71)
#define NEX_RET_EVENT_LAUNCHED              (0x88)
#define NEX_RET_EVENT_UPGRADED              (0x89)
#define NEX_RET_INVALID_COMPONENT_ID        (0x02)
#define NEX_RET_INVALID_PAGE_ID             (0x03)
#define NEX_RET_INVALID_VARIABLE            (0x1A)
#define NEX_RET_INVALID_OPERATION           (0x1B)

#define NEX_EVENT_PUSH  (0x01)
#define NEX_EVENT_POP   (0x00)  


NexPage pageAlarm    = NexPage(5, 0, "Alarm");


TaskHandle_t NexClockLoopTaskHandle;


std::vector<NexPage*> nexPages;
std::vector<NexObject*> nexListen;

uint8_t currentPage = 0xFF;

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
            case NEX_RET_EVENT_TOUCH_HEAD:  // Button event
                if (nexSerial.available() >= 6)
                {
                    __buffer[0] = c;  
                    for (i = 1; i < 7; i++) __buffer[i] = nexSerial.read();
                    __buffer[i] = 0x00;

                    if (0xFF == __buffer[4] && 0xFF == __buffer[5] && 0xFF == __buffer[6])
                        NexObject::iterate(&nexListen, __buffer[1], __buffer[2], (int32_t)__buffer[3]);
                }

            case NEX_RET_CURRENT_PAGE_ID_HEAD:  // sendme - current page being displayed
                if (nexSerial.available() >= 4)
                {
                    __buffer[0] = c;
                    for (i = 1; i < 5; i++) __buffer[i] = nexSerial.read();
                    __buffer[i] = 0x00;

                    if (0xFF == __buffer[2] && 0xFF == __buffer[3] && 0xFF == __buffer[4])
                    {
                        uint8_t newPage = __buffer[1];
                        if (currentPage != newPage)
                        {
                            currentPage = newPage;
                            log_w("Current page is %d", currentPage);

                            if (currentPage == pageRadio.getObjPid())
                                showStreamTitle();
                        }
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

      sendCommand("rest");
      bool ret3 = recvRetCommandFinished(100);

      return (ret1 && ret2 && ret3);
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


void NexObject::iterate(std::vector<NexObject*>* nexList, uint8_t pid, uint8_t cid, int32_t event)
{
    if (nexList->empty())
    {
        log_w("nexList is empty");
        return;
    }

    for (std::vector<NexObject*>::iterator it = nexList->begin(); it != nexListen.end(); ++it)
    {
        NexObject* pNexObject = *it;

        log_w("pNexObject=0x%08X", pNexObject);
        log_w("iterate checking '%s'", pNexObject->getObjName());

        if (pNexObject->getObjPid() == pid && pNexObject->getObjCid() == cid)
        {
            pNexObject->printObjInfo();
            if (event == NEX_EVENT_PUSH)
                pNexObject->push();
            else
                pNexObject->pop();

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

