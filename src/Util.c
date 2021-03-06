/*========================================================================
   Utils.c
   
   Utility functions used by the SlugWx weather monitor.  Mostly routines to dump 
   various data structures for debug or status reporting.

   This module also contains a generic routine that checks to see if a weather data field
   timestamp is present (ie - there's data in the field, not just 0's)

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF
   THE COPYRIGHT HOLDERS OR CONTRIBUTORS ARE AWARE OF THE POSSIBILITY OF SUCH DAMAGE.
   
========================================================================*/

#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/signal.h>
#include <sys/types.h>

#include "rtl-wx.h"

static void printTimeDateAndUptime(FILE *fd);

//--------------------------------------------------------------------------------------------------------------------------------------------
// Dump the current weather station data to the file specified in a human readable format.  This is called based on
// user input when running in interactive mode (either standalone or server with a client attached) or by remote
// command mode when used to produce output for the embedded web pages
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DumpInfo(FILE *fd) { 
   int sensorIdx;
  
   printTimeDateAndUptime(fd);
   
   // --------------------------- Outdoor Unit info ---------------------------------------------------
   if (isTimestampPresent(&wxData.odu.Timestamp))  {
      if (WxConfig.oduNameString[0] != 0)
         fprintf(fd, "   %s (ODU)  Temp: ",WxConfig.oduNameString);
      else
         fprintf(fd, "   Outdoor Temp: ");
      fprintf(fd, "%5.1f  Relative Humidity: %d%%  Dewpoint: ",
               wxData.odu.Temp*1.8 + 32, wxData.odu.RelHum);
      fprintf(fd,"%2.1f ",wxData.odu.Dewpoint*1.8+32);
      if (wxData.odu.BatteryLow == TRUE)
      fprintf(fd, "(**Low Sensor Battery)");
      fprintf(fd,"\n\n"); 
   }

   // --------------------------- Extra Sensor info ---------------------------------------------------
   for (sensorIdx=0;sensorIdx<=MAX_SENSOR_CHANNEL_INDEX;sensorIdx++) {
      if (isTimestampPresent(&wxData.ext[sensorIdx].Timestamp)) {
         char label[80];
         if (WxConfig.extNameStrings[sensorIdx][0] != 0)
            fprintf(fd, "   %s (Ch%2d) ",WxConfig.extNameStrings[sensorIdx], sensorIdx+1);
         else 
            fprintf(fd, "   Extra Sensor%2d ", sensorIdx+1);
            fprintf(fd, "Temp: %5.1f  Relative Humidity: %d%%  Dewpoint: ",
               wxData.ext[sensorIdx].Temp*1.8 + 32, wxData.ext[sensorIdx].RelHum);
            fprintf(fd,"%2.1f ",wxData.ext[sensorIdx].Dewpoint*1.8+32);
         if (wxData.ext[sensorIdx].BatteryLow == TRUE)
            fprintf(fd, "(** Sensor Battery Low)");
            fprintf(fd,"\n");
      }
   }
   // --------------------------- Indoor Unit info ---------------------------------------------------
   if (isTimestampPresent(&wxData.idu.Timestamp))  {
      if (WxConfig.iduNameString[0] != 0)
         fprintf(fd, "\n   %s (IDU)  Temp: ",WxConfig.iduNameString);
      else
         fprintf(fd, "\n   Indoor  Temp: ");
      fprintf(fd, "%5.1f  Relative Humidity: %d%%  Dewpoint: ",
               wxData.idu.Temp*1.8 + 32, wxData.idu.RelHum);
      fprintf(fd,"%2.1f ",wxData.idu.Dewpoint*1.8+32);
      if (wxData.idu.BatteryLow == TRUE)
         fprintf(fd, "(** Low Sensor Battery)");
      fprintf(fd,"\n");

      if (WxConfig.iduNameString[0] != 0)
         fprintf(fd, "   %s (IDU)  Pressure: ",WxConfig.iduNameString);
      else
         fprintf(fd, "   Indoor  Pressure: ");      
      fprintf(fd, "%d mbar   Sealevel: %d  Forecast: %s\n",
                    wxData.idu.Pressure, wxData.idu.Pressure+wxData.idu.SeaLevelOffset, wxData.idu.ForecastStr);
   }
   
   fprintf(fd, "\n");
   // --------------------------- Wind Gauge info ---------------------------------------------------
   if (isTimestampPresent(&wxData.wg.Timestamp)) {
      fprintf(fd, "   Wind Speed: %4.1f  Avg Speed: %4.1f  Direction: %d",
               wxData.wg.Speed, wxData.wg.AvgSpeed, wxData.wg.Bearing);
      if (wxData.wg.ChillValid)
         fprintf(fd," Wind Chill: %2.0f ",wxData.wg.WindChill*1.8+32);
      else 
         fprintf(fd," Wind Chill: --\n");
      if (wxData.wg.BatteryLow == TRUE)
         fprintf(fd, "(**Low Sensor Battery)");
      fprintf(fd,"\n");   ;
   }
   // --------------------------- Rain Gauge info ---------------------------------------------------
   if (isTimestampPresent(&wxData.rg.Timestamp)) {
      fprintf(fd, "   Rainfall: %dmm/hr  Total Rainfall: %dmm ", wxData.rg.Rate, wxData.rg.Total);
      if (wxData.wg.BatteryLow == TRUE)
         fprintf(fd, "(**Low Sensor Battery)");
      fprintf(fd,"\n");   
   }
  fprintf(fd,"\n");
}

