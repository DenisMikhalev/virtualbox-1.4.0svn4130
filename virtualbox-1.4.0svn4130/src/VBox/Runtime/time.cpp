/* $Id: time.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - Time.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_TIME
#include <iprt/time.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include "internal/time.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The max year we possibly could implode. */
#define RTTIME_MAX_YEAR         (292 + 1970)
/** The min year we possibly could implode. */
#define RTTIME_MIN_YEAR         (-293 + 1970)

/** The max day supported by our time representation. (2262-04-11T23-47-16.854775807) */
#define RTTIME_MAX_DAY          (365*292+71 + 101-1)
/** The min day supported by our time representation. (1677-09-21T00-12-43.145224192) */
#define RTTIME_MIN_DAY          (365*-293-70 + 264-1)

/** The max nano second into the max day.             (2262-04-11T23-47-16.854775807) */
#define RTTIME_MAX_DAY_NANO     ( INT64_C(1000000000) * (23*3600 + 47*60 + 16) + 854775807 )
/** The min nano second into the min day.             (1677-09-21T00-12-43.145224192) */
#define RTTIME_MIN_DAY_NANO     ( INT64_C(1000000000) * (00*3600 + 12*60 + 43) + 145224192 )


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Days per month in a common year.
 */
static const uint8_t g_acDaysInMonths[12] =
{
  /*Jan Feb Mar Arp May Jun Jul Aug Sep Oct Nov Dec */
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/**
 * Days per month in a leap year.
 */
static const uint8_t g_acDaysInMonthsLeap[12] =
{
  /*Jan Feb Mar Arp May Jun Jul Aug Sep Oct Nov Dec */
    31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/**
 * The day of year for each month in a common year.
 */
static const uint16_t g_aiDayOfYear[12 + 1] =
{
    1,                                  /* Jan */
    1+31,                               /* Feb */
    1+31+28,                            /* Mar */
    1+31+28+31,                         /* Apr */
    1+31+28+31+30,                      /* May */
    1+31+28+31+30+31,                   /* Jun */
    1+31+28+31+30+31+30,                /* Jul */
    1+31+28+31+30+31+30+31,             /* Aug */
    1+31+28+31+30+31+30+31+31,          /* Sep */
    1+31+28+31+30+31+30+31+31+30,       /* Oct */
    1+31+28+31+30+31+30+31+31+30+31,    /* Nov */
    1+31+28+31+30+31+30+31+31+30+31+30, /* Dec */
    1+31+28+31+30+31+30+31+31+30+31+30+31
};

/**
 * The day of year for each month in a leap year.
 */
static const uint16_t g_aiDayOfYearLeap[12 + 1] =
{
    1,                                  /* Jan */
    1+31,                               /* Feb */
    1+31+29,                            /* Mar */
    1+31+29+31,                         /* Apr */
    1+31+29+31+30,                      /* May */
    1+31+29+31+30+31,                   /* Jun */
    1+31+29+31+30+31+30,                /* Jul */
    1+31+29+31+30+31+30+31,             /* Aug */
    1+31+29+31+30+31+30+31+31,          /* Sep */
    1+31+29+31+30+31+30+31+31+30,       /* Oct */
    1+31+29+31+30+31+30+31+31+30+31,    /* Nov */
    1+31+29+31+30+31+30+31+31+30+31+30, /* Dec */
    1+31+29+31+30+31+30+31+31+30+31+30+31
};

/** The index of 1970 in g_aoffYear */
#define OFF_YEAR_IDX_EPOCH  300
/** The year of the first index. */
#define OFF_YEAR_IDX_0_YEAR 1670

/**
 * The number of days the 1st of january a year is offseted from 1970-01-01.
 */
static const int32_t g_aoffYear[] =
{
/*1670:*/ 365*-300+-72, 365*-299+-72, 365*-298+-72, 365*-297+-71, 365*-296+-71, 365*-295+-71, 365*-294+-71, 365*-293+-70, 365*-292+-70, 365*-291+-70,
/*1680:*/ 365*-290+-70, 365*-289+-69, 365*-288+-69, 365*-287+-69, 365*-286+-69, 365*-285+-68, 365*-284+-68, 365*-283+-68, 365*-282+-68, 365*-281+-67,
/*1690:*/ 365*-280+-67, 365*-279+-67, 365*-278+-67, 365*-277+-66, 365*-276+-66, 365*-275+-66, 365*-274+-66, 365*-273+-65, 365*-272+-65, 365*-271+-65,
/*1700:*/ 365*-270+-65, 365*-269+-65, 365*-268+-65, 365*-267+-65, 365*-266+-65, 365*-265+-64, 365*-264+-64, 365*-263+-64, 365*-262+-64, 365*-261+-63,
/*1710:*/ 365*-260+-63, 365*-259+-63, 365*-258+-63, 365*-257+-62, 365*-256+-62, 365*-255+-62, 365*-254+-62, 365*-253+-61, 365*-252+-61, 365*-251+-61,
/*1720:*/ 365*-250+-61, 365*-249+-60, 365*-248+-60, 365*-247+-60, 365*-246+-60, 365*-245+-59, 365*-244+-59, 365*-243+-59, 365*-242+-59, 365*-241+-58,
/*1730:*/ 365*-240+-58, 365*-239+-58, 365*-238+-58, 365*-237+-57, 365*-236+-57, 365*-235+-57, 365*-234+-57, 365*-233+-56, 365*-232+-56, 365*-231+-56,
/*1740:*/ 365*-230+-56, 365*-229+-55, 365*-228+-55, 365*-227+-55, 365*-226+-55, 365*-225+-54, 365*-224+-54, 365*-223+-54, 365*-222+-54, 365*-221+-53,
/*1750:*/ 365*-220+-53, 365*-219+-53, 365*-218+-53, 365*-217+-52, 365*-216+-52, 365*-215+-52, 365*-214+-52, 365*-213+-51, 365*-212+-51, 365*-211+-51,
/*1760:*/ 365*-210+-51, 365*-209+-50, 365*-208+-50, 365*-207+-50, 365*-206+-50, 365*-205+-49, 365*-204+-49, 365*-203+-49, 365*-202+-49, 365*-201+-48,
/*1770:*/ 365*-200+-48, 365*-199+-48, 365*-198+-48, 365*-197+-47, 365*-196+-47, 365*-195+-47, 365*-194+-47, 365*-193+-46, 365*-192+-46, 365*-191+-46,
/*1780:*/ 365*-190+-46, 365*-189+-45, 365*-188+-45, 365*-187+-45, 365*-186+-45, 365*-185+-44, 365*-184+-44, 365*-183+-44, 365*-182+-44, 365*-181+-43,
/*1790:*/ 365*-180+-43, 365*-179+-43, 365*-178+-43, 365*-177+-42, 365*-176+-42, 365*-175+-42, 365*-174+-42, 365*-173+-41, 365*-172+-41, 365*-171+-41,
/*1800:*/ 365*-170+-41, 365*-169+-41, 365*-168+-41, 365*-167+-41, 365*-166+-41, 365*-165+-40, 365*-164+-40, 365*-163+-40, 365*-162+-40, 365*-161+-39,
/*1810:*/ 365*-160+-39, 365*-159+-39, 365*-158+-39, 365*-157+-38, 365*-156+-38, 365*-155+-38, 365*-154+-38, 365*-153+-37, 365*-152+-37, 365*-151+-37,
/*1820:*/ 365*-150+-37, 365*-149+-36, 365*-148+-36, 365*-147+-36, 365*-146+-36, 365*-145+-35, 365*-144+-35, 365*-143+-35, 365*-142+-35, 365*-141+-34,
/*1830:*/ 365*-140+-34, 365*-139+-34, 365*-138+-34, 365*-137+-33, 365*-136+-33, 365*-135+-33, 365*-134+-33, 365*-133+-32, 365*-132+-32, 365*-131+-32,
/*1840:*/ 365*-130+-32, 365*-129+-31, 365*-128+-31, 365*-127+-31, 365*-126+-31, 365*-125+-30, 365*-124+-30, 365*-123+-30, 365*-122+-30, 365*-121+-29,
/*1850:*/ 365*-120+-29, 365*-119+-29, 365*-118+-29, 365*-117+-28, 365*-116+-28, 365*-115+-28, 365*-114+-28, 365*-113+-27, 365*-112+-27, 365*-111+-27,
/*1860:*/ 365*-110+-27, 365*-109+-26, 365*-108+-26, 365*-107+-26, 365*-106+-26, 365*-105+-25, 365*-104+-25, 365*-103+-25, 365*-102+-25, 365*-101+-24,
/*1870:*/ 365*-100+-24, 365* -99+-24, 365* -98+-24, 365* -97+-23, 365* -96+-23, 365* -95+-23, 365* -94+-23, 365* -93+-22, 365* -92+-22, 365* -91+-22,
/*1880:*/ 365* -90+-22, 365* -89+-21, 365* -88+-21, 365* -87+-21, 365* -86+-21, 365* -85+-20, 365* -84+-20, 365* -83+-20, 365* -82+-20, 365* -81+-19,
/*1890:*/ 365* -80+-19, 365* -79+-19, 365* -78+-19, 365* -77+-18, 365* -76+-18, 365* -75+-18, 365* -74+-18, 365* -73+-17, 365* -72+-17, 365* -71+-17,
/*1900:*/ 365* -70+-17, 365* -69+-17, 365* -68+-17, 365* -67+-17, 365* -66+-17, 365* -65+-16, 365* -64+-16, 365* -63+-16, 365* -62+-16, 365* -61+-15,
/*1910:*/ 365* -60+-15, 365* -59+-15, 365* -58+-15, 365* -57+-14, 365* -56+-14, 365* -55+-14, 365* -54+-14, 365* -53+-13, 365* -52+-13, 365* -51+-13,
/*1920:*/ 365* -50+-13, 365* -49+-12, 365* -48+-12, 365* -47+-12, 365* -46+-12, 365* -45+-11, 365* -44+-11, 365* -43+-11, 365* -42+-11, 365* -41+-10,
/*1930:*/ 365* -40+-10, 365* -39+-10, 365* -38+-10, 365* -37+-9 , 365* -36+-9 , 365* -35+-9 , 365* -34+-9 , 365* -33+-8 , 365* -32+-8 , 365* -31+-8 ,
/*1940:*/ 365* -30+-8 , 365* -29+-7 , 365* -28+-7 , 365* -27+-7 , 365* -26+-7 , 365* -25+-6 , 365* -24+-6 , 365* -23+-6 , 365* -22+-6 , 365* -21+-5 ,
/*1950:*/ 365* -20+-5 , 365* -19+-5 , 365* -18+-5 , 365* -17+-4 , 365* -16+-4 , 365* -15+-4 , 365* -14+-4 , 365* -13+-3 , 365* -12+-3 , 365* -11+-3 ,
/*1960:*/ 365* -10+-3 , 365*  -9+-2 , 365*  -8+-2 , 365*  -7+-2 , 365*  -6+-2 , 365*  -5+-1 , 365*  -4+-1 , 365*  -3+-1 , 365*  -2+-1 , 365*  -1+0  ,
/*1970:*/ 365*   0+0  , 365*   1+0  , 365*   2+0  , 365*   3+1  , 365*   4+1  , 365*   5+1  , 365*   6+1  , 365*   7+2  , 365*   8+2  , 365*   9+2  ,
/*1980:*/ 365*  10+2  , 365*  11+3  , 365*  12+3  , 365*  13+3  , 365*  14+3  , 365*  15+4  , 365*  16+4  , 365*  17+4  , 365*  18+4  , 365*  19+5  ,
/*1990:*/ 365*  20+5  , 365*  21+5  , 365*  22+5  , 365*  23+6  , 365*  24+6  , 365*  25+6  , 365*  26+6  , 365*  27+7  , 365*  28+7  , 365*  29+7  ,
/*2000:*/ 365*  30+7  , 365*  31+8  , 365*  32+8  , 365*  33+8  , 365*  34+8  , 365*  35+9  , 365*  36+9  , 365*  37+9  , 365*  38+9  , 365*  39+10 ,
/*2010:*/ 365*  40+10 , 365*  41+10 , 365*  42+10 , 365*  43+11 , 365*  44+11 , 365*  45+11 , 365*  46+11 , 365*  47+12 , 365*  48+12 , 365*  49+12 ,
/*2020:*/ 365*  50+12 , 365*  51+13 , 365*  52+13 , 365*  53+13 , 365*  54+13 , 365*  55+14 , 365*  56+14 , 365*  57+14 , 365*  58+14 , 365*  59+15 ,
/*2030:*/ 365*  60+15 , 365*  61+15 , 365*  62+15 , 365*  63+16 , 365*  64+16 , 365*  65+16 , 365*  66+16 , 365*  67+17 , 365*  68+17 , 365*  69+17 ,
/*2040:*/ 365*  70+17 , 365*  71+18 , 365*  72+18 , 365*  73+18 , 365*  74+18 , 365*  75+19 , 365*  76+19 , 365*  77+19 , 365*  78+19 , 365*  79+20 ,
/*2050:*/ 365*  80+20 , 365*  81+20 , 365*  82+20 , 365*  83+21 , 365*  84+21 , 365*  85+21 , 365*  86+21 , 365*  87+22 , 365*  88+22 , 365*  89+22 ,
/*2060:*/ 365*  90+22 , 365*  91+23 , 365*  92+23 , 365*  93+23 , 365*  94+23 , 365*  95+24 , 365*  96+24 , 365*  97+24 , 365*  98+24 , 365*  99+25 ,
/*2070:*/ 365* 100+25 , 365* 101+25 , 365* 102+25 , 365* 103+26 , 365* 104+26 , 365* 105+26 , 365* 106+26 , 365* 107+27 , 365* 108+27 , 365* 109+27 ,
/*2080:*/ 365* 110+27 , 365* 111+28 , 365* 112+28 , 365* 113+28 , 365* 114+28 , 365* 115+29 , 365* 116+29 , 365* 117+29 , 365* 118+29 , 365* 119+30 ,
/*2090:*/ 365* 120+30 , 365* 121+30 , 365* 122+30 , 365* 123+31 , 365* 124+31 , 365* 125+31 , 365* 126+31 , 365* 127+32 , 365* 128+32 , 365* 129+32 ,
/*2100:*/ 365* 130+32 , 365* 131+32 , 365* 132+32 , 365* 133+32 , 365* 134+32 , 365* 135+33 , 365* 136+33 , 365* 137+33 , 365* 138+33 , 365* 139+34 ,
/*2110:*/ 365* 140+34 , 365* 141+34 , 365* 142+34 , 365* 143+35 , 365* 144+35 , 365* 145+35 , 365* 146+35 , 365* 147+36 , 365* 148+36 , 365* 149+36 ,
/*2120:*/ 365* 150+36 , 365* 151+37 , 365* 152+37 , 365* 153+37 , 365* 154+37 , 365* 155+38 , 365* 156+38 , 365* 157+38 , 365* 158+38 , 365* 159+39 ,
/*2130:*/ 365* 160+39 , 365* 161+39 , 365* 162+39 , 365* 163+40 , 365* 164+40 , 365* 165+40 , 365* 166+40 , 365* 167+41 , 365* 168+41 , 365* 169+41 ,
/*2140:*/ 365* 170+41 , 365* 171+42 , 365* 172+42 , 365* 173+42 , 365* 174+42 , 365* 175+43 , 365* 176+43 , 365* 177+43 , 365* 178+43 , 365* 179+44 ,
/*2150:*/ 365* 180+44 , 365* 181+44 , 365* 182+44 , 365* 183+45 , 365* 184+45 , 365* 185+45 , 365* 186+45 , 365* 187+46 , 365* 188+46 , 365* 189+46 ,
/*2160:*/ 365* 190+46 , 365* 191+47 , 365* 192+47 , 365* 193+47 , 365* 194+47 , 365* 195+48 , 365* 196+48 , 365* 197+48 , 365* 198+48 , 365* 199+49 ,
/*2170:*/ 365* 200+49 , 365* 201+49 , 365* 202+49 , 365* 203+50 , 365* 204+50 , 365* 205+50 , 365* 206+50 , 365* 207+51 , 365* 208+51 , 365* 209+51 ,
/*2180:*/ 365* 210+51 , 365* 211+52 , 365* 212+52 , 365* 213+52 , 365* 214+52 , 365* 215+53 , 365* 216+53 , 365* 217+53 , 365* 218+53 , 365* 219+54 ,
/*2190:*/ 365* 220+54 , 365* 221+54 , 365* 222+54 , 365* 223+55 , 365* 224+55 , 365* 225+55 , 365* 226+55 , 365* 227+56 , 365* 228+56 , 365* 229+56 ,
/*2200:*/ 365* 230+56 , 365* 231+56 , 365* 232+56 , 365* 233+56 , 365* 234+56 , 365* 235+57 , 365* 236+57 , 365* 237+57 , 365* 238+57 , 365* 239+58 ,
/*2210:*/ 365* 240+58 , 365* 241+58 , 365* 242+58 , 365* 243+59 , 365* 244+59 , 365* 245+59 , 365* 246+59 , 365* 247+60 , 365* 248+60 , 365* 249+60 ,
/*2220:*/ 365* 250+60 , 365* 251+61 , 365* 252+61 , 365* 253+61 , 365* 254+61 , 365* 255+62 , 365* 256+62 , 365* 257+62 , 365* 258+62 , 365* 259+63 ,
/*2230:*/ 365* 260+63 , 365* 261+63 , 365* 262+63 , 365* 263+64 , 365* 264+64 , 365* 265+64 , 365* 266+64 , 365* 267+65 , 365* 268+65 , 365* 269+65 ,
/*2240:*/ 365* 270+65 , 365* 271+66 , 365* 272+66 , 365* 273+66 , 365* 274+66 , 365* 275+67 , 365* 276+67 , 365* 277+67 , 365* 278+67 , 365* 279+68 ,
/*2250:*/ 365* 280+68 , 365* 281+68 , 365* 282+68 , 365* 283+69 , 365* 284+69 , 365* 285+69 , 365* 286+69 , 365* 287+70 , 365* 288+70 , 365* 289+70 ,
/*2260:*/ 365* 290+70 , 365* 291+71 , 365* 292+71 , 365* 293+71 , 365* 294+71 , 365* 295+72 , 365* 296+72 , 365* 297+72 , 365* 298+72 , 365* 299+73
};

/* generator code:
#include <stdio.h>
bool isLeapYear(int iYear)
{
    return iYear % 4 == 0 && (iYear % 100 != 0 || iYear % 400 == 0);
}
void printYear(int iYear, int iLeap)
{
    if (!(iYear % 10))
        printf("\n/" "*%d:*" "/", iYear + 1970);
    printf(" 365*%4d+%-3d,", iYear, iLeap);
}
int main()
{
    int iYear = 0;
    int iLeap = 0;
    while (iYear > -300)
        iLeap -= isLeapYear(1970 + --iYear);
    while (iYear < 300)
    {
        printYear(iYear, iLeap);
        iLeap += isLeapYear(1970 + iYear++);
    }
    printf("\n");
    return 0;
}
*/


/**
 * Checks if a year is a leap year or not.
 *
 * @returns true if it's a leap year.
 * @returns false if it's a common year.
 * @param   i32Year     The year in question.
 */
DECLINLINE(bool) rtTimeIsLeapYear(int32_t i32Year)
{
    return i32Year % 4 == 0
        && (    i32Year % 100 != 0
            ||  i32Year % 400 == 0);
}

/**
 * Checks if a year is a leap year or not.
 *
 * @returns true if it's a leap year.
 * @returns false if it's a common year.
 * @param   i32Year     The year in question.
 */
RTDECL(bool) RTTimeIsLeapYear(int32_t i32Year)
{
    return rtTimeIsLeapYear(i32Year);
}


/**
 * Explodes a time spec (UTC).
 *
 * @returns pTime.
 * @param   pTime       Where to store the exploded time.
 * @param   pTimeSpec   The time spec to exploded.
 */
RTDECL(PRTTIME) RTTimeExplode(PRTTIME pTime, PCRTTIMESPEC pTimeSpec)
{
    AssertMsg(VALID_PTR(pTime), ("%p\n", pTime));
    AssertMsg(VALID_PTR(pTimeSpec), ("%p\n", pTime));

    /*
     * The simple stuff first.
     */
    pTime->fFlags        = RTTIME_FLAGS_TYPE_UTC;
    int64_t i64Div = pTimeSpec->i64NanosecondsRelativeToUnixEpoch;
    int32_t i32Rem = (int32_t)(i64Div % 1000000000);
    i64Div /= 1000000000;
    if (i32Rem < 0)
    {
        i32Rem += 1000000000;
        i64Div--;
    }
    pTime->u32Nanosecond = i32Rem;

    /* second */
    i32Rem = (int32_t)(i64Div % 60);
    i64Div /= 60;
    if (i32Rem < 0)
    {
        i32Rem += 60;
        i64Div--;
    }
    pTime->u8Second      = i32Rem;

    /* minute */
    int32_t i32Div = (int32_t)i64Div;   /* 60,000,000,000 > 33bit, so 31bit suffices. */
    i32Rem = i32Div % 60;
    i32Div /= 60;
    if (i32Rem < 0)
    {
        i32Rem += 60;
        i32Div--;
    }
    pTime->u8Minute      = i32Rem;

    /* hour */
    i32Rem = i32Div % 24;
    i32Div /= 24;                       /* days relative to 1970-01-01 */
    if (i32Rem < 0)
    {
        i32Rem += 24;
        i32Div--;
    }
    pTime->u8Hour        = i32Rem;

    /* weekday - 1970-01-01 was a Thursday (3) */
    pTime->u8WeekDay     = (uint32_t)(i32Div + 3) % 7;

    /*
     * We've now got a number of days relative to 1970-01-01.
     * To get the correct year number we have to mess with leap years. Fortunatly,
     * the represenation we've got only supports a few hundred years, so we can
     * generate a table and perform a simple two way search from the modulus 365 derived.
     */
    unsigned iYear = OFF_YEAR_IDX_EPOCH + i32Div / 365;
    while (g_aoffYear[iYear + 1] <= i32Div)
        iYear++;
    while (g_aoffYear[iYear] > i32Div)
        iYear--;
    pTime->i32Year       = iYear + OFF_YEAR_IDX_0_YEAR;
    i32Div -= g_aoffYear[iYear];
    pTime->u16YearDay    = i32Div + 1;

    /*
     * Figuring out the month is done in a manner similar to the year, only here we
     * ensure that the index is matching or too small.
     */
    const uint16_t *paiDayOfYear;
    if (rtTimeIsLeapYear(pTime->i32Year))
    {
        pTime->fFlags   |= RTTIME_FLAGS_LEAP_YEAR;
        paiDayOfYear = &g_aiDayOfYearLeap[0];
    }
    else
    {
        pTime->fFlags   |= RTTIME_FLAGS_COMMON_YEAR;
        paiDayOfYear = &g_aiDayOfYear[0];
    }
    int iMonth = i32Div / 32;
    i32Div++;
    while (paiDayOfYear[iMonth + 1] <= i32Div)
        iMonth++;
    pTime->u8Month       = iMonth + 1;
    i32Div -= paiDayOfYear[iMonth];
    pTime->u8MonthDay    = i32Div + 1;

    return pTime;
}


/**
 * Implodes exploded time to a time spec (UTC).
 *
 * @returns pTime on success.
 * @returns NULL if the pTime data is invalid.
 * @param   pTimeSpec   Where to store the imploded UTC time.
 *                      If pTime specifies a time which outside the range, maximum or
 *                      minimum values will be returned.
 * @param   pTime       Pointer to the exploded time to implode.
 *                      The fields u8Month, u8WeekDay and u8MonthDay are not used,
 *                      and all the other fields are expected to be within their
 *                      bounds. Use RTTimeNormalize() to calculate u16YearDay and
 *                      normalize the ranges of the fields.
 */
RTDECL(PRTTIMESPEC) RTTimeImplode(PRTTIMESPEC pTimeSpec, PCRTTIME pTime)
{
    /*
     * Validate input.
     */
    AssertReturn(VALID_PTR(pTimeSpec), NULL);
    AssertReturn(VALID_PTR(pTime), NULL);
    AssertReturn(pTime->u32Nanosecond < 1000000000, NULL);
    AssertReturn(pTime->u8Second < 60, NULL);
    AssertReturn(pTime->u8Minute < 60, NULL);
    AssertReturn(pTime->u8Hour < 24, NULL);
    AssertReturn(pTime->u16YearDay >= 1, NULL);
    AssertReturn(pTime->u16YearDay <= (rtTimeIsLeapYear(pTime->i32Year) ? 366 : 365), NULL);
    AssertMsgReturn(pTime->i32Year <= RTTIME_MAX_YEAR && pTime->i32Year >= RTTIME_MIN_YEAR, ("%RI32\n", pTime->i32Year), NULL);

    /*
     * Do the conversion to nanoseconds.
     */
    int32_t i32Days  = g_aoffYear[pTime->i32Year - OFF_YEAR_IDX_0_YEAR]
                     + pTime->u16YearDay - 1;
    AssertMsgReturn(i32Days <= RTTIME_MAX_DAY && i32Days >= RTTIME_MIN_DAY, ("%RI32\n", i32Days), NULL);

    uint32_t u32Secs = pTime->u8Second
                     + pTime->u8Minute * 60
                     + pTime->u8Hour   * 3600;
    int64_t i64Nanos = (uint64_t)pTime->u32Nanosecond
                     + u32Secs * UINT64_C(1000000000);
    AssertMsgReturn(i32Days != RTTIME_MAX_DAY || i64Nanos <= RTTIME_MAX_DAY_NANO, ("%RI64\n", i64Nanos), NULL);
    AssertMsgReturn(i32Days != RTTIME_MIN_DAY || i64Nanos >= RTTIME_MIN_DAY_NANO, ("%RI64\n", i64Nanos), NULL);

    i64Nanos += i32Days * UINT64_C(86400000000000);

    pTimeSpec->i64NanosecondsRelativeToUnixEpoch = i64Nanos;
    return pTimeSpec;
}


/**
 * Normalizes the fields of a timestructure.
 *
 * It is possible to calculate month/day fields in some
 * combinations. It's also possible to overflow other
 * fields, and these overflows will be adjusted for.
 *
 * @returns pTime on success.
 * @returns NULL if the data is invalid.
 * @param   pTime       The time structure to normalize.
 */
RTDECL(PRTTIME) RTTimeNormalize(PRTTIME pTime)
{
    /** @todo  */
    return NULL;
}


/**
 * Converts a time spec to a ISO date string.
 *
 * @returns psz on success.
 * @returns NULL on buffer underflow.
 * @param   pTime       The time. Caller should've normalized this.
 * @param   psz         Where to store the string.
 * @param   cb          The size of the buffer.
 */
RTDECL(char *) RTTimeToString(PCRTTIME pTime, char *psz, size_t cb)
{
    size_t cch = RTStrPrintf(psz, cb, "%RI32-%02u-%02uT%02u:%02u:%02u.%09RU32Z",
                             pTime->i32Year, pTime->u8Month, pTime->u8MonthDay,
                             pTime->u8Hour, pTime->u8Minute, pTime->u8Second, pTime->u32Nanosecond);
    if (    cch <= 1 
        ||  psz[cch - 1] != 'Z')
        return NULL;
    return psz;
}


/**
 * Converts a time spec to a ISO date string.
 *
 * @returns psz on success.
 * @returns NULL on buffer underflow.
 * @param   pTime       The time spec.
 * @param   psz         Where to store the string.
 * @param   cb          The size of the buffer.
 */
RTDECL(char *) RTTimeSpecToString(PCRTTIMESPEC pTime, char *psz, size_t cb)
{
    RTTIME Time;
    return RTTimeToString(RTTimeExplode(&Time, pTime), psz, cb);
}

