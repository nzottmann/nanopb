/* Common parts of the nanopb library. Most of these are quite low-level
 * stuff. For the high-level interface, see usr_pb_encode.h and usr_pb_decode.h.
 */

#ifndef usr_PB_H_INCLUDED
#define usr_PB_H_INCLUDED

/*****************************************************************
 * Nanopb compilation time options. You can change these here by *
 * uncommenting the lines, or on the compiler command line.      *
 *****************************************************************/

/* Enable support for dynamically allocated fields */
/* #define usr_PB_ENABLE_MALLOC 1 */

/* Define this if your CPU / compiler combination does not support
 * unaligned memory access to packed structures. */
/* #define usr_PB_NO_PACKED_STRUCTS 1 */

/* Increase the number of required fields that are tracked.
 * A compiler warning will tell if you need this. */
/* #define usr_PB_MAX_REQUIRED_FIELDS 256 */

/* Add support for tag numbers > 65536 and fields larger than 65536 bytes. */
/* #define usr_PB_FIELD_32BIT 1 */

/* Disable support for error messages in order to save some code space. */
/* #define usr_PB_NO_ERRMSG 1 */

/* Disable support for custom streams (support only memory buffers). */
/* #define usr_PB_BUFFER_ONLY 1 */

/* Disable support for 64-bit datatypes, for compilers without int64_t
   or to save some code space. */
/* #define usr_PB_WITHOUT_64BIT 1 */

/* Don't encode scalar arrays as packed. This is only to be used when
 * the decoder on the receiving side cannot process packed scalar arrays.
 * Such example is older protobuf.js. */
/* #define usr_PB_ENCODE_ARRAYS_UNPACKED 1 */

/* Enable conversion of doubles to floats for platforms that do not
 * support 64-bit doubles. Most commonly AVR. */
/* #define usr_PB_CONVERT_DOUBLE_FLOAT 1 */

/* Check whether incoming strings are valid UTF-8 sequences. Slows down
 * the string processing slightly and slightly increases code size. */
/* #define usr_PB_VALIDATE_UTF8 1 */

/******************************************************************
 * You usually don't need to change anything below this line.     *
 * Feel free to look around and use the defined macros, though.   *
 ******************************************************************/


/* Version of the nanopb library. Just in case you want to check it in
 * your own program. */
#define NANOusr_PB_VERSION nanopb-0.4.6-dev

/* Include all the system headers needed by nanopb. You will need the
 * definitions of the following:
 * - strlen, memcpy, memset functions
 * - [u]int_least8_t, uint_fast8_t, [u]int_least16_t, [u]int32_t, [u]int64_t
 * - size_t
 * - bool
 *
 * If you don't have the standard header files, you can instead provide
 * a custom header that defines or includes all this. In that case,
 * define usr_PB_SYSTEM_HEADER to the path of this file.
 */
#ifdef usr_PB_SYSTEM_HEADER
#include usr_PB_SYSTEM_HEADER
#else
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#ifdef usr_PB_ENABLE_MALLOC
#include <stdlib.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Macro for defining packed structures (compiler dependent).
 * This just reduces memory requirements, but is not required.
 */
#if defined(usr_PB_NO_PACKED_STRUCTS)
    /* Disable struct packing */
#   define usr_PB_PACKED_STRUCT_START
#   define usr_PB_PACKED_STRUCT_END
#   define usr_pb_packed
#elif defined(__GNUC__) || defined(__clang__)
    /* For GCC and clang */
#   define usr_PB_PACKED_STRUCT_START
#   define usr_PB_PACKED_STRUCT_END
#   define usr_pb_packed __attribute__((packed))
#elif defined(__ICCARM__) || defined(__CC_ARM)
    /* For IAR ARM and Keil MDK-ARM compilers */
#   define usr_PB_PACKED_STRUCT_START _Pragma("pack(push, 1)")
#   define usr_PB_PACKED_STRUCT_END _Pragma("pack(pop)")
#   define usr_pb_packed
#elif defined(_MSC_VER) && (_MSC_VER >= 1500)
    /* For Microsoft Visual C++ */
#   define usr_PB_PACKED_STRUCT_START __pragma(pack(push, 1))
#   define usr_PB_PACKED_STRUCT_END __pragma(pack(pop))
#   define usr_pb_packed
#else
    /* Unknown compiler */
#   define usr_PB_PACKED_STRUCT_START
#   define usr_PB_PACKED_STRUCT_END
#   define usr_pb_packed
#endif

/* Handly macro for suppressing unreferenced-parameter compiler warnings. */
#ifndef usr_PB_UNUSED
#define usr_PB_UNUSED(x) (void)(x)
#endif

/* Harvard-architecture processors may need special attributes for storing
 * field information in program memory. */
#ifndef usr_PB_PROGMEM
#ifdef __AVR__
#include <avr/pgmspace.h>
#define usr_PB_PROGMEM             PROGMEM
#define usr_PB_PROGMEM_READU32(x)  pgm_read_dword(&x)
#else
#define usr_PB_PROGMEM
#define usr_PB_PROGMEM_READU32(x)  (x)
#endif
#endif

/* Compile-time assertion, used for checking compatible compilation options.
 * If this does not work properly on your compiler, use
 * #define usr_PB_NO_STATIC_ASSERT to disable it.
 *
 * But before doing that, check carefully the error message / place where it
 * comes from to see if the error has a real cause. Unfortunately the error
 * message is not always very clear to read, but you can see the reason better
 * in the place where the usr_PB_STATIC_ASSERT macro was called.
 */
#ifndef usr_PB_NO_STATIC_ASSERT
#  ifndef usr_PB_STATIC_ASSERT
#    if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
       /* C11 standard _Static_assert mechanism */
#      define usr_PB_STATIC_ASSERT(COND,MSG) _Static_assert(COND,#MSG);
#    else
       /* Classic negative-size-array static assert mechanism */
#      define usr_PB_STATIC_ASSERT(COND,MSG) typedef char usr_PB_STATIC_ASSERT_MSG(MSG, __LINE__, __COUNTER__)[(COND)?1:-1];
#      define usr_PB_STATIC_ASSERT_MSG(MSG, LINE, COUNTER) usr_PB_STATIC_ASSERT_MSG_(MSG, LINE, COUNTER)
#      define usr_PB_STATIC_ASSERT_MSG_(MSG, LINE, COUNTER) usr_pb_static_assertion_##MSG##_##LINE##_##COUNTER
#    endif
#  endif
#else
   /* Static asserts disabled by usr_PB_NO_STATIC_ASSERT */
