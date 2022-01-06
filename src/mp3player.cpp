#include <Arduino.h>
#include <VS1053.h>
#include <LITTLEFS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "NexClock2.h"
#include "mp3player.h"
#include "Utils.h"
#include "Nextion.h"
#include "settings.h"


// Wiring of VS1053 board (SPI connected in a standard way) on ESP32 only
#define VS1053_CS 32
#define VS1053_DCS 33
#define VS1053_DREQ 15

// Number of entries in the queue
#define QSIZ 2500

#define TMP_BUFF_SIZE   16384

// Size of metaline buffer
#define METASIZ 1024

enum datamode_t 
{ 
    INIT           = 0x0001, 
    HEADER         = 0x0002, 
    DATA           = 0x0004,      // State for datastream
    METADATA       = 0x0008, 
    STOPREQD       = 0x0010, 
    STOPPED        = 0x0020
} ;

void pRadio_PlayPushCallback(void *ptr);
void pRadio_VolDnPushCallback(void *ptr);
void pRadio_VolUpPushCallback(void *ptr);
void pRadio_StaDnPushCallback(void *ptr);
void pRadio_StaUpPushCallback(void *ptr);


// MP3 decoder
VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

WiFiClient        mp3client ;                            // An instance of the mp3 client, also used for OTA


enum qdata_type 
{
    QDATA, 
    QSTARTSONG, 
    QSTOPSONG,
    QDISPLAY_STREAMTITLE
};    // datatyp in qdata_struct

struct qdata_struct
{
  int datatyp ;                                       // Identifier
  __attribute__((aligned(4))) uint8_t buf[32] ;       // Buffer for chunk
} ;


QueueHandle_t     dataqueue;                            // Queue for mp3 datastream

TaskHandle_t      xplaytask;                            // Task handle for playtask

qdata_struct      inchunk;                              // Data from queue
qdata_struct      outchunk;                             // Data to queue

String            icyname;                              // Icecast station name
String            streamTitle;
String            streamArtist;

char              metalinebf[METASIZ + 1];              // Buffer for metaline/ID3 tags

bool              hostreq = false;                      // Request for new host

int               metaint = 0;                          // Number of databytes between metadata
int               datacount;                            // Counter databytes before metadata
int               metacount;                            // Number of bytes in metadata

uint32_t          clength;                              // Content length found in http header

int16_t           metalinebfx;                          // Index for metalinebf

uint8_t*          outqp = outchunk.buf;                 // Pointer to buffer in outchunk
uint8_t           tmpbuff[TMP_BUFF_SIZE];                        // Input buffer for mp3 or data stream 

volatile datamode_t datamode;                            // State of datastream


#define PLAY		"4"
#define PAUSE		"<"


//**************************************************************************************************
//     S H O W S T R E A M T I T L E                                                               *
//**************************************************************************************************
void showStreamTitle()
{
    log_w("\nTitle='%s'\nArtist='%s'", streamTitle.c_str(), streamArtist.c_str());

    if ((streamTitle.length() > 0 || streamArtist.length() > 0) && 
        (ClockSettings.bMetadata && radioStations[ClockSettings.lastRadioStation].useMetaData))
    {
        pClock_Title.setText("%s %c %s", streamTitle.c_str(), streamArtist.length() ? '/' : ' ',  streamArtist.c_str());

        pRadio_Title.setText(streamTitle.c_str());
        pRadio_STitle.setText(streamTitle.c_str());
        pRadio_Artist.setText("%s", streamArtist.c_str());

log_w("currentPage=%d", currentPage);
log_w("pageRadio.getObjPid()=%d", pageRadio.getObjPid());

        if (currentPage == pageRadio.getObjPid())
        {
            if (streamTitle.length() < 30)
            {
log_w("show static title");
                sendCommand("vis gTitle,0");
                sendCommand("vis tTitle,1");
            }
            else
            {
log_w("show scrolling title");
                sendCommand("vis gTitle,1");
                sendCommand("vis tTitle,0");
            }
        }
    }
    else
    {
        pClock_Title.setText("");

        pRadio_Title.setText("");
        pRadio_STitle.setText("");
        pRadio_Title.setText("");
    }
}


