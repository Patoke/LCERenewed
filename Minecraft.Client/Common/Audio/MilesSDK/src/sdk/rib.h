//############################################################################
//##                                                                        ##
//##  RIB.H: RAD Interface Broker services                                  ##
//##                                                                        ##
//##  Version 1.00 of 21-May-98: Initial                                    ##
//##  Version 2.00 of 10-Mar-06: Switched to properties                     ##
//##                                                                        ##
//##  Author: John Miles                                                    ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#ifndef RIB_VERSION

#define RIB_VERSION      "1.2"
#define RIB_VERSION_DATE "15-Mar-06"

#define RIB_COPYRIGHT "Copyright (C) 1998-2010 RAD Game Tools, Inc."

#endif

#ifndef RIB_H
#define RIB_H

// IS_DOS for DOS
// IS_WINDOWS for Windows or Win32
// IS_WIN64 for Win64
// IS_WIN32 for Win32
// IS_WIN32API for Windows, Xbox and Xenon
// IS_64REG when CPU registers are 64-bit - Xenon, Win64, PS3 and PS2
// IS_32 for 32-bit DOS or Win32
// IS_16 for 16-bit Windows
// IS_LE for little endian (PCs)
// IS_BE for big endian (Macs, PS3, Xenon)
// IS_X86 for Intel
// IS_MAC for Mac
// IS_PPC for PPC Mac
// IS_68K for 68K Mac
// IS_LINUX for Linux
// IS_XBOX for Xbox
// IS_XENON for Xbox 360
// IS_PS2 for PS/2
// IS_PS3 for PS/3
// IS_STATIC for static versions (DOS, Xbox, Xbox 360, GameCube, PS2)

#include "rrCore.h"

#ifdef __RADNT__
#define IS_WIN32
#if defined(__RAD64__)
#define IS_WIN64
#endif
#endif

#if defined(__RADWIN__) && !defined(__RADXENON__) && !defined(__RADXBOX__) && !defined(__RADSEKRIT__)
#define IS_WINDOWS
#endif

#if defined(__RADWIN__)
#define IS_WIN32API
#endif

#if defined(__RAD64__) && defined(__RADWIN__)
#define IS_WIN64
#endif

// 16-bit not supported anymore
#define IS_32

#ifdef __RADLITTLEENDIAN__
#define IS_LE
#endif

#ifdef __RADBIGENDIAN__
#define IS_BE
#endif

#ifdef __RADX86__
#define IS_X86
#endif

#ifdef __RADMAC__
#define IS_MAC
#endif

#ifdef __RADPPC__
#define IS_PPC
#endif

#ifdef __RAD68K__
#define IS_68K
#endif

#ifdef __RADLINUX__
#define IS_LINUX
#endif

#ifdef __RADSTATIC__
#define IS_STATIC
#endif

#ifdef __RADXBOX__
#define IS_XBOX
#endif

#ifdef __RADXENON__
#define IS_XENON
#endif

#ifdef __RADWII__
#define IS_WII
#endif

#ifdef __RADPS2__
#define IS_PS2
#endif

#ifdef __RADWIIU__
#define IS_WIIU
#endif

#ifdef __RADPS3__
#define IS_PS3
#ifndef HOST_SPU_PROCESS
  #define HOST_SPU_PROCESS
#endif
#endif

#ifdef __RADSPU__
#define IS_PS3
#endif

#ifdef __RADPSP__
#define IS_PSP
#endif

#ifdef __RADPSP2__
#define IS_PSP2
#endif

#ifdef __RADDOS__
#define IS_DOS
#endif

#ifdef __RAD64REGS__
#define IS_64REGS
#endif

#ifdef __RADMACH__
#define IS_MACHO
#endif

#ifdef __RADIPHONE__
#define IS_IPHONE
#endif

#ifdef __RADIPHONESIM__
#define IS_IPHONESIM
#endif

#ifdef __RAD3DS__
#define IS_3DS
#endif

