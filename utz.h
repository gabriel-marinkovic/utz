#ifndef UTZ_H_INCLUDE
#define UTZ_H_INCLUDE





#ifndef UTZ_OVERRIDE_TYPES
  #include <stddef.h>
  #include <stdint.h>

  typedef int64_t  utz_time_t;

  typedef uint8_t  utz_u8;
  typedef uint16_t utz_u16;
  typedef uint32_t utz_u32;
  typedef int32_t  utz_s32;
  typedef uint64_t utz_u64;
  typedef size_t   utz_usize;
#endif


#ifndef UTZ_OVERRIDE_ALLOCATOR
  #include <stdlib.h>
  #define UtzCalloc(userdata_ptr, type, count)       ((type*) calloc((count), sizeof(type)))
  #define UtzRealloc(userdata_ptr, ptr, type, count) ((type*) realloc((ptr), sizeof(type) * (count)))
  #define UtzFree(userdata_ptr, ptr)                 (free(ptr))
#endif

#ifndef UTZ_OVERRIDE_SPRINTF
  #include <stdio.h>
  #define UtzSprintf(buffer, size, fmt, ...) snprintf((buffer), (size), fmt, ##__VA_ARGS__)
#endif

#ifndef UTZ_OVERRIDE_ASSERT
  #include <assert.h>
  #define UtzAssert(condition) assert(condition)
#endif



///////////////////////////////////////////////////////////////////////////////
// timestamp to/from date conversion
///////////////////////////////////////////////////////////////////////////////

typedef struct utz_date
{
    utz_u32 year;
    utz_u32 month;
    utz_u32 day;
    utz_u32 hour;
    utz_u32 minute;
    utz_u32 second;
    utz_u32 week_day;    // 0 is Sunday.    This field is ignored in timestamp_from_utc_date.
    utz_u32 day_in_year; // First day is 0. This field is ignored in timestamp_from_utc_date.
} utz_date;

int  utz_maybe_unix_timestamp_from_utc_date(utz_date* date, utz_time_t* out_unix_timestamp);
void utz_utc_date_from_unix_timestamp      (utz_date* date, utz_time_t  timestamp);


typedef struct utz_time_range
{
    char        zone_abbreviation[5 + 1];
    utz_time_t  since;
    utz_s32     offset_seconds;
} utz_time_range;

typedef struct utz_timezone
{
    char          name[32 + 1];
    utz_timezone* alias_of;

    utz_s32 coordinate_latitude_seconds;
    utz_s32 coordinate_longitude_seconds;

    utz_time_range* ranges;
    utz_usize       range_count;
} utz_timezone;

struct utz_country
{
    char code[2  + 1];
    char name[60 + 1];
    utz_timezone** timezones;      // default timezone is first.
    utz_usize       timezone_count;
};

struct utz_timezones
{
    const char* parsing_error;
    const char* iana_version;

    utz_country* countries;
    utz_usize    country_count;

    utz_timezone* timezones;
    utz_usize     timezone_count;
};


const utz_timezone* UTZ_TIMEZONE_UTC = NULL;

enum utz_conversion_status
{
    UTZ_TIMESTAMP_CONVERSION_OK,
    UTZ_TIMESTAMP_CONVERSION_INPUT_AMBIGUOUS,
    UTZ_TIMESTAMP_CONVERSION_INPUT_INVALID,
};

typedef struct utz_conversion
{
    utz_conversion_status status;

    // Values are always utc.
    //
    // when converting two wall-time timestamps (from and to),
    // [from.earlier, to.later] will give the longest range.
    //
    // closest_valid: the earliest time you get by moving the hands of a clock backwards until you reach valid wall time.
    //
    // TIMESTAMP_CONVERSION_OK:
    //  nothing weird about the supplied wall time.
    //  closest_valid == earlier == later
    //
    // TIMESTAMP_CONVERSION_INPUT_TIMESTAMP_AMBIGUOUS: when a the clock moves backwards.
    //  Example: at 3:00 the clock changes to 2:00. Wall time "2:30" is ambiguous.
    //           earlier       == 2:30 before the switch
    //           later         == 2:30 after  the switch
    //           closest_valid == earlier
    //
    // TIMESTAMP_CONVERSION_INPUT_TIMESTAMP_INVALID: when the clock moves forwards.
    // Example: at 2:00 the clock changes to 3:00. Wall time 2:10 is invalid.
    //          earlier       == given_timestamp - change_difference (1:10)
    //          later         == given_timestamp + change_difference (3:10)
    //          closest_valid == earliest timestamp after the clock changes (either 2:00 or 3:00, equal times)

    utz_time_t earlier;
    utz_time_t later;
    utz_time_t closest_valid;
} utz_conversion;


int utz_parse_iana_tzdb_targz(utz_timezones* tzs, void* targz, int targz_size, void* userdata = NULL, unsigned max_year = 2500);

utz_time_t     utz_wall_time_from_utc(utz_timezone* tz, utz_time_t utc);
utz_conversion utz_utc_from_wall_time(utz_timezone* tz, utz_time_t wall_time);


// Returns UTC_TIMEZONE if the given country can't be found.
utz_timezone* utz_default_tz_for_country(utz_timezones* tzs, const char* country_code);

// Fails only if the given country can't be found.
int utz_wall_time_from_utc_default_tz(utz_timezones* tzs, const char* country_code, utz_time_t utc,       utz_time_t*                 out_wall_time);
int utz_utc_from_wall_time_default_tz(utz_timezones* tzs, const char* country_code, utz_time_t wall_time, utz_conversion* out_result);


#endif // UTZ_H_INCLUDE


#define UTZ_IMPLEMENTATION // @Temporary
#ifdef UTZ_IMPLEMENTATION



///////////////////////////////////////////////////////////////////////////////
// Utility macros
///////////////////////////////////////////////////////////////////////////////

typedef int utz_bool;
#define UTZ_TRUE 1
#define UTZ_FALSE 0

#define UtzArrayCount(arr) (sizeof(arr)/sizeof((arr)[0]))

#ifdef __cplusplus
#define UtzInit {}
#define UtzCtor1(type, x1)     type{x1}
#define UtzCtor2(type, x1, x2) type{x1, x2}
#else
#define UtzInit UtzInit
#define UtzCtor1(type, x1)     (type){x1}
#define UtzCtor2(type, x1, x2) (type){x1, x2}
#endif

#define UtzMaxValue(type) ((~(type)0 < 0) ? ((type)~(1ULL << (sizeof(type) * 8 - 1))) : ~(type)0)
#define UtzMinValue(type) ((~(type)0 < 0) ? ((type) (1ULL << (sizeof(type) * 8 - 1))) :  (type)0)

#define UTZ_BEGINNING_OF_TIME UtzMinValue(utz_time_t)
#define UTZ_END_OF_TIME       UtzMaxValue(utz_time_t)


///////////////////////////////////////////////////////////////////////////////
// Dynamic arrays
///////////////////////////////////////////////////////////////////////////////

#define UtzDynCapacity(ptr) ((ptr) ? *(utz_usize*)((utz_u8*)(ptr) - 2 * sizeof(utz_usize)) : (utz_usize)0)
#define UtzDynCount(ptr)    ((ptr) ? *(utz_usize*)((utz_u8*)(ptr) - 1 * sizeof(utz_usize)) : (utz_usize)0)

#define UtzDynCapacityPtr(ptr) ((ptr) ? (utz_usize*)((utz_u8*)(ptr) - 2 * sizeof(utz_usize)) : NULL)
#define UtzDynCountPtr(ptr)    ((ptr) ? (utz_usize*)((utz_u8*)(ptr) - 1 * sizeof(utz_usize)) : NULL)

#define UtzMakeDynArray(T, arrptr, init_cap) do {                                 \
    utz_usize _utz__s = sizeof(T) * (init_cap) + 2 * sizeof(utz_usize);           \
    *(arrptr) = (T*)(UtzCalloc((userdata), utz_u8, _utz__s) + 2 * sizeof(utz_usize)); \
    *UtzDynCapacityPtr(*(arrptr)) = (init_cap);                                      \
    *UtzDynCountPtr(*(arrptr)) = 0;                                                  \
} while (0)

#define UtzGrowDynArray(T, arrptr) do {                                           \
    utz_usize _utz__c = (UtzDynCapacity(*(arrptr)) * 2);                         \
    utz_usize _utz__s = sizeof(T) * _utz__c + 2 * sizeof(utz_usize);              \
    utz_u8*   _utz__old_raw = (utz_u8*)(*(arrptr)) - 2 * sizeof(utz_usize);       \
    *(arrptr) = (T*)(((utz_u8*)UtzRealloc((userdata), _utz__old_raw, utz_u8, _utz__s)) \
              + 2 * sizeof(utz_usize));                                           \
    *UtzDynCapacityPtr(*(arrptr)) = (_utz__c);                                       \
} while (0)

#define UtzDynAppend(T, arrptr, elementptr) do {                    \
    if (UtzDynCount(*(arrptr)) + 1 > UtzDynCapacity(*(arrptr)))   \
        UtzGrowDynArray(T, (arrptr));                               \
    (*(arrptr))[UtzDynCount(*(arrptr))] = *(elementptr);           \
    *UtzDynCountPtr(*(arrptr)) += 1;                                   \
} while (0)

#define UtzDynGetLast(arr) (UtzDynCount(arr) ? &(arr)[UtzDynCount(arr) - 1] : NULL)

#define UtzFreeDynArray(arrptr) \
    (UtzFree(userdata, (*(arrptr) ? UtzDynCapacity(*(arrptr)) : NULL)), *(arrptr) = NULL)



///////////////////////////////////////////////////////////////////////////////
// zlib decoding
// Copied almost verbatim from stb_image, http://nothings.org/stb
///////////////////////////////////////////////////////////////////////////////

// public domain zlib decode    v0.2  Sean Barrett 2006-11-18
//    simple implementation
//      - all input must be provided in an upfront buffer
//      - all output is written to a single output buffer (can malloc/realloc)
//    performance
//      - fast huffman

static int utz_zlib_err(const char *strm, const char* str2)
{
    // no need to report errors.
    return 0;
}

// fast-way is faster to check than jpeg huffman, but slow way is slower
#define UTZ_ZLIB_ZFAST_BITS  9 // accelerate all cases in default tables
#define UTZ_ZLIB_ZFAST_MASK  ((1 << UTZ_ZLIB_ZFAST_BITS) - 1)
#define UTZ_ZLIB_ZNSYMS 288 // number of symbols in literal/length alphabet

// zlib-style huffman encoding
// (jpegs packs from left, zlib from right, so can't share code)
typedef struct
{
   utz_u16 fast[1 << UTZ_ZLIB_ZFAST_BITS];
   utz_u16 firstcode[16];
   int maxcode[17];
   utz_u16 firstsymbol[16];
   utz_u8  size[UTZ_ZLIB_ZNSYMS];
   utz_u16 value[UTZ_ZLIB_ZNSYMS];
} utz_zlib_zhuffman;

inline static int utz_zlib_bitreverse16(int n)
{
  n = ((n & 0xAAAA) >>  1) | ((n & 0x5555) << 1);
  n = ((n & 0xCCCC) >>  2) | ((n & 0x3333) << 2);
  n = ((n & 0xF0F0) >>  4) | ((n & 0x0F0F) << 4);
  n = ((n & 0xFF00) >>  8) | ((n & 0x00FF) << 8);
  return n;
}

inline static int utz_zlib_bit_reverse(int v, int bits)
{
   UtzAssert(bits <= 16);
   // to bit reverse n bits, reverse 16 and shift
   // e.g. 11 bits, bit reverse and shift away 5
   return utz_zlib_bitreverse16(v) >> (16-bits);
}