//**************************************************************************************************
//     P L A Y T A S K                                                                             *
//**************************************************************************************************
// Play stream data from input queue.                                                              *
// Handle all I/O to VS1053B during normal playing.                                                *
// Handles display of song title/artist, queue usage, and stack used                               *
//**************************************************************************************************
void playtask(void * parameter)
{
	static unsigned long prevMillis = millis();
	static unsigned long prevMillis2 = millis();
    static uint32_t nCnt = 0;
    while (true)
    {
        if (++nCnt > 50)
        {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            nCnt = 0;
        }

        if (xQueueReceive(dataqueue, &inchunk, 5))
        {
            while (!player.data_request())                                // If MP3 FIFO is full..
            {
                vTaskDelay (1) ;                                            // Yes, take a break
            }

            switch (inchunk.datatyp)                                      // What kind of chunk?
            {
            case QDATA:
                claimSPI("chunk") ;                                       // Claim SPI bus
                player.playChunk(inchunk.buf, sizeof(inchunk.buf));       // DATA, send to player
                releaseSPI() ;                                            // Release SPI bus
                break ;

            case QSTARTSONG:
                claimSPI("startsong") ;                                   // Claim SPI bus
                player.setVolume(ClockSettings.volumeLevel);
                player.startSong() ;                                      // START, start player
                releaseSPI() ;                                            // Release SPI bus
                break ;

            case QSTOPSONG:
                claimSPI("stopsong");                                 // Claim SPI bus
                player.setVolume(0);                           // Mute
                player.stopSong();                                // STOP, stop player
                releaseSPI() ;                                            // Release SPI bus
                while (xQueueReceive(dataqueue, &inchunk, 0));      // Flush rest of queue
                vTaskDelay(500 / portTICK_PERIOD_MS) ;                 // Pause for a short time
                break ;

            case QDISPLAY_STREAMTITLE:
                showStreamTitle() ;               // Show artist and title if present in metadata
                break;

            default:
                log_w("********* ERROR: Unknown inchunk.datatyp %d", inchunk.datatyp);
                break ;
            }
        }

        if (millis() - prevMillis2 > 500)
        {
            uint32_t qsize = QSIZ * sizeof(qdata_struct);
            if (qsize != 0)
            {
                uint32_t qspace = uxQueueSpacesAvailable(dataqueue) * sizeof(qdata_struct);      // Compute free space in data queue
                float qused = 100.0 - (((float)qspace / (float)qsize) * 100.0);
                pRadio_QUsage.setText("%2.0f%%", qused);
            }
            else
                pRadio_QUsage.setText("");

            prevMillis2 = millis();
        }             

		if (millis() - prevMillis > (60000 * 1));
		{
            UBaseType_t hwMark = uxTaskGetStackHighWaterMark(NULL);
            if (hwMark < 200)
			    log_w("***** Free stack:%lu", hwMark);
			prevMillis = millis();
		}
//        esp_task_wdt_reset() ;                                        // Protect against idle cpu
        vTaskDelay(1);
    }
}