#define MSSRESTRICT RADRESTRICT

#define MSS_STRUCT RADSTRUCT

#define C8 char
typedef void VOIDFUNC(void);


#if (!defined(IS_LE) && !defined(IS_BE))
  #error RIB.H did not detect your platform.  Define __DOS__, _WINDOWS, WIN32, or macintosh.
#endif

RADDEFSTART

#ifdef BUILD_RIB
#define BUILDING_RIB
#endif

#ifdef __RADINDLL__
#define BUILDING_RIB
#endif

#ifdef IS_PS2

  #define RIBCALLBACK
  #define RIBEXPORT
  #define RIBDEC extern
  #define RIBDEF
  #define RIBCALL
 
  #define RIBLIBCALLBACK __pascal

#elif defined( IS_PSP ) || defined(IS_PSP2) || defined( __RADANDROID__ )

  #define RIBCALLBACK
  #define RIBEXPORT
  #define RIBDEC extern
  #define RIBDEF
  #define RIBCALL

  #define RIBLIBCALLBACK __pascal

#elif defined( IS_MAC ) || defined(IS_IPHONE)

  #ifdef IS_X86
    #define RIBLIBCALLBACK __attribute__((cdecl))
    #define RIBCALL        __attribute__((cdecl))
    #define RIBCALLBACK    __attribute__((cdecl))
    #define RIBEXPORT      __attribute__((cdecl))

    #ifdef __RADINDLL__
      #define RIBDEC __attribute__((visibility("default")))
      #define RIBDEF __attribute__((visibility("default")))
    #else
      #define RIBDEC extern
      #define RIBDEF
    #endif

  #else
    #define RIBLIBCALLBACK 
    #define RIBCALL        
    #define RIBCALLBACK    
    #define RIBEXPORT      

    #ifdef __RADINDLL__
      #define RIBDEC __attribute__((visibility("default")))
      #define RIBDEF __attribute__((visibility("default")))
    #else
      #define RIBDEC extern
      #define RIBDEF
    #endif

  #endif

  #define RIBDLLNAME "Miles Shared Library"

#elif defined( IS_LINUX )

  #ifdef IS_X86
    #define RIBLIBCALLBACK __attribute__((cdecl))
    #define RIBCALL        __attribute__((cdecl))
    #define RIBCALLBACK    __attribute__((cdecl))
    #define RIBEXPORT      __attribute__((cdecl))
  #else
    #define RIBLIBCALLBACK 
    #define RIBCALL        
    #define RIBCALLBACK    
    #define RIBEXPORT      
  #endif

  #define RIBDEC __attribute__((visibility("default")))
  #define RIBDEF __attribute__((visibility("default")))

  #define RIBDLLNAME "Miles Shared Library"

#elif defined( IS_WINDOWS )

  //
  // If compiling RIB DLL, use __declspec(dllexport) for both
  // declarations and definitions
  //
  // If compiling RIB16 library or application, use "extern" in declarations
  // and nothing in definitions
  //
  
  #ifdef IS_WIN32
  
    #ifndef FORNONWIN
    #define RIBLIBCALLBACK __stdcall
    #define RIBCALL        __stdcall
    #define RIBCALLBACK    __stdcall
    #define RIBEXPORT      __stdcall
    #else
    #define RIBLIBCALLBACK __cdecl
    #define RIBCALL        __cdecl
    #define RIBCALLBACK    __cdecl
    #define RIBEXPORT      __cdecl
    #endif
  
    #ifdef BUILDING_RIB
      #define RIBDEC __declspec(dllexport)
      #define RIBDEF __declspec(dllexport)
    #else
  
      #ifdef __BORLANDC__
        #define RIBDEC extern
      #else
        #define RIBDEC __declspec(dllimport)
      #endif
  
    #endif
  
    #define RIBDLLNAME "MSS32.DLL"
  
  #else
  
    #define RIBCALL WINAPI
    #define RIBEXPORT __export WINAPI
    #define RIBCALLBACK AILEXPORT
    #define RIBLIBCALLBACK WINAPI
  
    #define RIBDEC  extern
    #define RIBDEF
  
    #define RIBDLLNAME "MSS16.DLL"
  
  #endif