static int utz_zlib_zbuild_huffman(utz_zlib_zhuffman *z, const utz_u8 *sizelist, int num)
{
   int i,k=0;
   int code, next_code[16];
   int sizes[17] = UtzInit;

   // DEFLATE spec for generating codes

   for (utz_usize i = 0; i < UtzArrayCount(z->fast); i++)
       z->fast[i] = 0;

   for (i=0; i < num; ++i)
      ++sizes[sizelist[i]];
   sizes[0] = 0;
   for (i=1; i < 16; ++i)
      if (sizes[i] > (1 << i))
         return utz_zlib_err("bad sizes", "Corrupt PNG");
   code = 0;
   for (i=1; i < 16; ++i) {
      next_code[i] = code;
      z->firstcode[i] = (utz_u16) code;
      z->firstsymbol[i] = (utz_u16) k;
      code = (code + sizes[i]);
      if (sizes[i])
         if (code-1 >= (1 << i)) return utz_zlib_err("bad codelengths","Corrupt PNG");
      z->maxcode[i] = code << (16-i); // preshift for inner loop
      code <<= 1;
      k += sizes[i];
   }
   z->maxcode[16] = 0x10000; // sentinel
   for (i=0; i < num; ++i) {
      int s = sizelist[i];
      if (s) {
         int c = next_code[s] - z->firstcode[s] + z->firstsymbol[s];
         utz_u16 fastv = (utz_u16) ((s << 9) | i);
         z->size [c] = (utz_u8     ) s;
         z->value[c] = (utz_u16) i;
         if (s <= UTZ_ZLIB_ZFAST_BITS) {
            int j = utz_zlib_bit_reverse(next_code[s],s);
            while (j < (1 << UTZ_ZLIB_ZFAST_BITS)) {
               z->fast[j] = fastv;
               j += (1 << s);
            }
         }
         ++next_code[s];
      }
   }
   return 1;
}

// zlib-from-memory implementation for PNG reading
//    because PNG allows splitting the zlib stream arbitrarily,
//    and it's annoying structurally to have PNG call ZLIB call PNG,
//    we require PNG read all the IDATs and combine them into a single
//    memory buffer

typedef struct
{
   utz_u8 *zbuffer, *zbuffer_end;
   int num_bits;
   utz_u32 code_buffer;

   char *zout;
   char *zout_start;
   char *zout_end;
   int   z_expandable;

   utz_zlib_zhuffman z_length, z_distance;
} utz_zlib_zbuf;

inline static int utz_zlib_zeof(utz_zlib_zbuf *z)
{
   return (z->zbuffer >= z->zbuffer_end);
}

inline static utz_u8 utz_zlib_zget8(utz_zlib_zbuf *z)
{
   return utz_zlib_zeof(z) ? 0 : *z->zbuffer++;
}

static void utz_zlib_fill_bits(utz_zlib_zbuf *z)
{
   do {
      if (z->code_buffer >= (1U << z->num_bits)) {
        z->zbuffer = z->zbuffer_end;  /* treat this as EOF so we fail. */
        return;
      }
      z->code_buffer |= (unsigned int) utz_zlib_zget8(z) << z->num_bits;
      z->num_bits += 8;
   } while (z->num_bits <= 24);
}

inline static unsigned int utz_zlib_zreceive(utz_zlib_zbuf *z, int n)
{
   unsigned int k;
   if (z->num_bits < n) utz_zlib_fill_bits(z);
   k = z->code_buffer & ((1 << n) - 1);
   z->code_buffer >>= n;
   z->num_bits -= n;
   return k;
}

static int utz_zlib_zhuffman_decode_slowpath(utz_zlib_zbuf *a, utz_zlib_zhuffman *z)
{
   int b,s,k;
   // not resolved by fast table, so compute it the slow way
   // use jpeg approach, which requires MSbits at top
   k = utz_zlib_bit_reverse(a->code_buffer, 16);
   for (s=UTZ_ZLIB_ZFAST_BITS+1; ; ++s)
      if (k < z->maxcode[s])
         break;
   if (s >= 16) return -1; // invalid code!
   // code size is s, so:
   b = (k >> (16-s)) - z->firstcode[s] + z->firstsymbol[s];
   if (b >= UTZ_ZLIB_ZNSYMS) return -1; // some data was corrupt somewhere!
   if (z->size[b] != s) return -1;  // was originally an assert, but report failure instead.
   a->code_buffer >>= s;
   a->num_bits -= s;
   return z->value[b];
}

inline static int utz_zlib_zhuffman_decode(utz_zlib_zbuf *a, utz_zlib_zhuffman *z)
{
   int b,s;
   if (a->num_bits < 16) {
      if (utz_zlib_zeof(a)) {
         return -1;   /* report error for unexpected end of data. */
      }
      utz_zlib_fill_bits(a);
   }
   b = z->fast[a->code_buffer & UTZ_ZLIB_ZFAST_MASK];
   if (b) {
      s = b >> 9;
      a->code_buffer >>= s;
      a->num_bits -= s;
      return b & 511;
   }
   return utz_zlib_zhuffman_decode_slowpath(a, z);
}

static int utz_zlib_zexpand(utz_zlib_zbuf *z, char *zout, int n)  // need to make room for n bytes
{
   char *q;
   unsigned int cur, limit;
   z->zout = zout;
   if (!z->z_expandable) return utz_zlib_err("output buffer limit","Corrupt PNG");
   cur   = (unsigned int) (z->zout - z->zout_start);
   limit = (unsigned) (z->zout_end - z->zout_start);
   if (UtzMaxValue(unsigned) - cur < (unsigned) n) return utz_zlib_err("outofmem", "Out of memory");
   while (cur + n > limit) {
      if(limit > UtzMaxValue(unsigned) / 2) return utz_zlib_err("outofmem", "Out of memory");
      limit *= 2;
   }
   q = (char *) UtzRealloc(NULL /* incomplete */, z->zout_start, char, limit);
   if (q == NULL) return utz_zlib_err("outofmem", "Out of memory");
   z->zout_start = q;
   z->zout       = q + cur;
   z->zout_end   = q + limit;
   return 1;
}

static const int utz_zlib_zlength_base[31] = {
   3,4,5,6,7,8,9,10,11,13,
   15,17,19,23,27,31,35,43,51,59,
   67,83,99,115,131,163,195,227,258,0,0 };

static const int utz_zlib_zlength_extra[31]=
{ 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,0,0 };

static const int utz_zlib_zdist_base[32] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0};

