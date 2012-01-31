
#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__

#ifndef VORBIS_HACK
#include <stdint.h>
#else
#include <math.h>
#endif

/* these are filled in by configure */
typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;

#endif
