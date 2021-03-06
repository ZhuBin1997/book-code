/**-----------------------------------------------------------
**  TESTMAIN.CPP :  Demonstrates using CFileIO class.
**  Environment:  Windows NT, MS Visual C/C++  (32-bit)
**---------------------------------------------------------**/
#include <windows.h>
#include <string.h>
#include "asynch.h"

#define CHAR_FORMFEED       0x0c;    // formfeed character
#define BUF_SIZE            4096     // arbitrary buffer size

/**-------- private prototypes -----------------**/
BOOL FindNextSpoolFilename(char *);
BOOL SynchronousFileCopy(char *, char *);
BOOL AsynchronousFileCopy(char *, char *);

/**-------- global variables and defines -------**/
char spoolPath[MAX_PATH];
char rootName[MAX_PATH];
char fsName[MAX_PATH];
char fileName[MAX_PATH];
char findFile[MAX_PATH];

/**--------------------------------------------------------**/
VOID main(VOID)
{
    HANDLE hChange;
    DWORD dwWait, dwSPC, dwBPS, dwFC, dwTC;
    DWORDLONG DiskFree;

    // first prompt user for spool directory
    printf("Enter the print spool directory: \n");
    gets(spoolPath);
    if (!SetCurrentDirectory(spoolPath)) {    
        CreateDirectory(spoolPath, NULL);
        SetCurrentDirectory(spoolPath);
    }
    // show a little volume information
    strcpy(rootName, spoolPath);
    rootName[3] = '\0';                 // truncate to "x:\"
   
    GetVolumeInformation(rootName, NULL, 0, NULL, NULL, NULL,
        fsName, MAX_PATH);
    printf("File system for volume %s is %s.\n", rootName, fsName);
    if (GetDiskFreeSpace(rootName, &dwSPC, &dwBPS, &dwFC, &dwTC)) {
        DiskFree = (DWORD)(((DWORDLONG)(dwFC * dwSPC * dwBPS))/1024);
        printf("Total free disk space: %d KB\n", DiskFree);
    }

    // form the search path, {spoolPath}\\*.*
    sprintf(findFile, "%s\\*.*", spoolPath);

    // monitor for create/deleted/renamed files in the spool dir
    if ((hChange = FindFirstChangeNotification(spoolPath, FALSE, 
        FILE_NOTIFY_CHANGE_FILE_NAME)) == INVALID_HANDLE_VALUE)
            ExitProcess(GetLastError());
 
    // now wait (forever!) for notification of that event
    while (TRUE) {
        dwWait = WaitForSingleObject(hChange, INFINITE);
        if (dwWait != WAIT_OBJECT_0) ExitProcess(GetLastError());

        // find the filename
        if(FindNextSpoolFilename(fileName)) {
            // copy the file asynchronously to the printer port
            AsynchronousFileCopy(fileName, "prn");

            // delete the file when done, triggers new search
            DeleteFile(fileName);
        }
        // now wait for the next event of this type
        if (FindNextChangeNotification(hChange) == FALSE)
            ExitProcess(GetLastError());
    } //while

    FindCloseChangeNotification(hChange);
} /* main */

/**--------------------------------------------------------**/
BOOL FindNextSpoolFilename(char *fileName)
{
    WIN32_FIND_DATA lpFind;
    HANDLE hSearch;
    BOOL status = TRUE;

    if ((hSearch = FindFirstFile(findFile, &lpFind))
        == INVALID_HANDLE_VALUE) return FALSE;
    
    // if this is a hidden or system file, keep searching
    while (lpFind.dwFileAttributes != 32 && status)
        status = FindNextFile(hSearch, &lpFind);    
    FindClose(hSearch);

    if (lpFind.dwFileAttributes == 32 && status) {
        sprintf(fileName, "%s\\%s", spoolPath, lpFind.cFileName);
        printf("Spooling file %s to the printer.\n", fileName);
        return TRUE;
    } //if
    return FALSE;
} /* FindNextSpoolFilename */

/**--------------------------------------------------------**/
BOOL SynchronousFileCopy(char *SourceName, char *DestName)
{
    unsigned long ulRead = 1, ulWritten = 1;
    BYTE formFeed = CHAR_FORMFEED;
    BYTE buffer[BUF_SIZE+1];
    DWORD dwStatus;

    CFileIO Source(SourceName, GENERIC_READ, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, &dwStatus);
    if (!dwStatus) return FALSE;

    CFileIO Dest(DestName, GENERIC_WRITE, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, &dwStatus);
    if (!dwStatus) return FALSE;

    while (ulRead != 0)
    {   
      ulRead = Source.Read(buffer, BUF_SIZE);
      ulWritten = Dest.Write(buffer, ulRead);
    }
    //send form feed to printer
    Dest.Write(&formFeed, sizeof(CHAR));
    return TRUE;
} // SynchronousFileCopy 

/**--------------------------------------------------------**/
BOOL AsynchronousFileCopy(char *SourceName, char *DestName)
{
    DWORD dwStatus=TRUE, dwRead=0, dwWritten=0, dwPend=0;
    BOOL bMore = FALSE;    
    BYTE formFeed = CHAR_FORMFEED;
    BYTE buf1[BUF_SIZE+1], buf2[BUF_SIZE+1], buf3[BUF_SIZE+1];
    
    CFileIO Source(SourceName,GENERIC_READ, OPEN_ALWAYS,
        FILE_FLAG_OVERLAPPED, &dwStatus);
    if (!dwStatus) return FALSE;

    CFileIO Dest(DestName, GENERIC_WRITE, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, &dwStatus);
    if (!dwStatus) return FALSE;
             
    LPBYTE pRead = buf1;
    LPBYTE pWrite = buf2;
    LPBYTE pSwing = buf3;
    LPBYTE pTemp = buf1;
 
    // read the first buffer from the source file
    Source.ReadWithAbandon(pRead, BUF_SIZE);
   
    while (!bMore)
    {
        // wait for pending read from source file
        while (!Source.IOComplete(TRUE, &dwRead));

        // swap buffer pointers
        pTemp = pRead; pRead = pSwing; pSwing = pTemp;

        // if more to read, start the next read from source file
        bMore = Source.ReadWithAbandon(pRead, BUF_SIZE);

        // wait for pending write to destination file
        while (!Dest.IOComplete(TRUE, &dwWritten));
        
        // swap buffer pointers
        pTemp = pWrite; pWrite = pSwing; pSwing = pTemp;

        // start the next write to parallel port
        Dest.WriteWithAbandon(pWrite, dwRead);
    } //while
       
    //send form feed to printer, wait until it's done
    Dest.WriteWithAbandon(&formFeed, 1);
    while (!Dest.IOComplete(TRUE, &dwWritten));
    return TRUE;
} // AsynchronousFileCopy
// End of File 