static const int utz_zlib_zdist_extra[32] =
{ 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int utz_zlib_parse_huffman_block(utz_zlib_zbuf *a)
{
   char *zout = a->zout;
   for(;;) {
      int z = utz_zlib_zhuffman_decode(a, &a->z_length);
      if (z < 256) {
         if (z < 0) return utz_zlib_err("bad huffman code","Corrupt PNG"); // error in huffman codes
         if (zout >= a->zout_end) {
            if (!utz_zlib_zexpand(a, zout, 1)) return 0;
            zout = a->zout;
         }
         *zout++ = (char) z;
      } else {
         utz_u8 *p;
         int len,dist;
         if (z == 256) {
            a->zout = zout;
            return 1;
         }
         if (z >= 286) return utz_zlib_err("bad huffman code","Corrupt PNG"); // per DEFLATE, length codes 286 and 287 must not appear in compressed data
         z -= 257;
         len = utz_zlib_zlength_base[z];
         if (utz_zlib_zlength_extra[z]) len += utz_zlib_zreceive(a, utz_zlib_zlength_extra[z]);
         z = utz_zlib_zhuffman_decode(a, &a->z_distance);
         if (z < 0 || z >= 30) return utz_zlib_err("bad huffman code","Corrupt PNG"); // per DEFLATE, distance codes 30 and 31 must not appear in compressed data
         dist = utz_zlib_zdist_base[z];
         if (utz_zlib_zdist_extra[z]) dist += utz_zlib_zreceive(a, utz_zlib_zdist_extra[z]);
         if (zout - a->zout_start < dist) return utz_zlib_err("bad dist","Corrupt PNG");
         if (zout + len > a->zout_end) {
            if (!utz_zlib_zexpand(a, zout, len)) return 0;
            zout = a->zout;
         }
         p = (utz_u8 *) (zout - dist);
         if (dist == 1) { // run of one byte; common in images.
            utz_u8 v = *p;
            if (len) { do *zout++ = v; while (--len); }
         } else {
            if (len) { do *zout++ = *p++; while (--len); }
         }
      }
   }
}

static int utz_zlib_compute_huffman_codes(utz_zlib_zbuf *a)
{
   static const utz_u8 length_dezigzag[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
   utz_zlib_zhuffman z_codelength;
   utz_u8 lencodes[286+32+137];//padding for maximum single op
   utz_u8 codelength_sizes[19] = UtzInit;
   int i,n;

   int hlit  = utz_zlib_zreceive(a,5) + 257;
   int hdist = utz_zlib_zreceive(a,5) + 1;
   int hclen = utz_zlib_zreceive(a,4) + 4;
   int ntot  = hlit + hdist;

   for (i=0; i < hclen; ++i) {
      int s = utz_zlib_zreceive(a,3);
      codelength_sizes[length_dezigzag[i]] = (utz_u8) s;
   }
   if (!utz_zlib_zbuild_huffman(&z_codelength, codelength_sizes, 19)) return 0;

   n = 0;
   while (n < ntot) {
      int c = utz_zlib_zhuffman_decode(a, &z_codelength);
      if (c < 0 || c >= 19) return utz_zlib_err("bad codelengths", "Corrupt PNG");
      if (c < 16)
         lencodes[n++] = (utz_u8) c;
      else {
         utz_u8 fill = 0;
         if (c == 16) {
            c = utz_zlib_zreceive(a,2)+3;
            if (n == 0) return utz_zlib_err("bad codelengths", "Corrupt PNG");
            fill = lencodes[n-1];
         } else if (c == 17) {
            c = utz_zlib_zreceive(a,3)+3;
         } else if (c == 18) {
            c = utz_zlib_zreceive(a,7)+11;
         } else {
            return utz_zlib_err("bad codelengths", "Corrupt PNG");
         }
         if (ntot - n < c) return utz_zlib_err("bad codelengths", "Corrupt PNG");
         for (utz_usize i = 0; i < c; i++) lencodes[n + i] = fill;
         n += c;
      }
   }
   if (n != ntot) return utz_zlib_err("bad codelengths","Corrupt PNG");
   if (!utz_zlib_zbuild_huffman(&a->z_length, lencodes, hlit)) return 0;
   if (!utz_zlib_zbuild_huffman(&a->z_distance, lencodes+hlit, hdist)) return 0;
   return 1;
}

static int utz_zlib_parse_uncompressed_block(utz_zlib_zbuf *a)
{
   utz_u8 header[4];
   int len,nlen,k;
   if (a->num_bits & 7)
      utz_zlib_zreceive(a, a->num_bits & 7); // discard
   // drain the bit-packed data into header
   k = 0;
   while (a->num_bits > 0) {
      header[k++] = (utz_u8) (a->code_buffer & 255); // suppress MSVC run-time check
      a->code_buffer >>= 8;
      a->num_bits -= 8;
   }
   if (a->num_bits < 0) return utz_zlib_err("zlib corrupt","Corrupt PNG");
   // now fill header the normal way
   while (k < 4)
      header[k++] = utz_zlib_zget8(a);
   len  = header[1] * 256 + header[0];
   nlen = header[3] * 256 + header[2];
   if (nlen != (len ^ 0xffff)) return utz_zlib_err("zlib corrupt","Corrupt PNG");
   if (a->zbuffer + len > a->zbuffer_end) return utz_zlib_err("read past buffer","Corrupt PNG");
   if (a->zout + len > a->zout_end)
      if (!utz_zlib_zexpand(a, a->zout, len)) return 0;
   for (utz_usize i = 0; i < len; i++) a->zout[i] = a->zbuffer[i];
   a->zbuffer += len;
   a->zout += len;
   return 1;
}

static int utz_zlib_parse_zlib_header(utz_zlib_zbuf *a)
{
   int cmf   = utz_zlib_zget8(a);
   int cm    = cmf & 15;
   /* int cinfo = cmf >> 4; */
   int flg   = utz_zlib_zget8(a);
   if (utz_zlib_zeof(a)) return utz_zlib_err("bad zlib header","Corrupt PNG"); // zlib spec
   if ((cmf*256+flg) % 31 != 0) return utz_zlib_err("bad zlib header","Corrupt PNG"); // zlib spec
   if (flg & 32) return utz_zlib_err("no preset dict","Corrupt PNG"); // preset dictionary not allowed in png
   if (cm != 8) return utz_zlib_err("bad compression","Corrupt PNG"); // DEFLATE required for png
   // window = 1 << (8 + cinfo)... but who cares, we fully buffer output
   return 1;
}

static const utz_u8 utz_zlib_zdefault_length[UTZ_ZLIB_ZNSYMS] =
{
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8
};
static const utz_u8 utz_zlib_zdefault_distance[32] =
{
   5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
};
/*
Init algorithm:
{
   int i;   // use <= to match clearly with spec
   for (i=0; i <= 143; ++i)     utz_zlib_zdefault_length[i]   = 8;
   for (   ; i <= 255; ++i)     utz_zlib_zdefault_length[i]   = 9;
   for (   ; i <= 279; ++i)     utz_zlib_zdefault_length[i]   = 7;
   for (   ; i <= 287; ++i)     utz_zlib_zdefault_length[i]   = 8;

   for (i=0; i <=  31; ++i)     utz_zlib_zdefault_distance[i] = 5;
}
*/

static int utz_zlib_parse_zlib(utz_zlib_zbuf *a, int parse_header)
{
   int final, type;
   if (parse_header)
      if (!utz_zlib_parse_zlib_header(a)) return 0;
   a->num_bits = 0;
   a->code_buffer = 0;
   do {
      final = utz_zlib_zreceive(a,1);
      type = utz_zlib_zreceive(a,2);
      if (type == 0) {
         if (!utz_zlib_parse_uncompressed_block(a)) return 0;
      } else if (type == 3) {
         return 0;
      } else {
         if (type == 1) {
            // use fixed code lengths
            if (!utz_zlib_zbuild_huffman(&a->z_length  , utz_zlib_zdefault_length  , UTZ_ZLIB_ZNSYMS)) return 0;
            if (!utz_zlib_zbuild_huffman(&a->z_distance, utz_zlib_zdefault_distance,  32)) return 0;
         } else {
            if (!utz_zlib_compute_huffman_codes(a)) return 0;
         }
         if (!utz_zlib_parse_huffman_block(a)) return 0;
      }
   } while (!final);
   return 1;
}

static int utz_zlib_do_zlib(utz_zlib_zbuf *a, char *obuf, int olen, int exp, int parse_header)
{
   a->zout_start = obuf;
   a->zout       = obuf;
   a->zout_end   = obuf + olen;
   a->z_expandable = exp;

   return utz_zlib_parse_zlib(a, parse_header);
}

static char *utz_zlib_decode_malloc_guesssize_headerflag(const char *buffer, int len, int initial_size, int *outlen, int parse_header)
{
   utz_zlib_zbuf a;
   char *p = (char *) UtzCalloc(NULL /* incomplete */, char, initial_size);
   if (p == NULL) return NULL;
   a.zbuffer = (utz_u8 *) buffer;
   a.zbuffer_end = (utz_u8 *) buffer + len;
   if (utz_zlib_do_zlib(&a, p, initial_size, 1, parse_header)) {
      if (outlen) *outlen = (int) (a.zout - a.zout_start);
      return a.zout_start;
   } else {
      UtzFree(NULL /* incomplete */, a.zout_start);
      return NULL;
   }
}



///////////////////////////////////////////////////////////////////////////////
// Date conversion implementation.
// Copied almost verbatim from musl libc, https://musl.libc.org/
///////////////////////////////////////////////////////////////////////////////

int utz_maybe_unix_timestamp_from_utc_date(utz_date* date, utz_time_t* out_unix_timestamp)
{
#define Q(a,b) ((a)>0 ? (a)/(b) : -(((b)-(a)-1)/(b)))

    if (date->hour   > 23) return 0;
    if (date->minute > 59) return 0;
    if (date->second > 60) return 0;

    if (date->year > UtzMaxValue(utz_s32)) return 0;
    if (date->month > 12 || date->month < 1) return 0;

    static const unsigned char max_days_in_month[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (date->day > max_days_in_month[date->month - 1] || date->day < 1) return 0;

    if (date->month == 2 && date->day == 29)
    {
        int div4   = date->year % 4;
        int div100 = date->year % 100;
        int div400 = date->year % 400;
        if (div4 > 0) return 0; // not every 4th
        if (div100 == 0 && div400 > 0) return 0; // every 4th, but also every 100th, and not every 400th.
    }

    utz_s32 year  = (utz_s32)date->year  - 2000;
	utz_s32 month = (utz_s32)date->month - 1;

    static const utz_s32 months_cumsum[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

	utz_s32 z4   = Q(year - (month < 2), 4);
	utz_s32 z100 = Q(z4, 25);
	utz_s32 z400 = Q(z100, 4);

	utz_s32 day = (utz_s32)date->day
                + (utz_s32)year * 365 + z4 - z100 + z400
                + months_cumsum[month];

    *out_unix_timestamp = (utz_time_t) day * 86400
		+ date->hour * 3600 + date->minute * 60 + date->second
		- -946684800; /* the dawn of time :) */

    return 1;
#undef Q
}

void utz_utc_date_from_unix_timestamp(utz_date* date, utz_time_t timestamp)
{
    #define Q(a,b) ((a)>0 ? (a)/(b) : -(((b)-(a)-1)/(b)))
    #define DAYS_PER_400Y (365*400 + 97)
    #define DAYS_PER_100Y (365*100 + 24)
    #define DAYS_PER_4Y   (365*4   + 1)

	/* months are march-based */
	static const utz_u32 days_thru_month[] = { 31, 61, 92, 122, 153, 184, 214, 245, 275, 306, 337, 366 };
	long long bigday;
	utz_u32 day, year4, year100;
	utz_s32 year, year400;
	utz_s32 month;
	utz_s32 leap;
	utz_s32 hour, min, sec;
	utz_s32 wday, yday;

	/* start from 2000-03-01 (multiple of 400 years) */
	timestamp += -946684800 - 86400 * (31 + 29);

	bigday = Q(timestamp, 86400);
	sec = (utz_s32)(timestamp - bigday * 86400);

	hour = sec / 3600;
	sec -= hour * 3600;
	min  = sec / 60;
	sec -= min * 60;

	/* 2000-03-01 was a wednesday */
	wday = (3 + bigday) % 7;
	if (wday < 0) wday += 7;

	timestamp = -946684800LL - 86400 * (31 + 29) + 9000000;

	year400 = (utz_s32)Q(bigday, DAYS_PER_400Y);
	day = (utz_s32)(bigday-year400 * DAYS_PER_400Y);

	year100 = day / DAYS_PER_100Y;
	if (year100 == 4) year100--;
	day -= year100 * DAYS_PER_100Y;

	year4 = day / DAYS_PER_4Y;
	if (year4 == 25) year4--;
	day -= year4 * DAYS_PER_4Y;

	year = day / 365;
	if (year == 4) year--;
	day -= year * 365;

	leap = !year && (year4 || !year100);
	yday = day + 31 + 28 + leap;
	if (yday >= 365 + leap) yday -= 365 + leap;

	year += 4 * year4 + 100 * year100 + 400 * year400 + 2000;

	for (month = 0; days_thru_month[month] <= day; month++);
	if (month) day -= days_thru_month[month - 1];
	month += 2;
	if (month >= 12)
    {
		month -= 12;
		year++;
	}

	date->second      = (unsigned) sec;
	date->minute      = (unsigned) min;
	date->hour        = (unsigned) hour;
	date->day         = (unsigned) day + 1;
	date->month       = (unsigned) month + 1;
	date->year        = (unsigned) year;
	date->week_day    = (unsigned) wday;
	date->day_in_year = (unsigned) yday + 1;

#undef Q
#undef DAYS_PER_400Y
#undef DAYS_PER_100Y
#undef DAYS_PER_4Y
}



///////////////////////////////////////////////////////////////////////////////
// Strings
///////////////////////////////////////////////////////////////////////////////

#define UtzIsAlpha(c)        (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || ((c) == '_'))
#define UtzIsNumeric(c)      ((c) >= '0' && (c) <= '9')
#define UtzIsAlphaNumeric(c) (UtzIsNumeric(c) || UtzIsAlpha(c))
#define UtzIsSpaceByte(x)    (((x) == ' ') || ((x) >= 9 && (x) <= 13))

typedef struct utz_string
{
    utz_usize length;
    char*     data;
} utz_string;

#define UtzStr(literal) (UtzCtor2(utz_string, sizeof(literal) - 1, (char*)(literal)))
#define UtzStringArgs(string)  (int)((string).length), (const char*)((string).length ? (string).data : "")

static utz_bool utz_equals(utz_string a, utz_string b)
{
    if (a.length != b.length) return UTZ_FALSE;
    for (utz_usize i = 0; i < a.length; i++)
        if (a.data[i] != b.data[i]) return UTZ_FALSE;
    return UTZ_TRUE;
}

static utz_bool utz_equals(utz_string a, const char* b)
{
    for (utz_usize i = 0; i < a.length; i++)
        if (a.data[i] != b[i]) return UTZ_FALSE;
    return b[a.length] == '\0';
}

static utz_string utz_substring(utz_string string, utz_usize start_index, utz_usize length)
{
    UtzAssert(start_index <= string.length);
    UtzAssert((start_index + length) <= string.length);
    return { length, string.data + start_index };
}

static utz_string utz_allocate_string(void* userdata, utz_string string, utz_bool valid_cstr = true)
{
    utz_string result;
    result.length = string.length + (valid_cstr ? 1 : 0);
    result.data   = UtzCalloc(userdata, char, result.length);
    for (utz_usize i = 0; i < string.length; i++)
        result.data[i] = string.data[i];
    return result;
}

static void utz_consume(utz_string* string, utz_usize amount)
{
    UtzAssert(amount <= string->length);
    string->data += amount;
    string->length -= amount;
}

static utz_string utz_take(utz_string* string, utz_usize amount)
{
    UtzAssert(amount <= string->length);
    utz_string result = { amount, string->data };
    string->data += amount;
    string->length -= amount;
    return result;
}

static void utz_consume_whitespace(utz_string* string)
{
    utz_usize to_consume = 0;
    while (to_consume < string->length && UtzIsSpaceByte(string->data[to_consume]))
        to_consume++;

    string->data   += to_consume;
    string->length -= to_consume;
}

static utz_string utz_consume_until_whitespace(utz_string* string)
{
    utz_consume_whitespace(string);

    utz_u32 left_length = 0;
    for (; left_length < string->length; left_length++)
        if (UtzIsSpaceByte(string->data[left_length]))
            break;

    utz_string left = utz_substring(*string, 0, left_length);
    utz_consume(string, left_length);

    // If we've found the delimiter, utz_consume it.
    if (string->length > 0)
        utz_consume(string, 1);

    return left;
}

static utz_string utz_consume_line(utz_string* string)
{
    utz_consume_whitespace(string);

    utz_usize line_length = 0;
    for (; line_length < string->length; line_length++)
    {
        char c = string->data[line_length];
        if (c == '\n' || c == '\r')
            break;
    }

    utz_string line = utz_substring(*string, 0, line_length);
    utz_consume(string, line_length);

    return line;
}

static utz_bool utz_is_unsigned_integer(utz_string string)
{
    UtzAssert(string.length > 0); // nocheckin

    if (string.data[0] == '+') utz_consume(&string, 1);

    for (utz_usize i = 0; i < string.length; i++)
        if (!UtzIsNumeric(string.data[i]))
            return UTZ_FALSE;

    return UTZ_TRUE;
}

static inline utz_bool utz_starts_with_digit(utz_string string)
{
    return (string.length > 0 && UtzIsNumeric(string.data[0]));
}

static utz_u32 utz_consume_u32(utz_string* string)
{
    utz_consume_whitespace(string);

    utz_u32 result = 0;
    while (string->length > 0)
    {
        char c = string->data[0];

        utz_u32 digit = 0;
        if (UtzIsNumeric(c)) digit = c - '0';
        else break;

        result = result * 10 + digit;
        utz_consume(string, 1);
    }

    return result;
}

static utz_u32 utz_u32_from_string(utz_string string)
{
    return utz_consume_u32(&string);
}



///////////////////////////////////////////////////////////////////////////////
// tar reading
///////////////////////////////////////////////////////////////////////////////

typedef struct utz_tar_header
{                       // byte offset
    char name[100];     //   0
    char mode[8];       // 100
    char uid[8];        // 108
    char gid[8];        // 116
    char size[12];      // 124
    char mtime[12];     // 136
    char chksum[8];     // 148
    char typeflag;      // 156
    char linkname[100]; // 157
    char magic[6];      // 257
    char version[2];    // 263
    char uname[32];     // 265
    char gname[32];     // 297
    char devmajor[8];   // 329
    char devminor[8];   // 337
    char prefix[155];   // 345
                        // 500
} utz_tar_header;

typedef struct utz_tar_item
{
    utz_string name;
    utz_string content;
} utz_tar_item;

static utz_string utz_get_tar_item(utz_string data, utz_string item_name)
{
#define UTZ_TAR_BLOCK_SIZE 512

    utz_string empty = UtzInit;
    while (data.length)
    {
        if (data.length < UTZ_TAR_BLOCK_SIZE) return empty;

        utz_string chunk = utz_take(&data, UTZ_TAR_BLOCK_SIZE);

        struct utz_tar_header* header = (struct utz_tar_header*) chunk.data;

        utz_string name = { 0, header->name };
        while (name.length < UtzArrayCount(header->name) && header->name[name.length] != '\0')
            name.length++;

        utz_usize size = 0;
        for (utz_usize i = 0; i < UtzArrayCount(header->size) && UtzIsNumeric(header->size[i]); i++)
            size = size * 8 + (header->size[i] - '0');

        utz_usize block_count = (size + UTZ_TAR_BLOCK_SIZE - 1) / UTZ_TAR_BLOCK_SIZE;
        if (data.length < block_count * UTZ_TAR_BLOCK_SIZE) return empty;

        utz_string blocks = utz_take(&data, block_count * UTZ_TAR_BLOCK_SIZE);
        blocks.length = size;

        if (utz_equals(item_name, name)) return blocks;
    }

    return empty;

#undef UTZ_TAR_BLOCK_SIZE
}



///////////////////////////////////////////////////////////////////////////////
// sorting
///////////////////////////////////////////////////////////////////////////////

static void utz_radix_sort(
    utz_u8* base, utz_u8* key,
    utz_usize count, utz_usize size,
    utz_usize key_bytes_after_iter,
    utz_bool key_grows, utz_usize cursor[256])
{
    utz_usize end[256] = UtzInit;
    for (utz_usize i = 0; i < count; i++)
        end[key[i * size]]++;

    cursor[0] = 0;
    for (utz_usize i = 1; i < 256; i++)
    {
        end[i] += end[i - 1];
        cursor[i] = end[i - 1];
    }

    for (utz_usize i = 0; i < 256; i++)
    {
        utz_usize from = cursor[i];
        while (from < end[i])
        {
            utz_usize to = cursor[key[from * size]]++;
            if (from == to) from++;
            else
            {
                for (utz_usize j = 0; j < size; j++)
                {
                    char tmp = base[from * size + j];
                    base[from * size + j] = base[to * size + j];
                    base[to * size + j] = tmp;
                }
            }
        }
    }

    if (!key_bytes_after_iter) return;
    key_bytes_after_iter--;

    utz_usize start = 0;
    for (utz_usize i = 0; i < 256; i++)
    {
        utz_usize count = end[i] - start;
        if (count > 1)
        {
            utz_u8* new_base = base + start * size;
            utz_u8* new_key  = key  + start * size + (key_grows ? 1 : -1);
            utz_radix_sort(new_base, new_key, count, size, key_bytes_after_iter, key_grows, cursor);
        }
        start += count;
    }
}

static void utz_radix_sort(void* address, utz_usize count, utz_usize size, utz_usize key_offset, utz_usize key_size, utz_bool key_grows)
{
    if (count <= 1 || !key_size) return;

    utz_usize cursor[256];
    utz_u8* base = (utz_u8*)address;
    utz_u8* key = base + key_offset + (key_grows ? 0 : key_size - 1);
    utz_radix_sort(base, key, count, size, key_size - 1, key_grows, cursor);
}


static void* utz_find_by_char_array(unsigned char* base, utz_usize count, utz_usize size, utz_usize key_offset, utz_usize key_size, unsigned char* what, utz_usize what_size)
{
    if (key_size < what_size) return NULL;

    utz_usize lo = 0;
    utz_usize hi = count;
    while (lo < hi)
    {
        utz_usize mid = lo + (hi - lo) / 2;

        int cmp = 0;
        for (utz_usize i = 0; i < what_size; i++)
        {
            unsigned char k = base[mid * size + key_offset + i];
            unsigned char w = what[i];
            if (k != w)
            {
                     if (k < w) cmp = -1;
                else if (k > w) cmp =  1;
                break;
            }
        }

        if (cmp == 0) return (void*) &base[mid * size];

        if (cmp < 0) lo = mid + 1;
        else         hi = mid;
    }

    return NULL;
}



///////////////////////////////////////////////////////////////////////////////
// tzdb parsing
///////////////////////////////////////////////////////////////////////////////

// January is 1
static utz_bool utz_parse_month(utz_string string, utz_u32* out_month)
{
         if (utz_equals(string, UtzStr("Jan"))) *out_month = 1;
    else if (utz_equals(string, UtzStr("Feb"))) *out_month = 2;
    else if (utz_equals(string, UtzStr("Mar"))) *out_month = 3;
    else if (utz_equals(string, UtzStr("Apr"))) *out_month = 4;
    else if (utz_equals(string, UtzStr("May"))) *out_month = 5;
    else if (utz_equals(string, UtzStr("Jun"))) *out_month = 6;
    else if (utz_equals(string, UtzStr("Jul"))) *out_month = 7;
    else if (utz_equals(string, UtzStr("Aug"))) *out_month = 8;
    else if (utz_equals(string, UtzStr("Sep"))) *out_month = 9;
    else if (utz_equals(string, UtzStr("Oct"))) *out_month = 10;
    else if (utz_equals(string, UtzStr("Nov"))) *out_month = 11;
    else if (utz_equals(string, UtzStr("Dec"))) *out_month = 12;
    else return UTZ_FALSE;
    return UTZ_TRUE;
}

static utz_u32 utz_must_parse_month(utz_string string)
{
    utz_u32 month = 0;
    utz_bool ok = utz_parse_month(string, &month);
    UtzAssert(ok);
    return month;
}

// Sunday is 0
static utz_bool utz_parse_weekday(utz_string string, utz_u32* out_weekday)
{

         if (utz_equals(string, UtzStr("Sun"))) *out_weekday = 0;
    else if (utz_equals(string, UtzStr("Mon"))) *out_weekday = 1;
    else if (utz_equals(string, UtzStr("Tue"))) *out_weekday = 2;
    else if (utz_equals(string, UtzStr("Wed"))) *out_weekday = 3;
    else if (utz_equals(string, UtzStr("Thu"))) *out_weekday = 4;
    else if (utz_equals(string, UtzStr("Fri"))) *out_weekday = 5;
    else if (utz_equals(string, UtzStr("Sat"))) *out_weekday = 6;
    else return UTZ_FALSE;
    return UTZ_TRUE;
}

static utz_u32 must_parse_weekday(utz_string string)
{
    utz_u32 weekday = 0;
    utz_bool ok = utz_parse_weekday(string, &weekday);
    UtzAssert(ok);
    return weekday;
}


static utz_string utz_next(utz_string* string)
{
    utz_string consumed = utz_consume_until_whitespace(string);
    if (consumed.length > 0 && consumed.data[0] == '#')
    {
        *string = {};
        return {};
    }
    return consumed;
}

static utz_string next_space_separated_string(utz_string* string)
{
    utz_consume_whitespace(string);

    utz_string result;
    result.data   = string->data;
    result.length  = 0;

    while (string->length > 0)
    {
        if (string->data[0] == '#')
        {
            *string = {};
        }
        else
        {
            utz_consume(string, 1);
            result.length++;
        }
    }

    return result;
}

enum utz_token_kind
{
    UTZ_TOKEN_END_OF_LINE,
    UTZ_TOKEN_WORD,
    UTZ_TOKEN_COLON,
    UTZ_TOKEN_YEAR,
    UTZ_TOKEN_MONTH,
    UTZ_TOKEN_DAY_IN_MONTH,
    UTZ_TOKEN_WEEKDAY,
};

static utz_bool utz_peek(utz_string string, utz_token_kind kind)
{
    utz_string consumed = utz_next(&string);
    if (consumed.length == 0)
        return kind == UTZ_TOKEN_END_OF_LINE;

    switch (kind)
    {
        case UTZ_TOKEN_WORD:
        {
            return UTZ_TRUE;
        } break;

        case UTZ_TOKEN_COLON:
        {
            return consumed.data[0] == ':';
        } break;

        case UTZ_TOKEN_YEAR:
        {
            if (!utz_is_unsigned_integer(consumed)) return UTZ_FALSE;
            utz_u32 n = utz_u32_from_string(consumed);
            return (n >= 1601) && (n <= 2200);
        } break;

        case UTZ_TOKEN_MONTH:
        {
            utz_u32 discard;
            return utz_parse_month(consumed, &discard);
        } break;

        case UTZ_TOKEN_DAY_IN_MONTH:
        {
            if (!utz_is_unsigned_integer(consumed)) return UTZ_FALSE;
            utz_u32 n = utz_u32_from_string(consumed);
            return (n <= 31);
        } break;

        case UTZ_TOKEN_WEEKDAY:
        {
            utz_u32 discard;
            return utz_parse_weekday(consumed, &discard);
        } break;

        default:
            UtzAssert(UTZ_FALSE && "Illegal default case");
    }

    UtzAssert(UTZ_FALSE && "Unreachable code");
    return UTZ_FALSE;
}

static utz_bool utz_maybe_next(utz_string* string, utz_string what)
{
    utz_string copy = *string;
    utz_bool match = (utz_equals(utz_next(&copy), what));
    if (match) *string = copy;
    return match;
}

static utz_bool utz_maybe_next_line(utz_string* string, utz_string* out_line)
{
    while (string->length > 0)
    {
        *out_line   = utz_consume_line(string);
        utz_string peek = *out_line;
        if (utz_next(&peek).length > 0)
            return UTZ_TRUE;
    }

    *out_line = {};
    return UTZ_FALSE;
}

static utz_bool utz_consume_prefix(utz_string* string, utz_string prefix)
{
    if (string->length < prefix.length) return UTZ_FALSE;

    for (utz_usize i = 0; i < prefix.length; i++)
        if (string->data[i] != prefix.data[i])
            return UTZ_FALSE;

    utz_consume(string, prefix.length);
    return UTZ_TRUE;
}


static utz_bool utz_maybe_next_hms(utz_string* string, utz_u32* out_seconds)
{
    utz_string hms = utz_next(string);
    if (!utz_starts_with_digit(hms))
        return UTZ_FALSE;

    utz_u32 h = utz_consume_u32(&hms);
    if (h > 47) return UTZ_FALSE; // h values are allowed to be over 23, but we won't allow 2-day wraps without manually checking it.

    utz_u32 m = 0;
    utz_u32 s = 0;
    if (utz_consume_prefix(&hms, UtzStr(":")))
    {
        if (!utz_starts_with_digit(hms))
            return UTZ_FALSE;
        m = utz_consume_u32(&hms);
        if (m > 60)
            return UTZ_FALSE;

        if (utz_consume_prefix(&hms, UtzStr(":")))
        {
            if (!utz_starts_with_digit(hms))
                return UTZ_FALSE;
            s = utz_consume_u32(&hms);
            if (s > 60)
                return UTZ_FALSE;
        }
    }

    *out_seconds = h * 3600 + m * 60 + s;

    utz_bool ok = (hms.length == 0);
    if (ok)
        return UTZ_TRUE;
    else
        return UTZ_FALSE;
}

enum utz_date_kind
{
    UTZ_DATE_KIND_UTC,
    UTZ_DATE_KIND_STANDARD_OFFSET,
    UTZ_DATE_KIND_STANDARD_OFFSET_AND_SAVINGS_OFFSET,
};

static utz_time_t utz_utc_from_timestamp_with_date_kind(utz_date_kind kind, utz_time_t timestamp, utz_s32 base_offset_seconds, utz_s32 savings_offset_seconds)
{
         if (kind == UTZ_DATE_KIND_UTC)                                 return timestamp;
    else if (kind == UTZ_DATE_KIND_STANDARD_OFFSET)                     return timestamp -  base_offset_seconds;
    else if (kind == UTZ_DATE_KIND_STANDARD_OFFSET_AND_SAVINGS_OFFSET)  return timestamp - (base_offset_seconds + savings_offset_seconds);
    else UtzAssert(0 && "Unreachable code");
    return 0;
}


static utz_bool utz_maybe_next_hms_date_part(utz_string* string, utz_u32* out_duration, utz_date_kind* out_kind)
{
    utz_string hms = utz_next(string);

    char c = (hms.length > 0) ? hms.data[hms.length - 1] : '\0';
         if (c == 's')                         { hms.length--; *out_kind = UTZ_DATE_KIND_STANDARD_OFFSET; }
    else if (c == 'g' || c == 'u' || c == 'z') { hms.length--; *out_kind = UTZ_DATE_KIND_UTC; }
    else if (c == 'w')                         { hms.length--; *out_kind = UTZ_DATE_KIND_STANDARD_OFFSET_AND_SAVINGS_OFFSET; }
    else                                       {               *out_kind = UTZ_DATE_KIND_STANDARD_OFFSET_AND_SAVINGS_OFFSET; }

    return utz_maybe_next_hms(&hms, out_duration);
}

static utz_bool utz_maybe_next_hms_duration(utz_string* string, utz_s32* out_duration_seconds)
{
    utz_consume_whitespace(string);
    if (!string->length) return UTZ_FALSE;

    utz_s32 sign = 1;
         if (string->data[0] == '+') {            utz_consume(string, 1); }
    else if (string->data[0] == '-') { sign = -1; utz_consume(string, 1); }

    utz_u32 seconds = 0;
    if (!utz_maybe_next_hms(string, &seconds))
        return UTZ_FALSE;

    *out_duration_seconds = (utz_s32)seconds * sign;
    return UTZ_TRUE;
}

static utz_bool utz_parse_latitude_and_longitude(utz_string latlong, utz_s32* out_latitude_seconds, utz_s32* out_longitude_seconds)
{
    utz_s32* outs[] = { out_latitude_seconds, out_longitude_seconds };
    for (utz_usize i = 0; i < UtzArrayCount(outs); i++)
    {
        utz_s32* out_seconds = outs[i];

        if (latlong.length == 0) return UTZ_FALSE;

        utz_s32 sign;
             if (latlong.data[0] == '+') sign =  1;
        else if (latlong.data[0] == '-') sign = -1;
        else return UTZ_FALSE;
        utz_consume(&latlong, 1);

        int len = (i == 1 /* longitude, goes to 180 degrees, 3 digits */) ? 3 : 2;
        if (latlong.length < len || !UtzIsNumeric(latlong.data[0]) || !UtzIsNumeric(latlong.data[1]))
            return UTZ_FALSE;
        *out_seconds = utz_u32_from_string(utz_take(&latlong, len));

        if (latlong.length < 2 || !UtzIsNumeric(latlong.data[0]) || !UtzIsNumeric(latlong.data[1]))
            return UTZ_FALSE;
        *out_seconds = *out_seconds * 60 + utz_u32_from_string(utz_take(&latlong, 2));

        // seconds are optional, and can be only one digit.
        utz_usize to_take = 0;
        if (latlong.length > 0 && UtzIsNumeric(latlong.data[0]))
        {
            to_take++;

            if (latlong.length > 1 && UtzIsNumeric(latlong.data[1]))
                to_take++;
        }

        utz_u32 seconds = utz_u32_from_string(utz_take(&latlong, to_take));

        *out_seconds  = *out_seconds * 60 + seconds;
        *out_seconds *= sign;
    }

    return (latlong.length == 0);
}


enum utz_day_rule_kind
{
    DAY_RULE_EQUAL_TO_DATE,
    DAY_RULE_WEEKDAY_BEFORE_OR_ON_DATE,
    DAY_RULE_WEEKDAY_AFTER_OR_ON_DATE,
};

struct utz_day_rule
{
    utz_day_rule_kind kind;

    // valid only if kind != DAY_RULE_BAD_OR_UNKNOWN
    utz_u32 date;       // invariant: 1 <= date <= 31
                        // if kind == DAY_RULE_EQUAL_TO_DATE: date SHOULD be valid for observed month.
                        // else: date doesn't have to exist for the observed month, eg date can be == 31 for February.

    utz_u32 weekday;    // invariant: 0 <= weekday <= 6
                        // only set for DAY_RULE_WEEKDAY_BEFORE_OR_ON_DATE OR DAY_RULE_WEEKDAY_AFTER_OR_ON_DATE
};

static utz_bool maybe_next_day_rule(utz_string* string, utz_day_rule* rule)
{
    utz_consume_whitespace(string);

    // attempt to parse DAY_RULE_EQUAL_TO_DATE
    // example: "23" => start on 23rd of this month.
    if (utz_peek(*string, UTZ_TOKEN_DAY_IN_MONTH))
    {
        rule->kind = DAY_RULE_EQUAL_TO_DATE;
        rule->date = utz_u32_from_string(utz_next(string));
        return UTZ_TRUE;
    }

    // attempt to pare either "last weekday rule" (DAY_RULE_WEEKDAY_BEFORE_OR_ON_DATE with date=31)
    // example: "lastSun" => last sunday of this month.
    if (utz_consume_prefix(string, UtzStr("last")))
    {
        if (!utz_peek(*string, UTZ_TOKEN_WEEKDAY))
            return UTZ_FALSE;

        rule->kind    = DAY_RULE_WEEKDAY_BEFORE_OR_ON_DATE;
        rule->date    = 31;
        rule->weekday = must_parse_weekday(utz_next(string));
        return UTZ_TRUE;
    }

    // next three characters must be a weekday.
    utz_string rest = utz_next(string);
    if (rest.length < 3) return UTZ_FALSE;
    utz_string weekday = utz_take(&rest, 3);
    if (!utz_peek(weekday, UTZ_TOKEN_WEEKDAY)) return UTZ_FALSE;
    rule->weekday = must_parse_weekday(weekday);

    // attempt to parse "first weeday rule" (DAY_RULE_WEEKDAY_AFTER_OR_ON_DATE with date=1)
    // example: "Sun" => first sunday of this month.
    if (rest.length == 0)
    {
        rule->kind = DAY_RULE_WEEKDAY_AFTER_OR_ON_DATE;
        rule->date = 1;
        return UTZ_TRUE;
    }

    // attempt to parse general DAY_RULE_WEEKDAY_BEFORE_OR_ON_DATE or DAY_RULE_WEEKDAY_AFTER_OR_ON_DATE
    // example: "Sun>=8" => first sunday in this month after or equal to the 8th in the month.

         if (utz_consume_prefix(&rest, UtzStr(">="))) rule->kind = DAY_RULE_WEEKDAY_AFTER_OR_ON_DATE;
    else if (utz_consume_prefix(&rest, UtzStr("<="))) rule->kind = DAY_RULE_WEEKDAY_BEFORE_OR_ON_DATE;
    else return UTZ_FALSE;

    if (!utz_peek(rest, UTZ_TOKEN_DAY_IN_MONTH))
        return UTZ_FALSE;

    rule->date = utz_u32_from_string(rest);
    return UTZ_TRUE;
}

static utz_bool utz_apply_day_rule_to_year_and_month(utz_day_rule rule, utz_u32 year, utz_u32 month, utz_time_t* out_date_timestamp)
{
    utz_date date = {};
    date.year  = year;
    date.month = month;

    if (rule.kind == DAY_RULE_EQUAL_TO_DATE)
    {
        date.day = rule.date;
        return (utz_bool) utz_maybe_unix_timestamp_from_utc_date(&date, out_date_timestamp);
    }
    else if (rule.kind == DAY_RULE_WEEKDAY_AFTER_OR_ON_DATE)
    {
        date.day = rule.date;

        utz_time_t t = 0;
        if (!utz_maybe_unix_timestamp_from_utc_date(&date, &t))
            return UTZ_FALSE;

        for (utz_usize i = 0; i < 7; i++)
        {
            utz_time_t offseted = t + i * 24 * 60 * 60;
            utz_date with_weekday  = {};
            utz_utc_date_from_unix_timestamp(&with_weekday, offseted);

            if (with_weekday.week_day == rule.weekday)
            {
                *out_date_timestamp = offseted;
                return UTZ_TRUE;
            }
        }
    }
    else if (rule.kind == DAY_RULE_WEEKDAY_BEFORE_OR_ON_DATE)
    {
        // Set date.day.
        // rule.date can be bigger than the maximum date for this month (example: month=Feb, rule.date=31)
        // If that's the case, seek until we get to the valid end of month.
        utz_time_t t = 0;
        utz_bool end_of_month_ok = UTZ_FALSE;
        for (utz_usize i = 0; i <= 3; i++) // test 31, 30, 29, 28
        {
            date.day = (utz_u32)(rule.date - i);
            if (utz_maybe_unix_timestamp_from_utc_date(&date, &t))
            {
                end_of_month_ok = UTZ_TRUE;
                break;
            }
        }
        if (!end_of_month_ok) return UTZ_FALSE;

        for (utz_usize i = 0; i < 7; i++)
        {
            utz_time_t offseted = t - i * 24 * 60 * 60;
            utz_date with_weekday  = {};
            utz_utc_date_from_unix_timestamp(&with_weekday, offseted);

            if (with_weekday.week_day == rule.weekday)
            {
                *out_date_timestamp = offseted;
                return UTZ_TRUE;
            }
        }
    }
    else UtzAssert(0 && "Unreachable code");

    return UTZ_FALSE;
}


typedef struct utz_parsed_savings_rule
{
    utz_time_t    active_since;
    utz_date_kind active_since_kind;
    utz_s32       offset_from_base_offset_seconds;
    utz_string    abbreviation_substitution;
    utz_time_t    sorted_by; // timestamp offseted by UTZ_BEGINNING_OF_TIME
} utz_parsed_savings_rule;

typedef struct utz_parsed_zone
{
    utz_time_t    until;
    utz_date_kind until_kind;
    utz_s32       standard_offset_seconds;
    utz_string    rule;
    utz_string    abbreviation_format;
} utz_parsed_zone;

typedef struct utz_parsed_link
{
    utz_string zone_main;
    utz_string zone_alias;
} utz_parsed_link;


// range->zone_abbreviation must be zeroed.
static utz_bool utz_get_abbreviation(utz_time_range* range, utz_parsed_zone* zone, utz_parsed_savings_rule* rule)
{
    // Handle CET/CEST case.
    utz_string fmt = zone->abbreviation_format;
    if (!fmt.length) return UTZ_TRUE;
    for (utz_usize i = 1; i < fmt.length - 1; i++)
    {
        if (fmt.data[i] != '/') continue;

        utz_usize from = 0, to = i;
        if (rule && rule->offset_from_base_offset_seconds != 0)
            from = i + 1, to = fmt.length;

        if (to - from > UtzArrayCount(range->zone_abbreviation) - 1) return UTZ_FALSE;

        for (utz_usize j = 0; j < to - from; j++)
            range->zone_abbreviation[j] = fmt.data[from + j];

        return UTZ_TRUE;
    }

    // Handle CE%sT, S case.
    for (utz_usize i = 0; i < fmt.length - 1; i++)
    {
        if (fmt.data[i] != '%' || fmt.data[i + 1] != 's') continue;

        utz_string substitution = rule ? rule->abbreviation_substitution : UtzStr("");

        if (fmt.length - 2 + substitution.length > UtzArrayCount(range->zone_abbreviation) - 1)
            return UTZ_FALSE;

        utz_usize abbr_cursor = 0;
        for (utz_usize j = 0;     j < i;                   j++) range->zone_abbreviation[abbr_cursor++] = fmt.data[j];
        for (utz_usize j = 0;     j < substitution.length; j++) range->zone_abbreviation[abbr_cursor++] = substitution.data[j];
        for (utz_usize j = i + 2; j < fmt.length;          j++) range->zone_abbreviation[abbr_cursor++] = fmt.data[j];
        return UTZ_TRUE;
    }

    // There is no substitution, just copy.
    if (fmt.length > UtzArrayCount(range->zone_abbreviation) - 1)
        return UTZ_FALSE;

    for (utz_usize i = 0; i < fmt.length; i++)
        range->zone_abbreviation[i] = fmt.data[i];

    return UTZ_TRUE;
}


int utz_parse_iana_tzdb_targz(utz_timezones* tzs, void* targz, int targz_size, void* userdata, unsigned max_year)
{
    //
    // helper macros
    //

#define CopyToCharArray(str, arr, doing_what) do {                      \
    if ((str).length + 1 > UtzArrayCount(arr))                          \
        ReportError("String '%.*s' didn't fit in char array while doing %s", \
        UtzStringArgs(str), (doing_what));                              \
    for (utz_usize _utz__i = 0; _utz__i < (str).length; _utz__i++)      \
        (arr)[_utz__i] = (str).data[_utz__i];                           \
    (arr)[(str).length] = '\0';                                         \
} while (0)

#define UtzOffsetOf(type, member) ((utz_usize)((unsigned char*)&((type*)0)->member - (unsigned char*)0))

#define SortByCharArray(type, field, arr) utz_radix_sort( \
    (utz_u8*)(arr), UtzDynCount(arr), sizeof(type), UtzOffsetOf(type, field), sizeof((UtzCtor1(type, 0)).field), UTZ_TRUE)

#define FindByCharArray(type, field, arr, what) \
    ((type*) utz_find_by_char_array( \
        (unsigned char*)(arr), UtzDynCount(arr), \
        sizeof(type), UtzOffsetOf(type, field), sizeof((UtzCtor1(type, 0)).field), \
        (unsigned char*)(what).data, (what).length))


    //
    // macros for error reporting and tar handling.
    //

    utz_string current_file = UtzStr("N/A");
    utz_string current_line = UtzStr("N/A");

#define ReportError(fmt, ...) do {                                      \
    char* buf = UtzCalloc(userdata, char, 2048);                        \
    UtzSprintf(buf, 2048, "Error: " fmt "\nFile: %.*s\nLine: %.*s\n",   \
               __VA_ARGS__, UtzStringArgs(current_file), UtzStringArgs(current_line)); \
    tzs->parsing_error = buf;                                           \
    return UTZ_FALSE;                                                   \
} while (0)

#define ReportStaticError(str) do {                                     \
    tzs->parsing_error = utz_allocate_string(userdata, UtzStr(str)).data;   \
    return UTZ_FALSE;                                                   \
} while (0)

#define MustFindFile(filename) do {                      \
    current_file = utz_get_tar_item(tar_data, filename); \
    if (!current_file.length) {                          \
        utz_string _utz__s = (filename);               \
        ReportError("Missing file '%.*s' in iana tarball.", UtzStringArgs(_utz__s)); \
    } \
} while (0)




    *tzs = UtzInit;

    // We only care about the deflate stream inside of the .gzip file.
    // Skip the header (10 bytes) and footer (8 bytes) and only process the stream.

    char* deflate_stream_start = (char*)targz + 10;
    int   deflate_stream_size  = targz_size - 10 - 8;

    if (deflate_stream_size < 0 || deflate_stream_size > targz_size)
        ReportStaticError("Invalid gzip file: doesn't contain header and/or footer.");

    int tar_size = 0;
    char* tarball = utz_zlib_decode_malloc_guesssize_headerflag(
        deflate_stream_start, deflate_stream_size,
        1332000 /* estimated size */, &tar_size, 0 /* don't parse zlib header */
    );
    if (!tarball) ReportStaticError("Failed to decompress .tar.gz");

    utz_string tar_data = { (utz_usize)tar_size, tarball };


    //
    // version and changelog.
    //

    MustFindFile(UtzStr("version"));

    // Trim trailing whitespace.
    while (UtzIsSpaceByte(current_file.data[current_file.length - 1]))
        current_file.length--;

    tzs->iana_version = utz_allocate_string(userdata, current_file).data;

    //
    // parse tzs.
    //

    utz_string timezone_filenames[] = {
        UtzStr("africa"),
        UtzStr("antarctica"),
        UtzStr("asia"),
        UtzStr("australasia"),
        UtzStr("europe"),
        UtzStr("northamerica"),
        UtzStr("southamerica"),
    };

    UtzMakeDynArray(utz_timezone, &tzs->timezones, 128);

    utz_parsed_link* links = NULL; // :free
    UtzMakeDynArray(utz_parsed_link, &links, 128);

    for (utz_usize i = 0; i < UtzArrayCount(timezone_filenames); i++)
    {
        utz_string filename = timezone_filenames[i];
        MustFindFile(filename);
        utz_string data = current_file;

        struct rules_bundle_t
        {
            utz_string           name;
            utz_parsed_savings_rule* rules;
            utz_bool               sorted_previously;
        };

        struct zones_bundle_t
        {
            utz_string   name;
            utz_parsed_zone* zones;
        };

        rules_bundle_t* rule_bundles = NULL; // :free
        zones_bundle_t* zone_bundles = NULL; // :free
        UtzMakeDynArray(rules_bundle_t, &rule_bundles, 32);
        UtzMakeDynArray(zones_bundle_t, &zone_bundles, 32);

        // Add empty rule, simplifies lookup logic later.
        {
            rules_bundle_t rule_bundle = UtzInit;
            UtzDynAppend(rules_bundle_t, &rule_bundles, &rule_bundle);
        }

        while (utz_maybe_next_line(&data, &current_line))
        {
            utz_string line = current_line;

            utz_string command = utz_next(&line);
            if (!command.length) continue;

            if (utz_equals(command, UtzStr("Rule")))
            {
                utz_parsed_savings_rule rule = {};

                if (!utz_peek(line, UTZ_TOKEN_WORD))
                    ReportStaticError("Failed to parse rule.name");
                utz_string name = utz_next(&line);

                if (!utz_peek(line, UTZ_TOKEN_YEAR))
                    ReportStaticError("rule.from_year bad or not in valid range.");
                utz_u32 from_year = utz_u32_from_string(utz_next(&line));

                utz_u32 to_year = 0;
                     if (utz_peek(line, UTZ_TOKEN_YEAR))        to_year = utz_u32_from_string(utz_next(&line));
                else if (utz_maybe_next(&line, UtzStr("only"))) to_year = from_year;
                else if (utz_maybe_next(&line, UtzStr("max")))  to_year = max_year;
                else ReportStaticError("Invalid rule.to_year");

                if (!utz_maybe_next(&line, UtzStr("-")))
                    ReportStaticError("Expected '-' between rule.to_year and rule.month");

                if (!utz_peek(line, UTZ_TOKEN_MONTH))
                    ReportStaticError("Bad rule.month");
                utz_u32 month = utz_must_parse_month(utz_next(&line));

                utz_day_rule day_rule = {};
                if (!maybe_next_day_rule(&line, &day_rule))
                    ReportStaticError("Bad rule.day_rule");

                utz_u32 hms_date_part = 0;
                if (!utz_maybe_next_hms_date_part(&line, &hms_date_part, &rule.active_since_kind))
                    ReportStaticError("Bad rule.change_at_hm");

                if (!utz_maybe_next_hms_duration(&line, &rule.offset_from_base_offset_seconds))
                    ReportStaticError("Bad rule.offset_from_standard_hm");

                if (!utz_maybe_next(&line, UtzStr("-")))
                {
                    if (!utz_peek(line, UTZ_TOKEN_WORD))
                        ReportStaticError("Bad rule.abbreviation_substitution");
                    rule.abbreviation_substitution = utz_next(&line);
                }

                if (utz_next(&line).length) ReportStaticError("Expected end of line but got garbage.");

                rules_bundle_t* rule_bundle = NULL;
                for (utz_usize i = 0; i < UtzDynCount(rule_bundles); i++)
                {
                    if (utz_equals(rule_bundles[i].name, name))
                    {
                        rule_bundle = &rule_bundles[i];
                        break;
                    }
                }
                if (!rule_bundle)
                {
                    rules_bundle_t zero = UtzInit;
                    UtzDynAppend(rules_bundle_t, &rule_bundles, &zero);
                    rule_bundle = UtzDynGetLast(rule_bundles);
                    rule_bundle->name = name;
                    UtzMakeDynArray(utz_parsed_savings_rule, &rule_bundle->rules, 32);
                }
                UtzAssert(rule_bundle);

                printf("DEBUG: year: %04u - %04u\n", from_year, to_year);

                for (utz_u32 year = from_year; year <= to_year; year++)
                {
                    if (!utz_apply_day_rule_to_year_and_month(day_rule, year, month, &rule.active_since))
                        ReportError("Bad rule '%.*s': Can't apply day_rule.kind=%d to year=%04u month=%02u", UtzStringArgs(name), day_rule.kind, year, month);

                    rule.active_since += hms_date_part;
                    UtzDynAppend(utz_parsed_savings_rule, &rule_bundle->rules, &rule);
                }
            }
            else if (utz_equals(command, UtzStr("Zone")))
            {
                if (!utz_peek(line, UTZ_TOKEN_WORD))
                    ReportStaticError("Missing zone.name.");
                utz_string name = utz_next(&line);

                zones_bundle_t* zone_bundle = NULL;
                for (utz_usize i = 0; i < UtzDynCount(zone_bundles); i++)
                {
                    if (utz_equals(zone_bundles[i].name, name))
                    {
                        zone_bundle = &zone_bundles[i];
                        break;
                    }
                }
                if (!zone_bundle)
                {
                    zones_bundle_t zero = UtzInit;
                    UtzDynAppend(zones_bundle_t, &zone_bundles, &zero);
                    zone_bundle = UtzDynGetLast(zone_bundles);
                    zone_bundle->name = name;
                    UtzMakeDynArray(utz_parsed_zone, &zone_bundle->zones, 32);
                }
                UtzAssert(zone_bundle);

                utz_bool zone_closed = UTZ_FALSE;
                while (UTZ_TRUE)
                {
                    utz_parsed_zone zone = {};

                    if (!utz_maybe_next_hms_duration(&line, &zone.standard_offset_seconds))
                        ReportStaticError("Bad zone.standard_offset_string.");

                    if (!utz_maybe_next(&line, UtzStr("-")))
                    {
                        if (!utz_peek(line, UTZ_TOKEN_WORD))
                            ReportStaticError("Missing zone.rule.");

                        utz_string maybe_rule = utz_next(&line);
                        {
                            utz_string maybe_hms = maybe_rule;

                            utz_s32 constant_offset_from_base_offset = 0;
                            if (utz_maybe_next_hms_duration(&maybe_hms, &constant_offset_from_base_offset))
                                zone.standard_offset_seconds += constant_offset_from_base_offset;
                            else
                                zone.rule = maybe_rule;
                        }
                    }

                    if (!utz_peek(line, UTZ_TOKEN_WORD))
                        ReportStaticError("Missing zone.abbreviation_format.");
                    zone.abbreviation_format = utz_consume_until_whitespace(&line);

                    utz_string check_format = zone.abbreviation_format;
                    while (1)
                    {
                        utz_usize percent_idx = 0;
                        for (; percent_idx < check_format.length; percent_idx++)
                            if (check_format.data[percent_idx] == '%')
                                break;

                        if (percent_idx == check_format.length) break;

                        utz_bool fmt_ok = (
                            percent_idx + 1 < zone.abbreviation_format.length &&
                            (zone.abbreviation_format.data[percent_idx + 1] == 's' ||
                             zone.abbreviation_format.data[percent_idx + 1] == '%')
                        );
                        if (!fmt_ok) ReportStaticError("Unrecognized zone.abbreviation_format. Only %%s and %%%% is allowed.");

                        utz_consume(&check_format, percent_idx + 1 + 1);
                    }

                    {
                        utz_string peek = line;
                        if (!utz_next(&peek).length) // UNTIL is blank.
                        {
                            zone.until      = UTZ_END_OF_TIME;
                            zone.until_kind = UTZ_DATE_KIND_UTC;
                            UtzDynAppend(utz_parsed_zone, &zone_bundle->zones, &zone);

                            zone_closed = UTZ_TRUE;
                            break;
                        }
                    }

                    utz_date date = {};
                    date.month = 1;
                    date.day   = 1;

                    if (!utz_peek(line, UTZ_TOKEN_YEAR))
                        ReportStaticError("Bad year for zone.until");
                    date.year  = utz_u32_from_string(utz_next(&line));

                    // dates like "2020", "2020 Jun" aren't parsed via utz_apply_day_rule_to_year_and_month().
                    utz_bool date_parsed_via_day_rule = UTZ_FALSE;
                    if (utz_peek(line, UTZ_TOKEN_MONTH))
                    {
                        date.month = utz_must_parse_month(utz_next(&line));

                        utz_day_rule day_rule = {};
                        if (maybe_next_day_rule(&line, &day_rule))
                        {
                            if (utz_apply_day_rule_to_year_and_month(day_rule, date.year, date.month, &zone.until))
                                date_parsed_via_day_rule = UTZ_TRUE;
                            else
                                ReportError("Failed to parse zone.until. "
                                            "Bad rule '%.*s': Can't apply day_rule.kind=%d to year=%04u month=%02u",
                                            UtzStringArgs(name), day_rule.kind, date.year, date.month);
                        }
                    }

                    if (!date_parsed_via_day_rule && !utz_maybe_unix_timestamp_from_utc_date(&date, &zone.until))
                        ReportStaticError("Bad zone.until.");

                    utz_u32 hms_part = 0;
                    utz_string hms = utz_next(&line);
                    if (hms.length && !utz_maybe_next_hms_date_part(&hms, &hms_part, &zone.until_kind))
                        ReportStaticError("Bad h:m:s for zone.until");
                    zone.until += hms_part;

                    if (utz_next(&line).length) ReportStaticError("Expected end of line but got garbage.");

                    UtzDynAppend(utz_parsed_zone, &zone_bundle->zones, &zone);

                    if (!utz_maybe_next_line(&data, &current_line)) break;
                    line = current_line;
                }

                if (!zone_closed) ReportStaticError("Zone not closed with an empty UNTIL column.");
            }
            else if (utz_equals(command, UtzStr("Link")))
            {
                utz_parsed_link link = {};

                if (!utz_peek(line, UTZ_TOKEN_WORD))
                    ReportStaticError("Missing zone.zone_main.");
                link.zone_main = utz_next(&line);

                if (!utz_peek(line, UTZ_TOKEN_WORD))
                    ReportStaticError("Missing zone.zone_alias.");
                link.zone_alias = utz_next(&line);

                if (utz_next(&line).length) ReportStaticError("Expected end of line but got garbage.");

                UtzDynAppend(utz_parsed_link, &links, &link);
            }
            else
            {
                ReportStaticError("Unknown command type.");
            }
        }
        current_file = {};
        current_line = {};


        for (utz_usize zone_bundle_idx = 0; zone_bundle_idx < UtzDynCount(zone_bundles); zone_bundle_idx++)
        {
            zones_bundle_t* it = &zone_bundles[zone_bundle_idx];

            // @Reconsider binary search
            utz_bool is_alias = UTZ_FALSE;
            for (utz_usize i = 0; i < UtzDynCount(links); i++)
                if (utz_equals(it->name, links[i].zone_alias))
                {
                    is_alias = UTZ_TRUE;
                    break;
                }
            if (is_alias) continue;

            utz_timezone* timezone = NULL;
            {
                utz_timezone zero = UtzInit;
                UtzDynAppend(utz_timezone, &tzs->timezones, &zero);
                timezone = UtzDynGetLast(tzs->timezones);
                CopyToCharArray(it->name, timezone->name, "making a new utz_timezone");
            }

            utz_time_range* time_ranges = NULL; // :free
            UtzMakeDynArray(utz_time_range, &time_ranges, 64);

            utz_time_t             cursor   = UTZ_BEGINNING_OF_TIME;
            utz_parsed_savings_rule  previous = UtzInit; // no rule at the beginning, so previous is zero.
            for (utz_usize zone_idx = 0; zone_idx < UtzDynCount(it->zones); zone_idx++)
            {
                utz_parsed_zone* zone = &it->zones[zone_idx];

                // @Reconsider binary search
                rules_bundle_t* rule_bundle = NULL;
                for (utz_usize i = 0; i < UtzDynCount(rule_bundles); i++)
                {
                    if (utz_equals(rule_bundles[i].name, zone->rule))
                    {
                        rule_bundle = &rule_bundles[i];
                        break;
                    }
                }
                if (!rule_bundle) ReportError("Zone '%s' tried to use non existant rule '%.*s'", timezone->name, UtzStringArgs(zone->rule));

                // Sort the rules. They aren't sorted because we expand each rule line into one rule object for each year it is valid.
                // Rules need to be sorted chronologically. This can't be done unambigously in the general case.
                //
                // [0] We assume that we can sort the rules by just applying the current zone->standard_offset.
                // This can be UTZ_FALSE only when standard offsets between zone entries differ by more than a day - this can affect day rules such as "lastSun".
                // In that case we report that the rules have been resorted between zone lines because the situation is sussy.
                //
                // [1] Similarly, we need to report when a rule switch happens before 2 days have been elapsed; rouding rule starts to the day can be wrong in that case too.

                if (!rule_bundle->sorted_previously)
                {
                    for (utz_usize i = 0; i < UtzDynCount(rule_bundle->rules); i++)
                    {
                        // @Reconsider @Reconsider @Reconsider @Reconsider @Reconsider remove div by days
                        // and check div on negative numbers
                        utz_parsed_savings_rule* r = &rule_bundle->rules[i];
                        r->sorted_by = utz_utc_from_timestamp_with_date_kind(r->active_since_kind, r->active_since, zone->standard_offset_seconds, 0 /* see [0] */);// / (24 * 3600);
                        r->sorted_by -= UtzMinValue(utz_time_t);
                    }

                    utz_radix_sort(
                        rule_bundle->rules, UtzDynCount(rule_bundle->rules), sizeof(utz_parsed_savings_rule),
                        UtzOffsetOf(utz_parsed_savings_rule, sorted_by), sizeof(utz_time_t), UTZ_FALSE
                    );
                    rule_bundle->sorted_previously = UTZ_TRUE;
                }
                else
                {
                    for (utz_usize i = 1; i < UtzDynCount(rule_bundle->rules); i++)
                    {

                        // @Reconsider @Reconsider @Reconsider @Reconsider @Reconsider remove div by days
                        // and check div on negative numbers
                        utz_parsed_savings_rule* a = &rule_bundle->rules[i - 1];
                        utz_parsed_savings_rule* b = &rule_bundle->rules[i];
                        utz_time_t a_t = utz_utc_from_timestamp_with_date_kind(a->active_since_kind, a->active_since, zone->standard_offset_seconds, 0 /* see [0] */);// / (24 * 3600);
                        utz_time_t b_t = utz_utc_from_timestamp_with_date_kind(b->active_since_kind, b->active_since, zone->standard_offset_seconds, 0 /* see [0] */);// / (24 * 3600);

                        if (b_t < a_t)
                            ReportError("Savings rules were sorted differently when applying rule '%.*s' to zone '%.*s'. Zone line index was %u.",
                                        UtzStringArgs(zone->rule), UtzStringArgs(it->name), (unsigned)zone_idx);
                    }
                }

                // Insert a "sentinel" range before evaluating rules.
                // Most of the time this rule will get squished when we remove redundant ranges.
                // This is needed to always have a range starting at 1970-01-01.
                // @Reconsider Abbreviation is just the minimally processed format string.
                if (!UtzDynCount(time_ranges))
                {
                    utz_time_range sentinel = UtzInit;
                    sentinel.since          = cursor;
                    sentinel.offset_seconds = zone->standard_offset_seconds;
                    if (!utz_get_abbreviation(&sentinel, zone, NULL))
                        ReportError("Can't get abbreviation for zone '%.*s': %.*s.", UtzStringArgs(zone->rule), UtzStringArgs(zone->abbreviation_format));

                    UtzDynAppend(utz_time_range, &time_ranges, &sentinel);
                }

                //if (utz_equals(it->name, UtzStr("Africa/Algiers"))) __debugbreak();

                for (utz_usize i = 0; i < UtzDynCount(rule_bundle->rules); i++)
                {
                    utz_parsed_savings_rule* rule = &rule_bundle->rules[i];

                    // we can only determine when the current rule starts if we know what was the wall time during the previous rule.
                    utz_time_t rule_since = utz_utc_from_timestamp_with_date_kind(rule->active_since_kind, rule->active_since, zone->standard_offset_seconds, previous.offset_from_base_offset_seconds);
                    utz_time_t zone_until = utz_utc_from_timestamp_with_date_kind(zone->until_kind,        zone->until,        zone->standard_offset_seconds, previous.offset_from_base_offset_seconds);

                    utz_bool rule_start_after_zone_ends = (rule_since >= zone_until);
                    utz_bool rule_is_zero_length        = (rule_since <= cursor);

                    if (rule_start_after_zone_ends || rule_is_zero_length)
                    {
                        previous = *rule; // :UpdatePrevRule
                        break;
                    }

                    UtzAssert(UtzDynCount(time_ranges) == 0 || rule_since >= UtzDynGetLast(time_ranges)->since);

                    cursor = rule_since;

                    utz_time_range new_range = UtzInit;
                    new_range.since          = cursor;
                    new_range.offset_seconds = zone->standard_offset_seconds + rule->offset_from_base_offset_seconds;
                    if (!utz_get_abbreviation(&new_range, zone, rule))
                        ReportError("Can't get abbreviation for zone '%.*s': %.*s (subs: '%.*s').",
                                    UtzStringArgs(zone->rule), UtzStringArgs(zone->abbreviation_format), UtzStringArgs(rule->abbreviation_substitution));

                    utz_usize inserted_so_far = UtzDynCount(time_ranges);
                    if (inserted_so_far > 0)
                    {
                        if (time_ranges[inserted_so_far - 1].offset_seconds == new_range.offset_seconds &&
                            utz_equals(UtzStr(time_ranges[inserted_so_far - 1].zone_abbreviation), UtzStr(new_range.zone_abbreviation)))
                        {
                            previous = *rule; // :UpdatePrevRule
                            break;
                        }
                    }

                    UtzDynAppend(utz_time_range, &time_ranges, &new_range);

                    // check for suspicious next_rule_since.
                    if (i + 1 < UtzDynCount(rule_bundle->rules))
                    {
                        utz_parsed_savings_rule* next = &rule_bundle->rules[i + 1];
                        utz_time_t next_rule_since = utz_utc_from_timestamp_with_date_kind(next->active_since_kind, next->active_since, zone->standard_offset_seconds, rule->offset_from_base_offset_seconds);
                        UtzAssert(next_rule_since > rule_since);
                        if (next_rule_since - rule_since < 2 * (24 * 3600))
                            ReportError("Rule '%.*s' switches to a new line faster than in 2 days. Switch between index %u and %u.",
                                        UtzStringArgs(zone->rule), (unsigned)i, (unsigned)(i + 1));
                    }

                    previous = *rule; // :UpdatePrevRule
                }

                // move the cursor up to zone's until date.
                utz_time_t zone_until = utz_utc_from_timestamp_with_date_kind(zone->until_kind, zone->until, zone->standard_offset_seconds, previous.offset_from_base_offset_seconds);

                if (cursor < zone_until)
                    cursor = zone_until;
                else
                    UtzAssert(cursor == UTZ_BEGINNING_OF_TIME);
            }

            timezone->ranges      = time_ranges;
            timezone->range_count = UtzDynCount(time_ranges);
        }

        /*
        For (time_ranges)
        {
            if (it->since < 132223104000000000llu) continue;

            Debug("short: % since: % offset: %:%",
                   string_format(it->zone_abbreviation, 7), debug_print_filetime(it->since),
                   it->offset_seconds / 3600, abs(it->offset_seconds / 60 - 60 * (it->offset_seconds / 3600)));
        }

        Debug("Parsed rules:");
        For (rules)
        {
            Debug("Rule: %", it->key);
            For (it->value)
            {
                Date date = {};
                utc_date_from_filetime(it->active_since, &date);

                Debug("active_since: %-%-% %:%:% offset (s): % offset (h:m): %:% subs: %",
                      u64_format(date.year, 4), u64_format(date.month,  2), u64_format(date.day,    2),
                      u64_format(date.hour, 2), u64_format(date.minute, 2), u64_format(date.second, 2),
                      it->offset_from_base_offset_seconds,
                      it->offset_from_base_offset_seconds / 3600,
                      abs(it->offset_from_base_offset_seconds / 60 - 60 * (it->offset_from_base_offset_seconds / 3600)),
                      it->abbreviation_substitution);
            }
            Debug("---------------------------------");
        }

        Debug("Parsed tzs:");
        For (zones)
        {
            Debug("Zone: %", it->key);
            For (it->value)
            {
                Date date = {};
                utc_date_from_filetime(it->until, &date);

                Debug("rule: % until: %-%-% %:%:% offset (s): % offset (h): %:% format: %",
                      it->rule, u64_format(date.year, 4), u64_format(date.month,  2), u64_format(date.day,    2),
                                u64_format(date.hour, 2), u64_format(date.minute, 2), u64_format(date.second, 2),
                      it->standard_offset_seconds,
                      it->standard_offset_seconds / 3600,
                      abs(it->standard_offset_seconds / 60 - 60 * (it->standard_offset_seconds / 3600)),
                      it->abbreviation_format);
            }
            Debug("---------------------------------");
        }

        Debug("Parsed links:");
        For (links)
        {
            Debug("% => %", it->zone_alias, it->zone_main);
        }
        Debug("---------------------------------");
        */
    }

    // resolve timezones and links.

    utz_usize timezone_count_before_links = UtzDynCount(tzs->timezones);
    for (utz_usize i = 0; i < UtzDynCount(links); i++)
    {
        utz_timezone* main = NULL;
        for (utz_usize j = 0; j < timezone_count_before_links; j++)
            if (utz_equals(links[i].zone_main, tzs->timezones[j].name))
            {
                main = &tzs->timezones[i];
                break;
            }
        if (!main) ReportError("Can't resolve alias '%.*s' because main zone '%.*s' doesn't exist.",
                               UtzStringArgs(links[i].zone_alias), UtzStringArgs(links[i].zone_main));


        UtzDynAppend(utz_timezone, &tzs->timezones, main);
        utz_timezone* newtz = UtzDynGetLast(tzs->timezones);
        newtz->alias_of = main;
        CopyToCharArray(links[i].zone_alias, newtz->name, "creating a link");
    }

    for (utz_usize i = 0; i < UtzDynCount(tzs->timezones); i++)
    {
        tzs->timezones[i].range_count = UtzDynCount(tzs->timezones[i].ranges);
        UtzAssert(tzs->timezones[i].range_count > 0);
        UtzAssert(tzs->timezones[i].ranges[0].since == UTZ_BEGINNING_OF_TIME);

        for (utz_usize j = 1; j < tzs->timezones[i].range_count; j++)
        {
            utz_time_range* a = &tzs->timezones[i].ranges[j - 1];
            utz_time_range* b = &tzs->timezones[i].ranges[j];

            UtzAssert(a->since < b->since);
            UtzAssert(a->offset_seconds != b->offset_seconds ||
                   !utz_equals(UtzStr(a->zone_abbreviation), UtzStr(b->zone_abbreviation)));
        }
    }

    tzs->timezone_count = UtzDynCount(tzs->timezones);
    SortByCharArray(utz_timezone, name, tzs->timezones);

    //
    // parse countries.
    //

    MustFindFile(UtzStr("iso3166.tab"));

    UtzMakeDynArray(utz_country, &tzs->countries, 128);

    utz_string countries = current_file;
    while (utz_maybe_next_line(&countries, &current_line))
    {
        utz_string line = current_line;

        if (!utz_peek(line, UTZ_TOKEN_WORD)) ReportStaticError("Expected country.code");
        utz_string code = utz_next(&line);

        utz_string name = next_space_separated_string(&line);
        if (!name.length)        ReportStaticError("Expected country.name");
        if (utz_next(&line).length)  ReportStaticError("Expected end of line but got garbage.");

        utz_country country = UtzInit;
        CopyToCharArray(name, country.name, "parsing country name");
        CopyToCharArray(code, country.code, "parsing country code");
        UtzDynAppend(utz_country, &tzs->countries, &country);

        // @Temporary
        // // Insert an alias if one references this country.
        // for (utz_usize i = 0; i < UtzArrayCount(the_country_aliases); i++)
        // {
        //     if (!utz_equals(the_country_aliases[i].main_country_code, code)) continue;
        //     utz_country alias = UtzInit;
        //     CopyToCharArray(the_country_aliases[i].alias_country_name, alias.name, "parsing alias country name");
        //     CopyToCharArray(the_country_aliases[i].alias_country_code, alias.code, "parsing alias country code");
        //     UtzDynAppend(utz_country, &tzs->countries, &alias);
        // }
    }

    tzs->country_count = UtzDynCount(tzs->countries);
    SortByCharArray(utz_country, code, tzs->countries);

    //
    // Parse country to timezone relations.
    //

    MustFindFile(UtzStr("zone1970.tab"));

    utz_string country_to_timezone = current_file;
    while (utz_maybe_next_line(&country_to_timezone, &current_line))
    {
        utz_string line = current_line;

        utz_string comma_separated_codes = utz_next(&line);
        if (!comma_separated_codes.length) ReportStaticError("Expected a comma separated list of country codes.");

        utz_string latlong = utz_next(&line);
        if (!latlong.length) ReportStaticError("Expected latitude/longitude after country code.");

        utz_string zone_name = utz_next(&line);
        if (!zone_name.length) ReportStaticError("Expected timezone name after latitude/longitude.");

        utz_timezone* zone = FindByCharArray(utz_timezone, name, tzs->timezones, zone_name);
        if (!zone) ReportError("Can't find timezone '%.*s'", UtzStringArgs(zone_name));

        if (!utz_parse_latitude_and_longitude(latlong, &zone->coordinate_latitude_seconds, &zone->coordinate_longitude_seconds))
            ReportStaticError("Bad latitude/longitude.");

        while (true)
        {
            utz_string code = { 0, comma_separated_codes.data };
            while (comma_separated_codes.length && comma_separated_codes.data[0] != ',')
            {
                utz_consume(&comma_separated_codes, 1);
                code.length++;
            }
            if (comma_separated_codes.length) utz_consume(&comma_separated_codes, 1);

            if (!code.length) break;

            utz_country* country = FindByCharArray(utz_country, code, tzs->countries, code);
            if (!country) ReportError("Can't find country with code '%.*s", UtzStringArgs(code));

            if (!country->timezones)
                UtzMakeDynArray(utz_timezone*, &country->timezones, 1);
            UtzDynAppend(utz_timezone*, &country->timezones, &zone);
        }

        // rest of `line` is comments
    }

    current_line = UtzStr("--- EOF (file already processed) ---");

    for (utz_usize country_idx = 0; country_idx < UtzDynCount(tzs->countries); country_idx++)
    {
        utz_country* country = &tzs->countries[country_idx];

        // We know that these countries don't have any timezone data, and we don't care.
        if (!country->timezones && (!utz_equals(UtzStr("BV"), country->code) && !utz_equals(UtzStr("HM"), country->code)))
            ReportError("No timezones for country '%s' (%s).", country->name, country->code);

#if 0
        // Resolve default timezone overrides. There should only be one override per country. I don't care to check this.
        for (utz_usize i = 0; i < ArrayCount(the_default_timezone_overrides); i++)
        {
            Default_Timezone_Override* override = &the_default_timezone_overrides[i];
            if (override->country_code != country->code) continue;

            // Find the default timezone and insert it at index 0.
            utz_bool found = UTZ_FALSE;
            utz_timezone* previous = country->timezones.count ? country->timezones[0] : NULL;
            for (utz_usize j = 1; j < country->timezones.count; j++)
            {
                if (previous->name == override->default_timezone_name)
                {
                    country->timezones[0] = previous;
                    found = UTZ_TRUE;
                    break;
                }
                else
                {
                    utz_timezone* temp = country->timezones[j];
                    country->timezones[j] = previous;
                    previous = temp;
                }
            }
            if (previous->name == override->default_timezone_name)
            {
                country->timezones[0] = previous;
                found = UTZ_TRUE;
            }

            if (!found)
            {
                utz_string_t_Concatenator cat = {};
                FormatAdd(&cat, "Specified a default timezone override '%' for country '% (%)', but the country doesn't have this timezone. "
                                "It has these timezones:",
                                override->default_timezone_name, country->name, country->code);

                For (country->timezones)
                {
                    add(&cat, "\n");
                    add(&cat, (*it)->name);
                }

                return report_error(resolve_to_string_and_free(&cat, temp));
            }
        }
#endif

        // @Temporary
        // // Resolve country aliases. These countries have already been inserted.
        // for (utz_usize ai = 0; ai < UtzArrayCount(the_country_aliases); ai++)
        // {
        //     if (!utz_equals(the_country_aliases[ai].main_country_code, country->code)) continue;
        //     for (utz_usize ci = 0; ci < UtzDynCount(tzs->countries); ci++)
        //         if (utz_equals(the_country_aliases[ai].alias_country_code, tzs->countries[ci].code))
        //         {
        //             tzs->countries[ci].timezones = country->timezones;
        //             break;
        //         }
        // }
    }

    return UTZ_TRUE;