#  define usr_PB_STATIC_ASSERT(COND,MSG)
#endif

/* Number of required fields to keep track of. */
#ifndef usr_PB_MAX_REQUIRED_FIELDS
#define usr_PB_MAX_REQUIRED_FIELDS 64
#endif

#if usr_PB_MAX_REQUIRED_FIELDS < 64
#error You should not lower usr_PB_MAX_REQUIRED_FIELDS from the default value (64).
#endif

#ifdef usr_PB_WITHOUT_64BIT
#ifdef usr_PB_CONVERT_DOUBLE_FLOAT
/* Cannot use doubles without 64-bit types */
#undef usr_PB_CONVERT_DOUBLE_FLOAT
#endif
#endif

/* List of possible field types. These are used in the autogenerated code.
 * Least-significant 4 bits tell the scalar type
 * Most-significant 4 bits specify repeated/required/packed etc.
 */

typedef uint_least8_t usr_pb_type_t;

/**** Field data types ****/

/* Numeric types */
#define usr_PB_LTYPE_BOOL    0x00U /* bool */
#define usr_PB_LTYPE_VARINT  0x01U /* int32, int64, enum, bool */
#define usr_PB_LTYPE_UVARINT 0x02U /* uint32, uint64 */
#define usr_PB_LTYPE_SVARINT 0x03U /* sint32, sint64 */
#define usr_PB_LTYPE_FIXED32 0x04U /* fixed32, sfixed32, float */
#define usr_PB_LTYPE_FIXED64 0x05U /* fixed64, sfixed64, double */

/* Marker for last packable field type. */
#define usr_PB_LTYPE_LAST_PACKABLE 0x05U

/* Byte array with pre-allocated buffer.
 * data_size is the length of the allocated usr_PB_BYTES_ARRAY structure. */
#define usr_PB_LTYPE_BYTES 0x06U

/* String with pre-allocated buffer.
 * data_size is the maximum length. */
#define usr_PB_LTYPE_STRING 0x07U

/* Submessage
 * submsg_fields is pointer to field descriptions */
#define usr_PB_LTYPE_SUBMESSAGE 0x08U

/* Submessage with pre-decoding callback
 * The pre-decoding callback is stored as usr_pb_callback_t right before pSize.
 * submsg_fields is pointer to field descriptions */
#define usr_PB_LTYPE_SUBMSG_W_CB 0x09U

/* Extension pseudo-field
 * The field contains a pointer to usr_pb_extension_t */
#define usr_PB_LTYPE_EXTENSION 0x0AU

/* Byte array with inline, pre-allocated byffer.
 * data_size is the length of the inline, allocated buffer.
 * This differs from usr_PB_LTYPE_BYTES by defining the element as
 * usr_pb_byte_t[data_size] rather than usr_pb_bytes_array_t. */
#define usr_PB_LTYPE_FIXED_LENGTH_BYTES 0x0BU

/* Number of declared LTYPES */
#define usr_PB_LTYPES_COUNT 0x0CU
#define usr_PB_LTYPE_MASK 0x0FU

/**** Field repetition rules ****/

#define usr_PB_HTYPE_REQUIRED 0x00U
#define usr_PB_HTYPE_OPTIONAL 0x10U
#define usr_PB_HTYPE_SINGULAR 0x10U
#define usr_PB_HTYPE_REPEATED 0x20U
#define usr_PB_HTYPE_FIXARRAY 0x20U
#define usr_PB_HTYPE_ONEOF    0x30U
#define usr_PB_HTYPE_MASK     0x30U

/**** Field allocation types ****/
 
#define usr_PB_ATYPE_STATIC   0x00U
#define usr_PB_ATYPE_POINTER  0x80U
#define usr_PB_ATYPE_CALLBACK 0x40U
#define usr_PB_ATYPE_MASK     0xC0U

#define usr_PB_ATYPE(x) ((x) & usr_PB_ATYPE_MASK)
#define usr_PB_HTYPE(x) ((x) & usr_PB_HTYPE_MASK)
#define usr_PB_LTYPE(x) ((x) & usr_PB_LTYPE_MASK)
#define usr_PB_LTYPE_IS_SUBMSG(x) (usr_PB_LTYPE(x) == usr_PB_LTYPE_SUBMESSAGE || \
                               usr_PB_LTYPE(x) == usr_PB_LTYPE_SUBMSG_W_CB)

/* Data type used for storing sizes of struct fields
 * and array counts.
 */
#if defined(usr_PB_FIELD_32BIT)
    typedef uint32_t usr_pb_size_t;
    typedef int32_t usr_pb_ssize_t;
#else
    typedef uint_least16_t usr_pb_size_t;
    typedef int_least16_t usr_pb_ssize_t;
#endif
#define usr_PB_SIZE_MAX ((usr_pb_size_t)-1)

/* Data type for storing encoded data and other byte streams.
 * This typedef exists to support platforms where uint8_t does not exist.
 * You can regard it as equivalent on uint8_t on other platforms.
 */
typedef uint_least8_t usr_pb_byte_t;

/* Forward declaration of struct types */
typedef struct usr_pb_istream_s usr_pb_istream_t;
typedef struct usr_pb_ostream_s usr_pb_ostream_t;
typedef struct usr_pb_field_iter_s usr_pb_field_iter_t;

/* This structure is used in auto-generated constants
 * to specify struct fields.
 */
typedef struct usr_pb_msgdesc_s usr_pb_msgdesc_t;
struct usr_pb_msgdesc_s {
    const uint32_t *field_info;
    const usr_pb_msgdesc_t * const * submsg_info;
    const usr_pb_byte_t *default_value;

    bool (*field_callback)(usr_pb_istream_t *istream, usr_pb_ostream_t *ostream, const usr_pb_field_iter_t *field);

    usr_pb_size_t field_count;
    usr_pb_size_t required_field_count;
    usr_pb_size_t largest_tag;
};

/* Iterator for message descriptor */
struct usr_pb_field_iter_s {
    const usr_pb_msgdesc_t *descriptor;  /* Pointer to message descriptor constant */
    void *message;                   /* Pointer to start of the structure */

