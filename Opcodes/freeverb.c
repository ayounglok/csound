/*
    freeverb.c:

    Copyright (C) 2005 Istvan Varga (based on public domain C++ code by Jezar)

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

#include "csdl.h"

#define DEFAULT_SRATE   44100.0
#define STEREO_SPREAD   23.0

#define NR_COMB         8
#define NR_ALLPASS      4

static const double comb_delays[NR_COMB][2] = {
    { 1116.0 / DEFAULT_SRATE, (1116.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 1188.0 / DEFAULT_SRATE, (1188.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 1277.0 / DEFAULT_SRATE, (1277.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 1356.0 / DEFAULT_SRATE, (1356.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 1422.0 / DEFAULT_SRATE, (1422.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 1491.0 / DEFAULT_SRATE, (1491.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 1557.0 / DEFAULT_SRATE, (1557.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 1617.0 / DEFAULT_SRATE, (1617.0 + STEREO_SPREAD) / DEFAULT_SRATE }
};

static const double allpass_delays[NR_ALLPASS][2] = {
    { 556.0 / DEFAULT_SRATE, (556.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 441.0 / DEFAULT_SRATE, (441.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 341.0 / DEFAULT_SRATE, (341.0 + STEREO_SPREAD) / DEFAULT_SRATE },
    { 225.0 / DEFAULT_SRATE, (225.0 + STEREO_SPREAD) / DEFAULT_SRATE }
};

static const double fixedGain   = 0.015;
static const double scaleDamp   = 0.4;
static const double scaleRoom   = 0.28;
static const double offsetRoom  = 0.7;

static const double allPassFeedBack = 0.5;

typedef struct {
    int     nSamples;
    int     bufPos;
    double  filterState;
    MYFLT   buf[1];
} freeVerbComb;

typedef struct {
    int     nSamples;
    int     bufPos;
    MYFLT   buf[1];
} freeVerbAllPass;

typedef struct {
    OPDS            h;
    MYFLT           *aOutL;
    MYFLT           *aOutR;
    MYFLT           *aInL;
    MYFLT           *aInR;
    MYFLT           *kRoomSize;
    MYFLT           *kDampFactor;
    MYFLT           *iSampleRate;
    MYFLT           *iSkipInit;
    freeVerbComb    *Comb[NR_COMB][2];
    freeVerbAllPass *AllPass[NR_ALLPASS][2];
    MYFLT           *tmpBuf;
    AUXCH           auxData;
} FREEVERB;

static int calc_nsamples(FREEVERB *p, double delTime)
{
    double  sampleRate;
    sampleRate = (double) *(p->iSampleRate);
    if (sampleRate <= 0.0)
      sampleRate = DEFAULT_SRATE;
    return (int) (delTime * sampleRate + 0.5);
}

static int comb_nbytes(FREEVERB *p, double delTime)
{
    int nbytes;
    nbytes = (int) sizeof(freeVerbComb) - (int) sizeof(MYFLT);
    nbytes += ((int) sizeof(MYFLT) * calc_nsamples(p, delTime));
    return ((nbytes + 15) & (~15));
}

static int allpass_nbytes(FREEVERB *p, double delTime)
{
    int nbytes;
    nbytes = (int) sizeof(freeVerbAllPass) - (int) sizeof(MYFLT);
    nbytes += ((int) sizeof(MYFLT) * calc_nsamples(p, delTime));
    return ((nbytes + 15) & (~15));
}

static int freeverb_init(ENVIRON *csound, FREEVERB *p)
{
    int             i, j, k, nbytes;
    freeVerbComb    *combp;
    freeVerbAllPass *allpassp;
    /* calculate the total number of bytes to allocate */
    nbytes = 0;
    for (i = 0; i < NR_COMB; i++) {
      nbytes += comb_nbytes(p, comb_delays[i][0]);
      nbytes += comb_nbytes(p, comb_delays[i][1]);
    }
    for (i = 0; i < NR_ALLPASS; i++) {
      nbytes += allpass_nbytes(p, allpass_delays[i][0]);
      nbytes += allpass_nbytes(p, allpass_delays[i][1]);
    }
    nbytes += (int) sizeof(MYFLT) * (int) csound->ksmps_;
    /* allocate space if size has changed */
    if (nbytes != (int) p->auxData.size)
      auxalloc(csound, (long) nbytes, &(p->auxData));
    else if (*(p->iSkipInit) != FL(0.0))    /* skip initialisation */
      return OK;                            /*   if requested      */
    /* set up comb and allpass filters */
    nbytes = 0;
    for (i = 0; i < (NR_COMB << 1); i++) {
      combp = (freeVerbComb*) ((unsigned char*) p->auxData.auxp + (int) nbytes);
      p->Comb[i >> 1][i & 1] = combp;
      k = calc_nsamples(p, comb_delays[i >> 1][i & 1]);
      combp->nSamples = k;
      combp->bufPos = 0;
      combp->filterState = 0.0;
      for (j = 0; j < k; j++)
        combp->buf[j] = FL(0.0);
      nbytes += comb_nbytes(p, comb_delays[i >> 1][i & 1]);
    }
    for (i = 0; i < (NR_ALLPASS << 1); i++) {
      allpassp = (freeVerbAllPass*) ((unsigned char*) p->auxData.auxp
                                     + (int) nbytes);
      p->AllPass[i >> 1][i & 1] = allpassp;
      k = calc_nsamples(p, allpass_delays[i >> 1][i & 1]);
      allpassp->nSamples = k;
      allpassp->bufPos = 0;
      for (j = 0; j < k; j++)
        allpassp->buf[j] = FL(0.0);
      nbytes += allpass_nbytes(p, allpass_delays[i >> 1][i & 1]);
    }
    p->tmpBuf = (MYFLT*) ((unsigned char*) p->auxData.auxp + (int) nbytes);
    return OK;
}