//**************************************************************************************************
//     M P 3 S T A R T                                                                             *
//**************************************************************************************************
// Attach callback functions to controls                                                           *
// Start the VS1053 decoder (player)                                                               *
// Play mp3 file Intro.mp3 (optional)                                                              *
// Create semaphore for SPI bus control                                                            *
// Create data queue for stream data                                                               *
// Create playback task                                                                            *
//**************************************************************************************************
void mp3Start()
{
    pRadio_Play.attachPush(pRadio_PlayPushCallback);
    pRadio_VolDn.attachPush(pRadio_VolDnPushCallback);
    pRadio_VolUp.attachPush(pRadio_VolUpPushCallback);
    pRadio_StaDn.attachPush(pRadio_StaDnPushCallback);
    pRadio_StaUp.attachPush(pRadio_StaUpPushCallback);

	// Start MP3 Decoder
	player.begin();
	player.setVolume(0);

	// Wait for the player to be ready to accept data
	while (!player.data_request())
		delay(1);
 
	// You MIGHT have to set the VS1053 to MP3 mode. No harm done if not required!
	player.switchToMp3Mode();

	uint8_t rtone[4];
	rtone[0] = 3;
	rtone[1] = 3;
	rtone[2] = 10;
	rtone[3] = 3;
	player.setTone(rtone);

#if 0
	File file = LITTLEFS.open("/Intro.mp3");
	if (file)
	{
		player.startSong();
        player.setVolume(ClockSettings.volumeLevel);
		uint8_t audioBuffer[32] __attribute__((aligned(4)));
		while (file.available())
		{
			while (!player.data_request()) delay(1);
			int bytesRead = file.read(audioBuffer, 32);
			player.playChunk(audioBuffer, bytesRead);
		}
		file.close();
        player.setVolume(0);
		player.stopSong();
	}
	else
	{
		log_w("Unable to open greetings file");
	}
#endif

    SPIsem = xSemaphoreCreateMutex(); ;                    // Semaphore for SPI bus

    outchunk.datatyp = QDATA ;                              // This chunk dedicated to QDATA
    dataqueue = xQueueCreate(QSIZ, sizeof(qdata_struct));  // Create queue for communication

    xTaskCreatePinnedToCore(
        playtask,                                             // Task to play data in dataqueue.
        "Playtask",                                           // name of task.
        2200,                                                 // Stack size of task
        NULL,                                                 // parameter of the task
        5,                                                    // priority of the task
        &xplaytask,                                           // Task handle to keep track of created task
        0);                                                 // Run on CPU 0
}


//**************************************************************************************************
//     S E T D A T A M O D E                                                                       *
//**************************************************************************************************
// Change the datamode and show in debug for testing.                                              *
//**************************************************************************************************
void setdatamode ( datamode_t newmode )
{
  //log_w("Change datamode from 0x%03X to 0x%03X", (int)datamode, (int)newmode);
  datamode = newmode ;
}


//**************************************************************************************************
//     S E T S T R E A M T I T L E                                                                 *
//**************************************************************************************************
// Parses the metadata StreamTitle value for song Title/Artist.  Sets a marker in the queue to     *
// mark when the new data is to be displayed.                                                      *
//**************************************************************************************************
// Test values that have been seen
//
// StreamTitle='Phil Collins - You'll Be In My Heart - From "Tarzan"/Soundtrack Version';
//
//**************************************************************************************************
void setstreamtitle(const char *ml)
{
    char*   p1 ;
    char*   p2 ;
    char    ctitle[128] ;           // Streamtitle from metadata

    const char*   label = "StreamTitle=";

    if (strstr(ml, label))
    {
        log_i("Streamtitle found, %d bytes", strlen(ml));
        log_i("%s", ml);
        p1 = (char*)ml + strlen(label) ;     // Begin of artist and title
        if ((p2 = strstr(ml, ";")))          // Search for end of title
        {
            if ( *p1 == '\'' )               // Surrounded by quotes?
            {
                p1++ ;
                p2-- ;
            }
            *p2 = '\0' ;                     // Strip the rest of the line
        }

        // Save last part of string as streamtitle.  Protect against buffer overflow
        strncpy(ctitle, p1, sizeof(ctitle)) ;
        ctitle[sizeof(ctitle) - 1] = '\0' ;
    }
    else
    {
        log_w("No streamtitle found");
        streamArtist = "";
        streamTitle = "";
        return;                                    // Do not show
    }

    if ((p1 = strstr(ctitle, " - "))) // look for artist/title separator
    {
        char* pTitle = p1 + 3;
        char* pArtist = ctitle;
        *p1 = '\0';
        while (*pTitle == ' ')
            pTitle++;

        log_i(" Title='%s'", pTitle);
        log_i("Artist='%s'", pArtist);

        streamTitle = pTitle;
        streamArtist = pArtist;
    }
    else
    {
        streamTitle = ctitle;
        streamArtist = "";
    }

    // The Nextion display code seems to have problems with double quotes (") in the title.
    // So replace any with a single quote (').
    streamTitle.replace("\"", "'");

    log_i(" streamTitle='%s'", streamTitle.c_str());
    log_i("streamArtist='%s'", streamArtist.c_str());
}


