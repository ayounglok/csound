/*
    sndinfUG.c:

    Copyright (C) 1999 matt ingalls, Richard Dobson
              (C) 2006 Istvan Varga

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

/* sndinfUG.c         -matt 7/25/99
             ugens to retrieve info about a sound file */

#include "csoundCore.h"
#include <sndfile.h>

#include "soundio.h"
#include "sndinfUG.h"
#include "oload.h"      /* for strset */
#include "pvfileio.h"
#include "convolve.h"

static int getsndinfo(CSOUND *csound, SNDINFO *p, SF_INFO *hdr)
{
    char    *sfname, *s, soundiname[512];
    SNDFILE *sf;
    SF_INFO sfinfo;

    memset(hdr, 0, sizeof(SF_INFO));
    /* leap thru std hoops to get the name */
    csound->strarg2name(csound, soundiname, p->ifilno, "soundin.",
                                p->XSTRCODE);
    sfname = soundiname;
    if (strcmp(sfname, "-i") == 0) {    /* get info on the -i    */
      if (!csound->oparms->infilename)  /* commandline inputfile */
        csound->Die(csound, Str("no infile specified in the commandline"));
      sfname = csound->oparms->infilename;
    }
    s = csoundFindInputFile(csound, sfname, "SFDIR;SSDIR");
    if (s == NULL) {                    /* open with full dir paths */
      s = csoundFindInputFile(csound, sfname, "SADIR");
      if (s == NULL) {
        /* RWD 5:2001 better to exit in this situation ! */
        csound->Die(csound, Str("diskinfo cannot open %s"), sfname);
      }
    }
    sfname = s;                         /* & record fullpath filnam */
    memset(&sfinfo, 0, sizeof(SF_INFO));
    sf = sf_open(sfname, SFM_READ, &sfinfo);
    if (sf == NULL) {
      /* open failed: maybe analysis or raw file ? */
      if (*(p->irawfiles) == FL(0.0)) {
        mfree(csound, sfname);
        return 0;
      }
      /* check for analysis files */
      memset(hdr, 0, sizeof(SF_INFO));
      {                                 /* convolve */
        FILE      *f;
        CVSTRUCT  cvdata;

        f = fopen(sfname, "rb");
        if (f != NULL) {
          int   n = (int)fread(&cvdata, sizeof(CVSTRUCT), 1, f);
          fclose(f);
          if (n == 1) {
            if (cvdata.magic == (long)CVMAGIC &&
                cvdata.dataFormat == (long)CVMYFLT &&
                cvdata.Format == (long)CVRECT) {
              hdr->frames = (sf_count_t)cvdata.Hlen;
              hdr->samplerate = (int)(cvdata.samplingRate + FL(0.5));
              hdr->channels = (cvdata.channel == (long)ALLCHNLS ?
                               (int)cvdata.src_chnls : 1);
              return 1;
            }
          }
        }
      }
      {                                 /* PVOC */
        int     fd;
        PVOCDATA pvdata;
        WAVEFORMATEX fmt;

        /* RWD: my prerogative: try pvocex file first! */
        fd = csound->PVOC_OpenFile(csound, sfname, &pvdata, &fmt);
        if (fd >= 0) {
          hdr->frames =
              (sf_count_t) (((long)csound->PVOC_FrameCount(csound, fd)
                             / (int)fmt.nChannels) * (int)pvdata.dwOverlap);
          hdr->samplerate = (int)fmt.nSamplesPerSec;
          hdr->channels = (int)fmt.nChannels;
          csound->PVOC_CloseFile(csound, fd);
          return 1;
        }
      }
      memset(&sfinfo, 0, sizeof(SF_INFO));
      sfinfo.samplerate = (int)(csound->esr + FL(0.5));
      sfinfo.channels = 1;
      sfinfo.format = (int)FORMAT2SF(csound->oparms->outformat)
                      | (int)TYPE2SF(TYP_RAW);
      /* try again */
      sf = sf_open(sfname, SFM_READ, &sfinfo);
    }
    if (sf == NULL) {
      csound->Die(csound, Str("diskinfo cannot open %s"), sfname);
    }
    mfree(csound, sfname);
    memcpy(hdr, &sfinfo, sizeof(SF_INFO));
    sf_close(sf);

    return 1;
}

int filelen(CSOUND *csound, SNDINFO *p)
{
    SF_INFO hdr;

    if (getsndinfo(csound, p, &hdr))
      *(p->r1) = (MYFLT)((long)hdr.frames) / (MYFLT)hdr.samplerate;
    else
      *(p->r1) = FL(0.0);

    return OK;
}

