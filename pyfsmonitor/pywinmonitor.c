#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <objimpl.h>

#define STRINGIFY(A) #A
#define CONCAT(A, B) A##B
#define MONITOR_NAME monitor

#include "monitor/fsmonitor.h"



static PyObject *start_monitor(PyObject *self, PyObject *args) {
    MonitorThreadInit();

    if (!StartThread())
        Py_RETURN_FALSE;
    
    Py_RETURN_TRUE;
}

void callback_decorator(void* args, event_info* info) {
    if (!args) return;
    PyObject* pyCallback = args;
    PyObject *pyArgs = PyTuple_New(2);

    PyTuple_SET_ITEM(args, 0, PyUnicode_FromString(info->path));
    PyTuple_SET_ITEM(args, 1, PyLong_FromUnsignedLongLong(info->event));

    PyObject_CallObject(pyCallback, pyArgs);
}

static PyObject *add_fs_event(PyObject *self, PyObject *args) {
    char* path;
    size_t events;
    PyObject *callback;

    if (!PyArg_ParseTuple(args, "siO", &path, &events))
        return Py_None;

    if (callback && !PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return Py_None;
    } 

    fs_entry *fe = AddFSEvent(path, events, callback_decorator, callback);
    if (!fe) 
        PyErr_SetString(PyExc_TypeError, "ENTRY DOESN'T INSERTED");
    
    return Py_None;
}

static PyObject *delete_fs_event(PyObject *self, PyObject *args) {
    char* path;
    size_t events;
    PyObject *callback;

    if (!PyArg_ParseTuple(args, "siO", &path, &events))
        return Py_None;
    return Py_None;
}

static PyObject *is_monitored(PyObject *self, PyObject *args) {
    char* path;
    if (!PyArg_ParseTuple(args, "s", &path))
        return Py_None;

    if (MONITOR_THREAD->data->files && 
        hashmap_get(MONITOR_THREAD->data->files, &(key_value){.key=path}))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *fs_entries(PyObject *self, PyObject *args) {
    if (!MONITOR_THREAD->data->files)
        return PyDict_New();

    
    void* item;
    size_t iter = 0;
    PyObject *fs_entries = PyDict_New();
    while (hashmap_iter(MONITOR_THREAD->data->files, &iter, &item)) {
        key_value* kv = item;

        PyDict_SetItemString(
            fs_entries, 
            kv->key,
            PyLong_FromUnsignedLongLong((CAST_VALUE(kv, fs_entry*))->events)
        );
    }
    return fs_entries;
}

// static PyObject *pppp(PyObject *self, PyObject *args) {
//     printf("HELLO HELLO HELLO\n");
//     return Py_None;
// }

static PyMethodDef CONCAT(MONITOR_NAME, _methods)[] = {
    // Method definition structure
    {"start_monitor",   start_monitor,   METH_VARARGS, "start monitor thread"},
    {"add_file_event",  add_fs_event,    METH_VARARGS, "Add "},
    {"delete_fs_entry", delete_fs_event, METH_VARARGS, "Function from C with arguments"},
    {"is_monitored",    is_monitored,    METH_VARARGS, "Function from C with arguments"},
    {"fs_entries",      fs_entries,      METH_VARARGS, "Function from C with arguments"},
    // {"pppp",            pppp,            METH_VARARGS, "Function from C with arguments"},
    {NULL, NULL, 0, NULL} // Sentinel
};

// Module definition to register the module
static struct PyModuleDef MONITOR_NAME = {
    PyModuleDef_HEAD_INIT,
    
    STRINGIFY(MONITOR_NAME), // Module name
    STRINGIFY(MONITOR_NAME) " documantation",             // Module documentation
    -1,               // Size of per-interpreter state
    CONCAT(MONITOR_NAME, _methods), 
    NULL,
    NULL, 
    NULL, 
    NULL
};  

// Module initialization
PyMODINIT_FUNC CONCAT(PyInit_, MONITOR_NAME)(void) {
    // printf("MODULE WORKS\n");
    return PyModule_Create(&win_monitor);
}