//**************************************************************************************************
//     C H K H D R L I N E                                                                         *
//**************************************************************************************************
// Check if a line in the header is a reasonable headerline.                                       *
// Normally it should contain something like "icy-xxxx:abcdef".                                    *
//**************************************************************************************************
bool chkhdrline ( const char* str )
{
    char    b ;                                         // Byte examined
    int     len = 0 ;                                   // Lengte van de string

    while ( ( b = *str++ ) )                            // Search to end of string
    {
    len++ ;                                           // Update string length
        if ( ! isalpha ( b ) )                            // Alpha (a-z, A-Z)
        {
            if ( b != '-' )                                 // Minus sign is allowed
            {
                if ( b == ':' )                               // Found a colon?
                {
                    return ( ( len > 5 ) && ( len < 70 ) ) ;    // Yes, okay if length is okay
                }
                else
                {
                    return false ;                              // Not a legal character
                }
            }
        }
    }
    return false ;                                      // End of string without colon
}


//**************************************************************************************************
//      Q U E U E F U N C                                                                          *
//**************************************************************************************************
// Queue a special function for the play task.                                                     *
//**************************************************************************************************
void queuefunc ( int func )
{
    qdata_struct     specchunk ;                          // Special function to queue

    specchunk.datatyp = func ;                            // Put function in datatyp
    xQueueSendToFront ( dataqueue, &specchunk, 200 ) ;    // Send to queue (First Out)
}


//**************************************************************************************************
//     D E C O D E _ S P E C _ C H A R S                                                           *
//**************************************************************************************************
// Decode special characters like "&#39;".                                                         *
//**************************************************************************************************
String decode_spec_chars ( String str )
{
    int    inx, inx2 ;                                // Indexes in string
    char   c ;                                        // Character from string
    char   val ;                                      // Converted character
    String res = str ;

    while ( ( inx = res.indexOf ( "&#" ) ) >= 0 )     // Start sequence in string?
    {
        inx2 = res.indexOf ( ";", inx ) ;               // Yes, search for stop character
        if ( inx2 < 0 )                                 // Stop character found
        {
            break ;                                       // Malformed string
        }
        res = str.substring ( 0, inx ) ;                // First part
        inx += 2 ;                                      // skip over start sequence
        val = 0 ;                                       // Init result of 
        while ( ( c = str[inx++] ) != ';' )             // Convert character
        {
            val = val * 10 + c - '0' ;
        }
        res += ( String ( val ) +                       // Add special char to string
        str.substring ( ++inx2 ) ) ;           // Add rest of string
    }
    return res ;
}


//**************************************************************************************************
//     S C A N _ C O N T E N T _ L E N G T H                                                       *
//**************************************************************************************************
// If the line contains content-length information: set clength (content length counter).          *
//**************************************************************************************************
void scan_content_length ( const char* metalinebf )
{
    if ( strstr ( metalinebf, "Content-Length" ) )        // Line contains content length
    {
        clength = atoi ( metalinebf + 15 ) ;                // Yes, set clength
        log_w ( "Content-Length is %d", clength ) ;      // Show for debugging purposes
    }
}