#elif defined( IS_XENON ) || defined(__RADSEKRIT__)

  #define RIBEXPORT __stdcall
  
  #define RIBDEC extern
  #define RIBDEF 
  
  #define RIBCALL RIBEXPORT
  #define RIBCALLBACK RIBEXPORT

#elif defined( IS_PS3 ) || defined( IS_3DS ) || defined(__RADSEKRIT2__) || defined(IS_PSP2)

  #define RIBCALL       
  #define RIBEXPORT     
  #define RIBCALLBACK   

  #define RIBDEC extern
  #define RIBDEF

#elif defined( IS_WII ) || defined(IS_WIIU)

  #define RIBCALL       
  #define RIBEXPORT     
  #define RIBCALLBACK   

  #define RIBDEC extern
  #define RIBDEF

#else

#define RIBEXPORT __stdcall

#define RIBDEC extern
#define RIBDEF 

#define RIBCALL RIBEXPORT
#define RIBCALLBACK RIBEXPORT

#endif


#if defined(IS_WIN32API) || defined(IS_WII)
  #pragma pack(push,1)
#endif

#define ARY_CNT(x) (sizeof((x)) / sizeof((x)[0]))

// -------------------------------------------------------------------------
// RIB data types         
// -------------------------------------------------------------------------

typedef S32 RIBRESULT;

#define RIB_NOERR                    0   // Success -- no error
#define RIB_NOT_ALL_AVAILABLE        1   // One or more requested functions/attribs were not available
#define RIB_NOT_FOUND                2   // Resource not found      
#define RIB_OUT_OF_MEM               3   // Out of system RAM       

// -------------------------------------------------------------------------
// Handle to RIB interface provider
// -------------------------------------------------------------------------

typedef UINTa HPROVIDER;

// -------------------------------------------------------------------------
// Handle representing token used to obtain property
// data from RIB provider
//
// These tokens are arbitrary, unique numeric values used by the
// interface provider to identify named properties
// -------------------------------------------------------------------------

typedef UINTa HPROPERTY;  // must be large enough to hold a function pointer

#ifndef MSS_FIRST
typedef SINTa HMSSENUM;
#define MSS_FIRST ((HMSSENUM)-1)
#endif

// -------------------------------------------------------------------------
// Data types for RIB properties
// -------------------------------------------------------------------------

typedef enum
{
   RIB_NONE = 0, // No type
   RIB_CUSTOM,   // Used for pointers to application-specific structures
   RIB_DEC,      // Used for 32-bit integer values to be reported in decimal
   RIB_HEX,      // Used for 32-bit integer values to be reported in hex
   RIB_FLOAT,    // Used for 32-bit single-precision FP values
   RIB_PERCENT,  // Used for 32-bit single-precision FP values to be reported as percentages
   RIB_BOOL,     // Used for Boolean-constrained integer values to be reported as TRUE or FALSE
   RIB_STRING,   // Used for pointers to null-terminated ASCII strings
   RIB_READONLY = 0x80000000  // Property is read-only
}
RIB_DATA_SUBTYPE;  

// -------------------------------------------------------------------------
// RIB_ENTRY_TYPE structure, used to register an interface or request one
// -------------------------------------------------------------------------

typedef enum
{
   RIB_FUNCTION = 0,  // Function
   RIB_PROPERTY,       // Property: read-only or read-write data type
   RIB_ENTRY_FORCE_32 = 0x7fffffff
}
RIB_ENTRY_TYPE;

// -------------------------------------------------------------------------
// RIB_INTERFACE_ENTRY, used to represent a function or data entry in an
// interface
// -------------------------------------------------------------------------

