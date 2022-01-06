#if !defined(__NEXTION_H__)
#define __NEXTION_H__


bool recvRetCommandFinished(uint32_t timeout = 200);
void sendCommand(const char* cmd, ...);
bool NexClockInit(int8_t rxPin = -1, int8_t txPin = -1);
uint16_t recvRetString(char *buffer, uint16_t len, uint32_t timeout = 200);
bool recvRetNumber(uint32_t *number, uint32_t timeout = 100);

extern uint8_t currentPage;

extern TaskHandle_t NexClockLoopTaskHandle;


typedef void (*NexObjectEventCb)(void *ptr);


class NexPage
{
public:
    NexPage(uint8_t pid, uint8_t cid, const char *name);
    
    bool show(void);

    uint8_t getObjPid() { return __pid; }
    const char* getObjName() { return __name; }

private:
    uint8_t __pid;
    const char* __name;
};



class NexObject 
{
public: /* methods */
//    NexObject(uint8_t pid, uint8_t cid, const char *name);
    NexObject(NexPage* page, uint8_t cid, const char *name);

    void printObjInfo(void);

public: /* methods */
    bool getValue(uint32_t *number) { return getAttrNumber("val", number); }
    bool setValue(uint32_t number) { return setAttrNumber("val", number); }
    uint16_t getText(char *buffer, uint16_t len) { return getAttrText("txt", buffer, len); }
    bool setText(const char *buffer, ...);   
    uint32_t Get_background_color_bco(uint32_t *number) { return getAttrNumber("bco", number); }
    bool Set_background_color_bco(uint32_t number) { return setAttrNumber("bco", number); }
    uint32_t Get_press_background_color_bco2(uint32_t *number) { return getAttrNumber("bco2", number); }
    bool Set_press_background_color_bco2(uint32_t number) { return setAttrNumber("bco2", number); }		
    uint32_t Get_font_color_pco(uint32_t *number) { return getAttrNumber("pco", number); }
    bool Set_font_color_pco(uint32_t number) { return setAttrNumber("pco", number); }
    uint32_t Get_press_font_color_pco2(uint32_t *number) { return getAttrNumber("pco2", number); }
    bool Set_press_font_color_pco2(uint32_t number) { return setAttrNumber("pco2", number); }
    uint32_t Get_place_xcen(uint32_t *number) { return getAttrNumber("xcen", number); }
    bool Set_place_xcen(uint32_t number) { return setAttrNumber("xcen", number); }
    uint32_t Get_place_ycen(uint32_t *number) { return getAttrNumber(".ycen", number); }
    bool Set_place_ycen(uint32_t number) { return setAttrNumber("ycen", number); };			
    uint32_t getFont(uint32_t *number) { return getAttrNumber("font", number); }
    bool setFont(uint32_t number) { return setAttrNumber("font", number); }
    uint32_t Get_background_crop_picc(uint32_t *number) { return getAttrNumber("picc", number); }
    bool Set_background_crop_picc(uint32_t number) { return setAttrNumber("picc", number); }
    uint32_t Get_press_background_crop_picc2(uint32_t *number) { return getAttrNumber("picc2", number); }
    bool Set_press_background_crop_picc2(uint32_t number) { return setAttrNumber("picc2", number); }
    uint32_t Get_background_image_pic(uint32_t *number) { return getAttrNumber("pic", number); }
    bool Set_background_image_pic(uint32_t number) { return setAttrNumber("pic", number); }
    uint32_t Get_press_background_image_pic2(uint32_t *number) { return getAttrNumber("pic2", number); }
    bool Set_press_background_image_pic2(uint32_t number) { return setAttrNumber("pic2", number); }
    uint32_t Get_scroll_dir(uint32_t *number) { return getAttrNumber("dir", number); }
    bool Set_scroll_dir(uint32_t number) { return setAttrNumber("dir", number); }
    uint32_t Get_scroll_distance(uint32_t *number) { return getAttrNumber("dis", number); }
    bool Set_scroll_distance(uint32_t number) { return setAttrNumber("dis", number); }
    uint32_t Get_cycle_tim(uint32_t *number) { return getAttrNumber("tim", number); }
    bool Set_cycle_tim(uint32_t number) { return setAttrNumber("tim", number); }

    bool Get_Pic(uint32_t *number) { return getAttrNumber("picc", number); }
    bool Set_Pic(uint32_t number) { return setAttrNumber("picc", number); }

    
    
    bool enable(bool state);
    uint8_t getObjPid(void);    
    uint8_t getObjCid(void);
    const char *getObjName(void);    
    const char* getPageName(void);
    static void iterate(NexObject **list, uint8_t pid, uint8_t cid, int32_t event);
    void attachPush(NexObjectEventCb push, void *ptr = NULL);
    void detachPush(void);
    void attachPop(NexObjectEventCb pop, void *ptr = NULL);
    void detachPop(void);
    
private: /* methods */ 
    void push(void);
    void pop(void);
    

private: /* data */ 
    uint8_t __pid; /* Page ID */
    uint8_t __cid; /* Component ID */
    const char *__name; /* An unique name */
    NexPage* __page;

    NexObjectEventCb __cb_push;
    void *__cbpush_ptr;
    NexObjectEventCb __cb_pop;
    void *__cbpop_ptr;

protected:
    uint16_t getAttrText(const char* attr, char* buffer, uint16_t len);
    bool setAttrText(const char* attr, const char *buffer);
    
    uint32_t getAttrNumber(const char* attr, uint32_t* number);
    bool setAttrNumber(const char* attr, uint32_t number);
};



extern NexPage      pageStartup;
extern NexPage      pageClock;
extern NexPage      pageSetup;
extern NexPage      pageAlarm;
extern NexPage      pageUpload;
extern NexPage      pageRadio;

extern NexObject    pStartup_Status1;
extern NexObject    pStartup_Status2;
extern NexObject    pStartup_Status3;
extern NexObject    pStartup_Status4;
extern NexObject    pStartup_Spinner;

extern NexObject    pClock_Colon;
extern NexObject    pClock_AM;
extern NexObject    pClock_PM;
extern NexObject    pClock_THour;
extern NexObject    pClock_Hour;
extern NexObject    pClock_TMin;
extern NexObject    pClock_Min;
extern NexObject    pClock_Date;
extern NexObject    pClock_Title;
extern NexObject    pClock_bRadio;
extern NexObject    pClock_bSetup;

extern NexObject    pSetup_qFlashColon;
extern NexObject    pSetup_q24Hour;
extern NexObject    pSetup_qMetaData;

extern NexObject    pUpload_Message;
extern NexObject    pUpload_Status;
extern NexObject    pUpload_Progress;


extern NexObject    pRadio_VolDn;
extern NexObject    pRadio_VolUp;
extern NexObject    pRadio_StaDn;
extern NexObject    pRadio_StaUp;
extern NexObject    pRadio_Play;
extern NexObject    pRadio_STitle;
extern NexObject    pRadio_Title;
extern NexObject    pRadio_Artist;
extern NexObject    pRadio_StationName;
extern NexObject    pRadio_QUsage;
extern NexObject    pRadio_Time;
extern NexObject    pRadio_VolDisplay;
extern NexObject    pRadio_state;
extern NexObject    pRadio_vTitle;

#endif  // __NEXTION_H__