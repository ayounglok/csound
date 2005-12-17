/*
    csPerfThread.hpp:

    Copyright (C) 2005 Istvan Varga

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

#ifndef CSOUND_CSPERFTHREAD_HPP
#define CSOUND_CSPERFTHREAD_HPP

class CsoundPerformanceThreadMessage;
class CsPerfThread_PerformScore;

/**
 * CsoundPerformanceThread(Csound *)
 * CsoundPerformanceThread(CSOUND *)
 *
 * Performs a score in a separate thread until the end of score is reached,
 * the playback (which is paused by default) is stopped by calling
 * CsoundPerformanceThread::Stop(), or an error occurs.
 * The constructor takes a Csound instance pointer as argument; it assumes
 * that csoundCompile() was called successfully before creating the
 * performance thread. Once the playback is stopped for one of the above
 * mentioned reasons, the performance thread calls csoundCleanup() and
 * returns.
 */

class CsoundPerformanceThread {
 private:
    volatile CsoundPerformanceThreadMessage *firstMessage;
    CsoundPerformanceThreadMessage *lastMessage;
    CSOUND  *csound;
    void    *queueLock;
    void    *pauseLock;
    void    *flushLock;
    void    *perfThread;
    int     paused;
    int     status;
    // --------
    int  Perform();
    void csPerfThread_constructor();
    void QueueMessage(CsoundPerformanceThreadMessage *);
 public:
    /**
     * Returns the Csound instance pointer.
     */
    CSOUND *GetCsound()
    {
      return csound;
    }
    /**
     * Returns the current status, zero if still playing, positive if
     * the end of score was reached or performance was stopped, and
     * negative if an error occured.
     */
    int GetStatus()
    {
      return status;
    }
    /**
     * Continues performance if it was paused.
     */
    void Play();
    /**
     * Pauses performance (can be continued by calling Play()).
     */
    void Pause();
    /**
     * Pauses performance unless it is already paused, in which case
     * it is continued.
     */
    void TogglePause();
    /**
     * Stops performance (cannot be continued).
     */
    void Stop();
    /**
     * Sends a score event of type 'opcod' (e.g. 'i' for a note event), with
     * 'pcnt' p-fields in array 'p' (p[0] is p1). If absp2mode is non-zero,
     * the start time of the event is measured from the beginning of
     * performance, instead of the default of relative to the current time.
     */
    void ScoreEvent(int absp2mode, char opcod, int pcnt, const MYFLT *p);
    /**
     * Sends a score event as a string, similarly to line events (-L).
     */
    void InputMessage(const char *s);
    /**
     * Sets the playback time pointer to the specified value (in seconds).
     */
    void SetScoreOffsetSeconds(double timeVal);
    /**
     * Waits until the performance is finished or fails, and returns a
     * positive value if the end of score was reached or Stop() was called,
     * and a negative value if an error occured. Also releases any resources
     * associated with the performance thread object.
     */
    int Join();
    /**
     * Waits until all pending messages (pause, send score event, etc.)
     * are actually received by the performance thread.
     */
    void FlushMessageQueue();
    // --------
    CsoundPerformanceThread(Csound *);
    CsoundPerformanceThread(CSOUND *);
    ~CsoundPerformanceThread();
    // --------
    friend class CsoundPerformanceThreadMessage;
    friend class CsPerfThread_PerformScore;
};

#endif  // CSOUND_CSPERFTHREAD_HPP

