#ifndef APPS_APRS_PROFILE_H
#define APPS_APRS_PROFILE_H

/* Mutable APRS TX asset block.  APRS Edit owns the UI; APRS TX only consumes
 * this fixed layout when it builds a frame. */
#define APRS_PROFILE_MAGIC        0x37435041u /* "APC7" */
#define APRS_PROFILE_LEN          122u
#define APRS_COMMENT_MAX          89u
#define APRS_POS_DIGITS           13u
#define APRS_CALL_MAX             6u

#define APRS_PROFILE_COMMENT_LEN  4u
#define APRS_PROFILE_ROUTE        5u  /* path in bits 5:4, SSID in bits 3:0 */
#define APRS_PROFILE_SYMBOL       6u
#define APRS_PROFILE_HEMI         7u
#define APRS_PROFILE_POS          8u
#define APRS_PROFILE_COMMENT      21u
#define APRS_PROFILE_CALL_LEN     110u
#define APRS_PROFILE_CALL         111u
#define APRS_PROFILE_FREQ_KHZ     117u /* uint32_t, little endian */
#define APRS_PROFILE_POWER        121u /* APP_APRS_POWER_* */

#define APRS_FREQ_KHZ_MIN         18000u
#define APRS_FREQ_KHZ_MAX         999999u

enum {
    APP_APRS_POWER_LOW1 = 0,
    APP_APRS_POWER_LOW2,
    APP_APRS_POWER_LOW3,
    APP_APRS_POWER_LOW4,
    APP_APRS_POWER_LOW5,
    APP_APRS_POWER_MID,
    APP_APRS_POWER_HIGH,
    APP_APRS_POWER_COUNT
};

_Static_assert(APRS_PROFILE_POS + APRS_POS_DIGITS == APRS_PROFILE_COMMENT &&
               APRS_PROFILE_COMMENT + APRS_COMMENT_MAX == APRS_PROFILE_CALL_LEN &&
               APRS_PROFILE_CALL + APRS_CALL_MAX == APRS_PROFILE_FREQ_KHZ &&
               APRS_PROFILE_FREQ_KHZ + 4u == APRS_PROFILE_POWER &&
               APRS_PROFILE_POWER + 1u == APRS_PROFILE_LEN,
               "APRS profile layout must match its declared length");

#endif
