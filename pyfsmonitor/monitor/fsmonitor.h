#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <winbase.h>
#include <processthreadsapi.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "tools/kvmap.h"

#ifndef FILEMONITOR_H
#define FILEMONITOR_H


typedef struct _event_info{
    size_t event;
    char* path;
    FILE_NOTIFY_EXTENDED_INFORMATION* info;
} event_info;


typedef void (*fs_callback)(void*, event_info*);


typedef struct _fs_entry {
    char* path;
    size_t events;
    HANDLE descriptor;
    fs_callback callback;
    void* args;
} fs_entry;


typedef struct _monitor_data {
    BYTE buffer[2048];
    DWORD bytesReturned;
    OVERLAPPED overlapped; // must = {0} as a start value;
    HANDLE iocp;
    hashmap files; // monitored files and folders
} monitor_data;

typedef struct _monitor_thread {
    monitor_data* data;
    BOOL status;
    HANDLE thread;
} monitor_thread;

extern monitor_thread* MONITOR_THREAD;

bool StartThread();

void MonitorThreadInit();

bool ModifyFSEvent(const char *path, fs_entry* fe);

fs_entry* 
AddFSEvent(const char *path, size_t events, fs_callback cbk, void* cbkArgs);

void DeleteFSEvent(const char *path, size_t events);

DWORD WINAPI FsEventsMonitor(void* data);

#endif