#undef CopyToCharArray
#undef UtzOffsetOf
#undef SortByCharArray
#undef FindByCharArray
#undef ReportError
#undef ReportStaticError
#undef MustFindFile
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
// Conversion

utz_time_t utz_wall_time_from_utc(utz_timezone* tz, utz_time_t utc)
{
    if (tz == NULL)           return utc;
    if (utc < 0)              return utc; // We pretend there are no timezones before UNIX_EPOCH
    if (tz->range_count == 0) return utc; // utz_timezone without ranges - this is just the "UTC" timezone.

    utz_usize lo = 0;
    utz_usize hi = tz->range_count;
    while (lo < hi)
    {
        utz_usize m = lo + (hi - lo) / 2;

        if (tz->ranges[m].since <= utc) lo = m + 1;
        else                            hi = m;
    }

    // First range of all timezones must have since == UNIX_EPOCH, so a result (lo - 1) always exists.
    UtzAssert(lo > 0);

    return utc + tz->ranges[lo - 1].offset_seconds;
}

utz_conversion utz_utc_from_wall_time(utz_timezone* tz, utz_time_t wall_time)
{
    if (tz == NULL)
        return { UTZ_TIMESTAMP_CONVERSION_OK, wall_time, wall_time, wall_time };

    if (wall_time < 24 * 60 * 60)
        return { UTZ_TIMESTAMP_CONVERSION_OK, wall_time, wall_time, wall_time };  // too close to zero, might underflow

    for (utz_usize i = 0; i < tz->range_count; i++)
    {
        utz_time_range* current = &tz->ranges[i];
        utz_time_range* next    = (i + 1 < tz->range_count) ? &tz->ranges[i + 1] : NULL;

        utz_time_t to  = next ? next->since : UtzMaxValue(utz_time_t); // :File_Time_Sign
        utz_time_t utc = wall_time - current->offset_seconds;

        // Exact moment of changing a range belongs to current.
        // Because of this, both wall times when clocks go forward are not treated as invalid,
        // and both times when clocks go backwards are treated as ambiguous.
        if (utc > to) continue;

        if (next)
        {
            utz_time_t utc_with_next = wall_time - next->offset_seconds;
            // We belong in both current and next (ambiguity).
            if (utc_with_next >= to)
                return { UTZ_TIMESTAMP_CONVERSION_INPUT_AMBIGUOUS, utc, utc_with_next, utc };
        }

        if (utc < current->since)
        {
            // Wall time is invalid.
            if (i == 0)
            {
                // It's invalid for us to be in the first range, but the range starts at UNIX_EPOCH.
                // We ignore timezones prior to UNIX_EPOCH, return input as UTC.
                return { UTZ_TIMESTAMP_CONVERSION_OK, wall_time, wall_time, wall_time };
            }
            else
            {
                utz_time_range* previous = &tz->ranges[i - 1];
                utz_time_t utc_with_previous = wall_time - previous->offset_seconds;
                return { UTZ_TIMESTAMP_CONVERSION_INPUT_INVALID, utc, utc_with_previous, current->since };
            }
        }

        return { UTZ_TIMESTAMP_CONVERSION_OK, utc, utc, utc };
    }

    UtzAssert(0 && "Unreachable code");
    utz_conversion ret = {(utz_conversion_status) 0};
    return ret;
}

