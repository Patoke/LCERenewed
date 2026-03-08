//############################################################################
//##                                                                        ##
//##  MSSA32.CPP - PPC assembly mixing kernals...                           ##
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#include "mss.h"
#include "imssapi.h"

static U32 ADPCM_step[89*16]={
  0x0, 0x4000, 0xC000, 0x10000, 0x1C080, 0x20100, 0x28180, 0x2C200, 0x0, 0xFFFFC000, 0xFFFF4000, 0xFFFF0000, 0xFFFE4080, 0xFFFE0100, 0xFFFD8180, 0xFFFD4200,
  0x4000, 0xC000, 0x14000, 0x1C000, 0x240C0, 0x2C140, 0x341C0, 0x3C240, 0xFFFFC000, 0xFFFF4000, 0xFFFEC000, 0xFFFE4000, 0xFFFDC0C0, 0xFFFD4140, 0xFFFCC1C0, 0xFFFC4240,
  0x4040, 0xC040, 0x14040, 0x1C040, 0x28100, 0x30180, 0x38200, 0x40280, 0xFFFFC040, 0xFFFF4040, 0xFFFEC040, 0xFFFE4040, 0xFFFD8100, 0xFFFD0180, 0xFFFC8200, 0xFFFC0280,
  0x4080, 0xC080, 0x18080, 0x20080, 0x2C140, 0x341C0, 0x40240, 0x482C0, 0xFFFFC080, 0xFFFF4080, 0xFFFE8080, 0xFFFE0080, 0xFFFD4140, 0xFFFCC1C0, 0xFFFC0240, 0xFFFB82C0,
  0x40C0, 0xC0C0, 0x180C0, 0x200C0, 0x30180, 0x38200, 0x44280, 0x4C300, 0xFFFFC0C0, 0xFFFF40C0, 0xFFFE80C0, 0xFFFE00C0, 0xFFFD0180, 0xFFFC8200, 0xFFFBC280, 0xFFFB4300,
  0x4100, 0x10100, 0x1C100, 0x28100, 0x341C0, 0x40240, 0x4C2C0, 0x58340, 0xFFFFC100, 0xFFFF0100, 0xFFFE4100, 0xFFFD8100, 0xFFFCC1C0, 0xFFFC0240, 0xFFFB42C0, 0xFFFA8340,
  0x4140, 0x10140, 0x1C140, 0x28140, 0x38200, 0x44280, 0x50300, 0x5C380, 0xFFFFC140, 0xFFFF0140, 0xFFFE4140, 0xFFFD8140, 0xFFFC8200, 0xFFFBC280, 0xFFFB0300, 0xFFFA4380,
  0x4180, 0x10180, 0x20180, 0x2C180, 0x3C240, 0x482C0, 0x58340, 0x643C0, 0xFFFFC180, 0xFFFF0180, 0xFFFE0180, 0xFFFD4180, 0xFFFC4240, 0xFFFB82C0, 0xFFFA8340, 0xFFF9C3C0,
  0x81C0, 0x181C0, 0x281C0, 0x381C0, 0x48280, 0x58300, 0x68380, 0x78400, 0xFFFF81C0, 0xFFFE81C0, 0xFFFD81C0, 0xFFFC81C0, 0xFFFB8280, 0xFFFA8300, 0xFFF98380, 0xFFF88400,
  0x8200, 0x18200, 0x28200, 0x38200, 0x4C2C0, 0x5C340, 0x6C3C0, 0x7C440, 0xFFFF8200, 0xFFFE8200, 0xFFFD8200, 0xFFFC8200, 0xFFFB42C0, 0xFFFA4340, 0xFFF943C0, 0xFFF84440,
  0x8240, 0x18240, 0x2C240, 0x3C240, 0x54300, 0x64380, 0x78400, 0x88480, 0xFFFF8240, 0xFFFE8240, 0xFFFD4240, 0xFFFC4240, 0xFFFAC300, 0xFFF9C380, 0xFFF88400, 0xFFF78480, 
  0x8280, 0x1C280, 0x30280, 0x44280, 0x5C340, 0x703C0, 0x84440, 0x984C0, 0xFFFF8280, 0xFFFE4280, 0xFFFD0280, 0xFFFBC280, 0xFFFA4340, 0xFFF903C0, 0xFFF7C440, 0xFFF684C0, 
  0x82C0, 0x1C2C0, 0x342C0, 0x482C0, 0x64380, 0x78400, 0x90480, 0xA4500, 0xFFFF82C0, 0xFFFE42C0, 0xFFFCC2C0, 0xFFFB82C0, 0xFFF9C380, 0xFFF88400, 0xFFF70480, 0xFFF5C500, 
  0xC300, 0x24300, 0x3C300, 0x54300, 0x703C0, 0x88440, 0xA04C0, 0xB8540, 0xFFFF4300, 0xFFFDC300, 0xFFFC4300, 0xFFFAC300, 0xFFF903C0, 0xFFF78440, 0xFFF604C0, 0xFFF48540, 
  0xC340, 0x28340, 0x44340, 0x60340, 0x7C400, 0x98480, 0xB4500, 0xD0580, 0xFFFF4340, 0xFFFD8340, 0xFFFBC340, 0xFFFA0340, 0xFFF84400, 0xFFF68480, 0xFFF4C500, 0xFFF30580, 
  0xC380, 0x28380, 0x48380, 0x64380, 0x88440, 0xA44C0, 0xC4540, 0xE05C0, 0xFFFF4380, 0xFFFD8380, 0xFFFB8380, 0xFFF9C380, 0xFFF78440, 0xFFF5C4C0, 0xFFF3C540, 0xFFF205C0, 
  0x103C0, 0x303C0, 0x543C0, 0x743C0, 0x98480, 0xB8500, 0xDC580, 0xFC600, 0xFFFF03C0, 0xFFFD03C0, 0xFFFAC3C0, 0xFFF8C3C0, 0xFFF68480, 0xFFF48500, 0xFFF24580, 0xFFF04600,
  0x10400, 0x34400, 0x58400, 0x7C400, 0xA44C0, 0xC8540, 0xEC5C0, 0x110640, 0xFFFF0400, 0xFFFCC400, 0xFFFA8400, 0xFFF84400, 0xFFF5C4C0, 0xFFF38540, 0xFFF145C0, 0xFFEF0640, 
  0x14440, 0x3C440, 0x64440, 0x8C440, 0xB8500, 0xE0580, 0x108600, 0x130680, 0xFFFEC440, 0xFFFC4440, 0xFFF9C440, 0xFFF74440, 0xFFF48500, 0xFFF20580, 0xFFEF8600, 0xFFED0680,
  0x14480, 0x40480, 0x6C480, 0x98480, 0xC8540, 0xF45C0, 0x120640, 0x14C6C0, 0xFFFEC480, 0xFFFC0480, 0xFFF94480, 0xFFF68480, 0xFFF38540, 0xFFF0C5C0, 0xFFEE0640, 0xFFEB46C0, 
  0x184C0, 0x484C0, 0x7C4C0, 0xAC4C0, 0xE0580, 0x110600, 0x144680, 0x174700, 0xFFFE84C0, 0xFFFB84C0, 0xFFF844C0, 0xFFF544C0, 0xFFF20580, 0xFFEF0600, 0xFFEBC680, 0xFFE8C700, 
  0x18500, 0x4C500, 0x84500, 0xB8500, 0xF45C0, 0x128640, 0x1606C0, 0x194740, 0xFFFE8500, 0xFFFB4500, 0xFFF7C500, 0xFFF48500, 0xFFF0C5C0, 0xFFED8640, 0xFFEA06C0, 0xFFE6C740,
  0x1C540, 0x58540, 0x94540, 0xD0540, 0x10C600, 0x148680, 0x184700, 0x1C0780, 0xFFFE4540, 0xFFFA8540, 0xFFF6C540, 0xFFF30540, 0xFFEF4600, 0xFFEB8680, 0xFFE7C700, 0xFFE40780, 
  0x20580, 0x60580, 0xA4580, 0xE4580, 0x128640, 0x1686C0, 0x1AC740, 0x1EC7C0, 0xFFFE0580, 0xFFFA0580, 0xFFF5C580, 0xFFF1C580, 0xFFED8640, 0xFFE986C0, 0xFFE54740, 0xFFE147C0, 
  0x245C0, 0x6C5C0, 0xB45C0, 0xFC5C0, 0x148680, 0x190700, 0x1D8780, 0x220800, 0xFFFDC5C0, 0xFFF945C0, 0xFFF4C5C0, 0xFFF045C0, 0xFFEB8680, 0xFFE70700, 0xFFE28780, 0xFFDE0800, 
  0x28600, 0x78600, 0xC8600, 0x118600, 0x1686C0, 0x1B8740, 0x2087C0, 0x258840, 0xFFFD8600, 0xFFF88600, 0xFFF38600, 0xFFEE8600, 0xFFE986C0, 0xFFE48740, 0xFFDF87C0, 0xFFDA8840, 
  0x2C640, 0x84640, 0xDC640, 0x134640, 0x18C700, 0x1E4780, 0x23C800, 0x294880, 0xFFFD4640, 0xFFF7C640, 0xFFF24640, 0xFFECC640, 0xFFE74700, 0xFFE1C780, 0xFFDC4800, 0xFFD6C880, 
  0x30680, 0x90680, 0xF0680, 0x150680, 0x1B4740, 0x2147C0, 0x274840, 0x2D48C0, 0xFFFD0680, 0xFFF70680, 0xFFF10680, 0xFFEB0680, 0xFFE4C740, 0xFFDEC7C0, 0xFFD8C840, 0xFFD2C8C0, 
  0x346C0, 0x9C6C0, 0x1086C0, 0x1706C0, 0x1E0780, 0x248800, 0x2B4880, 0x31C900, 0xFFFCC6C0, 0xFFF646C0, 0xFFEF86C0, 0xFFE906C0, 0xFFE20780, 0xFFDB8800, 0xFFD4C880, 0xFFCE4900, 
  0x38700, 0xAC700, 0x124700, 0x198700, 0x2107C0, 0x284840, 0x2FC8C0, 0x370940, 0xFFFC8700, 0xFFF54700, 0xFFEDC700, 0xFFE68700, 0xFFDF07C0, 0xFFD7C840, 0xFFD048C0, 0xFFC90940, 
  0x40740, 0xC0740, 0x144740, 0x1C4740, 0x248800, 0x2C8880, 0x34C900, 0x3CC980, 0xFFFC0740, 0xFFF40740, 0xFFEBC740, 0xFFE3C740, 0xFFDB8800, 0xFFD38880, 0xFFCB4900, 0xFFC34980, 
  0x44780, 0xD0780, 0x160780, 0x1EC780, 0x280840, 0x30C8C0, 0x39C940, 0x4289C0, 0xFFFBC780, 0xFFF30780, 0xFFEA0780, 0xFFE14780, 0xFFD80840, 0xFFCF48C0, 0xFFC64940, 0xFFBD89C0, 
  0x4C7C0, 0xE87C0, 0x1847C0, 0x2207C0, 0x2C0880, 0x35C900, 0x3F8980, 0x494A00, 0xFFFB47C0, 0xFFF187C0, 0xFFE7C7C0, 0xFFDE07C0, 0xFFD40880, 0xFFCA4900, 0xFFC08980, 0xFFB6CA00,
  0x54800, 0x100800, 0x1AC800, 0x258800, 0x3088C0, 0x3B4940, 0x4609C0, 0x50CA40, 0xFFFAC800, 0xFFF00800, 0xFFE54800, 0xFFDA8800, 0xFFCF88C0, 0xFFC4C940, 0xFFBA09C0, 0xFFAF4A40, 
  0x5C840, 0x118840, 0x1D8840, 0x294840, 0x354900, 0x410980, 0x4D0A00, 0x58CA80, 0xFFFA4840, 0xFFEE8840, 0xFFE28840, 0xFFD6C840, 0xFFCAC900, 0xFFBF0980, 0xFFB30A00, 0xFFA74A80, 
  0x68880, 0x138880, 0x208880, 0x2D8880, 0x3AC940, 0x47C9C0, 0x54CA40, 0x61CAC0, 0xFFF98880, 0xFFEC8880, 0xFFDF8880, 0xFFD28880, 0xFFC54940, 0xFFB849C0, 0xFFAB4A40, 0xFF9E4AC0, 
  0x708C0, 0x1548C0, 0x23C8C0, 0x3208C0, 0x408980, 0x4ECA00, 0x5D4A80, 0x6B8B00, 0xFFF908C0, 0xFFEAC8C0, 0xFFDC48C0, 0xFFCE08C0, 0xFFBF8980, 0xFFB14A00, 0xFFA2CA80, 0xFF948B00, 
  0x7C900, 0x178900, 0x274900, 0x370900, 0x4709C0, 0x56CA40, 0x668AC0, 0x764B40, 0xFFF84900, 0xFFE88900, 0xFFD8C900, 0xFFC90900, 0xFFB909C0, 0xFFA94A40, 0xFF998AC0, 0xFF89CB40, 
  0x88940, 0x19C940, 0x2B4940, 0x3C8940, 0x4E4A00, 0x5F8A80, 0x710B00, 0x824B80, 0xFFF78940, 0xFFE64940, 0xFFD4C940, 0xFFC38940, 0xFFB1CA00, 0xFFA08A80, 0xFF8F0B00, 0xFF7DCB80, 
  0x98980, 0x1C8980, 0x2FC980, 0x42C980, 0x564A40, 0x694AC0, 0x7C8B40, 0x8F8BC0, 0xFFF68980, 0xFFE38980, 0xFFD04980, 0xFFBD4980, 0xFFA9CA40, 0xFF96CAC0, 0xFF838B40, 0xFF708BC0, 
  0xA89C0, 0x1F89C0, 0x3489C0, 0x4989C0, 0x5ECA80, 0x73CB00, 0x88CB80, 0x9DCC00, 0xFFF589C0, 0xFFE089C0, 0xFFCB89C0, 0xFFB689C0, 0xFFA14A80, 0xFF8C4B00, 0xFF774B80, 0xFF624C00, 
  0xB8A00, 0x228A00, 0x39CA00, 0x50CA00, 0x684AC0, 0x7F4B40, 0x968BC0, 0xAD8C40, 0xFFF48A00, 0xFFDD8A00, 0xFFC64A00, 0xFFAF4A00, 0xFF97CAC0, 0xFF80CB40, 0xFF698BC0, 0xFF528C40, 
  0xCCA40, 0x264A40, 0x3FCA40, 0x594A40, 0x72CB00, 0x8C4B80, 0xA5CC00, 0xBF4C80, 0xFFF34A40, 0xFFD9CA40, 0xFFC04A40, 0xFFA6CA40, 0xFF8D4B00, 0xFF73CB80, 0xFF5A4C00, 0xFF40CC80, 
  0xE0A80, 0x2A0A80, 0x460A80, 0x620A80, 0x7E4B40, 0x9A4BC0, 0xB64C40, 0xD24CC0, 0xFFF20A80, 0xFFD60A80, 0xFFBA0A80, 0xFF9E0A80, 0xFF81CB40, 0xFF65CBC0, 0xFF49CC40, 0xFF2DCCC0, 
  0xF4AC0, 0x2E0AC0, 0x4D0AC0, 0x6BCAC0, 0x8ACB80, 0xA98C00, 0xC88C80, 0xE74D00, 0xFFF0CAC0, 0xFFD20AC0, 0xFFB30AC0, 0xFF944AC0, 0xFF754B80, 0xFF568C00, 0xFF378C80, 0xFF18CD00, 
  0x110B00, 0x330B00, 0x550B00, 0x770B00, 0x990BC0, 0xBB0C40, 0xDD0CC0, 0xFF0D40, 0xFFEF0B00, 0xFFCD0B00, 0xFFAB0B00, 0xFF890B00, 0xFF670BC0, 0xFF450C40, 0xFF230CC0, 0xFF010D40, 
  0x128B40, 0x37CB40, 0x5D4B40, 0x828B40, 0xA80C00, 0xCD4C80, 0xF2CD00, 0x1180D80, 0xFFED8B40, 0xFFC84B40, 0xFFA2CB40, 0xFF7D8B40, 0xFF580C00, 0xFF32CC80, 0xFF0D4D00, 0xFEE80D80, 
  0x148B80, 0x3D8B80, 0x66CB80, 0x8FCB80, 0xB90C40, 0xE20CC0, 0x10B4D40, 0x1344DC0, 0xFFEB8B80, 0xFFC28B80, 0xFF994B80, 0xFF704B80, 0xFF470C40, 0xFF1E0CC0, 0xFEF4CD40, 0xFECBCDC0,
  0x168BC0, 0x43CBC0, 0x710BC0, 0x9E4BC0, 0xCB8C80, 0xF8CD00, 0x1260D80, 0x1534E00, 0xFFE98BC0, 0xFFBC4BC0, 0xFF8F0BC0, 0xFF61CBC0, 0xFF348C80, 0xFF074D00, 0xFEDA0D80, 0xFEACCE00,
  0x18CC00, 0x4A8C00, 0x7C4C00, 0xAE0C00, 0xDFCCC0, 0x1118D40, 0x1434DC0, 0x1750E40, 0xFFE74C00, 0xFFB58C00, 0xFF83CC00, 0xFF520C00, 0xFF204CC0, 0xFEEE8D40, 0xFEBCCDC0, 0xFE8B0E40,
  0x1B4C40, 0x520C40, 0x88CC40, 0xBF8C40, 0xF64D00, 0x12D0D80, 0x163CE00, 0x19A8E80, 0xFFE4CC40, 0xFFAE0C40, 0xFF774C40, 0xFF408C40, 0xFF09CD00, 0xFED30D80, 0xFE9C4E00, 0xFE658E80,
  0x1E0C80, 0x5A0C80, 0x964C80, 0xD24C80, 0x10ECD40, 0x14ACDC0, 0x1870E40, 0x1C30EC0, 0xFFE20C80, 0xFFA60C80, 0xFF69CC80, 0xFF2DCC80, 0xFEF14D40, 0xFEB54DC0, 0xFE790E40, 0xFE3D0EC0, 
  0x210CC0, 0x634CC0, 0xA58CC0, 0xE7CCC0, 0x12A0D80, 0x16C4E00, 0x1AE8E80, 0x1F0CF00, 0xFFDF0CC0, 0xFF9CCCC0, 0xFF5A8CC0, 0xFF184CC0, 0xFED60D80, 0xFE93CE00, 0xFE518E80, 0xFE0F4F00, 
  0x244D00, 0x6D0D00, 0xB60D00, 0xFECD00, 0x147CDC0, 0x1908E40, 0x1D98EC0, 0x2224F40, 0xFFDBCD00, 0xFF930D00, 0xFF4A0D00, 0xFF014D00, 0xFEB84DC0, 0xFE6F8E40, 0xFE268EC0, 0xFDDDCF40, 
  0x280D40, 0x780D40, 0xC84D40, 0x1184D40, 0x1688E00, 0x1B88E80, 0x208CF00, 0x258CF80, 0xFFD80D40, 0xFF880D40, 0xFF37CD40, 0xFEE7CD40, 0xFE978E00, 0xFE478E80, 0xFDF74F00, 0xFDA74F80, 
  0x2C0D80, 0x840D80, 0xDC4D80, 0x1344D80, 0x18CCE40, 0x1E4CEC0, 0x23D0F40, 0x2950FC0, 0xFFD40D80, 0xFF7C0D80, 0xFF23CD80, 0xFECBCD80, 0xFE734E40, 0xFE1B4EC0, 0xFDC30F40, 0xFD6B0FC0, 
  0x308DC0, 0x918DC0, 0xF28DC0, 0x1538DC0, 0x1B48E80, 0x2158F00, 0x2768F80, 0x2D79000, 0xFFCF8DC0, 0xFF6E8DC0, 0xFF0D8DC0, 0xFEAC8DC0, 0xFE4B8E80, 0xFDEA8F00, 0xFD898F80, 0xFD289000, 
  0x354E00, 0x9FCE00, 0x10A8E00, 0x1750E00, 0x1E00EC0, 0x24A8F40, 0x2B54FC0, 0x31FD040, 0xFFCACE00, 0xFF604E00, 0xFEF58E00, 0xFE8B0E00, 0xFE200EC0, 0xFDB58F40, 0xFD4ACFC0, 0xFCE05040, 
  0x3A8E40, 0xAFCE40, 0x1254E40, 0x19A8E40, 0x2100F00, 0x2854F80, 0x2FAD000, 0x3701080, 0xFFC58E40, 0xFF504E40, 0xFEDACE40, 0xFE658E40, 0xFDF00F00, 0xFD7ACF80, 0xFD055000, 0xFC901080,
  0x408E80, 0xC18E80, 0x142CE80, 0x1C3CE80, 0x2450F40, 0x2C60FC0, 0x3475040, 0x3C850C0, 0xFFBF8E80, 0xFF3E8E80, 0xFEBD4E80, 0xFE3C4E80, 0xFDBB0F40, 0xFD3A0FC0, 0xFCB8D040, 0xFC37D0C0, 
  0x470EC0, 0xD50EC0, 0x1630EC0, 0x1F10EC0, 0x27F0F80, 0x30D1000, 0x39B1080, 0x4291100, 0xFFB90EC0, 0xFF2B0EC0, 0xFE9D0EC0, 0xFE0F0EC0, 0xFD810F80, 0xFCF31000, 0xFC651080, 0xFBD71100,
  0x4E0F00, 0xEA0F00, 0x1864F00, 0x2224F00, 0x2BECFC0, 0x35AD040, 0x3F710C0, 0x4931140, 0xFFB20F00, 0xFF160F00, 0xFE79CF00, 0xFDDDCF00, 0xFD414FC0, 0xFCA55040, 0xFC0910C0, 0xFB6D1140,
  0x55CF40, 0x1018F40, 0x1AD4F40, 0x2590F40, 0x3051000, 0x3B0D080, 0x45C9100, 0x5085180, 0xFFAA4F40, 0xFEFE8F40, 0xFE52CF40, 0xFDA70F40, 0xFCFB1000, 0xFC4F5080, 0xFBA39100, 0xFAF7D180,
  0x5E8F80, 0x11B8F80, 0x1D88F80, 0x2958F80, 0x3529040, 0x40F90C0, 0x4CC9140, 0x58991C0, 0xFFA18F80, 0xFEE48F80, 0xFE278F80, 0xFD6A8F80, 0xFCAD9040, 0xFBF090C0, 0xFB339140, 0xFA7691C0,
  0x67CFC0, 0x1378FC0, 0x2078FC0, 0x2D74FC0, 0x3A79080, 0x4775100, 0x5475180, 0x6171200, 0xFF984FC0, 0xFEC88FC0, 0xFDF88FC0, 0xFD28CFC0, 0xFC589080, 0xFB88D100, 0xFAB8D180, 0xF9E91200,
  0x725000, 0x1571000, 0x23BD000, 0x3209000, 0x40550C0, 0x4EA1140, 0x5CED1C0, 0x6B39240, 0xFF8DD000, 0xFEA91000, 0xFDC45000, 0xFCDF9000, 0xFBFAD0C0, 0xFB161140, 0xFA3151C0, 0xF94C9240,
  0x7DD040, 0x1795040, 0x2751040, 0x3709040, 0x46C5100, 0x567D180, 0x6639200, 0x75F1280, 0xFF825040, 0xFE86D040, 0xFD8B1040, 0xFC8F9040, 0xFB93D100, 0xFA985180, 0xF99C9200, 0xF8A11280,
  0x8A5080, 0x19F1080, 0x2B3D080, 0x3C89080, 0x4DD5140, 0x5F211C0, 0x706D240, 0x81B92C0, 0xFF75D080, 0xFE611080, 0xFD4C5080, 0xFC379080, 0xFB22D140, 0xFA0E11C0, 0xF8F95240, 0xF7E492C0,
  0x9810C0, 0x1C850C0, 0x2F8D0C0, 0x42910C0, 0x559D180, 0x68A1200, 0x7BA9280, 0x8EAD300, 0xFF6810C0, 0xFE37D0C0, 0xFD0750C0, 0xFBD710C0, 0xFAA65180, 0xF9761200, 0xF8459280, 0xF7155300,
  0xA75100, 0x1F61100, 0x3451100, 0x493D100, 0x5E2D1C0, 0x7319240, 0x88092C0, 0x9CF5340, 0xFF58D100, 0xFE0A1100, 0xFCBB1100, 0xFB6C5100, 0xFA1D51C0, 0xF8CE9240, 0xF77F92C0, 0xF630D340,
  0xB81140, 0x2285140, 0x398D140, 0x5091140, 0x6799200, 0x7E9D280, 0x95A5300, 0xACA9380, 0xFF481140, 0xFDD7D140, 0xFC675140, 0xFAF71140, 0xF9869200, 0xF8165280, 0xF6A5D300, 0xF5359380,
  0xCA9180, 0x25FD180, 0x3F51180, 0x58A5180, 0x71F9240, 0x8B4D2C0, 0xA4A1340, 0xBDF53C0, 0xFF359180, 0xFDA05180, 0xFC0B1180, 0xFA75D180, 0xF8E09240, 0xF74B52C0, 0xF5B61340, 0xF420D3C0,
  0xDED1C0, 0x29C91C0, 0x45A51C0, 0x61811C0, 0x7D5D280, 0x9939300, 0xB515380, 0xD0F1400, 0xFF2151C0, 0xFD6391C0, 0xFBA5D1C0, 0xF9E811C0, 0xF82A5280, 0xF66C9300, 0xF4AED380, 0xF2F11400,
  0xF51200, 0x2DF5200, 0x4C99200, 0x6B3D200, 0x89E52C0, 0xA889340, 0xC72D3C0, 0xE5D1440, 0xFF0B1200, 0xFD20D200, 0xFB369200, 0xF94C5200, 0xF761D2C0, 0xF5779340, 0xF38D53C0, 0xF1A31440,
  0x10D9240, 0x328D240, 0x5445240, 0x75F9240, 0x97B1300, 0xB965380, 0xDB1D400, 0xFCD1480, 0xFEF29240, 0xFCD75240, 0xFABBD240, 0xF8A09240, 0xF6851300, 0xF469D380, 0xF24E5400, 0xF0331480,
  0x1289280, 0x379D280, 0x5CB1280, 0x81C5280, 0xA6DD340, 0xCBF13C0, 0xF105440, 0x116194C0, 0xFED79280, 0xFC865280, 0xFA351280, 0xF7E3D280, 0xF5925340, 0xF34113C0, 0xF0EFD440, 0xEE9E94C0,
  0x14652C0, 0x3D2D2C0, 0x65F92C0, 0x8EC12C0, 0xB78D380, 0xE055400, 0x10921480, 0x131E9500, 0xFEB9D2C0, 0xFC2D52C0, 0xF9A092C0, 0xF71412C0, 0xF4875380, 0xF1FAD400, 0xEF6E1480, 0xECE19500,
  0x166D300, 0x4349300, 0x7029300, 0x9D05300, 0xC9E93C0, 0xF6C5440, 0x123A54C0, 0x15081540, 0xFE995300, 0xFBCB9300, 0xF8FD9300, 0xF62FD300, 0xF36193C0, 0xF093D440, 0xEDC5D4C0, 0xEAF81540,
  0x18AD340, 0x4A05340, 0x7B61340, 0xACB9340, 0xDE19400, 0x10F71480, 0x140CD500, 0x17225580, 0xFE755340, 0xFB5FD340, 0xF84A1340, 0xF5349340, 0xF21E9400, 0xEF091480, 0xEBF35500, 0xE8DDD580,
  0x1B25380, 0x516D380, 0x87B9380, 0xBE01380, 0xF451440, 0x12A994C0, 0x160E5540, 0x1972D5C0, 0xFE4DD380, 0xFAE95380, 0xF7849380, 0xF4201380, 0xF0BB1440, 0xED5694C0, 0xE9F1D540, 0xE68D55C0,
  0x1DDD3C0, 0x59953C0, 0x954D3C0, 0xD1053C0, 0x10CC1480, 0x14879500, 0x18431580, 0x1BFE9600, 0xFE2253C0, 0xFA66D3C0, 0xF6AB53C0, 0xF2EFD3C0, 0xEF341480, 0xEB789500, 0xE7BD1580, 0xE4019600,
  0x20D9400, 0x6289400, 0xA43D400, 0xE5ED400, 0x127A14C0, 0x16951540, 0x1AB055C0, 0x1ECB5600, 0xFDF29400, 0xF9D79400, 0xF5BC5400, 0xF1A15400, 0xED8614C0, 0xE96B1540, 0xE54FD5C0, 0xE134D600,
  0x2421440, 0x6C65440, 0xB4A9440, 0xFCED440, 0x14531500, 0x18D75580, 0x1D5B9600, 0x21DFD600, 0xFDBE1440, 0xF939D440, 0xF4B59440, 0xF0315440, 0xEBAD1500, 0xE728D580, 0xE2A49600, 0xDE205600,
  0x27BD480, 0x7739480, 0xC6B9480, 0x11635480, 0x165B5540, 0x1B5315C0, 0x204B1600, 0x2542D600, 0xFD845480, 0xF88C9480, 0xF3949480, 0xEE9CD480, 0xE9A4D540, 0xE4AD15C0, 0xDFB51600, 0xDABD5600,
  0x2BB94C0, 0x83294C0, 0xDA994C0, 0x132094C0, 0x1897D580, 0x1E0ED600, 0x2385D600, 0x28FCD600, 0xFD4494C0, 0xF7CD94C0, 0xF25694C0, 0xECDF94C0, 0xE7685580, 0xE1F15600, 0xDC7A5600, 0xD7035600,
  0x3015500, 0x9041500, 0xF071500, 0x1509D500, 0x1B0D15C0, 0x210FD600, 0x2712D600, 0x2D159600, 0xFCFED500, 0xF6FC1500, 0xF0F91500, 0xEAF65500, 0xE4F315C0, 0xDEF05600, 0xD8ED5600, 0xD2EA9600,
  0x34E5540, 0x9EB1540, 0x10881540, 0x1724D540, 0x1DC1D600, 0x245E9600, 0x2AFB9600, 0x31985600, 0xFCB1D540, 0xF6151540, 0xEF781540, 0xE8DB5540, 0xE23E5600, 0xDBA19600, 0xD5049600, 0xCE67D600,
  0x3A31580, 0xAE91580, 0x122F5580, 0x19755580, 0x20BB9600, 0x28019600, 0x2F47D600, 0x368DD600, 0xFC5D1580, 0xF5171580, 0xEDD0D580, 0xE68AD580, 0xDF449600, 0xD7FE9600, 0xD0B85600, 0xC9725600,
  0x3FFD5C0, 0xBFF95C0, 0x13FF95C0, 0x1BFF55C0, 0x23FF9600, 0x2BFF5600, 0x33FF5600, 0x3BFF1600, 0xFC0055C0, 0xF40095C0, 0xEC0095C0, 0xE400D5C0, 0xDC009600, 0xD400D600, 0xCC00D600, 0xC4011600,
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void DecodeADPCM_MONO(
  register void * destp,
  register void const * srcp,
  register S32    dest_lenp,
  register S32    src_lenp,
  register ADPCMDATA* adpcm_ptrp )
{
  // Load pointers
  char* Src = *(char**) srcp;
  char* Dest = *(char**) destp;

  // Compute ending points.
  char* DestEnd = (char*) AIL_ptr_add( Dest, dest_lenp );
  char* SrcEnd = (char*) AIL_ptr_add( Src, src_lenp );

  // Grab current state.
  char* BlockEnd = (char*) AIL_ptr_add( Src, adpcm_ptrp->blockleft );
  U32 BlockSize = adpcm_ptrp->blocksize;
  U32 Extras = adpcm_ptrp->extrasamples;
  U32 LookupL = adpcm_ptrp->step;
  S32 SampleL = adpcm_ptrp->sample;
  U32 BitsL = adpcm_ptrp->samplesL;

  // are we in the middle of a partial block? if so, jump right into the loop
  if ( Extras )
    goto expand_dword;
  if ( Src < BlockEnd )
    goto do_next_dword;

  // start the loop do an entire ADPCM block
  do
  {
    // Load the starting sample and lookup.
    LookupL = LEU_SWAP32 ( Src );
    BlockEnd = Src + BlockSize;

    // Sign extend lower 16 bits of lookupL into the starting sample.
    SampleL = (S16) LookupL;

    // Save the first output sample.
    STORE_LE_SWAP16( Dest, LookupL );

    Src += 4;
    LookupL = ( LookupL >> 10 ) & 0x3FC0;  // this is the top 16-bits, shifted left 6 (4 for bits + 2 for *sizeof(U32))
    Dest += 2;

    // start the loop that does the 8 samples per 32-bit word
    do
    {
      if ( Src >= SrcEnd )
        goto done;

     do_next_dword:

      // Load the next 32 bits of stepping info.
      BitsL = LEU_SWAP32( Src );
      Src += 4;
      LookupL |= ( ( BitsL & 15 ) << 2 ); // or the bottom 4 bits * sizeof(U32)
      Extras = 8;

     expand_dword:

      do
      {
        if ( Dest >= DestEnd )
          goto done;

        // Advance...
        BitsL >>= 4;

        // Load the current lookup
        LookupL = *(U32*) AIL_ptr_add( ADPCM_step, LookupL );

        // Mask and step sample
        SampleL += ( ( (S32) LookupL ) >> 14 );
        LookupL &= 0x3FFF;

        // Update lookup...
        LookupL |= ( ( BitsL & 15 ) << 2 );

        // Clamp
#ifdef IS_X86
        if ( SampleL < -32768 )
          SampleL = -32768;
        else if ( SampleL > 32767 )
          SampleL = 32767;
#elif defined (IS_PSP)
        {
            register const int maxsize = 32767;
            register const int minsize = -32767;
            asm volatile ("min %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (maxsize));
            asm volatile ("max %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (minsize));
        }
#elif defined(IS_PS2)
        {
            register const int maxsize = 32767;
            register const int minsize = -32767;
            asm volatile ("pminw %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (maxsize));
            asm volatile ("pmaxw %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (minsize));
        }
#else
        SampleL -= 32767;
        S32 temp = SampleL >> 31;
        SampleL &= temp;
        SampleL += 65535;
        temp = SampleL >> 31;
        SampleL &= (~temp);
        SampleL -= 32768;
#endif
        // Save output.
        STORE_LE_SWAP16( Dest, SampleL );

        Dest += 2;
        Extras--;
      } while ( Extras );

      if (Dest >= DestEnd)
        goto done;

    } while ( Src < BlockEnd );

  } while ( Src < SrcEnd );

 done:

  // Save state back.
  adpcm_ptrp->extrasamples = Extras;
  adpcm_ptrp->blockleft = (U32) AIL_ptr_dif( BlockEnd, Src );
  adpcm_ptrp->step = LookupL;
  adpcm_ptrp->sample = SampleL;
  adpcm_ptrp->samplesL = BitsL;

  // Save output pointers.
  *(char**) destp = Dest;
  *(char**) srcp = Src;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void DecodeADPCM_MONO_8(
  register void * destp,
  register void const * srcp,
  register S32    dest_lenp,
  register S32    src_lenp,
  register ADPCMDATA* adpcm_ptrp )
{
  // Load pointers
  char* Src = *(char**) srcp;
  char* Dest = *(char**) destp;

  // Compute ending points.
  char* DestEnd = (char*) AIL_ptr_add( Dest, dest_lenp );
  char* SrcEnd = (char*) AIL_ptr_add( Src, src_lenp );

  // Grab current state.
  char* BlockEnd = (char*) AIL_ptr_add( Src, adpcm_ptrp->blockleft );
  U32 BlockSize = adpcm_ptrp->blocksize;
  U32 Extras = adpcm_ptrp->extrasamples;
  U32 LookupL = adpcm_ptrp->step;
  S32 SampleL = adpcm_ptrp->sample;
  U32 BitsL = adpcm_ptrp->samplesL;

  // are we in the middle of a partial block? if so, jump right into the loop
  if ( Extras )
    goto expand_dword;
  if ( Src < BlockEnd )
    goto do_next_dword;

  // start the loop do an entire ADPCM block
  do
  {
    // Load the starting sample and lookup.
    LookupL = LEU_SWAP32 ( Src );
    BlockEnd = Src + BlockSize;

    // Sign extend lower 16 bits of lookupL into the starting sample.
    SampleL = (S16) LookupL;

    // Save the first output sample.
    *(U8*)Dest = (U8)((LookupL >> 8) + 128);

    Src += 4;
    LookupL = ( LookupL >> 10 ) & 0x3FC0;  // this is the top 16-bits, shifted left 6 (4 for bits + 2 for *sizeof(U32))
    Dest += 1;

    // start the loop that does the 8 samples per 32-bit word
    do
    {
      if ( Src >= SrcEnd )
        goto done;

     do_next_dword:

      // Load the next 32 bits of stepping info.
      BitsL = LEU_SWAP32( Src );
      Src += 4;
      LookupL |= ( ( BitsL & 15 ) << 2 ); // or the bottom 4 bits * sizeof(U32)
      Extras = 8;

     expand_dword:

      do
      {
        if ( Dest >= DestEnd )
          goto done;

        // Advance...
        BitsL >>= 4;

        // Load the current lookup
        LookupL = *(U32*) AIL_ptr_add( ADPCM_step, LookupL );

        // Mask and step sample
        SampleL += ( ( (S32) LookupL ) >> 14 );
        LookupL &= 0x3FFF;

        // Update lookup...
        LookupL |= ( ( BitsL & 15 ) << 2 );

        // Clamp
#ifdef IS_X86
        if ( SampleL < -32768 )
          SampleL = -32768;
        else if ( SampleL > 32767 )
          SampleL = 32767;
#elif defined (IS_PSP)
        {
            register const int maxsize = 32767;
            register const int minsize = -32767;
            asm volatile ("min %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (maxsize));
            asm volatile ("max %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (minsize));
        }
#elif defined(IS_PS2)
        {
            register const int maxsize = 32767;
            register const int minsize = -32767;
            asm volatile ("pminw %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (maxsize));
            asm volatile ("pmaxw %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (minsize));
        }
#else
        SampleL -= 32767;
        S32 temp = SampleL >> 31;
        SampleL &= temp;
        SampleL += 65535;
        temp = SampleL >> 31;
        SampleL &= (~temp);
        SampleL -= 32768;
#endif
        // Save output.
        *(U8*)Dest = (U8)((SampleL >> 8) + 128);

        Dest += 1;
        Extras--;
      } while ( Extras );

      if (Dest >= DestEnd)
        goto done;

    } while ( Src < BlockEnd );

  } while ( Src < SrcEnd );

 done:

  // Save state back.
  adpcm_ptrp->extrasamples = Extras;
  adpcm_ptrp->blockleft = (U32) AIL_ptr_dif( BlockEnd, Src );
  adpcm_ptrp->step = LookupL;
  adpcm_ptrp->sample = SampleL;
  adpcm_ptrp->samplesL = BitsL;

  // Save output pointers.
  *(char**) destp = Dest;
  *(char**) srcp = Src;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void DecodeADPCM_STEREO(
  register void * destp,
  register void const * srcp,
  register S32    dest_lenp,
  register S32    src_lenp,
  register ADPCMDATA* adpcm_ptrp )
{
  U32 temp;

  // Load pointers
  char* Src = *(char**) srcp;
  char* Dest = *(char**) destp;

  // Compute ending points.
  char* DestEnd = (char*) AIL_ptr_add( Dest, dest_lenp );
  char* SrcEnd = (char*) AIL_ptr_add( Src, src_lenp );

  // Grab current state.
  char* BlockEnd = (char*) AIL_ptr_add( Src, adpcm_ptrp->blockleft );
  U32 BlockSize = adpcm_ptrp->blocksize;
  U32 Extras = adpcm_ptrp->extrasamples;
  U32 LookupL = adpcm_ptrp->step;
  U32 LookupR = LookupL >> 16;
  LookupL = (U16) LookupL;
  S32 SampleL = adpcm_ptrp->sample;
  S32 SampleR = SampleL >> 16;
  SampleL = (S16) SampleL;
  U32 BitsL = adpcm_ptrp->samplesL;
  U32 BitsR = adpcm_ptrp->samplesR;

  // are we in the middle of a partial block? if so, jump right into the loop
  if ( Extras )
    goto expand_dword;
  if ( Src < BlockEnd )
    goto do_next_dword;

  // start the loop do an entire ADPCM block
  do
  {
    // Load the starting sample and lookup.
    LookupL = LE_SWAP32 ( Src );
    LookupR = LE_SWAP32_OFS ( Src, 4 );
    BlockEnd = Src + BlockSize;

    // Sign extend lower 16 bits of lookupL into the starting sample.
    SampleL = (S16) LookupL;
    SampleR = (S16) LookupR;

    // Save the first output sample.
    temp = ( SampleL & 0xffff ) | ( SampleR << 16 );
    STORE_LE_SWAP32( Dest, temp );

    Src += 8;
    LookupL = ( LookupL >> 10 ) & 0x3FC0;  // this is the top 16-bits, shifted left 6 (4 for bits + 2 for *sizeof(U32))
    LookupR = ( LookupR >> 10 ) & 0x3FC0;  // this is the top 16-bits, shifted left 6 (4 for bits + 2 for *sizeof(U32))
    Dest += 4;

    // start the loop that does the 8 samples per 32-bit word
    do
    {
      if (Src >= SrcEnd)
        goto done;

     do_next_dword:

      // Load the next 32 bits of stepping info.
      BitsL = LE_SWAP32( Src );
      BitsR = LE_SWAP32_OFS( Src, 4 );
      Src += 8;
      LookupL |= ( ( BitsL & 15 ) << 2 ); // or the bottom 4 bits * sizeof(U32)
      LookupR |= ( ( BitsR & 15 ) << 2 ); // or the bottom 4 bits * sizeof(U32)
      Extras = 8;

     expand_dword:

      do
      {
        if ( Dest >= DestEnd )
          goto done;

        //MSSBreakPoint();

        // Advance...
        BitsL >>= 4;
        BitsR >>= 4;

        // Load the current lookup
        LookupL = *(U32*) AIL_ptr_add( ADPCM_step, LookupL );
        LookupR = *(U32*) AIL_ptr_add( ADPCM_step, LookupR );

        // Mask and step sample
        SampleL += ( ( (S32) LookupL ) >> 14 );
        SampleR += ( ( (S32) LookupR ) >> 14 );
        LookupL &= 0x3FFF;
        LookupR &= 0x3FFF;

        // Update lookup...
        LookupL |= ( ( BitsL & 15 ) << 2 );
        LookupR |= ( ( BitsR & 15 ) << 2 );

        // Clamp
#if defined(IS_X86)
        if ( SampleL < -32768 )
          SampleL = -32768;
        else if ( SampleL > 32767 )
          SampleL = 32767;
        if ( SampleR < -32768 )
          SampleR = -32768;
        else if ( SampleR > 32767 )
          SampleR = 32767;
#elif defined(IS_PSP)
        {
            register const int maxsize = 32767;
            register const int minsize = -32767;
            asm volatile ("min %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (maxsize));
            asm volatile ("min %0, %1, %2" : "=r" (SampleR) : "r" (SampleR), "r" (maxsize));
            asm volatile ("max %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (minsize));
            asm volatile ("max %0, %1, %2" : "=r" (SampleR) : "r" (SampleR), "r" (minsize));
        }
#elif defined(IS_PS2)
        {
            register const int maxsize = 32767;
            register const int minsize = -32767;
            asm volatile ("pminw %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (maxsize));
            asm volatile ("pminw %0, %1, %2" : "=r" (SampleR) : "r" (SampleR), "r" (maxsize));
            asm volatile ("pmaxw %0, %1, %2" : "=r" (SampleL) : "r" (SampleL), "r" (minsize));
            asm volatile ("pmaxw %0, %1, %2" : "=r" (SampleR) : "r" (SampleR), "r" (minsize));
        }
#else
        SampleL -= 32767;
        temp = SampleL >> 31;
        SampleL &= temp;
        SampleL += 65535;
        temp = SampleL >> 31;
        SampleL &= (~temp);
        SampleL -= 32768;

        SampleR -= 32767;
        temp = SampleR >> 31;
        SampleR &= temp;
        SampleR += 65535;
        temp = SampleR >> 31;
        SampleR &= (~temp);
        SampleR -= 32768;
#endif

        // Save output.
        temp = ( SampleL & 0xffff ) | ( SampleR << 16 );
        STORE_LE_SWAP32( Dest, temp );

        Dest += 4;
        Extras--;
      } while ( Extras );

      if ( Dest >= DestEnd )
        goto done;

    } while ( Src < BlockEnd );

  } while ( Src < SrcEnd );

 done:

  // Save state back.
  adpcm_ptrp->extrasamples = Extras;
  adpcm_ptrp->blockleft = (U32) AIL_ptr_dif( BlockEnd, Src );
  adpcm_ptrp->step = ( LookupL & 0xffff ) | ( LookupR << 16 );
  adpcm_ptrp->sample = ( SampleL & 0xffff ) | ( SampleR << 16 );
  adpcm_ptrp->samplesL = BitsL;
  adpcm_ptrp->samplesR = BitsR;

  // Save output pointers.
  *(char**) destp = Dest;
  *(char**) srcp = Src;
}
