//############################################################################
//##                                                                        ##
//##  WAVMUX.CPP                                                            ##
//##                                                                        ##
//##  Command-line utility to generate multichannel .WAV files suitable     ##
//##  for direct playback by MSS or compression by other utilities such     ##
//##  as OggEnc                                                             ##
//##                                                                        ##
//##  V1.00 of 4-Aug-07: Initial                                            ##
//##                                                                        ##
//##  Author: John Miles                                                    ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "mss.h"

#pragma pack(1)

struct EXTENSIBLE_WAV
{
  U32 riffmark;
  U32 rifflen;
  U32 wavemark;

  U32 fmtmark;
  U32 fmtlen;

  U16 wFormatTag;        
  U16 nChannels;         
  U32 nSamplesPerSec;    
  U32 nAvgBytesPerSec;   
  U16 nBlockAlign;       
  U16 wBitsPerSample;
  U16 cbSize;

  U16 wValidBitsPerSample;
  U32 dwChannelMask;
  U8  SubFormat[16];

  U32 datamark;
  U32 datalen;
  C8  data[1];
};

#pragma pack()

const U8 KSDATAFORMAT_SUBTYPE_PCM[16] = 
{ 
   0x01,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71,
};

void main(S32 argc, C8 **argv)
{
   printf("WAVMUX - Version " MSS_VERSION "            " MSS_COPYRIGHT " \n");
   printf("-------------------------------------------------------------------------------\n\n");

   if (argc < 3)
      {
      printf("Usage: wavmux output.wav channel_1.wav [channel_n.wav...]\n\n");
      printf("Input .WAV files should be specified in standard KSMEDIA (Windows) order:\n\n");
      printf("   FL FR C LFE BL BR FLoC FRoC BC SL SR TC TFL TFC TFR TBL TBC TBR\n\n");
      printf("(Use 'none' to create gaps in the channel-order mask)\n");
      exit(1);
      }

   S32 chans =  0;                         // # of chans
   S32 rate  = -1;                         // rate
   U32 maskval = 1;                        // channel mask
   U32 mask = 0;                           // current mask value
   S32 total_samples = -1;                 // # of samples in longest source file determines overall output file length

   //
   // Allocate descriptors for .wav files and read them
   //

   S32 n_files = argc-2;

   AILSOUNDINFO *info = (AILSOUNDINFO *) malloc(sizeof(AILSOUNDINFO) * n_files);

   if (info == NULL)
      {
      printf("Out of memory\n");
      exit(1);
      }

   memset(info, 0, sizeof(AILSOUNDINFO) * n_files);

   printf("\n");

   S32 i;
   for (i=0; i < n_files; i++)
      {
      if (!_stricmp(argv[i+2],"none"))
         {
         printf("Skipping channel slot 0x%X\n",1 << i);

         maskval <<= 1;
         continue;
         }

      printf("Loading %s ... ",argv[i+2]);

      S32 *f = (S32 *) AIL_file_read(argv[i+2],FILE_READ_WITH_SIZE);

      if (f == NULL)
         {
         printf("Error: %s\n",AIL_last_error());
         exit(1);
         }

      if (!AIL_WAV_info(&f[1], &info[i]))
         {
         printf("Error: %s\n",AIL_last_error());
         exit(1);
         }

      printf("%d channel(s), %d Hz, %d bytes\n",
         info[i].channels,
         info[i].rate,
         info[i].data_len);

      if (rate == -1) 
         {
         rate = info[i].rate;
         }
      else
         {
         if (rate != (S32) info[i].rate)
            {
            printf("Error: all files must have the same sample rate\n");
            exit(1);
            }
         }

      chans += info[i].channels;

      for (S32 j=0; j < info[i].channels; j++)
         {
         mask |= maskval;
         maskval <<= 1;
         }

      S32 n_samples = info[i].data_len / (sizeof(S16) * info[i].channels);

      if (n_samples > total_samples)
         {
         total_samples = n_samples;
         }
      }

   S32 total_data_bytes = total_samples * chans * sizeof(S16);

   S32 total = total_data_bytes + sizeof(EXTENSIBLE_WAV)-1;  // total size of wave image 

   printf("\nSaving %s, %d channel(s), %d bytes, dwChannelMask=0x%X ... ",
      argv[1],chans,total_data_bytes,mask);

   //
   // Write interlaced data
   //

   EXTENSIBLE_WAV *wav = (EXTENSIBLE_WAV *) malloc(total);

   if (wav == NULL)
      {
      printf("Out of memory\n");
      exit(1);
      }

   S16 *dest = (S16 *) &wav->data;

   for (S32 s=0; s < total_samples; s++)
      {
      for (i=0; i < n_files; i++)
         {
         for (S32 ch=0; ch < info[i].channels; ch++)
            {
            if (info[i].data_len <= 0)
               {
               *dest++ = 0;
               }
            else
               {
               *dest++ = *(S16 *) info[i].data_ptr;
               info[i].data_ptr = ((S16 *) info[i].data_ptr) + 1;
               info[i].data_len -= sizeof(S16);
               }
            }
         }
      }

   memcpy(&wav->riffmark,"RIFF",4);
   wav->rifflen=total-8;
   memcpy(&wav->wavemark,"WAVE",4);
   memcpy(&wav->fmtmark,"fmt ",4);
   wav->fmtlen=0x28;

   wav->wFormatTag=WAVE_FORMAT_EXTENSIBLE;
   wav->nChannels=(S16) chans;
   wav->nSamplesPerSec=rate;
   wav->nAvgBytesPerSec=(rate * 16 * chans) / 8;
   wav->nBlockAlign=(U8) (chans * sizeof(S16));
   wav->wBitsPerSample=16;
   wav->cbSize = 22;

   wav->wValidBitsPerSample = 16;
   wav->dwChannelMask = mask;
   memcpy(wav->SubFormat, KSDATAFORMAT_SUBTYPE_PCM, 16);

   memcpy(&wav->datamark,"data",4);
   wav->datalen=total_data_bytes;

   FILE *out = fopen(argv[1],"w+b");

   if (out == NULL)
      {
      printf("Can't write file\n");
      exit(1);
      }

   if (!fwrite(wav,total,1,out))
      {
      printf("Disk full\n");
      exit(1);
      }

   fclose(out);
   printf("Done\n");
}

// @cdep pre $DefaultsConsoleMSS
// @cdep post $BuildConsoleMSS


