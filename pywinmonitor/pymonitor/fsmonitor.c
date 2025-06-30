#include "fsmonitor.h"


static bool closeFSEntries(const void *item, void *udata);

static inline char* wcharToMultiByte(LPCWSTR wideStr);

static inline WCHAR* multiByteToWCHAR(const char* str);

static inline DWORD checkAction(fs_entry* fe, size_t action);

extern monitor_thread* MONITOR_THREAD = NULL;

bool StartThread() 
{
    if (!MONITOR_THREAD->data) 
        MonitorThreadInit();

    if (!MONITOR_THREAD->thread) {
        MONITOR_THREAD->thread = CreateThread(NULL, 0, FsEventsMonitor, MONITOR_THREAD->data, 0, NULL);
        MONITOR_THREAD->status = MONITOR_THREAD->thread != NULL;
    }
}

void MonitorThreadInit() 
{
    if(!MONITOR_THREAD) {
        MONITOR_THREAD = (monitor_thread*)calloc(1, sizeof(monitor_thread));
        MONITOR_THREAD->status = false;
    }
    
    if (!MONITOR_THREAD->data) {
        MONITOR_THREAD->data = (monitor_data*)calloc(1, sizeof(monitor_data));
        CREATE_KEY_VALUE_HASHMAP(MONITOR_THREAD->data->files)
    }
}

bool ModifyFSEvent(const char *path, fs_entry* fe) 
{
    if (!MONITOR_THREAD->status)
        return false;

    CancelIo(fe->descriptor);        // Cancel any pending ReadDirectoryChangesW
    CloseHandle(fe->descriptor);     // Close the handle to stop monitoring
    if (!fe->events) {
        WCHAR* wstr = multiByteToWCHAR(path);
        
        fe->descriptor = CreateFileW(
            wstr, // Directory to monitor
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL
        );

        CreateIoCompletionPort(
            fe->descriptor, 
            MONITOR_THREAD->data->iocp, 
            (ULONG_PTR)fe->descriptor, 
            1
        );

        ReadDirectoryChangesW(
            fe->descriptor,
            MONITOR_THREAD->data->buffer,
            sizeof(MONITOR_THREAD->data->buffer),
            TRUE, // watch subtree
            fe->events,
            NULL,
            &MONITOR_THREAD->data->overlapped,
            NULL
        );
    }
    return true;
}

fs_entry* 
AddFSEvent(const char *path, size_t events, fs_callback cbk, void* cbkArgs) 
{
    if (!MONITOR_THREAD)
        MonitorThreadInit();
        
    hashmap map = MONITOR_THREAD->data->files;
    
    key_value* kv = hashmap_get(map, &(key_value){ path });
    if (kv) {
        fs_entry* fe = (fs_entry*)kv->value;
        fe->events |= events;
        if (cbk) {
            fe->callback = cbk;
            fe->args = cbkArgs;
        }
    }
    else {
        fs_entry* fe = calloc(1, sizeof(fs_entry));
        fe->events |= events;
        fe->callback = cbk;
        fe->args = cbkArgs;
        kv = hashmap_set(map, &(key_value){ path, fe });
    }

    if (MONITOR_THREAD->status)
        ModifyFSEvent(path, (fs_entry*)kv->value);
    return (fs_entry*)kv->value;
}

void DeleteFSEvent(const char *path, size_t events) 
{
    if (!MONITOR_THREAD) return;
    
    hashmap map = MONITOR_THREAD->data->files;
    key_value* kv = hashmap_get(map, &(key_value){ .key=path });
    if (!map || !kv) 
        return;

    fs_entry* fe = (fs_entry*)kv->value;
    fe->events ^= events;

    if (MONITOR_THREAD->status)
        ModifyFSEvent(path, fe);
        
    if (!fe->events)
        hashmap_delete(map, &(key_value){ .key=path });
}

