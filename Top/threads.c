/*
    threads.c:

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "csoundCore.h"

#if defined(WIN32) && !defined(__CYGWIN__)

#include <windows.h>
#include <process.h>

typedef struct {
    uintptr_t   (*func)(void *);
    void        *userdata;
    HANDLE      threadLock;
} threadParams;

static unsigned int __stdcall threadRoutineWrapper(void *p)
{
    uintptr_t (*threadRoutine)(void *);
    void      *userData;

    threadRoutine = ((threadParams*) p)->func;
    userData = ((threadParams*) p)->userdata;
    SetEvent(((threadParams*) p)->threadLock);
    return (unsigned int) threadRoutine(userData);
}

PUBLIC void *csoundCreateThread(uintptr_t (*threadRoutine)(void *),
                                void *userdata)
{
    threadParams  p;
    void          *h;
    unsigned int  threadID;

    p.func = threadRoutine;
    p.userdata = userdata;
    p.threadLock = CreateEvent(0, 0, 0, 0);
    if (p.threadLock == (HANDLE) 0)
      return NULL;
    h = (void*) _beginthreadex(NULL, (unsigned) 0, threadRoutineWrapper,
                               (void*) &p, (unsigned) 0, &threadID);
    if (h != NULL)
      WaitForSingleObject(p.threadLock, INFINITE);
    CloseHandle(p.threadLock);
    return h;
}

PUBLIC uintptr_t csoundJoinThread(void *thread)
{
    DWORD   retval = (DWORD) 0;
    WaitForSingleObject((HANDLE) thread, INFINITE);
    GetExitCodeThread((HANDLE) thread, &retval);
    CloseHandle((HANDLE) thread);
    return (uintptr_t) retval;
}

PUBLIC void *csoundCreateThreadLock(void)
{
    HANDLE threadLock = CreateEvent(0, 0, TRUE, 0);
    return (void*) threadLock;
}

PUBLIC void csoundWaitThreadLock(void *lock, size_t milliseconds)
{
    WaitForSingleObject((HANDLE) lock, milliseconds);
}

PUBLIC void csoundNotifyThreadLock(void *lock)
{
    SetEvent((HANDLE) lock);
}

PUBLIC void csoundDestroyThreadLock(void *lock)
{
    CloseHandle((HANDLE) lock);
}

/* internal functions for csound.c */

static  HANDLE  cs_mutex = (HANDLE) 0;

void csoundLock(void)
{
    if (cs_mutex == (HANDLE) 0)
      cs_mutex = CreateEvent(0, 0, 0, 0);
    else
      WaitForSingleObject(cs_mutex, 10000);
}

void csoundUnLock(void)
{
    if (cs_mutex != (HANDLE) 0)
      SetEvent(cs_mutex);
}

#elif defined(LINUX) || defined(__CYGWIN__) || defined(__MACH__)

#include <pthread.h>

PUBLIC void *csoundCreateThread(uintptr_t (*threadRoutine)(void *),
                                void *userdata)
{
    pthread_t pthread = 0;
    if (!pthread_create(&pthread, (pthread_attr_t*) NULL,
                        (void *(*)(void *)) threadRoutine, userdata)) {
      return (void*) pthread;
    }
    return NULL;
}

PUBLIC uintptr_t csoundJoinThread(void *thread)
{
    pthread_t pthread = (pthread_t) thread;
    void      *threadRoutineReturnValue = NULL;
    int       pthreadReturnValue;

    pthreadReturnValue = pthread_join(pthread, &threadRoutineReturnValue);
    if (pthreadReturnValue)
      return (uintptr_t) ((intptr_t) pthreadReturnValue);
    return (uintptr_t) threadRoutineReturnValue;
}

PUBLIC void *csoundCreateThreadLock(void)
{
    pthread_mutex_t *pthread_mutex;

    pthread_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex == NULL)
      return NULL;
    if (pthread_mutex_init(pthread_mutex, NULL) != 0) {
      free(pthread_mutex);
      return NULL;
    }
    return (void*) pthread_mutex;
}

PUBLIC void csoundWaitThreadLock(void *lock, size_t milliseconds)
{
    /* TODO: implement timeout */
    (void) milliseconds;
    pthread_mutex_lock((pthread_mutex_t*) lock);
}

PUBLIC void csoundNotifyThreadLock(void *lock)
{
    pthread_mutex_unlock((pthread_mutex_t*) lock);
}

PUBLIC void csoundDestroyThreadLock(void *lock)
{
    pthread_mutex_destroy((pthread_mutex_t*) lock);
    free(lock);
}

/* internal functions for csound.c */

static  pthread_mutex_t cs_mutex = PTHREAD_MUTEX_INITIALIZER;

void csoundLock(void)
{
    pthread_mutex_lock(&cs_mutex);
}

void csoundUnLock(void)
{
    pthread_mutex_unlock(&cs_mutex);
}

#else

PUBLIC void *csoundCreateThread(uintptr_t (*threadRoutine)(void *),
                                void *userdata)
{
    fprintf(stderr,
            "csoundCreateThread() is not implemented on this platform.\n");
    return NULL;
}

PUBLIC uintptr_t csoundJoinThread(void *thread)
{
    fprintf(stderr,
            "csoundJoinThread() is not implemented on this platform.\n");
    return (uintptr_t) 0;
}

PUBLIC void *csoundCreateThreadLock(void)
{
    fprintf(stderr,
            "csoundCreateThreadLock() is not implemented on this platform.\n");
    return NULL;
}

PUBLIC void csoundWaitThreadLock(void *lock, size_t milliseconds)
{
    fprintf(stderr,
            "csoundWaitThreadLock() is not implemented on this platform.\n");
}

PUBLIC void csoundNotifyThreadLock(void *lock)
{
    fprintf(stderr,
            "csoundNotifyThreadLock() is not implemented on this platform.\n");
}

PUBLIC void csoundDestroyThreadLock(void *lock)
{
    fprintf(stderr,
            "csoundDestroyThreadLock() is not implemented on this platform.\n");
}

/* internal functions for csound.c */

void csoundLock(void)
{
}

void csoundUnLock(void)
{
}

#endif

