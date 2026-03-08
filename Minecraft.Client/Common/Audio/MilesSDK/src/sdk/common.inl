#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mss.h"
#include "imssapi.h"

// Allow the filters to pass in additional flags to return to miles.
#ifndef FILTER_PROPERTY_FLAGS
#define FILTER_PROPERTY_FLAGS 0
#endif

// Pentium Floating Point Processor Denormal Protect Value
#define  _FX_DENORMVAL     ( 1.192092896e-06F * 2.0f )

// PI definition
#define  F_PI  3.14159265F

// Clip Range Macro
#define  FX_CLIPRANGE( p, min, max ) \
            if ( (p) > (max) ) \
               (p) = (max); \
            else \
               if ( (p) < (min) ) \
                  (p) = (min);

#ifndef MSS_SPU_PROCESS

//
// Globals
//

static S32 FLT_started = 0;

static C8 FLT_error_text[256];


static void init_sample( SAMPLESTATE * SS );
static void close_sample( SAMPLESTATE * SS );

//############################################################################
//#                                                                          #
//# Retrieve a standard RIB provider attribute by index                      #
//#                                                                          #
//############################################################################

static S32 AILCALL PROVIDER_property (HPROPERTY index, void * before_value, void const * new_value, void * after_value)
{
   switch ( index )
      {
      case PROVIDER_NAME:
        if (before_value)
        {
          *(char const **)before_value = FILTER_NAME;
          return 1;
        }
        break;

      case PROVIDER_VERSION:
        if (before_value)
        {
          *(S32*)before_value = 0x130;
          return 1;
        }
        break;

      case _FX_PROVIDER_FLAGS:
        if (before_value)
        {
#ifndef PROVIDER_SPU_INDEX
          *(S32*)before_value = FPROV_ON_SAMPLES|FILTER_PROPERTY_FLAGS;
#else
          *(S32*)before_value = FPROV_ON_SAMPLES|FPROV_MAKE_SPU_INDEX(PROVIDER_SPU_INDEX)|FILTER_PROPERTY_FLAGS;
#endif
          return 1;
        }
        break;

      case _FX_PROVIDER_SAMPLE_STATE_SIZE:
        if (before_value)
        {
          *(S32*)before_value = ( sizeof(SAMPLESTATE) + 15 ) & ~15;
          return 1;
        }
        break;

      case _FX_PROVIDER_DRIVER_STATE_SIZE:
        if (before_value)
        {
          *(S32*)before_value = ( sizeof(DRIVERSTATE) + 15 ) & ~15;
          return 1;
        }
        break;

      }
  return 0;
}

// Clear the flags in case we get #included again...
#undef FILTER_PROPERTY_FLAGS

//############################################################################
//#                                                                          #
//# Return FLT filter error text                                             #
//#                                                                          #
//############################################################################

static C8 *       AILCALL FLT_error       (void)
{
   if (FLT_error_text[0]==0)
      {
      return 0;
      }

   return FLT_error_text;
}

//############################################################################
//#                                                                          #
//# Initialize FLT sample filter                                             #
//#                                                                          #
//############################################################################

static FLTRESULT AILCALL FLT_startup     (void)
{
   if (FLT_started++)
      {
      strcpy(FLT_error_text,"Already started");
      return FLT_ALREADY_STARTED;
      }

   //
   // Init static prefs/attributes
   //

   FLT_error_text[0]            = 0;

   return FLT_NOERR;
}

//############################################################################
//#                                                                          #
//# Shut down FLT sample filter                                              #
//#                                                                          #
//############################################################################

static FLTRESULT      AILCALL FLT_shutdown    (void)
{
   if (!FLT_started)
      {
      strcpy(FLT_error_text,"Not initialized");
      return FLT_NOT_INIT;
      }

   --FLT_started;

   return FLT_NOERR;
}

//############################################################################
//#                                                                          #
//# Allocate driver-specific descriptor                                      #
//#                                                                          #
//############################################################################

static HDRIVERSTATE AILCALL FLT_open_driver (MSS_ALLOC_TYPE * palloc,
                                             MSS_FREE_TYPE  * pfree,
                                             UINTa         user,  
                                             HDIGDRIVER  dig, void * memory)

{
   DRIVERSTATE *DRV = (DRIVERSTATE *) memory;

   if (DRV == NULL)
      {
      strcpy(FLT_error_text,"Out of memory");
      return 0;
      }

   AIL_memset(DRV,
              0,
              sizeof(DRIVERSTATE));

   //
   // Initialize generic members
   //

   DRV->dig = dig;
   DRV->alloc_CB = palloc;
   DRV->free_CB = pfree;
   DRV->user = user;

   //
   // Return descriptor address cast to handle
   //

   return (HDRIVERSTATE) DRV;
}

//############################################################################
//#                                                                          #
//# Close filter driver instance                                             #
//#                                                                          #
//############################################################################

static FLTRESULT     AILCALL FLT_close_driver (HDRIVERSTATE state)
{
//   DRIVERSTATE *DRV = (DRIVERSTATE *) state;

   return FLT_NOERR;
}

//############################################################################
//#                                                                          #
//#  Perform any needed processing before per-sample mixing begins           #
//#                                                                          #
//#  Called after the build buffer has been flushed prior to the mixing      #
//#  phase, but before any samples have been mixed into it                   #
//#                                                                          #
//############################################################################

static void AILCALL FLT_premix_process (HDRIVERSTATE driver)
{
}

//############################################################################
//#                                                                          #
//#  Process data after mixing                                               #
//#                                                                          #
//#  Called after all samples have been mixed into the 32-bit build buffer,  #
//#  prior to copying the build-buffer contents to the driver's output       #
//#  buffer                                                                  #
//#                                                                          #
//#  This function may optionally handle the task of filling the driver's    #
//#  output buffer.  If it does, it should return a nonzero value to         #
//#  indicate that it has done so                                            #
//#                                                                          #
//############################################################################

static S32 AILCALL FLT_postmix_process (HDRIVERSTATE driver, //)
                                          void    *output_buffer)
{
   return 0;
}

//############################################################################
//#                                                                          #
//# Assign filter to specified sample                                        #
//#                                                                          #
//############################################################################

static HSAMPLESTATE AILCALL FLTSMP_open_sample (HDRIVERSTATE driver, //)
                                                HSAMPLE      S,
                                                void *       memory)
{
   //
   // Allocate SAMPLESTATE descriptor
   //

   SAMPLESTATE *SS = (SAMPLESTATE *) memory;

   if (SS == NULL)
      {
      strcpy(FLT_error_text,"Out of memory");
      return 0;
      }

   AIL_memset(SS,
              0,
              sizeof(SAMPLESTATE));

   //
   // Initialize generic members
   //

   SS->driver = (DRIVERSTATE *) driver;
   SS->sample = S;

   init_sample( SS );

   //
   // Return descriptor address cast to handle
   //

   return (HSAMPLESTATE) SS;
}

//############################################################################
//#                                                                          #
//# Close filter sample instance                                             #
//#                                                                          #
//############################################################################

static FLTRESULT     AILCALL FLTSMP_close_sample (HSAMPLESTATE state)
{
   SAMPLESTATE *SS = (SAMPLESTATE *) state;

   close_sample( SS );

   return FLT_NOERR;
}

#endif