//**************************************************************************************************
//      S T O P _ M P 3 C L I E N T                                                               *
//**************************************************************************************************
// Disconnect from the server.  Does not stop any processing.                                     *
//**************************************************************************************************
void stop_mp3client()
{
    while (mp3client.connected())
    {
        log_w ("Stopping client") ;               // Stop connection to host
        mp3client.stop() ;
        mp3client.flush() ;
        delay ( 500 ) ;
    }
    mp3client.stop() ;                               // Stop stream client
    mp3client.flush() ;                              // Flush stream client
}
//**************************************************************************************************
//     C O N N E C T T O S T A T I O N                                                             *
//**************************************************************************************************
// Connect to the Internet radio server specified by the station number.                           *
// If a stream is playing, it should pause the stream and flush any existing data in the queue.    *
// When the new station is connected, it should resume the stream.                                 *
//**************************************************************************************************
bool connectToStation(uint8_t stationNumber)
{
    int         inx;                            // Position of ":" in hostname
    uint16_t    port = 80;                      // Port number for host
    String      extension;                      // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
    String      hostwoext;                      // Host without extension and portnumber
//    String      auth;                           // For basic authentication

    // Validate the station number.  If invalid, return false.
    if (stationNumber > radioStations.size())
    {
        log_w("Invalid station number.  Requested %d.  Max is %d", stationNumber, radioStations.size());
        return false;
    }
    else
    {
        ClockSettings.lastRadioStation = stationNumber;
        writeSettings();
    }

    // Clear song title/artist and station name.
    pRadio_Title.setText("");
    pRadio_STitle.setText("");
    pRadio_StationName.setText("Connecting...");

    // Get the host to connect to.
    String FName = radioStations[stationNumber].friendlyName;
    String host = radioStations[stationNumber].host;
    bool bUseMetaData = radioStations[stationNumber].useMetaData;

    log_w("Station %d - %s '%s' useMeta=%d", stationNumber, FName.c_str(), host.c_str(), bUseMetaData);

    // Set the input parser to the initialize state.
    setdatamode(INIT);

    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    // Search for begin of extension
    if ((inx = host.indexOf( "/" )) > 0)
    {
        extension = host.substring ( inx ) ;            // Yes, change the default
        hostwoext = host.substring ( 0, inx ) ;         // Host without extension
    }

    // In the host there may be a portnumber
    // Search for separator
    if ((inx = hostwoext.indexOf ( ":" )) >= 0 )
    {
        port = host.substring ( inx + 1 ).toInt() ;     // Get portnumber as integer
        hostwoext = host.substring ( 0, inx ) ;         // Host without portnumber
    }

    // Connect to the station
    log_w("Starting client");
    if (!mp3client.connect(hostwoext.c_str(), port))
    {
        log_w ( "Request %s failed!", radioStations[stationNumber].friendlyName) ;
        return false ;
    }

    log_w ( "Connected to server" ) ;
    pRadio_StationName.setText(radioStations[9].friendlyName);

#if 0
    if ( nvssearch ( "basicauth" ) )                // Does "basicauth" exists?
    {
        auth = nvsgetstr ( "basicauth" ) ;            // Use basic authentication?
        if ( auth != "" )                             // Should be user:passwd
        { 
            auth = base64::encode ( auth.c_str() ) ;   // Encode
            auth = String ( "Authorization: Basic " ) +
            auth + String ( "\r\n" ) ;
        }
    }
#endif

    // Send a request to the server to start the data stream
    char getReq[128];
    snprintf(getReq, sizeof(getReq),
        "GET %s HTTP/1.0\r\nHost: %s\r\nIcy-MetaData: 1\r\nConnection: close\r\n\r\n",
        extension.c_str(), hostwoext.c_str());

    mp3client.print(getReq) ;                            // Send get request
    vTaskDelay ( 1000 / portTICK_PERIOD_MS ) ;           // Give some time to react

    // Display the station name
    pRadio_StationName.setText(radioStations[stationNumber].friendlyName);

    log_w("finished connecting to station");
    return true ;                                        // Send is probably okay
}