typedef struct
{
   RIB_ENTRY_TYPE   type;        // See list of RIB_ENTRY_TYPEs above
   char const*      entry_name;  // Name of desired function or property
   UINTa            token;       // Function pointer or property token
   RIB_DATA_SUBTYPE subtype;     // Data (property) subtype
}
RIB_INTERFACE_ENTRY;

// -------------------------------------------------------------------------
// Standard RAD Interface Broker provider identification properties
// -------------------------------------------------------------------------

#define PROVIDER_NAME    ((U32) (S32) (-100))    // RIB_STRING name of decoder
#define PROVIDER_VERSION ((U32) (S32) (-101))    // RIB_HEX BCD version number

//
// -------------------------------------------------------------------------
// Standard function to obtain provider properties (see PROVIDER_ defines
// above)
//
// Each provider of a searchable interface must export this function
// -------------------------------------------------------------------------
//

typedef S32 (RIBCALL *PROVIDER_PROPERTY) (HPROPERTY        index,
                                              void *       before_value,
                                              void const * new_value,
                                              void *       after_value
                                              );

// -------------------------------------------------------------------------
// Macros to simplify interface registrations/requests for functions
// and properties
// 
// FN(entry_name): Request address of function entry_name to be placed in 
//                 variable of same name.  Variable must be in scope.
//
// REG_FN(entry_name): Used by a RIB provider to register (i.e., export)
//                     function entry_name.  
//
// PR(entry_name,ID):             Properties can be read or written to with the
//                                standard PROVIDER_property() function or via a
//                                RIB-specific property function.
//
// REG_PR(entry_name,ID,subtype): Used by a RIB provider to register a named
//                                property (entry_name) with a numeric 
//                                token (ID) which may be used by the application
//                                to retrieve the value of that property.  The
//                                subtype is a value of type RIB_DATA_SUBTYPE 
//                                which allows generic property browsers and 
//                                the RIB_type_string() function to identify the
//                                type of enumerated properties at runtime.

// -------------------------------------------------------------------------

#define FN(entry_name)        { RIB_FUNCTION, #entry_name, (UINTa) &(entry_name), RIB_NONE }
#define REG_FN(entry_name)    { RIB_FUNCTION, #entry_name, (UINTa) &(entry_name), RIB_NONE }

#define PR(entry_name,ID)             { RIB_PROPERTY, (entry_name), (UINTa) &(ID), RIB_NONE }
#define REG_PR(entry_name,ID,subtype) { RIB_PROPERTY, (entry_name), (UINTa)  (ID), subtype  }

// -------------------------------------------------------------------------
// RIB_register(x,y,z): Macro to conveniently register one or more interface entries in an
//                      array.  x=provider handle, y=name of interface with which the 
//                      entries are associated, z=address of RIB_INTERFACE_ENTRY declarations.
// 
// RIB_unregister(x,y,z): Unregister one or more arrayed interface entries.
//
// RIB_unregister_all(): Unregister all entries in all interfaces associated with a 
//                       given provider.
//
// RIB_free_libraries(): Unload all RIB libraries.  Should be preceded by RIB_unregister_all() for
//                       all loaded providers.
//
// RIB_request(x,y,z): Macro to conveniently request fixups for one or more interface entries
//                     in an array.  x=provider handle, y=name of interface containing 
//                     named entries, z=address of RIB_INTERFACE_ENTRY declarations.
// -------------------------------------------------------------------------

#define RIB_register(x,y,z)   RIB_register_interface  (HPROVIDER(x), y, ARY_CNT(z), z)
#define RIB_unregister(x,y,z) RIB_unregister_interface(HPROVIDER(x), y, ARY_CNT(z), z)
#define RIB_unregister_all(x) RIB_unregister_interface(HPROVIDER(x), NULL, 0, NULL)
#define RIB_free_libraries()  RIB_free_provider_library(HPROVIDER(NULL));
#define RIB_request(x,y,z)    RIB_request_interface   (x, y, ARY_CNT(z), z)