static int freeverb_perf(ENVIRON *csound, FREEVERB *p)
{
    double          feedback, damp1, damp2, x;
    freeVerbComb    *combp;
    freeVerbAllPass *allpassp;
    int             i, n;

    /* check if opcode was correctly initialised */
    if (p->auxData.size <= 0L || p->auxData.auxp == NULL) {
      perferror(Str("freeverb: not initialised"));
      return NOTOK;
    }
    /* calculate reverb parameters */
    feedback = (double) *(p->kRoomSize) * scaleRoom + offsetRoom;
    damp1 = (double) *(p->kDampFactor) * scaleDamp;
    damp2 = 1.0 - damp1;
    /* comb filters (left channel) */
    for (n = 0; n < csound->ksmps_; n++)
      p->tmpBuf[n] = FL(0.0);
    for (i = 0; i < NR_COMB; i++) {
      combp = p->Comb[i][0];
      for (n = 0; n < csound->ksmps_; n++) {
        p->tmpBuf[n] += combp->buf[combp->bufPos];
        x = (double) combp->buf[combp->bufPos];
        combp->filterState = (combp->filterState * damp1) + (x * damp2);
        x = combp->filterState * feedback + (double) p->aInL[n];
        combp->buf[combp->bufPos] = (MYFLT) x;
        if (++(combp->bufPos) >= combp->nSamples)
          combp->bufPos = 0;
      }
    }
    /* allpass filters (left channel) */
    for (i = 0; i < NR_ALLPASS; i++) {
      allpassp = p->AllPass[i][0];
      for (n = 0; n < csound->ksmps_; n++) {
        x = (double) allpassp->buf[allpassp->bufPos] - (double) p->tmpBuf[n];
        allpassp->buf[allpassp->bufPos] *= (MYFLT) allPassFeedBack;
        allpassp->buf[allpassp->bufPos] += p->tmpBuf[n];
        if (++(allpassp->bufPos) >= allpassp->nSamples)
          allpassp->bufPos = 0;
        p->tmpBuf[n] = (MYFLT) x;
      }
    }
    /* write left channel output */
    for (n = 0; n < csound->ksmps_; n++)
      p->aOutL[n] = p->tmpBuf[n] * (MYFLT) fixedGain;
    /* comb filters (right channel) */
    for (n = 0; n < csound->ksmps_; n++)
      p->tmpBuf[n] = FL(0.0);
    for (i = 0; i < NR_COMB; i++) {
      combp = p->Comb[i][1];
      for (n = 0; n < csound->ksmps_; n++) {
        p->tmpBuf[n] += combp->buf[combp->bufPos];
        x = (double) combp->buf[combp->bufPos];
        combp->filterState = (combp->filterState * damp1) + (x * damp2);
        x = combp->filterState * feedback + (double) p->aInR[n];
        combp->buf[combp->bufPos] = (MYFLT) x;
        if (++(combp->bufPos) >= combp->nSamples)
          combp->bufPos = 0;
      }
    }
    /* allpass filters (right channel) */
    for (i = 0; i < NR_ALLPASS; i++) {
      allpassp = p->AllPass[i][1];
      for (n = 0; n < csound->ksmps_; n++) {
        x = (double) allpassp->buf[allpassp->bufPos] - (double) p->tmpBuf[n];
        allpassp->buf[allpassp->bufPos] *= (MYFLT) allPassFeedBack;
        allpassp->buf[allpassp->bufPos] += p->tmpBuf[n];
        if (++(allpassp->bufPos) >= allpassp->nSamples)
          allpassp->bufPos = 0;
        p->tmpBuf[n] = (MYFLT) x;
      }
    }
    /* write right channel output */
    for (n = 0; n < csound->ksmps_; n++)
      p->aOutR[n] = p->tmpBuf[n] * (MYFLT) fixedGain;

    return OK;
}

/* module interface functions */

int csoundModuleCreate(void *csound)
{
    return 0;
}

int csoundModuleInit(void *csound_)
{
    ENVIRON *csound;
    csound = (ENVIRON*) csound_;
    return (csound->AppendOpcode(csound, "freeverb",
                                 (int) sizeof(FREEVERB), 5, "aa", "aakkjo",
                                 (int (*)(void*, void*)) freeverb_init,
                                 (int (*)(void*, void*)) NULL,
                                 (int (*)(void*, void*)) freeverb_perf,
                                 (int (*)(void*, void*)) NULL));
}