    usr_pb_size_t index;                 /* Index of the field */
    usr_pb_size_t field_info_index;      /* Index to descriptor->field_info array */
    usr_pb_size_t required_field_index;  /* Index that counts only the required fields */
    usr_pb_size_t submessage_index;      /* Index that counts only submessages */

    usr_pb_size_t tag;                   /* Tag of current field */
    usr_pb_size_t data_size;             /* sizeof() of a single item */
    usr_pb_size_t array_size;            /* Number of array entries */
    usr_pb_type_t type;                  /* Type of current field */

    void *pField;                    /* Pointer to current field in struct */
    void *pData;                     /* Pointer to current data contents. Different than pField for arrays and pointers. */
    void *pSize;                     /* Pointer to count/has field */

    const usr_pb_msgdesc_t *submsg_desc; /* For submessage fields, pointer to field descriptor for the submessage. */
};

/* For compatibility with legacy code */
typedef usr_pb_field_iter_t usr_pb_field_t;

/* Make sure that the standard integer types are of the expected sizes.
 * Otherwise fixed32/fixed64 fields can break.
 *
 * If you get errors here, it probably means that your stdint.h is not
 * correct for your platform.
 */
#ifndef usr_PB_WITHOUT_64BIT
usr_PB_STATIC_ASSERT(sizeof(int64_t) == 2 * sizeof(int32_t), INT64_T_WRONG_SIZE)
usr_PB_STATIC_ASSERT(sizeof(uint64_t) == 2 * sizeof(uint32_t), UINT64_T_WRONG_SIZE)
#endif

/* This structure is used for 'bytes' arrays.
 * It has the number of bytes in the beginning, and after that an array.
 * Note that actual structs used will have a different length of bytes array.
 */
#define usr_PB_BYTES_ARRAY_T(n) struct { usr_pb_size_t size; usr_pb_byte_t bytes[n]; }
#define usr_PB_BYTES_ARRAY_T_ALLOCSIZE(n) ((size_t)n + offsetof(usr_pb_bytes_array_t, bytes))

struct usr_pb_bytes_array_s {
    usr_pb_size_t size;
    usr_pb_byte_t bytes[1];
};
typedef struct usr_pb_bytes_array_s usr_pb_bytes_array_t;

/* This structure is used for giving the callback function.
 * It is stored in the message structure and filled in by the method that
 * calls usr_pb_decode.
 *
 * The decoding callback will be given a limited-length stream
 * If the wire type was string, the length is the length of the string.
 * If the wire type was a varint/fixed32/fixed64, the length is the length
 * of the actual value.
 * The function may be called multiple times (especially for repeated types,
 * but also otherwise if the message happens to contain the field multiple
 * times.)
 *
 * The encoding callback will receive the actual output stream.
 * It should write all the data in one call, including the field tag and
 * wire type. It can write multiple fields.
 *
 * The callback can be null if you want to skip a field.
 */
typedef struct usr_pb_callback_s usr_pb_callback_t;
struct usr_pb_callback_s {
    /* Callback functions receive a pointer to the arg field.
     * You can access the value of the field as *arg, and modify it if needed.
     */
    union {
        bool (*decode)(usr_pb_istream_t *stream, const usr_pb_field_t *field, void **arg);
        bool (*encode)(usr_pb_ostream_t *stream, const usr_pb_field_t *field, void * const *arg);
    } funcs;
    
    /* Free arg for use by callback */
    void *arg;
};

extern bool usr_pb_default_field_callback(usr_pb_istream_t *istream, usr_pb_ostream_t *ostream, const usr_pb_field_t *field);

/* Wire types. Library user needs these only in encoder callbacks. */
typedef enum {
    usr_PB_WT_VARINT = 0,
    usr_PB_WT_64BIT  = 1,
    usr_PB_WT_STRING = 2,
    usr_PB_WT_32BIT  = 5
} usr_pb_wire_type_t;

/* Structure for defining the handling of unknown/extension fields.
 * Usually the usr_pb_extension_type_t structure is automatically generated,
 * while the usr_pb_extension_t structure is created by the user. However,
 * if you want to catch all unknown fields, you can also create a custom
 * usr_pb_extension_type_t with your own callback.
 */
typedef struct usr_pb_extension_type_s usr_pb_extension_type_t;
typedef struct usr_pb_extension_s usr_pb_extension_t;
struct usr_pb_extension_type_s {
    /* Called for each unknown field in the message.
     * If you handle the field, read off all of its data and return true.
     * If you do not handle the field, do not read anything and return true.
     * If you run into an error, return false.
     * Set to NULL for default handler.
     */
    bool (*decode)(usr_pb_istream_t *stream, usr_pb_extension_t *extension,
                   uint32_t tag, usr_pb_wire_type_t wire_type);
    
    /* Called once after all regular fields have been encoded.
     * If you have something to write, do so and return true.
     * If you do not have anything to write, just return true.
     * If you run into an error, return false.
     * Set to NULL for default handler.
     */
    bool (*encode)(usr_pb_ostream_t *stream, const usr_pb_extension_t *extension);
    
    /* Free field for use by the callback. */
    const void *arg;
};

struct usr_pb_extension_s {
    /* Type describing the extension field. Usually you'll initialize
     * this to a pointer to the automatically generated structure. */
    const usr_pb_extension_type_t *type;
    
    /* Destination for the decoded data. This must match the datatype
     * of the extension field. */
    void *dest;
    
    /* Pointer to the next extension handler, or NULL.
     * If this extension does not match a field, the next handler is
     * automatically called. */
    usr_pb_extension_t *next;

    /* The decoder sets this to true if the extension was found.
     * Ignored for encoding. */
    bool found;
};

#define usr_pb_extension_init_zero {NULL,NULL,NULL,false}

/* Memory allocation functions to use. You can define usr_pb_realloc and
 * usr_pb_free to custom functions if you want. */
#ifdef usr_PB_ENABLE_MALLOC
#   ifndef usr_pb_realloc
#       define usr_pb_realloc(ptr, size) realloc(ptr, size)
#   endif
#   ifndef usr_pb_free
#       define usr_pb_free(ptr) free(ptr)
#   endif
#endif