// passed to RIB DLLs in Miles 9 and up (so RIBS don't have to link to MSS32.dll)
typedef HPROVIDER RIBCALL RIB_ALLOC_PROVIDER_HANDLE_TYPE(long           module);

typedef RIBRESULT RIBCALL RIB_REGISTER_INTERFACE_TYPE (HPROVIDER                      provider,
                                                       C8 const                   *interface_name,
                                                       S32                            entry_count,
                                                       RIB_INTERFACE_ENTRY const *rlist);

typedef RIBRESULT  RIBCALL RIB_UNREGISTER_INTERFACE_TYPE (HPROVIDER                      provider,
                                                          C8 const                  *interface_name,
                                                          S32                            entry_count,
                                                          RIB_INTERFACE_ENTRY const *rlist);

#define RIB_registerP(x,y,z)   rib_reg   ((HPROVIDER)(x), y, ARY_CNT(z), z)
#define RIB_unregister_allP(x) rib_unreg ((HPROVIDER)(x), 0, 0, 0)


// ----------------------------------
// Standard RIB API prototypes
// ----------------------------------

// -------------------------------------------------------------------------
// RIB_alloc_provider_handle(module): Allocate a new HPROVIDER handle for a
// RIB provider, where module=the library's module handle.
//
// RIB_free_provider_handle(provider): Free an HPROVIDER handle allocated from
// the RIB_alloc_provider_handle() function (above).
//
// Most applications which only load RIB libraries do not need to call 
// either of these functions.  Their functionality is encapsulated by the
// RIB_load_provider_library() / RIB_free_provider_library() functions.
// -------------------------------------------------------------------------

RIBDEC HPROVIDER  RIBCALL RIB_alloc_provider_handle   (long           module);
RIBDEC void       RIBCALL RIB_free_provider_handle    (HPROVIDER      provider);

// -------------------------------------------------------------------------
// RIB_load_provider_library(filename): Load a RIB provider DLL, returning its
// HPROVIDER handle or NULL on error.  Internally, this function combines a 
// call to LoadLibrary() with a call to RIB_alloc_provider_handle().  
//
// RIB_free_provider_library(provider): Free a previously-loaded provider DLL.
//
// Under DOS, RIB_load_provider_library() is equivalent to 
// calling RIB_alloc_provider_handle(0).
// -------------------------------------------------------------------------

RIBDEC HPROVIDER  RIBCALL RIB_load_provider_library   (C8 const  *filename);
RIBDEC void       RIBCALL RIB_free_provider_library   (HPROVIDER      provider);

// -------------------------------------------------------------------------
// RIB_register_interface(provider, interface_name, entry_count, list): Allows
// a RIB provider to register an interface -- i.e., a named collection of the specific 
// functions, and/or properties it supports.  provider is normally the
// value obtained by calling RIB_provider_library_handle() from the library's
// LibMain() procedure.  interface_name is the name of the interface, entry_count
// is the number of interface entries being registered, and list is a pointer to an
// array of one or more RIB_INTERFACE_ENTRY structures representing the entries 
// to be registered.  All fields of each RIB_INTERFACE_ENTRY structure should be valid
// when calling RIB_register_interface().  For convenience, the macros REG_FN, REG_AT,
// and REG_PR can be used instead of RIB_register_interface() in many cases.
// 
// RIB_unregister_interface(provider,interface_name,entry_count,list) removes 
// the specified entries from the interface.  If list_name is NULL, all
// entries associated with the specified interface are unregistered.  If interface_name
// is NULL, all entries associated with all interfaces associated with the
// specified provider are unregistered.
// -------------------------------------------------------------------------


RIBDEC RIBRESULT  RIBCALL RIB_register_interface      (HPROVIDER                      provider,
                                                       C8 const                  *interface_name,
                                                       S32                            entry_count,
                                                       const RIB_INTERFACE_ENTRY *list);