DWORD WINAPI FsEventsMonitor(void* data) 
{   
    monitor_data *md = data;
    md->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    BYTE* buffer = md->buffer;
    DWORD* bytesReturned = &md->bytesReturned;
    OVERLAPPED overlapped = md->overlapped;

    // Setting async ReadDirectoryChangesW
    void *item;
    size_t iter = 0;
    while (hashmap_iter(md->files, &iter, &item)) {
        key_value* kv = (key_value*)item;
        const char* path = kv->key;
        fs_entry *fe = kv->value;
        fe->descriptor = CreateFileW(
            multiByteToWCHAR(path),   // Directory to monitor
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL
        );
        if (fe->descriptor == INVALID_HANDLE_VALUE) 
            break;

        HANDLE checkIo = CreateIoCompletionPort(
            fe->descriptor, 
            md->iocp, 
            (ULONG_PTR)path, 
            0
        );
        if (checkIo == INVALID_HANDLE_VALUE) 
            break;

        bool check = ReadDirectoryChangesW(
            fe->descriptor,
            buffer,
            sizeof(buffer),
            TRUE, // watch subtree
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            NULL,
            &overlapped,
            NULL
        );
        if (!check) 
            break;
    }
    if (iter < hashmap_count(md->files)) {
        hashmap_scan(md->files, closeFSEntries, NULL);
        return false;
    }
    
    // Worker thread loop
    while (MONITOR_THREAD->status) {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        LPOVERLAPPED pOverlapped;
        BOOL success = GetQueuedCompletionStatus(
            md->iocp, 
            &bytesTransferred, 
            &completionKey, 
            &pOverlapped, 
            INFINITE
        );
        if (success) {
            printf("TRUE\n");
            DWORD offset = 0;
            FILE_NOTIFY_EXTENDED_INFORMATION *cur_event;
            const char* path = (char*)completionKey;
            key_value* kv = hashmap_get(md->files, &(key_value){path});
            fs_entry *fe = kv->value;
            do {
                cur_event = (FILE_NOTIFY_EXTENDED_INFORMATION *)&buffer[offset];
                char filename[256] = {0};
                int filenameLength = WideCharToMultiByte(CP_ACP, 0, cur_event->FileName, cur_event->FileNameLength/2, NULL, 0, NULL, NULL);
                WideCharToMultiByte(CP_ACP, 0, cur_event->FileName, cur_event->FileNameLength/2, filename, sizeof(filename)-1, NULL, NULL);
                
                if (fe->callback && checkAction(fe, cur_event->Action)) {
                    event_info info = (event_info) {
                        .event = cur_event->Action, 
                        .path = fe->path, 
                        .info = cur_event
                    };
                    fe->callback(fe->args, &info);
                }

                offset += cur_event->NextEntryOffset;   
            } 
            while (cur_event->NextEntryOffset != 0);

            // Process notifications in buffer associated with pOverlapped
            // Reissue ReadDirectoryChangesW for the directory handle (completionKey)
            ReadDirectoryChangesW(
                fe->descriptor,
                buffer,
                sizeof(buffer),
                TRUE, // watch subtree
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                NULL,
                &overlapped,
                NULL
            );
        }
    }

    hashmap_scan(md->files, closeFSEntries, NULL);
    
    return 0;
}

static bool closeFSEntries(const void *item, void *udata) 
{
    key_value *file = item;
    fs_entry *fe = file->value;
    if (fe->descriptor) {
        CancelIo(fe->descriptor);        // Cancel any pending ReadDirectoryChangesW
        CloseHandle(fe->descriptor);     // Close the handle to stop monitoring
        fe->descriptor = NULL;
    }
    return true;
}

static inline WCHAR* multiByteToWCHAR(const char* str) 
{
    int srcLen = strlen(str) + 1; // +1 for null terminator
    int wlen = MultiByteToWideChar(CP_ACP, 0, str, srcLen, NULL, 0);
    WCHAR* wstr = malloc(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, str, srcLen, wstr, wlen); 
    return wstr;
}

static inline char* wcharToMultiByte(LPCWSTR wideStr) 
{
    if (!wideStr) return NULL;

    // Get the required buffer size (in bytes) for the converted string
    int sizeNeeded = WideCharToMultiByte(
        CP_UTF8,            // Code page for UTF-8 output; use CP_ACP for system default ANSI
        0,                  // Flags
        wideStr,            // Input wide string
        -1,                 // Null-terminated input
        NULL,               // No output buffer yet
        0,                  // Request buffer size
        NULL, NULL
    );

    if (sizeNeeded == 0) {
        // Error handling
        return NULL;
    }

    // Allocate buffer for converted string
    char* buffer = (char*)malloc(sizeNeeded);
    if (!buffer) return NULL;

    // Perform the actual conversion
    int convertedChars = WideCharToMultiByte(
        CP_UTF8,
        0,
        wideStr,
        -1,
        buffer,
        sizeNeeded,
        NULL,
        NULL);

    if (convertedChars == 0) {
        // Conversion failed
        free(buffer);
        return NULL;
    }

    return buffer; // Caller must free this buffer when done
}

static inline DWORD checkAction(fs_entry* fe, size_t action)
{
    size_t mask = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_CREATION;
    bool action_check = action&FILE_ACTION_ADDED || 
                        action&FILE_ACTION_REMOVED || 
                        action&FILE_ACTION_RENAMED_OLD_NAME || 
                        action&FILE_ACTION_RENAMED_NEW_NAME;
    if ((fe->events&mask) && action_check) // FILE_ACTION_ADDED
        return true;

    mask = FILE_NOTIFY_CHANGE_SIZE |
           FILE_NOTIFY_CHANGE_DIR_NAME | 
           FILE_NOTIFY_CHANGE_SECURITY | 
           FILE_NOTIFY_CHANGE_CREATION | 
           FILE_NOTIFY_CHANGE_LAST_WRITE | 
           FILE_NOTIFY_CHANGE_ATTRIBUTES |
           FILE_NOTIFY_CHANGE_LAST_ACCESS;
                  
    if (fe->events&mask && (action&FILE_ACTION_MODIFIED))
        return true;

    return false;
}