/* This is used to inform about need to regenerate .pb.h/.pb.c files. */
#define usr_PB_PROTO_HEADER_VERSION 40

/* These macros are used to declare usr_pb_field_t's in the constant array. */
/* Size of a structure member, in bytes. */
#define usr_pb_membersize(st, m) (sizeof ((st*)0)->m)
/* Number of entries in an array. */
#define usr_pb_arraysize(st, m) (usr_pb_membersize(st, m) / usr_pb_membersize(st, m[0]))
/* Delta from start of one member to the start of another member. */
#define usr_pb_delta(st, m1, m2) ((int)offsetof(st, m1) - (int)offsetof(st, m2))

/* Force expansion of macro value */
#define usr_PB_EXPAND(x) x

/* Binding of a message field set into a specific structure */
#define usr_PB_BIND(msgname, structname, width) \
    const uint32_t structname ## _field_info[] usr_PB_PROGMEM = \
    { \
        msgname ## _FIELDLIST(usr_PB_GEN_FIELD_INFO_ ## width, structname) \
        0 \
    }; \
    const usr_pb_msgdesc_t* const structname ## _submsg_info[] = \
    { \
        msgname ## _FIELDLIST(usr_PB_GEN_SUBMSG_INFO, structname) \
        NULL \
    }; \
    const usr_pb_msgdesc_t structname ## _msg = \
    { \
       structname ## _field_info, \
       structname ## _submsg_info, \
       msgname ## _DEFAULT, \
       msgname ## _CALLBACK, \
       0 msgname ## _FIELDLIST(usr_PB_GEN_FIELD_COUNT, structname), \
       0 msgname ## _FIELDLIST(usr_PB_GEN_REQ_FIELD_COUNT, structname), \
       0 msgname ## _FIELDLIST(usr_PB_GEN_LARGEST_TAG, structname), \
    }; \
    msgname ## _FIELDLIST(usr_PB_GEN_FIELD_INFO_ASSERT_ ## width, structname)