RIBDEC RIBRESULT  RIBCALL RIB_unregister_interface    (HPROVIDER                      provider,
                                                       C8 const                  *interface_name,
                                                       S32                            entry_count,
                                                       const RIB_INTERFACE_ENTRY *list);

// -------------------------------------------------------------------------
// RIB_request_interface(provider, interface_name, entry_count, list): Allows
// a RIB application to request an interface -- i.e., a named collection of the specific
// functions, and/or properties supported by a RIB provider.
//
// interface_name is the name of the requested interface, entry_count is the number of
// interface entries being requested, and list is a pointer to an array of one or more
// RIB_INTERFACE_ENTRY structures representing the entries to be requested.  The 'token' fields
// of the entry structures should consist of a pointer to the application variables which will
// receive the requested function pointers or property tokens.  The data subtype fields 
// do not need to be filled in when requesting interface entries, and may be set to RIB_NONE.
//
// For convenience, the macros FN, AT, and PR can be used instead of RIB_request_interface()
// whenever the entry names and their token variables share the same names, and the token
// variables are in scope.
// 
// If a requested interface entry is not supported by the specified provider,
// the entry structure's 'token' field will receive the value -1 for property 
// entries, or NULL for function entries.  If one or more entries are not
// found, the return value will be RIB_NOT_ALL_AVAILABLE.  Otherwise, RIB_NOERR will
// be returned if all requested entries were successfully resolved.
// -------------------------------------------------------------------------

RIBDEC RIBRESULT  RIBCALL RIB_request_interface       (HPROVIDER                provider,
                                                       C8 const            *interface_name,
                                                       S32                      entry_count,
                                                       RIB_INTERFACE_ENTRY *list);

// -------------------------------------------------------------------------
// RIB_request_interface_entry(provider, interface_name, entry_type, entry_name, token) provides
// a convenient way to request interface entries one at a time, without the need to declare an array
// of RIB_INTERFACE_ENTRY variables as with RIB_request_interface() (above).
//
// provider specifies the provider handle; interface_name is the name of the requested
// interface; entry_type specifies the type of the requested entry (RIB_FUNCTION,
// or RIB_PROPERTY); entry_name is the name of the desired function,
// property; and token specifies the address of the variable to
// receive the property token or function address.
//
// If a requested interface entry is not supported by the specified provider,
// the token' variable will receive the value -1 for property
// entries, or NULL for function entries, and the value RIB_NOT_FOUND will be
// returned from the function.  Otherwise, RIB_NOERR will be returned if
// the requested entry was successfully resolved.
// -------------------------------------------------------------------------

RIBDEC RIBRESULT  RIBCALL RIB_request_interface_entry (HPROVIDER                provider,
                                                       C8 const            *interface_name,
                                                       RIB_ENTRY_TYPE           entry_type,
                                                       C8 const            *entry_name,
                                                       UINTa               *token);

// -------------------------------------------------------------------------
// RIB_enumerate_interface(provider, interface_name, type, next, dest) allows a RIB application
// to determine the set of available RIB interface entries associated with a given provider and
// interface.
//
// provider specifies the provider handle; interface_name is the name of the interface to 
// be examined; type specifies the type of entry to return (properties or
// functions); next is a pointer to a variable of type HMSSENUM which is initialized to NULL
// prior to the first call to RIB_enumerate_interface(), and subsequently used by the
// function to maintain the state of the enumeration operation; and dest is a pointer to
// a RIB_INTERFACE_ENTRY structure which receives information about the name, subtype, and 
// access token or function pointer for each qualified interface entry.
//
// A return value of 0 indicates that no (further) interface entries are available.
// -------------------------------------------------------------------------

RIBDEC S32        RIBCALL RIB_enumerate_interface     (HPROVIDER                provider,
                                                       C8 const            *interface_name,
                                                       RIB_ENTRY_TYPE           type,
                                                       HMSSENUM            *next,
                                                       RIB_INTERFACE_ENTRY *dest);

