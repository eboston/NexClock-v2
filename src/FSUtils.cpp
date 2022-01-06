#include <Arduino.h>
#include <LITTLEFS.h>


#define FORMAT_FS_IF_FAILED false

bool bFSStarted = false;


bool StartLITTLEFS()
{
  if (!LITTLEFS.begin(FORMAT_FS_IF_FAILED))
  {
    log_w("setup: LITTLEFS Mount Failed\n");
    return false;
  }
  log_w("LITTLEFS mounted\n");
  bFSStarted = true;

//  LITTLEFS.remove(CONFIG);

  return true;
}


void StopLITTLEFS()
{
    log_w("Stopping LITTLEFS");
    LITTLEFS.end();
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels)
{
    if (!bFSStarted)
    {
        Serial.printf("File System has not been started yet.\n");
        return;
    }
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root)
    {
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory())
    {
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file)
    {
        if(file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels)
            {
                listDir(fs, file.name(), levels -1);
            }
        } 
        else 
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}


void listDir(const char * dirname)
{
    listDir(LITTLEFS, dirname, 0);
}