static void printMaxMinTemp(FILE *fd, char *label,float maxTemp, WX_Timestamp *maxTs, float minTemp, WX_Timestamp *minTs);
static void printMaxMinRelHum(FILE *fd, char *label, int maxHum, WX_Timestamp *maxTs,int minHum, WX_Timestamp *minTs);
static void printMaxMinDewpoint(FILE *fd,char *label,float maxDewpoint,WX_Timestamp *maxTs,float minDewpoint,WX_Timestamp *minTs);
static void printMaxMinWindSpeed(FILE *fd, char *label,float maxSpeed, WX_Timestamp *maxTs,float minSpeed, WX_Timestamp *minTs);
static void printTimestamp(FILE *fd, WX_Timestamp *ts);

//--------------------------------------------------------------------------------------------------------------------------------------------
// Dump  max/min weather station data to the file specified in a human readable format.  This is called based on
// user input when running in interactive mode (either standalone or server with a client attached) or by the remote
// command mode (to produce web output)
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DumpMaxMinInfo(FILE *fd)
{ 
  int i;
  WX_Data *maxDatap = WX_GetMaxDataRecord();
  WX_Data *minDatap = WX_GetMinDataRecord();
  char label[100];
   
  printTimeDateAndUptime(fd);
   
  fprintf(fd,"                   Min           Time                  Max            Time\n\n");
         
  if (WxConfig.iduNameString[0] != 0)
      sprintf(label, "\n   %s (IDU)\n     Temperature",WxConfig.iduNameString);
  else
      sprintf(label, "\n   Indoor\n     Temperature");
  printMaxMinTemp(fd, label,
                  maxDatap->idu.Temp, &maxDatap->idu.TempTimestamp, 
                  minDatap->idu.Temp, &minDatap->idu.TempTimestamp);
  printMaxMinDewpoint(fd, "        Dewpoint",
                  maxDatap->idu.Dewpoint, &maxDatap->idu.DewpointTimestamp, 
                  minDatap->idu.Dewpoint, &minDatap->idu.DewpointTimestamp);
  printMaxMinRelHum(fd, "        Humidity",
                  maxDatap->idu.RelHum, &maxDatap->idu.RelHumTimestamp, 
                  minDatap->idu.RelHum, &minDatap->idu.RelHumTimestamp);
  
  fprintf(fd, "\n");
  if (WxConfig.oduNameString[0] != 0)
      sprintf(label, "\n   %s (ODU)\n     Temperature",WxConfig.oduNameString);
  else
      sprintf(label, "\n   Outdoor\n     Temperature");
  printMaxMinTemp(fd, label,
                  maxDatap->odu.Temp, &maxDatap->odu.TempTimestamp, 
                  minDatap->odu.Temp, &minDatap->odu.TempTimestamp);
  printMaxMinDewpoint(fd, "        Dewpoint",
                  maxDatap->odu.Dewpoint, &maxDatap->odu.DewpointTimestamp, 
                  minDatap->idu.Dewpoint, &minDatap->odu.DewpointTimestamp);
  printMaxMinRelHum(fd, "        Humidity",
                  maxDatap->odu.RelHum, &maxDatap->odu.RelHumTimestamp, 
                  minDatap->odu.RelHum, &minDatap->odu.RelHumTimestamp);

  for(i=0;i<=MAX_SENSOR_CHANNEL_INDEX;i++) {
      if (WxConfig.extNameStrings[i][0] != 0)
        sprintf(label, "\n   %s (Ch %d)\n     Temperature",WxConfig.extNameStrings[i], i+1);
      else
        sprintf(label, "\n   Extra Sensor (Ch %d)\n     Temperature", i+1);
      printMaxMinTemp(fd, label,
                  maxDatap->ext[i].Temp, &maxDatap->ext[i].TempTimestamp, 
                  minDatap->ext[i].Temp, &minDatap->ext[i].TempTimestamp);
      printMaxMinDewpoint(fd, "        Dewpoint",
                  maxDatap->ext[i].Dewpoint, &maxDatap->ext[i].DewpointTimestamp, 
                  minDatap->ext[i].Dewpoint, &minDatap->ext[i].DewpointTimestamp);
      printMaxMinRelHum(fd, "        Humidity",
                  maxDatap->ext[i].RelHum, &maxDatap->ext[i].RelHumTimestamp, 
                  minDatap->ext[i].RelHum, &minDatap->ext[i].RelHumTimestamp);
  }

  printMaxMinWindSpeed(fd, "\n   Wind Gauge\n      Wind Speed",
                  maxDatap->wg.Speed, &maxDatap->wg.SpeedTimestamp, 
                  minDatap->wg.Speed, &minDatap->wg.SpeedTimestamp);
  printMaxMinWindSpeed(fd,"      Avg. Speed",
                  maxDatap->wg.AvgSpeed, &maxDatap->wg.AvgSpeedTimestamp, 
                  minDatap->wg.AvgSpeed, &minDatap->wg.AvgSpeedTimestamp);

  if (isTimestampPresent(&maxDatap->rg.RateTimestamp) || 
      isTimestampPresent(&minDatap->rg.RateTimestamp)) {
     fprintf(fd, "\n   Rain Gauge\n       Rain Rate");
     fprintf(fd, "  %4d  ", minDatap->rg.Rate);
     printTimestamp(fd, &minDatap->rg.RateTimestamp); 
     fprintf(fd, "   %4d  ", maxDatap->rg.Rate);
     printTimestamp(fd, &maxDatap->rg.RateTimestamp);
     fprintf(fd, "\n"); 
    }
  fprintf(fd, "\n");
 }

