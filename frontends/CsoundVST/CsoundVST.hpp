/*
 * C S O U N D   V S T
 *
 * A VST plugin version of Csound, with Python scripting.
 *
 * L I C E N S E
 *
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __CSOUNDVST_H
#define __CSOUNDVST_H

#include "Platform.hpp"
#ifdef SWIG

%module CsoundVST
%{
#include "CppSound.hpp"
#include "Shell.hpp"
#include <list>
  %}

#else
// Hack to compile all this GNU stuff on Windows.
#ifdef _MSC_VER
#include <windows.h>
#include <mmsystem.h>
#endif

#include "audioeffectx.h"
#include "CppSound.hpp"
#include "Shell.hpp"
#include <list>

#endif

class SILENCE_PUBLIC CsoundVstFltk;

class SILENCE_PUBLIC Preset
{
public:
  std::string name;
  std::string text;
};

class SILENCE_PUBLIC CsoundVST :
  public AudioEffectX,
  public csound::Shell
{
public:
  enum
    {
      kNumInputs = 2
    };
  enum
    {
      kNumOutputs = 2
    };
  enum
    {
      kNumPrograms = 10
    };
  static double inputScale;
  static double outputScale;
  /**
   * The thread that calls Fl::wait().
   */
  static void *fltkWaitThreadId;
  CppSound cppSound_;
  CppSound *cppSound;
  bool isSynth;
  bool isVst;
  bool isPython;
  bool isMultiThreaded;
  bool isAutoPlayback;
  size_t csoundFrameI;
  size_t csoundLastFrame;
  size_t channelI;
  size_t channelN;
  size_t hostFrameI;
  float vstSr;
  float vstCurrentSampleBlockStart;
  float vstCurrentSampleBlockEnd;
  float vstCurrentSamplePosition;
  float vstPriorSamplePosition;
  CsoundVstFltk *csoundVstFltk;
  std::list<VstMidiEvent> midiEventQueue;
  std::vector<Preset> bank;
  // AudioEffectX overrides.
  CsoundVST(audioMasterCallback audioMaster);
  virtual ~CsoundVST();
  virtual AEffEditor *getEditor();
  virtual bool getEffectName(char* name);
  virtual bool getVendorString(char* name);
  virtual bool getProductString(char* name);
  virtual long canDo(char* text);
  virtual bool getInputProperties(long index, VstPinProperties* properties);
  virtual bool getOutputProperties(long index, VstPinProperties* properties);
  virtual bool keysRequired();
  virtual long getProgram();
  virtual void setProgram(long program);
  virtual void setProgramName(char *name);
  virtual void getProgramName(char *name);
  virtual bool copyProgram(long destination);
  virtual bool getProgramNameIndexed(long category, long index, char* text);
  virtual long getChunk(void** data, bool isPreset);
  virtual long setChunk(void* data, long byteSize, bool isPreset);
  virtual void suspend();
  virtual void resume();
  virtual long processEvents(VstEvents *vstEvents);
  virtual void process(float **inputs, float **outputs, long sampleFrames);
  virtual void processReplacing(float **inputs, float **outputs, long sampleFrames);

  // Shell overrides.
  virtual void open();
  // Peculiar to CsoundVST.
  CsoundVST();
  virtual CppSound *getCppSound();
  virtual bool getIsSynth() const;
  virtual void setIsSynth(bool isSynth);
  virtual bool getIsVst() const;
  virtual void setIsVst(bool isSynth);
  virtual bool getIsPython() const;
  virtual void setIsPython(bool isPython);
  virtual void performanceThreadRoutine();
  virtual int perform();
  virtual std::string getText();
  virtual void setText(const std::string text);
  virtual void synchronizeScore();
  virtual void reset();
  virtual void openFile(std::string filename);
  virtual int run();
  virtual void openView(bool doRun = true);
  virtual void closeView();
  virtual bool getIsMultiThreaded() const;
  virtual void setIsMultiThreaded(bool isMultiThreaded);
  virtual bool getIsAutoPlayback() const;
  virtual void setIsAutoPlayback(bool autoPlay);
  virtual void fltklock();
  virtual void fltkunlock();
  virtual void fltkflush();
  virtual void fltkwait();
  static int midiDeviceOpen(CSOUND *csound, void **userData,
                            const char *devName);

  static int midiRead(CSOUND *csound, void *userData,
                      unsigned char *buf, int nbytes);
};

#if !defined(SWIGJAVA)

extern "C"
{
   CsoundVST* SILENCE_PUBLIC CreateCsoundVST();
}

#endif

#endif