//**************************************************************************************************
//     H A N D L E B Y T E _ C H                                                                  *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
//**************************************************************************************************
void handlebyte_ch (uint8_t b)
{
    static int       LFcount ;                                  // Detection of end of header
    static bool      ctseen = false ;                           // First line of header seen or not

    if (datamode == DATA)                                       // Handle next byte of MP3/Ogg data
    {
        *outqp++ = b ;                                          // Save the byte in the buffer
        if (outqp == (outchunk.buf + sizeof(outchunk.buf)))     // Buffer full?
        {
            // Send data to playtask queue.  If the buffer cannot be placed within 200 ticks,
            // the queue is full, while the sender tries to send more.  The chunk will be dis-
            // carded in that case.
            xQueueSend(dataqueue, &outchunk, 200);          // Send to queue
            outqp = outchunk.buf ;                              // Item empty now

        }

        if (metaint)                                            // No METADATA on Ogg streams or mp3 files
        {
            if (--datacount == 0)                               // End of datablock?
            {
                setdatamode(METADATA);                          // Change to metadata processing mode
                metalinebfx = -1 ;                              // Expecting first metabyte (counter)
            }
        }
        return ;
    }

    if (datamode == INIT)                                       // Initialize for header receive - start of new stream
    {
        log_w( "Switch to HEADER" ) ;
        setdatamode ( HEADER ) ;                                // Handle header

        ctseen = false ;                                        // Contents type not seen yet
        metaint = 0 ;                                           // No metaint found
        LFcount = 0 ;                                           // For detection end of header
        metalinebfx = 0 ;                                       // No metadata yet
        metalinebf[0] = '\0' ;
    }

    if ( datamode == HEADER )                                   // Handle next byte of MP3 header
    {
        if ( ( b > 0x7F ) ||                                    // Ignore unprintable characters
            ( b == '\r' ) ||                                    // Ignore CR
            ( b == '\0' ) )                                     // Ignore NULL
        {
            // Yes, ignore
        }
        else if ( b == '\n' )                                   // Linefeed ?  Process the line.
        {
            LFcount++ ;                                         // Count linefeeds
            metalinebf[metalinebfx] = '\0' ;                    // Take care of delimiter
            if (chkhdrline(metalinebf))                         // Reasonable input?
            {
                //log_w("Headerline: %s",  metalinebf );         // Show headerline

                String metaline = String(metalinebf);          // Convert to string
                String lcml = metaline;                        // Use lower case for compare
                lcml.toLowerCase();

                if (lcml.startsWith("location: http://"))      // Redirection?
                {
                    // Save the redirected address to the station list.
                    strncpy(
                        radioStations[ClockSettings.lastRadioStation].host, 
                        metaline.substring(17).c_str(), 
                        sizeof(radioStations[ClockSettings.lastRadioStation].host));
                    writeRadioStations();
                    hostreq = true ;                           // And request this one
                }

                if (lcml.startsWith("location: https://"))     // Redirection?
                {
                    // Save the redirected address to the station list.
                    strncpy(
                        radioStations[ClockSettings.lastRadioStation].host, 
                        metaline.substring(18).c_str(), 
                        sizeof(radioStations[ClockSettings.lastRadioStation].host));
                    writeRadioStations();
                    hostreq = true ;                             // And request this one
                }

                if (lcml.indexOf("content-type") >= 0)           // Line with "Content-Type: xxxx/yyy"
                {
                    ctseen = true;                               // Yes, remember seeing this
                    String ct = metaline.substring(13);         // Set contentstype. Not used yet
                    ct.trim() ;
                    log_w ("%s seen.", ct.c_str());
                }

                if (lcml.startsWith("icy-metaint:"))
                {
                    metaint = metaline.substring(12).toInt();    // Found metaint tag, read the value
                }
                else if (lcml.startsWith("icy-name:"))
                {
                    icyname = metaline.substring(9) ;            // Get station name
                    icyname = decode_spec_chars ( icyname ) ;    // Decode special characters in name
                    icyname.trim() ;                             // Remove leading and trailing spaces
                }
                else if (lcml.startsWith("transfer-encoding:"))  // Chunky stuff possably?
                {
                    log_w(">>>>>>>>>> transfer-encoding header found. <<<<<<<<<<");
                }
            }

            metalinebfx = 0 ;                                    // Reset this line

            if ((LFcount == 2) && ctseen)                        // Content type seen and a double LF?
            {
                log_w ( "Switch to DATA, metaint is %d", metaint);    // Show metaint
                setdatamode(DATA);                               // Expecting data now
                datacount = metaint;                             // Number of bytes before first metadata
                queuefunc(QSTARTSONG);                           // Queue a request to start song
            }
        }
        else
        {
            metalinebf[metalinebfx++] = (char)b;                 // Normal character, put new char in metaline
            if (metalinebfx >= METASIZ)                          // Prevent overflow
            {
                metalinebfx-- ;
            }
            LFcount = 0 ;                                        // Reset double CRLF detection
        }

        return ;
    }

    if (datamode == METADATA)                                    // Handle next byte of metadata
    {
        if (metalinebfx < 0)                                     // First byte of metadata?
        {
            metalinebfx = 0;                                     // Prepare to store first character
            metacount = b * 16 + 1;                              // New count for metadata including length byte
            if (metacount > 1)
            {
                log_i("Metadata block %d bytes", metacount - 1); // Most of the time there are zero bytes of metadata
            }
        }
        else
        {
            metalinebf[metalinebfx++] = (char)b ;                // Normal character, put new char in metaline
            if (metalinebfx >= METASIZ)                          // Prevent overflow
            {
                metalinebfx-- ;
            }
        }
        if (--metacount == 0)
        {
            metalinebf[metalinebfx] = '\0' ;                     // Make sure line is limited
            if (strlen(metalinebf))                              // Any info present?
            {
                // metaline contains artist and song name.  For example:
                // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
                // Sometimes it is just other info like:
                // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
                // Isolate the StreamTitle, remove leading and trailing quotes if present.

                qdata_struct specchunk ;                         // Special function to queue
                specchunk.datatyp = QDISPLAY_STREAMTITLE;        // Set a flag to indicate the info should be displayed when processed from the queue
                xQueueSend (dataqueue, &specchunk, 200);         // Send to queue
                setstreamtitle(metalinebf);                      // Show artist and title if present in metadata
            }

            if ( metalinebfx  > ( METASIZ - 10 ) )               // Unlikely metaline length?
            {
                log_w ( "Metadata block too long! Skipping all Metadata from now on." ) ;
                metaint = 0 ;                                    // Probably no metadata
            }

            datacount = metaint;                                 // Reset data count
            setdatamode(DATA);                                   // Expecting data
        }
    }
}


