#ifndef _TIF_CONFIG_H_
#define _TIF_CONFIG_H_

#include <stdint.h>

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define as 0 or 1 according to the floating point format suported by the
   machine */
#define HAVE_IEEEFP 1

/* Define to 1 if you have the `jbg_newlen' function. */
#define HAVE_JBG_NEWLEN 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <io.h> header file. */
#define HAVE_IO_H 1

/* Define to 1 if you have the <search.h> header file. */
#define HAVE_SEARCH_H 1

/* Define to 1 if you have the `setmode' function. */
#define HAVE_SETMODE 1

/* Define to 1 if you have the declaration of `optarg', and to 0 if you don't. */
#define HAVE_DECL_OPTARG 0

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* Signed 64-bit type formatter */
#define TIFF_INT64_FORMAT "%I64d"

/* Signed 64-bit type */
#define TIFF_INT64_T signed __int64

/* Unsigned 64-bit type formatter */
#define TIFF_UINT64_FORMAT "%I64u"

/* Unsigned 64-bit type */
#define TIFF_UINT64_T unsigned __int64

#if _WIN64
/*
  Windows 64-bit build
*/

/* Pointer difference type */
#  define TIFF_PTRDIFF_T TIFF_INT64_T

/* The size of `size_t', as computed by sizeof. */
#  define SIZEOF_SIZE_T 8

/* Size type formatter */
#  define TIFF_SIZE_FORMAT TIFF_INT64_FORMAT

/* Unsigned size type */
#  define TIFF_SIZE_T TIFF_UINT64_T

/* Signed size type formatter */
#  define TIFF_SSIZE_FORMAT TIFF_INT64_FORMAT

/* Signed size type */
#  define TIFF_SSIZE_T TIFF_INT64_T

#else
/*
  Windows 32-bit build
*/

/* Pointer difference type */
#  define TIFF_PTRDIFF_T signed int

/* The size of `size_t', as computed by sizeof. */
#  define SIZEOF_SIZE_T 4

/* Size type formatter */
#  define TIFF_SIZE_FORMAT "%u"

/* Size type formatter */
#  define TIFF_SIZE_FORMAT "%u"

/* Unsigned size type */
#  define TIFF_SIZE_T unsigned int

/* Signed size type formatter */
#  define TIFF_SSIZE_FORMAT "%d"

/* Signed size type */
#  define TIFF_SSIZE_T signed int

#endif

/* Set the native cpu bit order */
#define HOST_FILLORDER FILLORDER_LSB2MSB


/* Signed 16-bit type */
#define TIFF_INT16_T int16_t

/* Signed 32-bit type */
#define TIFF_INT32_T int32_t

/* Signed 8-bit type */
#define TIFF_INT8_T int8_t

/* Unsigned 16-bit type */
#define TIFF_UINT16_T uint16_t

/* Unsigned 32-bit type */
#define TIFF_UINT32_T uint32_t

/* Unsigned 8-bit type */
#define TIFF_UINT8_T uint8_t

/* Compatibility stuff. */

/* Define as 0 or 1 according to the floating point format suported by the
   machine */
#define HAVE_IEEEFP 1

/* Native cpu byte order: 1 if big-endian (Motorola) or 0 if little-endian (Intel) */
#define HOST_BIGENDIAN 0

/* Support CCITT Group 3 & 4 algorithms */
#undef CCITT_SUPPORT

/* Support JPEG compression (requires IJG JPEG library) */
#undef JPEG_SUPPORT

/* Support JBIG compression (requires JBIG-KIT library) */
#undef JBIG_SUPPORT

/* Support LogLuv high dynamic range encoding */
#undef LOGLUV_SUPPORT

/* Support LZW algorithm */
#undef LZW_SUPPORT

/* Support NeXT 2-bit RLE algorithm */
#undef NEXT_SUPPORT

/* Support Old JPEG compresson (read contrib/ojpeg/README first! Compilation
   fails with unpatched IJG JPEG library) */
#undef OJPEG_SUPPORT

/* Support Macintosh PackBits algorithm */
#undef PACKBITS_SUPPORT

/* Support Pixar log-format algorithm (requires Zlib) */
#undef PIXARLOG_SUPPORT

/* Support ThunderScan 4-bit RLE algorithm */
#undef THUNDER_SUPPORT

/* Support Deflate compression */
#define ZIP_SUPPORT 1

/* Support strip chopping (whether or not to convert single-strip uncompressed
   images to mutiple strips of ~8Kb to reduce memory usage) */
#undef STRIPCHOP_DEFAULT

   /* Enable SubIFD tag (330) support */
#undef SUBIFD_SUPPORT

/* Treat extra sample as alpha (default enabled). The RGBA interface will
   treat a fourth sample with no EXTRASAMPLE_ value as being ASSOCALPHA. Many
   packages produce RGBA files but don't mark the alpha properly. */
#undef DEFAULT_EXTRASAMPLE_AS_ALPHA

   /* Pick up YCbCr subsampling info from the JPEG data stream to support files
      lacking the tag (default enabled). */
#undef CHECK_JPEG_YCBCR_SUBSAMPLING

      /* Support MS MDI magic number files as TIFF */
#undef MDI_SUPPORT

/* Visual Studio 2015 / VC 14 / MSVC 19.00 finally has snprintf() */
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#else
#define HAVE_SNPRINTF 1
#endif

//#define lfind _lfind

#pragma warning(disable : 4996) /* function deprecation warnings */

#define COLORIMETRY_SUPPORT
#define YCBCR_SUPPORT
#define CMYK_SUPPORT
#define ICC_SUPPORT
#define PHOTOSHOP_SUPPORT
#define IPTC_SUPPORT

#endif /* _TIF_CONFIG_H_ */