// -------------------------------------------------------------------------
// RIB_enumerate_providers(interface_name, next, dest) allows a RIB application
// to determine the set of available RIB providers of the specified interface.
//
// interface_name is the name of the specified interface; next is a pointer to a 
// variable of type HMSSENUM which is initialized to NULL prior to the first call
// to RIB_enumerate_providers(), and subsequently used by the function to maintain the
// state of the enumeration operation; and dest is a pointer to a variable of type 
// HPROVIDER which receives, in turn, each qualified RIB provider handle.
//
// If interface_name is NULL, all available HPROVIDERs are enumerated, regardless of
// the interface(s) they support.
//
// A return value of 0 indicates that no (further) providers are available.
// -------------------------------------------------------------------------

RIBDEC S32        RIBCALL RIB_enumerate_providers     (C8 const            *interface_name,
                                                       HMSSENUM            *next,
                                                       HPROVIDER           *dest);

// -------------------------------------------------------------------------
// RIB_type_string(data, subtype): Returns a pointer to a string representing
// the value of a variable of a given RIB data subtype, in a format consistent with
// that subtype.  data represents the value to be converted to a string;
// subtype represents the data subtype which determines the string's format
// (RIB_DEC, RIB_HEX, RIB_FLOAT, etc.)
//
// For example, a variable of type RIB_PERCENT will be treated as a floating-
// point percentage, with its value returned in a string of the form "100.0%"
// (without the quotes).
// -------------------------------------------------------------------------

RIBDEC C8 *   RIBCALL RIB_type_string             (void const *         data,
                                                       RIB_DATA_SUBTYPE         subtype);

// -------------------------------------------------------------------------
// RIB_find_file_provider(interface_name, property_name, file_suffix) provides a
// convenient way for the application to locate a RIB provider which exports a given
// property name matching a specified name.
//
// This function uses the PROVIDER_property() function exported by compliant
// RIB interfaces to help an application locate a RIB which is capable of processing
// a given data format.  The file_suffix string may be specified as either a full filename
// ("c:\foo\bar\test.xyz") or simply the suffix in question (".xyz" or "*.xyz").
// interface_name and property_name should be set by the application to correspond to
// the interface and property names required to process files of the specified type.
//
// RIB provider interfaces may be made compatible with this function by providing a
// PROVIDER_property() handler exporting the appropriate property and filespec
// string compatible with Win32 common file-open dialogs.  As an example, the
// function
//
// S32 AILEXPORT PROVIDER_property (HPROPERTY index,
//                                  void * before_value,
//                                  void const * new_value,
//                                  void * after_value )
// {
//    switch (index)
//       {
//       case PROVIDER_NAME:    
//         if ( before_value ) *before_value = "MSS MPEG Audio Decoder";
//         return 1;
//           
//       case PROVIDER_VERSION:    
//         if ( before_value ) *(S32*)before_value = 0x200;
//         return 1;
//
//       case IN_FTYPES:    
//         if ( before_value ) *before_value = "MPEG Layer 3 audio files\0*.MP3\0";
//         return 1;
//           
//       case OUT_FTYPES:    
//         if ( before_value ) *before_value = "Raw PCM files\0*.RAW\0";
//         return 1;
//       }
//
//    return 0;
// }
//
// will allow its provider to be returned to an application which calls RIB_find_file_provider()
// with the name of the interface which exports the PROVIDER_provider() function
// and the property name corresponding to the IN_FTYPES or OUT_FTYPES properties.  (It is up
// to each provider author to decide what property and interface names to make available to
// applications which conduct provider searches with this function.)
// -------------------------------------------------------------------------


RIBDEC HPROVIDER  RIBCALL RIB_find_file_provider      (C8 const                  *interface_name,
                                                       C8 const                  *property_name,
                                                       C8 const                  *file_suffix);

// -------------------------------------------------------------------------
// RIB_find_files_provider is exactly like RIB_find_file_providers, except
// it can search for two file types at once.
// -------------------------------------------------------------------------