int filenchnls(CSOUND *csound, SNDINFO *p)
{
    SF_INFO hdr;

    getsndinfo(csound, p, &hdr);
    *(p->r1) = (MYFLT)hdr.channels;

    return OK;
}

int filesr(CSOUND *csound, SNDINFO *p)
{
    SF_INFO hdr;

    getsndinfo(csound, p, &hdr);
    *(p->r1) = (MYFLT)hdr.samplerate;

    return OK;
}

/* RWD 8:2001: now supports all relevant files, */
/* and scans overall peak properly */

int filepeak(CSOUND *csound, SNDINFOPEAK *p)
{
    int     channel = (int)(*p->channel + FL(0.5));
    char    *sfname, soundiname[512];
    void    *fd;
    SNDFILE *sf;
    double  peakVal = -1.0;
    int     fmt, typ;
    SF_INFO sfinfo;

    csound->strarg2name(csound, soundiname, p->ifilno,
                        "soundin.", p->XSTRCODE);
    sfname = soundiname;
    if (strcmp(sfname, "-i") == 0) {        /* get info on the -i    */
      sfname = csound->oparms->infilename;  /* commandline inputfile */
      if (sfname == NULL)
        csound->Die(csound,
                    Str("no infile specified in the commandline"));
    }
    memset(&sfinfo, 0, sizeof(SF_INFO));    /* open with full dir paths */
    fd = csound->FileOpen2(csound, &sf, CSFILE_SND_R, sfname, &sfinfo,
                             "SFDIR;SSDIR", CSFTYPE_UNKNOWN_AUDIO, FALSE);
    if (fd == NULL) {
      /* RWD 5:2001 better to exit in this situation ! */
      csound->Die(csound, Str("diskinfo cannot open %s"), sfname);
    }
    if (channel <= 0) {
#if defined(HAVE_LIBSNDFILE) && HAVE_LIBSNDFILE >= 1016
      if (sf_command(sf, SFC_GET_SIGNAL_MAX, &peakVal, sizeof(double))
          == SF_FALSE) {
        csound->Warning(csound, Str("%s: no PEAK chunk was found, scanning "
                                    "file for maximum amplitude"), sfname);
#endif
        if (sf_command(sf, SFC_CALC_NORM_SIGNAL_MAX,
                       &peakVal, sizeof(double)) != 0)
          peakVal = -1.0;
#if defined(HAVE_LIBSNDFILE) && HAVE_LIBSNDFILE >= 1016
      }
#endif
    }
    else {
      double  *peaks;
      size_t  nBytes;
      if (channel > sfinfo.channels)
        csound->Die(csound, Str("Input channel for peak exceeds number "
                                "of channels in file"));
      nBytes = sizeof(double)* sfinfo.channels;
      peaks = (double*)csound->Malloc(csound, nBytes);
#if defined(HAVE_LIBSNDFILE) && HAVE_LIBSNDFILE >= 1016
      if (sf_command(sf, SFC_GET_MAX_ALL_CHANNELS, peaks, nBytes) == SF_FALSE) {
        csound->Warning(csound, Str("%s: no PEAK chunk was found, scanning "
                                    "file for maximum amplitude"), sfname);
#endif
        if (sf_command(sf, SFC_CALC_NORM_MAX_ALL_CHANNELS, peaks, nBytes) == 0)
          peakVal = peaks[channel - 1];
#if defined(HAVE_LIBSNDFILE) && HAVE_LIBSNDFILE >= 1016
      }
#endif
      csound->Free(csound, peaks);
    }
    if (peakVal < 0.0)
      csound->Die(csound, Str("filepeak: error getting peak value"));
    /* scale output consistently with soundin opcode (see diskin2.c) */
    fmt = sfinfo.format & SF_FORMAT_SUBMASK;
    typ = sfinfo.format & SF_FORMAT_TYPEMASK;
    if ((fmt != SF_FORMAT_FLOAT && fmt != SF_FORMAT_DOUBLE) ||
        (typ == SF_FORMAT_WAV || typ == SF_FORMAT_W64 || typ == SF_FORMAT_AIFF))
      *p->r1 = (MYFLT)(peakVal * (double)csound->e0dbfs);
    else
      *p->r1 = (MYFLT)peakVal;
    csound->FileClose(csound, fd);

    return OK;
}