#define usr_PB_GEN_FIELD_COUNT(structname, atype, htype, ltype, fieldname, tag) +1
#define usr_PB_GEN_REQ_FIELD_COUNT(structname, atype, htype, ltype, fieldname, tag) \
    + (usr_PB_HTYPE_ ## htype == usr_PB_HTYPE_REQUIRED)
#define usr_PB_GEN_LARGEST_TAG(structname, atype, htype, ltype, fieldname, tag) \
    * 0 + tag

/* X-macro for generating the entries in struct_field_info[] array. */
#define usr_PB_GEN_FIELD_INFO_1(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_1(tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_GEN_FIELD_INFO_2(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_2(tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_GEN_FIELD_INFO_4(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_4(tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_GEN_FIELD_INFO_8(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_8(tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_GEN_FIELD_INFO_AUTO(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_AUTO2(usr_PB_FIELDINFO_WIDTH_AUTO(_usr_PB_ATYPE_ ## atype, _usr_PB_HTYPE_ ## htype, _usr_PB_LTYPE_ ## ltype), \
                   tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_FIELDINFO_AUTO2(width, tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_FIELDINFO_AUTO3(width, tag, type, data_offset, data_size, size_offset, array_size)

#define usr_PB_FIELDINFO_AUTO3(width, tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_FIELDINFO_ ## width(tag, type, data_offset, data_size, size_offset, array_size)

/* X-macro for generating asserts that entries fit in struct_field_info[] array.
 * The structure of macros here must match the structure above in usr_PB_GEN_FIELD_INFO_x(),
 * but it is not easily reused because of how macro substitutions work. */
#define usr_PB_GEN_FIELD_INFO_ASSERT_1(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_ASSERT_1(tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_GEN_FIELD_INFO_ASSERT_2(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_ASSERT_2(tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_GEN_FIELD_INFO_ASSERT_4(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_ASSERT_4(tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_GEN_FIELD_INFO_ASSERT_8(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_ASSERT_8(tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_GEN_FIELD_INFO_ASSERT_AUTO(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_FIELDINFO_ASSERT_AUTO2(usr_PB_FIELDINFO_WIDTH_AUTO(_usr_PB_ATYPE_ ## atype, _usr_PB_HTYPE_ ## htype, _usr_PB_LTYPE_ ## ltype), \
                   tag, usr_PB_ATYPE_ ## atype | usr_PB_HTYPE_ ## htype | usr_PB_LTYPE_MAP_ ## ltype, \
                   usr_PB_DATA_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_DATA_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_SIZE_OFFSET_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname), \
                   usr_PB_ARRAY_SIZE_ ## atype(_usr_PB_HTYPE_ ## htype, structname, fieldname))

#define usr_PB_FIELDINFO_ASSERT_AUTO2(width, tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_FIELDINFO_ASSERT_AUTO3(width, tag, type, data_offset, data_size, size_offset, array_size)

#define usr_PB_FIELDINFO_ASSERT_AUTO3(width, tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_FIELDINFO_ASSERT_ ## width(tag, type, data_offset, data_size, size_offset, array_size)

#define usr_PB_DATA_OFFSET_STATIC(htype, structname, fieldname) usr_PB_DO ## htype(structname, fieldname)
#define usr_PB_DATA_OFFSET_POINTER(htype, structname, fieldname) usr_PB_DO ## htype(structname, fieldname)
#define usr_PB_DATA_OFFSET_CALLBACK(htype, structname, fieldname) usr_PB_DO ## htype(structname, fieldname)
#define usr_PB_DO_usr_PB_HTYPE_REQUIRED(structname, fieldname) offsetof(structname, fieldname)
#define usr_PB_DO_usr_PB_HTYPE_SINGULAR(structname, fieldname) offsetof(structname, fieldname)
#define usr_PB_DO_usr_PB_HTYPE_ONEOF(structname, fieldname) offsetof(structname, usr_PB_ONEOF_NAME(FULL, fieldname))
#define usr_PB_DO_usr_PB_HTYPE_OPTIONAL(structname, fieldname) offsetof(structname, fieldname)
#define usr_PB_DO_usr_PB_HTYPE_REPEATED(structname, fieldname) offsetof(structname, fieldname)
#define usr_PB_DO_usr_PB_HTYPE_FIXARRAY(structname, fieldname) offsetof(structname, fieldname)

#define usr_PB_SIZE_OFFSET_STATIC(htype, structname, fieldname) usr_PB_SO ## htype(structname, fieldname)
#define usr_PB_SIZE_OFFSET_POINTER(htype, structname, fieldname) usr_PB_SO_PTR ## htype(structname, fieldname)
#define usr_PB_SIZE_OFFSET_CALLBACK(htype, structname, fieldname) usr_PB_SO_CB ## htype(structname, fieldname)
#define usr_PB_SO_usr_PB_HTYPE_REQUIRED(structname, fieldname) 0
#define usr_PB_SO_usr_PB_HTYPE_SINGULAR(structname, fieldname) 0
#define usr_PB_SO_usr_PB_HTYPE_ONEOF(structname, fieldname) usr_PB_SO_usr_PB_HTYPE_ONEOF2(structname, usr_PB_ONEOF_NAME(FULL, fieldname), usr_PB_ONEOF_NAME(UNION, fieldname))
#define usr_PB_SO_usr_PB_HTYPE_ONEOF2(structname, fullname, unionname) usr_PB_SO_usr_PB_HTYPE_ONEOF3(structname, fullname, unionname)
#define usr_PB_SO_usr_PB_HTYPE_ONEOF3(structname, fullname, unionname) usr_pb_delta(structname, fullname, which_ ## unionname)
#define usr_PB_SO_usr_PB_HTYPE_OPTIONAL(structname, fieldname) usr_pb_delta(structname, fieldname, has_ ## fieldname)
#define usr_PB_SO_usr_PB_HTYPE_REPEATED(structname, fieldname) usr_pb_delta(structname, fieldname, fieldname ## _count)
#define usr_PB_SO_usr_PB_HTYPE_FIXARRAY(structname, fieldname) 0
#define usr_PB_SO_PTR_usr_PB_HTYPE_REQUIRED(structname, fieldname) 0
#define usr_PB_SO_PTR_usr_PB_HTYPE_SINGULAR(structname, fieldname) 0
#define usr_PB_SO_PTR_usr_PB_HTYPE_ONEOF(structname, fieldname) usr_PB_SO_usr_PB_HTYPE_ONEOF(structname, fieldname)
#define usr_PB_SO_PTR_usr_PB_HTYPE_OPTIONAL(structname, fieldname) 0
#define usr_PB_SO_PTR_usr_PB_HTYPE_REPEATED(structname, fieldname) usr_PB_SO_usr_PB_HTYPE_REPEATED(structname, fieldname)
#define usr_PB_SO_PTR_usr_PB_HTYPE_FIXARRAY(structname, fieldname) 0
#define usr_PB_SO_CB_usr_PB_HTYPE_REQUIRED(structname, fieldname) 0
#define usr_PB_SO_CB_usr_PB_HTYPE_SINGULAR(structname, fieldname) 0
#define usr_PB_SO_CB_usr_PB_HTYPE_ONEOF(structname, fieldname) usr_PB_SO_usr_PB_HTYPE_ONEOF(structname, fieldname)
#define usr_PB_SO_CB_usr_PB_HTYPE_OPTIONAL(structname, fieldname) 0
#define usr_PB_SO_CB_usr_PB_HTYPE_REPEATED(structname, fieldname) 0
#define usr_PB_SO_CB_usr_PB_HTYPE_FIXARRAY(structname, fieldname) 0

#define usr_PB_ARRAY_SIZE_STATIC(htype, structname, fieldname) usr_PB_AS ## htype(structname, fieldname)
#define usr_PB_ARRAY_SIZE_POINTER(htype, structname, fieldname) usr_PB_AS_PTR ## htype(structname, fieldname)
#define usr_PB_ARRAY_SIZE_CALLBACK(htype, structname, fieldname) 1
#define usr_PB_AS_usr_PB_HTYPE_REQUIRED(structname, fieldname) 1
#define usr_PB_AS_usr_PB_HTYPE_SINGULAR(structname, fieldname) 1
#define usr_PB_AS_usr_PB_HTYPE_OPTIONAL(structname, fieldname) 1
#define usr_PB_AS_usr_PB_HTYPE_ONEOF(structname, fieldname) 1
#define usr_PB_AS_usr_PB_HTYPE_REPEATED(structname, fieldname) usr_pb_arraysize(structname, fieldname)
#define usr_PB_AS_usr_PB_HTYPE_FIXARRAY(structname, fieldname) usr_pb_arraysize(structname, fieldname)
#define usr_PB_AS_PTR_usr_PB_HTYPE_REQUIRED(structname, fieldname) 1
#define usr_PB_AS_PTR_usr_PB_HTYPE_SINGULAR(structname, fieldname) 1
#define usr_PB_AS_PTR_usr_PB_HTYPE_OPTIONAL(structname, fieldname) 1
#define usr_PB_AS_PTR_usr_PB_HTYPE_ONEOF(structname, fieldname) 1
#define usr_PB_AS_PTR_usr_PB_HTYPE_REPEATED(structname, fieldname) 1
#define usr_PB_AS_PTR_usr_PB_HTYPE_FIXARRAY(structname, fieldname) usr_pb_arraysize(structname, fieldname[0])

#define usr_PB_DATA_SIZE_STATIC(htype, structname, fieldname) usr_PB_DS ## htype(structname, fieldname)
#define usr_PB_DATA_SIZE_POINTER(htype, structname, fieldname) usr_PB_DS_PTR ## htype(structname, fieldname)
#define usr_PB_DATA_SIZE_CALLBACK(htype, structname, fieldname) usr_PB_DS_CB ## htype(structname, fieldname)
#define usr_PB_DS_usr_PB_HTYPE_REQUIRED(structname, fieldname) usr_pb_membersize(structname, fieldname)
#define usr_PB_DS_usr_PB_HTYPE_SINGULAR(structname, fieldname) usr_pb_membersize(structname, fieldname)
#define usr_PB_DS_usr_PB_HTYPE_OPTIONAL(structname, fieldname) usr_pb_membersize(structname, fieldname)
#define usr_PB_DS_usr_PB_HTYPE_ONEOF(structname, fieldname) usr_pb_membersize(structname, usr_PB_ONEOF_NAME(FULL, fieldname))
#define usr_PB_DS_usr_PB_HTYPE_REPEATED(structname, fieldname) usr_pb_membersize(structname, fieldname[0])
#define usr_PB_DS_usr_PB_HTYPE_FIXARRAY(structname, fieldname) usr_pb_membersize(structname, fieldname[0])
#define usr_PB_DS_PTR_usr_PB_HTYPE_REQUIRED(structname, fieldname) usr_pb_membersize(structname, fieldname[0])
#define usr_PB_DS_PTR_usr_PB_HTYPE_SINGULAR(structname, fieldname) usr_pb_membersize(structname, fieldname[0])
#define usr_PB_DS_PTR_usr_PB_HTYPE_OPTIONAL(structname, fieldname) usr_pb_membersize(structname, fieldname[0])
#define usr_PB_DS_PTR_usr_PB_HTYPE_ONEOF(structname, fieldname) usr_pb_membersize(structname, usr_PB_ONEOF_NAME(FULL, fieldname)[0])
#define usr_PB_DS_PTR_usr_PB_HTYPE_REPEATED(structname, fieldname) usr_pb_membersize(structname, fieldname[0])
#define usr_PB_DS_PTR_usr_PB_HTYPE_FIXARRAY(structname, fieldname) usr_pb_membersize(structname, fieldname[0][0])
#define usr_PB_DS_CB_usr_PB_HTYPE_REQUIRED(structname, fieldname) usr_pb_membersize(structname, fieldname)
#define usr_PB_DS_CB_usr_PB_HTYPE_SINGULAR(structname, fieldname) usr_pb_membersize(structname, fieldname)
#define usr_PB_DS_CB_usr_PB_HTYPE_OPTIONAL(structname, fieldname) usr_pb_membersize(structname, fieldname)
#define usr_PB_DS_CB_usr_PB_HTYPE_ONEOF(structname, fieldname) usr_pb_membersize(structname, usr_PB_ONEOF_NAME(FULL, fieldname))
#define usr_PB_DS_CB_usr_PB_HTYPE_REPEATED(structname, fieldname) usr_pb_membersize(structname, fieldname)
#define usr_PB_DS_CB_usr_PB_HTYPE_FIXARRAY(structname, fieldname) usr_pb_membersize(structname, fieldname)

#define usr_PB_ONEOF_NAME(type, tuple) usr_PB_EXPAND(usr_PB_ONEOF_NAME_ ## type tuple)
#define usr_PB_ONEOF_NAME_UNION(unionname,membername,fullname) unionname
#define usr_PB_ONEOF_NAME_MEMBER(unionname,membername,fullname) membername
#define usr_PB_ONEOF_NAME_FULL(unionname,membername,fullname) fullname

#define usr_PB_GEN_SUBMSG_INFO(structname, atype, htype, ltype, fieldname, tag) \
    usr_PB_SUBMSG_INFO_ ## htype(_usr_PB_LTYPE_ ## ltype, structname, fieldname)

#define usr_PB_SUBMSG_INFO_REQUIRED(ltype, structname, fieldname) usr_PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define usr_PB_SUBMSG_INFO_SINGULAR(ltype, structname, fieldname) usr_PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define usr_PB_SUBMSG_INFO_OPTIONAL(ltype, structname, fieldname) usr_PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define usr_PB_SUBMSG_INFO_ONEOF(ltype, structname, fieldname) usr_PB_SUBMSG_INFO_ONEOF2(ltype, structname, usr_PB_ONEOF_NAME(UNION, fieldname), usr_PB_ONEOF_NAME(MEMBER, fieldname))
#define usr_PB_SUBMSG_INFO_ONEOF2(ltype, structname, unionname, membername) usr_PB_SUBMSG_INFO_ONEOF3(ltype, structname, unionname, membername)
#define usr_PB_SUBMSG_INFO_ONEOF3(ltype, structname, unionname, membername) usr_PB_SI ## ltype(structname ## _ ## unionname ## _ ## membername ## _MSGTYPE)
#define usr_PB_SUBMSG_INFO_REPEATED(ltype, structname, fieldname) usr_PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define usr_PB_SUBMSG_INFO_FIXARRAY(ltype, structname, fieldname) usr_PB_SI ## ltype(structname ## _ ## fieldname ## _MSGTYPE)
#define usr_PB_SI_usr_PB_LTYPE_BOOL(t)
#define usr_PB_SI_usr_PB_LTYPE_BYTES(t)
#define usr_PB_SI_usr_PB_LTYPE_DOUBLE(t)
#define usr_PB_SI_usr_PB_LTYPE_ENUM(t)
#define usr_PB_SI_usr_PB_LTYPE_UENUM(t)
#define usr_PB_SI_usr_PB_LTYPE_FIXED32(t)
#define usr_PB_SI_usr_PB_LTYPE_FIXED64(t)
#define usr_PB_SI_usr_PB_LTYPE_FLOAT(t)
#define usr_PB_SI_usr_PB_LTYPE_INT32(t)
#define usr_PB_SI_usr_PB_LTYPE_INT64(t)
#define usr_PB_SI_usr_PB_LTYPE_MESSAGE(t)  usr_PB_SUBMSG_DESCRIPTOR(t)
#define usr_PB_SI_usr_PB_LTYPE_MSG_W_CB(t) usr_PB_SUBMSG_DESCRIPTOR(t)
#define usr_PB_SI_usr_PB_LTYPE_SFIXED32(t)
#define usr_PB_SI_usr_PB_LTYPE_SFIXED64(t)
#define usr_PB_SI_usr_PB_LTYPE_SINT32(t)
#define usr_PB_SI_usr_PB_LTYPE_SINT64(t)
#define usr_PB_SI_usr_PB_LTYPE_STRING(t)
#define usr_PB_SI_usr_PB_LTYPE_UINT32(t)
#define usr_PB_SI_usr_PB_LTYPE_UINT64(t)
#define usr_PB_SI_usr_PB_LTYPE_EXTENSION(t)
#define usr_PB_SI_usr_PB_LTYPE_FIXED_LENGTH_BYTES(t)
#define usr_PB_SUBMSG_DESCRIPTOR(t)    &(t ## _msg),

/* The field descriptors use a variable width format, with width of either
 * 1, 2, 4 or 8 of 32-bit words. The two lowest bytes of the first byte always
 * encode the descriptor size, 6 lowest bits of field tag number, and 8 bits
 * of the field type.
 *
 * Descriptor size is encoded as 0 = 1 word, 1 = 2 words, 2 = 4 words, 3 = 8 words.
 *
 * Formats, listed starting with the least significant bit of the first word.
 * 1 word:  [2-bit len] [6-bit tag] [8-bit type] [8-bit data_offset] [4-bit size_offset] [4-bit data_size]
 *
 * 2 words: [2-bit len] [6-bit tag] [8-bit type] [12-bit array_size] [4-bit size_offset]
 *          [16-bit data_offset] [12-bit data_size] [4-bit tag>>6]
 *
 * 4 words: [2-bit len] [6-bit tag] [8-bit type] [16-bit array_size]
 *          [8-bit size_offset] [24-bit tag>>6]
 *          [32-bit data_offset]
 *          [32-bit data_size]
 *
 * 8 words: [2-bit len] [6-bit tag] [8-bit type] [16-bit reserved]
 *          [8-bit size_offset] [24-bit tag>>6]
 *          [32-bit data_offset]
 *          [32-bit data_size]
 *          [32-bit array_size]
 *          [32-bit reserved]
 *          [32-bit reserved]
 *          [32-bit reserved]
 */

#define usr_PB_FIELDINFO_1(tag, type, data_offset, data_size, size_offset, array_size) \
    (0 | (((tag) << 2) & 0xFF) | ((type) << 8) | (((uint32_t)(data_offset) & 0xFF) << 16) | \
     (((uint32_t)(size_offset) & 0x0F) << 24) | (((uint32_t)(data_size) & 0x0F) << 28)),

#define usr_PB_FIELDINFO_2(tag, type, data_offset, data_size, size_offset, array_size) \
    (1 | (((tag) << 2) & 0xFF) | ((type) << 8) | (((uint32_t)(array_size) & 0xFFF) << 16) | (((uint32_t)(size_offset) & 0x0F) << 28)), \
    (((uint32_t)(data_offset) & 0xFFFF) | (((uint32_t)(data_size) & 0xFFF) << 16) | (((uint32_t)(tag) & 0x3c0) << 22)),

#define usr_PB_FIELDINFO_4(tag, type, data_offset, data_size, size_offset, array_size) \
    (2 | (((tag) << 2) & 0xFF) | ((type) << 8) | (((uint32_t)(array_size) & 0xFFFF) << 16)), \
    ((uint32_t)(int_least8_t)(size_offset) | (((uint32_t)(tag) << 2) & 0xFFFFFF00)), \
    (data_offset), (data_size),

#define usr_PB_FIELDINFO_8(tag, type, data_offset, data_size, size_offset, array_size) \
    (3 | (((tag) << 2) & 0xFF) | ((type) << 8)), \
    ((uint32_t)(int_least8_t)(size_offset) | (((uint32_t)(tag) << 2) & 0xFFFFFF00)), \
    (data_offset), (data_size), (array_size), 0, 0, 0,

/* These assertions verify that the field information fits in the allocated space.
 * The generator tries to automatically determine the correct width that can fit all
 * data associated with a message. These asserts will fail only if there has been a
 * problem in the automatic logic - this may be worth reporting as a bug. As a workaround,
 * you can increase the descriptor width by defining usr_PB_FIELDINFO_WIDTH or by setting
 * descriptorsize option in .options file.
 */
#define usr_PB_FITS(value,bits) ((uint32_t)(value) < ((uint32_t)1<<bits))
#define usr_PB_FIELDINFO_ASSERT_1(tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_STATIC_ASSERT(usr_PB_FITS(tag,6) && usr_PB_FITS(data_offset,8) && usr_PB_FITS(size_offset,4) && usr_PB_FITS(data_size,4) && usr_PB_FITS(array_size,1), FIELDINFO_DOES_NOT_FIT_width1_field ## tag)

#define usr_PB_FIELDINFO_ASSERT_2(tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_STATIC_ASSERT(usr_PB_FITS(tag,10) && usr_PB_FITS(data_offset,16) && usr_PB_FITS(size_offset,4) && usr_PB_FITS(data_size,12) && usr_PB_FITS(array_size,12), FIELDINFO_DOES_NOT_FIT_width2_field ## tag)

#ifndef usr_PB_FIELD_32BIT
/* Maximum field sizes are still 16-bit if usr_pb_size_t is 16-bit */
#define usr_PB_FIELDINFO_ASSERT_4(tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_STATIC_ASSERT(usr_PB_FITS(tag,16) && usr_PB_FITS(data_offset,16) && usr_PB_FITS((int_least8_t)size_offset,8) && usr_PB_FITS(data_size,16) && usr_PB_FITS(array_size,16), FIELDINFO_DOES_NOT_FIT_width4_field ## tag)

#define usr_PB_FIELDINFO_ASSERT_8(tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_STATIC_ASSERT(usr_PB_FITS(tag,16) && usr_PB_FITS(data_offset,16) && usr_PB_FITS((int_least8_t)size_offset,8) && usr_PB_FITS(data_size,16) && usr_PB_FITS(array_size,16), FIELDINFO_DOES_NOT_FIT_width8_field ## tag)
#else
/* Up to 32-bit fields supported.
 * Note that the checks are against 31 bits to avoid compiler warnings about shift wider than type in the test.
 * I expect that there is no reasonable use for >2GB messages with nanopb anyway.
 */
#define usr_PB_FIELDINFO_ASSERT_4(tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_STATIC_ASSERT(usr_PB_FITS(tag,30) && usr_PB_FITS(data_offset,31) && usr_PB_FITS(size_offset,8) && usr_PB_FITS(data_size,31) && usr_PB_FITS(array_size,16), FIELDINFO_DOES_NOT_FIT_width4_field ## tag)

#define usr_PB_FIELDINFO_ASSERT_8(tag, type, data_offset, data_size, size_offset, array_size) \
    usr_PB_STATIC_ASSERT(usr_PB_FITS(tag,30) && usr_PB_FITS(data_offset,31) && usr_PB_FITS(size_offset,8) && usr_PB_FITS(data_size,31) && usr_PB_FITS(array_size,31), FIELDINFO_DOES_NOT_FIT_width8_field ## tag)
#endif


/* Automatic picking of FIELDINFO width:
 * Uses width 1 when possible, otherwise resorts to width 2.
 * This is used when usr_PB_BIND() is called with "AUTO" as the argument.
 * The generator will give explicit size argument when it knows that a message
 * structure grows beyond 1-word format limits.
 */
#define usr_PB_FIELDINFO_WIDTH_AUTO(atype, htype, ltype) usr_PB_FI_WIDTH ## atype(htype, ltype)
#define usr_PB_FI_WIDTH_usr_PB_ATYPE_STATIC(htype, ltype) usr_PB_FI_WIDTH ## htype(ltype)
#define usr_PB_FI_WIDTH_usr_PB_ATYPE_POINTER(htype, ltype) usr_PB_FI_WIDTH ## htype(ltype)
#define usr_PB_FI_WIDTH_usr_PB_ATYPE_CALLBACK(htype, ltype) 2
#define usr_PB_FI_WIDTH_usr_PB_HTYPE_REQUIRED(ltype) usr_PB_FI_WIDTH ## ltype
#define usr_PB_FI_WIDTH_usr_PB_HTYPE_SINGULAR(ltype) usr_PB_FI_WIDTH ## ltype
#define usr_PB_FI_WIDTH_usr_PB_HTYPE_OPTIONAL(ltype) usr_PB_FI_WIDTH ## ltype
#define usr_PB_FI_WIDTH_usr_PB_HTYPE_ONEOF(ltype) usr_PB_FI_WIDTH ## ltype
#define usr_PB_FI_WIDTH_usr_PB_HTYPE_REPEATED(ltype) 2
#define usr_PB_FI_WIDTH_usr_PB_HTYPE_FIXARRAY(ltype) 2
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_BOOL      1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_BYTES     2
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_DOUBLE    1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_ENUM      1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_UENUM     1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_FIXED32   1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_FIXED64   1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_FLOAT     1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_INT32     1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_INT64     1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_MESSAGE   2
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_MSG_W_CB  2
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_SFIXED32  1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_SFIXED64  1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_SINT32    1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_SINT64    1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_STRING    2
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_UINT32    1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_UINT64    1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_EXTENSION 1
#define usr_PB_FI_WIDTH_usr_PB_LTYPE_FIXED_LENGTH_BYTES 2

/* The mapping from protobuf types to LTYPEs is done using these macros. */
#define usr_PB_LTYPE_MAP_BOOL               usr_PB_LTYPE_BOOL
#define usr_PB_LTYPE_MAP_BYTES              usr_PB_LTYPE_BYTES
#define usr_PB_LTYPE_MAP_DOUBLE             usr_PB_LTYPE_FIXED64
#define usr_PB_LTYPE_MAP_ENUM               usr_PB_LTYPE_VARINT
#define usr_PB_LTYPE_MAP_UENUM              usr_PB_LTYPE_UVARINT
#define usr_PB_LTYPE_MAP_FIXED32            usr_PB_LTYPE_FIXED32
#define usr_PB_LTYPE_MAP_FIXED64            usr_PB_LTYPE_FIXED64
#define usr_PB_LTYPE_MAP_FLOAT              usr_PB_LTYPE_FIXED32
#define usr_PB_LTYPE_MAP_INT32              usr_PB_LTYPE_VARINT
#define usr_PB_LTYPE_MAP_INT64              usr_PB_LTYPE_VARINT
#define usr_PB_LTYPE_MAP_MESSAGE            usr_PB_LTYPE_SUBMESSAGE
#define usr_PB_LTYPE_MAP_MSG_W_CB           usr_PB_LTYPE_SUBMSG_W_CB
#define usr_PB_LTYPE_MAP_SFIXED32           usr_PB_LTYPE_FIXED32
#define usr_PB_LTYPE_MAP_SFIXED64           usr_PB_LTYPE_FIXED64
#define usr_PB_LTYPE_MAP_SINT32             usr_PB_LTYPE_SVARINT
#define usr_PB_LTYPE_MAP_SINT64             usr_PB_LTYPE_SVARINT
#define usr_PB_LTYPE_MAP_STRING             usr_PB_LTYPE_STRING
#define usr_PB_LTYPE_MAP_UINT32             usr_PB_LTYPE_UVARINT
#define usr_PB_LTYPE_MAP_UINT64             usr_PB_LTYPE_UVARINT
#define usr_PB_LTYPE_MAP_EXTENSION          usr_PB_LTYPE_EXTENSION
#define usr_PB_LTYPE_MAP_FIXED_LENGTH_BYTES usr_PB_LTYPE_FIXED_LENGTH_BYTES

/* These macros are used for giving out error messages.
 * They are mostly a debugging aid; the main error information
 * is the true/false return value from functions.
 * Some code space can be saved by disabling the error
 * messages if not used.
 *
 * usr_PB_SET_ERROR() sets the error message if none has been set yet.
 *                msg must be a constant string literal.
 * usr_PB_GET_ERROR() always returns a pointer to a string.
 * usr_PB_RETURN_ERROR() sets the error and returns false from current
 *                   function.
 */
#ifdef usr_PB_NO_ERRMSG
#define usr_PB_SET_ERROR(stream, msg) usr_PB_UNUSED(stream)
#define usr_PB_GET_ERROR(stream) "(errmsg disabled)"
#else
#define usr_PB_SET_ERROR(stream, msg) (stream->errmsg = (stream)->errmsg ? (stream)->errmsg : (msg))
#define usr_PB_GET_ERROR(stream) ((stream)->errmsg ? (stream)->errmsg : "(none)")
#endif

#define usr_PB_RETURN_ERROR(stream, msg) return usr_PB_SET_ERROR(stream, msg), false

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
#if __cplusplus >= 201103L
#define usr_PB_CONSTEXPR constexpr
#else  // __cplusplus >= 201103L
#define usr_PB_CONSTEXPR
#endif  // __cplusplus >= 201103L

#if __cplusplus >= 201703L
#define usr_PB_INLINE_CONSTEXPR inline constexpr
#else  // __cplusplus >= 201703L
#define usr_PB_INLINE_CONSTEXPR usr_PB_CONSTEXPR
#endif  // __cplusplus >= 201703L

namespace nanopb {
// Each type will be partially specialized by the generator.
template <typename GenMessageT> struct MessageDescriptor;
}  // namespace nanopb
#endif  /* __cplusplus */

#endif