void printMaxMinTemp(FILE *fd, char *label, 
                     float maxTemp, WX_Timestamp *maxTs,
                     float minTemp, WX_Timestamp *minTs)
{
  if (isTimestampPresent(maxTs) || isTimestampPresent(minTs)) {
    fprintf(fd, "%s",label);
    if (isTimestampPresent(minTs)) {
     fprintf(fd, "  %4.1f  ", minTemp*1.8+32);
     printTimestamp(fd, minTs);
    }
    else
     fprintf(fd, "                        ");
    if (isTimestampPresent(maxTs)) {
     fprintf(fd, "   %4.1f  ", maxTemp*1.8+32);
     printTimestamp(fd, maxTs); 
    }
    fprintf(fd, "\n");
  }
}
void printMaxMinRelHum(FILE *fd,char *label,int maxHum,WX_Timestamp *maxTs,int minHum,WX_Timestamp *minTs)
{
  if (isTimestampPresent(maxTs) || isTimestampPresent(minTs)) {
    fprintf(fd, "%s",label);
    if (isTimestampPresent(minTs)) {
     fprintf(fd, "   %d%%  ", minHum);
     printTimestamp(fd, minTs); 
    }
    else
     fprintf(fd, "                        ");
    if (isTimestampPresent(maxTs)) {
     fprintf(fd, "    %d%%  ", maxHum);
     printTimestamp(fd, maxTs); 
    }
    fprintf(fd, "\n");
  }
}
void printMaxMinDewpoint(FILE *fd, char *label,float maxDewpoint,WX_Timestamp *maxTs,float minDewpoint,WX_Timestamp *minTs)
{
  if (isTimestampPresent(maxTs) || isTimestampPresent(minTs)) {
    fprintf(fd, "%s",label);
    if (isTimestampPresent(minTs)) {
     fprintf(fd, "  %4.1f  ", minDewpoint*1.8+32);
     printTimestamp(fd, minTs); 
    }
    else
     fprintf(fd, "                        ");
    if (isTimestampPresent(maxTs)) {
     fprintf(fd, "   %4.1f  ", maxDewpoint*1.8+32);
     printTimestamp(fd, maxTs); 
    }
    fprintf(fd, "\n");
  }
}

