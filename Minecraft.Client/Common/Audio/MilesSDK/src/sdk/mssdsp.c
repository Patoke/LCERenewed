#include "mss.h"
#include "imssapi.h"

/* @cdep pre
  $requires(bandpass.cpp)
  $requires(allpass.cpp)
  $requires(capture.cpp)

  $if($equals($BuildPlatform,orbis), ,
    $requires(chorus.cpp))
    
  $//requires(compress.cpp)
  $requires(flange.cpp)
  $requires(highpass.cpp)
  $requires(lowpass.cpp)
  $requires(parmeq.cpp)
  $requires(phaser.cpp)
  $requires(reson.cpp)
  $requires(ringmod.cpp)
  $requires(delay.cpp)
  $requires(shelfeq.cpp)
  $requires(volramp.cpp)
  $requires(gainblk.cpp)
  $requires(sidecompress.cpp)
*/

extern S32 BandPassMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 AllPassMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
#if defined( IS_WIN32API ) && !defined(FORNONWIN) && !defined(__RADSEKRIT__)
extern S32 CaptureMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
#endif
extern S32 ChorusMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
//extern S32 CompressMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 FlangeMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 HighPassMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 LowPassMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 DelayMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 ParmEqMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 PhaserMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 ResonMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 RingModMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 ShelfEqMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 RampMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 GainMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
extern S32 SideCompressMain( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );

DXDEF S32 AILEXPORT RIB_MAIN_NAME(DSP)( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
{
  static HPROVIDER bandpassPH,
                   allpassPH,
#if defined( IS_WIN32API ) && !defined(FORNONWIN) && !defined(__RADSEKRIT__)
                   capturePH,
#endif
                   chorusPH, flangePH,
                   highpassPH, lowpassPH, delayPH, parmeqPH,
                   phaserPH, resonPH, ringmodPH, shelfeqPH, rampPH, gainPH,
                   sidecompressPH;

  if ( up_down )
  {
    rampPH = provider_handle;
    bandpassPH = rib_alloc(0);
    allpassPH  = rib_alloc(0);
#if defined( IS_WIN32API ) && !defined(FORNONWIN) && !defined(__RADSEKRIT__)
    capturePH = rib_alloc(0);
#endif
    chorusPH = rib_alloc(0);
    //compressPH = rib_alloc(0);
    flangePH = rib_alloc(0);
    highpassPH = rib_alloc(0);
    lowpassPH = rib_alloc(0);
    delayPH = rib_alloc(0);
    parmeqPH = rib_alloc(0);
    phaserPH = rib_alloc(0);
    resonPH = rib_alloc(0);
    ringmodPH = rib_alloc(0);
    shelfeqPH = rib_alloc(0);
    gainPH = rib_alloc(0);
    sidecompressPH = rib_alloc(0);
  }

  BandPassMain( bandpassPH , up_down, rib_alloc, rib_reg, rib_unreg );
  AllPassMain( allpassPH , up_down, rib_alloc, rib_reg, rib_unreg );
#if defined( IS_WIN32API ) && !defined(FORNONWIN) && !defined(__RADSEKRIT__)
  CaptureMain( capturePH, up_down, rib_alloc, rib_reg, rib_unreg );
#endif
  ChorusMain( chorusPH, up_down, rib_alloc, rib_reg, rib_unreg );
  //CompressMain( compressPH, up_down, rib_alloc, rib_reg, rib_unreg );
  FlangeMain( flangePH, up_down, rib_alloc, rib_reg, rib_unreg );
  HighPassMain( highpassPH, up_down, rib_alloc, rib_reg, rib_unreg );
  LowPassMain( lowpassPH, up_down, rib_alloc, rib_reg, rib_unreg );
  DelayMain( delayPH, up_down, rib_alloc, rib_reg, rib_unreg );
  ParmEqMain( parmeqPH, up_down, rib_alloc, rib_reg, rib_unreg );
  PhaserMain( phaserPH, up_down, rib_alloc, rib_reg, rib_unreg );
  ResonMain( resonPH, up_down, rib_alloc, rib_reg, rib_unreg );
  RingModMain( ringmodPH, up_down, rib_alloc, rib_reg, rib_unreg );
  ShelfEqMain( shelfeqPH, up_down, rib_alloc, rib_reg, rib_unreg );
  RampMain( rampPH, up_down, rib_alloc, rib_reg, rib_unreg );
  GainMain( gainPH, up_down, rib_alloc, rib_reg, rib_unreg );
  SideCompressMain( sidecompressPH, up_down, rib_alloc, rib_reg, rib_unreg );

  return( TRUE );
}

