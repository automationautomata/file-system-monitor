# PYTHON FILESYSTEM MONITOR
Handle files and folders events with a callbacks

## Windows 
Based on WinAPI (*ReadDirectoryChangesW* and *IoCompletionPort*)

## Linux 
Based on inotify

#### How to Build
```
poetry build
pip install -e .
```