void printMaxMinWindSpeed(FILE *fd,char *label,float maxSpeed,WX_Timestamp *maxTs,float minSpeed,WX_Timestamp *minTs)
{
  if (isTimestampPresent(maxTs) || isTimestampPresent(minTs)) {
    fprintf(fd, "%s",label);
    if (isTimestampPresent(minTs)) {
     fprintf(fd, "  %4.1f  ", minSpeed);
     printTimestamp(fd, minTs); 
    }
    else
     fprintf(fd, "                        ");
    if (isTimestampPresent(maxTs)) {
     fprintf(fd, "   %4.1f  ", maxSpeed);
     printTimestamp(fd, maxTs); 
    }
    fprintf(fd, "\n");
  }
}
void printTimestamp(FILE *fd, WX_Timestamp *ts) {
  struct tm *localtm = localtime(&ts->timet);
  fprintf(fd, "Date: %02d/%02d/%04d Time: %02d:%02d",
      localtm->tm_mon+1, localtm->tm_mday, localtm->tm_year+1900, localtm->tm_hour, localtm->tm_min);      
}

static void printSensorStatus(FILE *fd,char *str, int lock_code, int lock_code_change_count, int no_data_for_180_secs, int no_data_between_snapshots, WX_Timestamp *ts);
static void printExtraSensorStatus(FILE *fd, int sensorIdx);