utz_timezone* utz_default_tz_for_country(utz_timezones* tzs, const char* country_code)
{
    for (utz_usize ci = 0; ci < tzs->country_count; ci++)
    {
        if (!utz_equals(UtzStr(tzs->countries[ci].code), country_code)) continue;
        return tzs->countries[ci].timezone_count ? tzs->countries[ci].timezones[0] : NULL;
    }
    return NULL;
}

utz_bool utz_wall_time_from_utc_default_tz(utz_timezones* tzs, const char* country_code, utz_time_t utc, utz_time_t* out_wall_time)
{
    utz_timezone* default_zone = utz_default_tz_for_country(tzs, country_code);
    if (!default_zone)
    {
        *out_wall_time = utc;
        return UTZ_FALSE;
    }

    *out_wall_time = utz_wall_time_from_utc(default_zone, utc);
    return UTZ_TRUE;
}

utz_bool utz_utc_from_wall_time_default_tz(utz_timezones* tzs, const char* country_code, utz_time_t wall_time, utz_conversion* out_result)
{
    utz_timezone* default_zone = utz_default_tz_for_country(tzs, country_code);
    if (!default_zone)
    {
        out_result->status        = UTZ_TIMESTAMP_CONVERSION_OK;
        out_result->earlier       = wall_time;
        out_result->later         = wall_time;
        out_result->closest_valid = wall_time;
        return UTZ_FALSE;
    }

    *out_result = utz_utc_from_wall_time(default_zone, wall_time);
    return UTZ_TRUE;
}



#undef UtzCalloc
#undef UtzRealloc
#undef UtzFree
#undef UtzSprintf
#undef UtzAssert

#undef UTZ_TRUE
#undef UTZ_FALSE
#undef UtzArrayCount
#undef UtzInit
#undef UtzCtor1
#undef UtzCtor2
#undef UtzInit
#undef UtzCtor1
#undef UtzCtor2
#undef UtzMaxValue
#undef UtzMinValue
#undef UTZ_BEGINNING_OF_TIME
#undef UTZ_END_OF_TIME
#undef UtzDynCapacity
#undef UtzDynCount
#undef UtzDynCapacityPtr
#undef UtzDynCountPtr
#undef UtzMakeDynArray
#undef UtzGrowDynArray
#undef UtzDynAppend
#undef UtzDynGetLast
#undef UtzFreeDynArray


#endif