//**************************************************************************************************
//     M P 3 L O O P                                                                               *
//**************************************************************************************************
// Called from the main loop() for the mp3 functions.                                              *
// A connection to an MP3 server is active and we are ready to receive data.                       *
// Normally there is about 2 to 4 kB available in the data stream.  This depends on the sender.    *
//**************************************************************************************************
void mp3loop()
{
    uint32_t        maxchunk ;                                  // Max number of bytes to read
    int             res = 0 ;                                   // Result reading from mp3 stream
    uint32_t        av = 0 ;                                    // Available in stream
    String          nodeID ;                                    // Next nodeID of track on SD
    uint32_t        qspace ;                                    // Free space in data queue
    String          tmp ;                                       // Needed for station name in pref

    // Try to keep the Queue to playtask filled up by adding as much bytes as possible
    if (datamode & ( INIT | HEADER | DATA | METADATA))          // Test op playing
    {
        maxchunk = sizeof(tmpbuff);                             // Reduce byte count for this mp3loop()
        qspace = uxQueueSpacesAvailable(dataqueue) * sizeof(qdata_struct);  // Compute free space in data queue

        av = mp3client.available();                             // Available from stream

        if (av < maxchunk)                                      // Limit read size to what was read or size of buffer.  Which ever is smaller
        {
            maxchunk = av;
        }
        
        if (maxchunk > qspace)                                  // Enough space in queue?
        {
            maxchunk = qspace;                                  // No, limit to free queue space
        }

        if ((maxchunk > 1000) || (datamode == INIT))
        {
            if (maxchunk)                                       
            {
                res = mp3client.read(tmpbuff, maxchunk);        // Read a number of bytes from the stream
            }
        }

        // Process the data from the stream one byte at a time.
        for (int i = 0; i < res; i++)
        {
            handlebyte_ch(tmpbuff[i]) ;                         // Handle one byte
        }
    }

    if  (datamode == STOPREQD)                                  // STOP requested?
    {
        log_w ( "STOP requested" ) ;

        stop_mp3client();                                       // Disconnect if still connected

        datacount = 0;                                          // Reset datacount
        outqp = outchunk.buf;                                   // and pointer
        queuefunc(QSTOPSONG);                                   // Queue a request to stop the song
        metaint = 0;                                            // No metaint known now
        setdatamode(STOPPED);                                   // Yes, state becomes STOPPED
        return ;
    }

    if (hostreq)                                                // New preset or station?  Set inside handlebyte_ch if there was a redirect.
    {
        log_w ( "New station request" ) ;
        hostreq = false;

        connectToStation(ClockSettings.lastRadioStation);       // Switch to new host
    }
}