void WX_DumpSensorInfo(FILE *fd)
{ 
   printTimeDateAndUptime(fd);

   fprintf(fd, "   Messages Processed: %d     Messages With Errors: %d\n",wxData.currentTime.PktCnt, wxData.BadPktCnt);
   
   if (wxData.UnsupportedPktCnt > 0)
     fprintf(fd, "   Count of snapshots without new data: %d\n",wxData.noDataBetweenSnapshots);
   if (wxData.UnsupportedPktCnt > 0)
     fprintf(fd, "   Unsupported Packets: %d\n",wxData.UnsupportedPktCnt);
              
   fprintf(fd,"\n                   Lock  Lock  Code  300 sec.  Snapshot\n");
     fprintf(fd,"   Sensor Name     Code  Mismatches  Timeouts  Timeouts  Last Valid Message Received\n\n");

   char label[80];
   if (WxConfig.oduNameString[0] != 0)
      sprintf(label,"   %s (ODU) ",WxConfig.oduNameString);
   else
      sprintf(label,"   Outdoor Unit  ");
   printSensorStatus(fd, label, wxData.odu.LockCode, wxData.odu.LockCodeMismatchCount, 
             wxData.odu.noDataFor300Seconds, wxData.odu.noDataBetweenSnapshots, &wxData.odu.Timestamp);
   
   if (WxConfig.iduNameString[0] != 0)
      sprintf(label,"   %s (IDU) ",WxConfig.iduNameString);
   else
      sprintf(label,"   Indoor Unit   ");
   printSensorStatus(fd,label, wxData.idu.LockCode, wxData.idu.LockCodeMismatchCount, 
             wxData.idu.noDataFor300Seconds, wxData.idu.noDataBetweenSnapshots, &wxData.idu.Timestamp);
   
   int sensorIdx;
   for (sensorIdx=0;sensorIdx<=MAX_SENSOR_CHANNEL_INDEX;sensorIdx++)
      printExtraSensorStatus(fd, sensorIdx);
        
   printSensorStatus(fd,"   Wind Gauge    ", wxData.wg.LockCode,  wxData.wg.LockCodeMismatchCount,  
          wxData.wg.noDataFor300Seconds, wxData.wg.noDataBetweenSnapshots, &wxData.wg.Timestamp);
   printSensorStatus(fd,"   Rain Gauge    ", wxData.rg.LockCode,  wxData.rg.LockCodeMismatchCount,  
          wxData.rg.noDataFor300Seconds, wxData.rg.noDataBetweenSnapshots, &wxData.rg.Timestamp);
     
   if (WxConfig.sensorLockingEnabled)
     fprintf(fd, "\n   Sensor Locking is ENABLED (edit rtl-wx.conf to change)\n\n");
   else
     fprintf(fd, "\n   Sensor Locking is DISABLED (edit rtl-wx.conf to change)\n\n");
}

void printSensorStatus(FILE *fd,char *str, int lock_code, int lock_code_change_count, int no_data_for_180_secs, int no_data_between_snapshots, WX_Timestamp *ts)
{
  if (ts->PktCnt != 0) {
    fprintf(fd,"%s  ", str);
   if (lock_code == -1)
      fprintf(fd,"                                      ");
   else
      fprintf(fd,"0x%02x     %3d        %3d       %3d     ", lock_code, lock_code_change_count, 
                                                         no_data_for_180_secs, no_data_between_snapshots);
 
   struct tm *localtm = localtime(&ts->timet);
   fprintf(fd, "%02d/%02d/%04d at %02d:%02d:%02d",
      localtm->tm_mon+1, localtm->tm_mday, localtm->tm_year+1900, localtm->tm_hour, localtm->tm_min, localtm->tm_sec);      
   fprintf(fd," (Msg# %d)\n", ts->PktCnt);
   }
}

void printExtraSensorStatus(FILE *fd, int sensorIdx) {
  char label[80];
  if (WxConfig.extNameStrings[sensorIdx][0] != 0)
      sprintf(label,"   %s (Ch%2d)",WxConfig.extNameStrings[sensorIdx], sensorIdx+1);
  else
      sprintf(label,"   Ext Sensor %2d ", sensorIdx+1);
  printSensorStatus(fd,label, wxData.ext[sensorIdx].LockCode, wxData.ext[sensorIdx].LockCodeMismatchCount, 
      wxData.ext[sensorIdx].noDataFor300Seconds, wxData.ext[sensorIdx].noDataBetweenSnapshots, &wxData.ext[sensorIdx].Timestamp);
}