RIBDEC HPROVIDER  RIBCALL RIB_find_files_provider     (C8 const                  *interface_name,
                                                       C8 const                  *property_name_1,
                                                       C8 const                  *file_suffix_1,
                                                       C8 const                  *property_name_2,
                                                       C8 const                  *file_suffix_2);


// -------------------------------------------------------------------------
// RIB_find_file_dec_provider is exactly like RIB_find_file_providers, except
// it can search for one file type and one decimal value
// -------------------------------------------------------------------------

RIBDEC HPROVIDER  RIBCALL RIB_find_file_dec_provider  (C8 const                  *interface_name,
                                                       C8 const                  *property_name_1,
                                                       U32                            decimal_property_value_1,
                                                       C8 const                  *property_name_2,
                                                       C8 const                  *file_suffix_2);


// -------------------------------------------------------------------------
// RIB_find_provider will find the provider that contains an property
// name matching the specified property value.
// -------------------------------------------------------------------------

RIBDEC HPROVIDER  RIBCALL RIB_find_provider           (C8 const                  *interface_name,
                                                       C8 const                  *property_name,
                                                       void const                *property_value );


//
// Static library definitions
//

#ifdef IS_STATIC
  #define RIB_MAIN_NAME( name ) name##_RIB_Main
#else
  #define RIB_MAIN_NAME( name ) RIB_Main
#endif

typedef S32 ( RIBCALL * RIB_MAIN_FUNC) ( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );

RIBDEC HPROVIDER RIBCALL RIB_load_static_provider_library (RIB_MAIN_FUNC main, const char * description);

// -------------------------------------------------------------------------
// RIB_load_application_providers(filespec) accepts a wildcard file
// specification of the form ("*.xyz"), and loads all RIB provider
// library DLLs named with the suffix .xyz.  Internally, it performs a
// file search in the current working directory, calling the RIB_load_provider_library()
// function for each provider DLL it finds.
//
// This function has no effect under DOS.
// -------------------------------------------------------------------------

RIBDEC S32        RIBCALL RIB_load_application_providers 
                                                      (C8 const *filespec);

// -------------------------------------------------------------------------
// RIB_set_provider_user/system_data(provider, index, value) and
// RIB_provider_user/system_data(provider, index) allow the application and
// system to associate one or more generic data values with a 
// specified RIB provider handle.  
//
// provider represents the provider handle; index, which may range from 0 to
// 7, represents the "slot" in which the data value is to be stored or 
// retrieved; and value, in the case of the data write functions,
// represents the value to be written.  
//
// Applications should not use the RIB_set_provider_system_data() or 
// RIB_provider_system_data() functions.  These functions are reserved 
// for use by RAD system components only.
// -------------------------------------------------------------------------

RIBDEC  void      RIBCALL RIB_set_provider_user_data  (HPROVIDER provider,
                                                       U32       index,
                                                       SINTa     value);

RIBDEC  SINTa     RIBCALL RIB_provider_user_data      (HPROVIDER provider,
                                                       U32       index);

RIBDEC  void      RIBCALL RIB_set_provider_system_data 
                                                      (HPROVIDER provider,
                                                       U32       index,
                                                       SINTa     value);

RIBDEC  SINTa     RIBCALL RIB_provider_system_data    (HPROVIDER provider,
                                                       U32       index);

// -------------------------------------------------------------------------
// RIB_error(): Returns ASCII text string associated with last error
// condition
// -------------------------------------------------------------------------

RIBDEC C8 *   RIBCALL RIB_error                   (void);


//
// Prototype RIB_Main() here so that it will always be exported from 
// Windows RIB DLLs in undecorated form
//

#ifdef IS_WIN32
__declspec(dllexport) S32 __stdcall RIB_Main( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg );
#endif

RADDEFEND

#if defined(IS_WIN32API) || defined(IS_WII)
  #pragma pack(pop)
#endif

#endif