//**************************************************************************************************
//     P L A Y R A D I O                                                                           *
//**************************************************************************************************
// Starts or stops the playing of the data stream                                                  *
//**************************************************************************************************
void playRadio(bool state, bool saveChange)
{
	log_w("playRadio = %d", state);
	if (state)
	{
        vTaskResume(xplaytask);

        // Start playing the data stream.
		pRadio_Play.setText(PAUSE);
		pRadio_Play.Set_font_color_pco(63488);

		pClock_bRadio.Set_background_color_bco(2016);
		pClock_bRadio.Set_font_color_pco(0);

        connectToStation(ClockSettings.lastRadioStation);

        player.setVolume(ClockSettings.volumeLevel);
    }
    else
    {
        player.setVolume(0);

        vTaskSuspend(xplaytask);

        // Stop playing the data stream
        setdatamode(STOPREQD);

		pRadio_Play.setText(PLAY);
		pRadio_Play.Set_font_color_pco(2016);

        pRadio_STitle.setText("");
        pRadio_Artist.setText("");
        pRadio_StationName.setText("");
        pRadio_QUsage.setText("");
        streamTitle = "";
        streamArtist = "";

        pClock_Title.setText("");
		pClock_bRadio.Set_background_color_bco(50712);
		pClock_bRadio.Set_font_color_pco(0);
		pClock_Title.setText("");
    }

    if (saveChange)
    {
        ClockSettings.bRadioOn = state;
        writeSettings();
    }
}


void pRadio_PlayPushCallback(void *ptr)
{
    log_w("pRadio_PlayPushCallback called");

	char ret[8];

	uint16_t rsize = pRadio_Play.getText(ret, sizeof(ret));

	ret[rsize] = '\0';

	if (rsize)
		playRadio(strncmp(ret, PLAY, rsize) == 0);
}


void pRadio_VolDnPushCallback(void *ptr)
{
	ClockSettings.volumeLevel--;
	pRadio_VolDisplay.setValue(ClockSettings.volumeLevel);
    saveSettings = true;
	player.setVolume(ClockSettings.volumeLevel);
}


void pRadio_VolUpPushCallback(void *ptr)
{
	ClockSettings.volumeLevel++;
	pRadio_VolDisplay.setValue(ClockSettings.volumeLevel);
    saveSettings = true;
	player.setVolume(ClockSettings.volumeLevel);
}


void pRadio_StaDnPushCallback(void *ptr)
{
    if (datamode & (STOPREQD | STOPPED))
        return;

    log_w("Station Down called");
    playRadio(false, false);

	if (ClockSettings.lastRadioStation == 0)
		ClockSettings.lastRadioStation = radioStations.size() - 1;
	else
		ClockSettings.lastRadioStation--;
    log_w("new station: %d", ClockSettings.lastRadioStation);

	writeSettings();

    playRadio(true, false);
}


void pRadio_StaUpPushCallback(void *ptr)
{
    if (datamode & (STOPREQD | STOPPED))
        return;

    log_w("Station Up called");
    playRadio(false, false);

	ClockSettings.lastRadioStation++;
	if (ClockSettings.lastRadioStation > radioStations.size() - 1)
		ClockSettings.lastRadioStation = 0;

	writeSettings();

    playRadio(true, false);
}