void WX_DumpConfigInfo(FILE *fd)
{
 int i;
 char passwdStr[20];
 int configFileReadFrequency;

 fprintf(fd, "    sensorLockingEnabled: %d\n",WxConfig.sensorLockingEnabled);
 fprintf(fd, "          altitudeInFeet: %d\n",WxConfig.altitudeInFeet);
 fprintf(fd, " configFileReadFrequency: %d\n",WxConfig.configFileReadFrequency);
 fprintf(fd, "   dataSnapshotFrequency: %d\n",WxConfig.dataSnapshotFrequency);
 fprintf(fd, " webcamSnapshotFrequency: %d\n\n",WxConfig.webcamSnapshotFrequency);
 fprintf(fd, "   tagFileParseFrequency: %d\n",WxConfig.tagFileParseFrequency);
 fprintf(fd, "      NumTagFilesToParse: %d\n",WxConfig.NumTagFilesToParse);
 for (i=0;i<WxConfig.NumTagFilesToParse;i++)
    fprintf(fd, "                    %-15s -> %s\n",WxConfig.tagFiles[i].inFile, WxConfig.tagFiles[i].outFile);
 fprintf(fd, "\n");
 fprintf(fd, "      ftpUploadFrequency: %d\n",WxConfig.ftpUploadFrequency);
 fprintf(fd, "       ftpServerHostname: %s\n",WxConfig.ftpServerHostname);
 fprintf(fd, "       ftpServerUsername: %s\n",WxConfig.ftpServerUsername);
 strncpy(passwdStr, WxConfig.ftpServerPassword,20);
 passwdStr[20]=0;
 for (i=2;i<(int) strlen(passwdStr);i++)
    passwdStr[i] = '*';
 fprintf(fd, "       ftpServerPassword: %s\n", passwdStr);
 fprintf(fd, "           numFilesToFtp: %d\n",WxConfig.numFilesToFtp);
 for (i=0;i<WxConfig.numFilesToFtp;i++) 
    fprintf(fd, "                    %-15s -> %s\n",WxConfig.ftpFiles[i].filename, WxConfig.ftpFiles[i].destpath);
 fprintf(fd, "\n");
 /*
 fprintf(fd, "      mailSendFrequency: %d\n",WxConfig.mailSendFrequency);
 fprintf(fd, "     mailServerHostname: %s\n",WxConfig.mailServerHostname);
 fprintf(fd, "     mailServerUsername: %s\n",WxConfig.mailServerUsername);
 fprintf(fd, "     mailServerPassword: %s\n",WxConfig.mailServerPassword);
 fprintf(fd, "      numMailMsgsToSend: %d\n",WxConfig.numMailMsgsToSend);
 for (i=0;i<WxConfig.numMailMsgsToSend;i++)
    fprintf(fd, "                         --%s-%s-%s--\n",WxConfig.mailMsgList[i].subject, 
                                       WxConfig.mailMsgList[i].recipients, WxConfig.mailMsgList[i].bodyFilename);
 fprintf(fd, "\n");
 */
}

void printTimeDateAndUptime(FILE *fd) {
   struct tm *localtm = localtime(&wxData.currentTime.timet);
   fprintf(fd, "   Date: %02d/%02d/%04d    Time: %02d:%02d:%02d",
      localtm->tm_mon+1, localtm->tm_mday, localtm->tm_year+1900, localtm->tm_hour, localtm->tm_min, localtm->tm_sec);   

   time_t timeNow;
   time(&timeNow);
   long int seconds = difftime(timeNow, WX_programStartTime);
      
   int days = seconds / (60*60*24);
   seconds = seconds % (60*60*24);
   
   int hours = seconds / (60*60);
   seconds = seconds % (60*60);
   
   int minutes = seconds / 60;
   seconds = seconds % 60;
   
   if (days > 0)
     fprintf(fd,"    Uptime: %d days, %d hours, %d Minutes\n\n", days, hours, minutes);
   else
     fprintf(fd,"    Uptime: %d hours, %d Minutes\n\n", hours, minutes);
}

BOOL isTimestampPresent(WX_Timestamp *ts) {
  BOOL retVal=1;
  if (ts->PktCnt == 0) // If pkt count is non-zero - data has been stored
      retVal=0;
  return(retVal);
}


