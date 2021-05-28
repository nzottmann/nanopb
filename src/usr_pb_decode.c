/* usr_pb_decode.c -- decode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

/* Use the GCC warn_unused_result attribute to check that all return values
 * are propagated correctly. On other compilers and gcc before 3.4.0 just
 * ignore the annotation.
 */
#if !defined(__GNUC__) || ( __GNUC__ < 3) || (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
    #define checkreturn
#else
    #define checkreturn __attribute__((warn_unused_result))
#endif

#include "usr_pb.h"
#include "usr_pb_decode.h"
#include "usr_pb_common.h"

/**************************************
 * Declarations internal to this file *
 **************************************/

static bool checkreturn buf_read(usr_pb_istream_t *stream, usr_pb_byte_t *buf, size_t count);
static bool checkreturn usr_pb_decode_varint32_eof(usr_pb_istream_t *stream, uint32_t *dest, bool *eof);
static bool checkreturn read_raw_value(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_byte_t *buf, size_t *size);
static bool checkreturn decode_basic_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field);
static bool checkreturn decode_static_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field);
static bool checkreturn decode_pointer_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field);
static bool checkreturn decode_callback_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field);
static bool checkreturn decode_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field);
static bool checkreturn default_extension_decoder(usr_pb_istream_t *stream, usr_pb_extension_t *extension, uint32_t tag, usr_pb_wire_type_t wire_type);
static bool checkreturn decode_extension(usr_pb_istream_t *stream, uint32_t tag, usr_pb_wire_type_t wire_type, usr_pb_extension_t *extension);
static bool usr_pb_field_set_to_default(usr_pb_field_iter_t *field);
static bool usr_pb_message_set_to_defaults(usr_pb_field_iter_t *iter);
static bool checkreturn usr_pb_dec_bool(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field);
static bool checkreturn usr_pb_dec_varint(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field);
static bool checkreturn usr_pb_dec_bytes(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field);
static bool checkreturn usr_pb_dec_string(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field);
static bool checkreturn usr_pb_dec_submessage(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field);
static bool checkreturn usr_pb_dec_fixed_length_bytes(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field);
static bool checkreturn usr_pb_skip_varint(usr_pb_istream_t *stream);
static bool checkreturn usr_pb_skip_string(usr_pb_istream_t *stream);

#ifdef usr_PB_ENABLE_MALLOC
static bool checkreturn allocate_field(usr_pb_istream_t *stream, void *pData, size_t data_size, size_t array_size);
static void initialize_pointer_field(void *pItem, usr_pb_field_iter_t *field);
static bool checkreturn usr_pb_release_union_field(usr_pb_istream_t *stream, usr_pb_field_iter_t *field);
static void usr_pb_release_single_field(usr_pb_field_iter_t *field);
#endif

#ifdef usr_PB_WITHOUT_64BIT
#define usr_pb_int64_t int32_t
#define usr_pb_uint64_t uint32_t
#else
#define usr_pb_int64_t int64_t
#define usr_pb_uint64_t uint64_t
#endif

#define usr_PB_WT_PACKED ((usr_pb_wire_type_t)0xFF)

typedef struct {
    uint32_t bitfield[(usr_PB_MAX_REQUIRED_FIELDS + 31) / 32];
} usr_pb_fields_seen_t;

/*******************************
 * usr_pb_istream_t implementation *
 *******************************/

static bool checkreturn buf_read(usr_pb_istream_t *stream, usr_pb_byte_t *buf, size_t count)
{
    size_t i;
    const usr_pb_byte_t *source = (const usr_pb_byte_t*)stream->state;
    stream->state = (usr_pb_byte_t*)stream->state + count;
    
    if (buf != NULL)
    {
        for (i = 0; i < count; i++)
            buf[i] = source[i];
    }
    
    return true;
}

bool checkreturn usr_pb_read(usr_pb_istream_t *stream, usr_pb_byte_t *buf, size_t count)
{
    if (count == 0)
        return true;

#ifndef usr_PB_BUFFER_ONLY
	if (buf == NULL && stream->callback != buf_read)
	{
		/* Skip input bytes */
		usr_pb_byte_t tmp[16];
		while (count > 16)
		{
			if (!usr_pb_read(stream, tmp, 16))
				return false;
			
			count -= 16;
		}
		
		return usr_pb_read(stream, tmp, count);
	}
#endif

    if (stream->bytes_left < count)
        usr_PB_RETURN_ERROR(stream, "end-of-stream");
    
#ifndef usr_PB_BUFFER_ONLY
    if (!stream->callback(stream, buf, count))
        usr_PB_RETURN_ERROR(stream, "io error");
#else
    if (!buf_read(stream, buf, count))
        return false;
#endif
    
    stream->bytes_left -= count;
    return true;
}

/* Read a single byte from input stream. buf may not be NULL.
 * This is an optimization for the varint decoding. */
static bool checkreturn usr_pb_readbyte(usr_pb_istream_t *stream, usr_pb_byte_t *buf)
{
    if (stream->bytes_left == 0)
        usr_PB_RETURN_ERROR(stream, "end-of-stream");

#ifndef usr_PB_BUFFER_ONLY
    if (!stream->callback(stream, buf, 1))
        usr_PB_RETURN_ERROR(stream, "io error");
#else
    *buf = *(const usr_pb_byte_t*)stream->state;
    stream->state = (usr_pb_byte_t*)stream->state + 1;
#endif

    stream->bytes_left--;
    
    return true;    
}

usr_pb_istream_t usr_pb_istream_from_buffer(const usr_pb_byte_t *buf, size_t msglen)
{
    usr_pb_istream_t stream;
    /* Cast away the const from buf without a compiler error.  We are
     * careful to use it only in a const manner in the callbacks.
     */
    union {
        void *state;
        const void *c_state;
    } state;
#ifdef usr_PB_BUFFER_ONLY
    stream.callback = NULL;
#else
    stream.callback = &buf_read;
#endif
    state.c_state = buf;
    stream.state = state.state;
    stream.bytes_left = msglen;
#ifndef usr_PB_NO_ERRMSG
    stream.errmsg = NULL;
#endif
    return stream;
}

/********************
 * Helper functions *
 ********************/

static bool checkreturn usr_pb_decode_varint32_eof(usr_pb_istream_t *stream, uint32_t *dest, bool *eof)
{
    usr_pb_byte_t byte;
    uint32_t result;
    
    if (!usr_pb_readbyte(stream, &byte))
    {
        if (stream->bytes_left == 0)
        {
            if (eof)
            {
                *eof = true;
            }
        }

        return false;
    }
    
    if ((byte & 0x80) == 0)
    {
        /* Quick case, 1 byte value */
        result = byte;
    }
    else
    {
        /* Multibyte case */
        uint_fast8_t bitpos = 7;
        result = byte & 0x7F;
        
        do
        {
            if (!usr_pb_readbyte(stream, &byte))
                return false;
            
            if (bitpos >= 32)
            {
                /* Note: The varint could have trailing 0x80 bytes, or 0xFF for negative. */
                usr_pb_byte_t sign_extension = (bitpos < 63) ? 0xFF : 0x01;
                bool valid_extension = ((byte & 0x7F) == 0x00 ||
                         ((result >> 31) != 0 && byte == sign_extension));

                if (bitpos >= 64 || !valid_extension)
                {
                    usr_PB_RETURN_ERROR(stream, "varint overflow");
                }
            }
            else
            {
                result |= (uint32_t)(byte & 0x7F) << bitpos;
            }
            bitpos = (uint_fast8_t)(bitpos + 7);
        } while (byte & 0x80);
        
        if (bitpos == 35 && (byte & 0x70) != 0)
        {
            /* The last byte was at bitpos=28, so only bottom 4 bits fit. */
            usr_PB_RETURN_ERROR(stream, "varint overflow");
        }
   }
   
   *dest = result;
   return true;
}

bool checkreturn usr_pb_decode_varint32(usr_pb_istream_t *stream, uint32_t *dest)
{
    return usr_pb_decode_varint32_eof(stream, dest, NULL);
}

#ifndef usr_PB_WITHOUT_64BIT
bool checkreturn usr_pb_decode_varint(usr_pb_istream_t *stream, uint64_t *dest)
{
    usr_pb_byte_t byte;
    uint_fast8_t bitpos = 0;
    uint64_t result = 0;
    
    do
    {
        if (bitpos >= 64)
            usr_PB_RETURN_ERROR(stream, "varint overflow");
        
        if (!usr_pb_readbyte(stream, &byte))
            return false;

        result |= (uint64_t)(byte & 0x7F) << bitpos;
        bitpos = (uint_fast8_t)(bitpos + 7);
    } while (byte & 0x80);
    
    *dest = result;
    return true;
}
#endif

bool checkreturn usr_pb_skip_varint(usr_pb_istream_t *stream)
{
    usr_pb_byte_t byte;
    do
    {
        if (!usr_pb_read(stream, &byte, 1))
            return false;
    } while (byte & 0x80);
    return true;
}

bool checkreturn usr_pb_skip_string(usr_pb_istream_t *stream)
{
    uint32_t length;
    if (!usr_pb_decode_varint32(stream, &length))
        return false;
    
    if ((size_t)length != length)
    {
        usr_PB_RETURN_ERROR(stream, "size too large");
    }

    return usr_pb_read(stream, NULL, (size_t)length);
}

bool checkreturn usr_pb_decode_tag(usr_pb_istream_t *stream, usr_pb_wire_type_t *wire_type, uint32_t *tag, bool *eof)
{
    uint32_t temp;
    *eof = false;
    *wire_type = (usr_pb_wire_type_t) 0;
    *tag = 0;
    
    if (!usr_pb_decode_varint32_eof(stream, &temp, eof))
    {
        return false;
    }
    
    *tag = temp >> 3;
    *wire_type = (usr_pb_wire_type_t)(temp & 7);
    return true;
}

bool checkreturn usr_pb_skip_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type)
{
    switch (wire_type)
    {
        case usr_PB_WT_VARINT: return usr_pb_skip_varint(stream);
        case usr_PB_WT_64BIT: return usr_pb_read(stream, NULL, 8);
        case usr_PB_WT_STRING: return usr_pb_skip_string(stream);
        case usr_PB_WT_32BIT: return usr_pb_read(stream, NULL, 4);
        default: usr_PB_RETURN_ERROR(stream, "invalid wire_type");
    }
}

/* Read a raw value to buffer, for the purpose of passing it to callback as
 * a substream. Size is maximum size on call, and actual size on return.
 */
static bool checkreturn read_raw_value(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_byte_t *buf, size_t *size)
{
    size_t max_size = *size;
    switch (wire_type)
    {
        case usr_PB_WT_VARINT:
            *size = 0;
            do
            {
                (*size)++;
                if (*size > max_size)
                    usr_PB_RETURN_ERROR(stream, "varint overflow");

                if (!usr_pb_read(stream, buf, 1))
                    return false;
            } while (*buf++ & 0x80);
            return true;
            
        case usr_PB_WT_64BIT:
            *size = 8;
            return usr_pb_read(stream, buf, 8);
        
        case usr_PB_WT_32BIT:
            *size = 4;
            return usr_pb_read(stream, buf, 4);
        
        case usr_PB_WT_STRING:
            /* Calling read_raw_value with a usr_PB_WT_STRING is an error.
             * Explicitly handle this case and fallthrough to default to avoid
             * compiler warnings.
             */

        default: usr_PB_RETURN_ERROR(stream, "invalid wire_type");
    }
}

/* Decode string length from stream and return a substream with limited length.
 * Remember to close the substream using usr_pb_close_string_substream().
 */
bool checkreturn usr_pb_make_string_substream(usr_pb_istream_t *stream, usr_pb_istream_t *substream)
{
    uint32_t size;
    if (!usr_pb_decode_varint32(stream, &size))
        return false;
    
    *substream = *stream;
    if (substream->bytes_left < size)
        usr_PB_RETURN_ERROR(stream, "parent stream too short");
    
    substream->bytes_left = (size_t)size;
    stream->bytes_left -= (size_t)size;
    return true;
}

bool checkreturn usr_pb_close_string_substream(usr_pb_istream_t *stream, usr_pb_istream_t *substream)
{
    if (substream->bytes_left) {
        if (!usr_pb_read(substream, NULL, substream->bytes_left))
            return false;
    }

    stream->state = substream->state;

#ifndef usr_PB_NO_ERRMSG
    stream->errmsg = substream->errmsg;
#endif
    return true;
}

/*************************
 * Decode a single field *
 *************************/

static bool checkreturn decode_basic_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field)
{
    switch (usr_PB_LTYPE(field->type))
    {
        case usr_PB_LTYPE_BOOL:
            if (wire_type != usr_PB_WT_VARINT && wire_type != usr_PB_WT_PACKED)
                usr_PB_RETURN_ERROR(stream, "wrong wire type");

            return usr_pb_dec_bool(stream, field);

        case usr_PB_LTYPE_VARINT:
        case usr_PB_LTYPE_UVARINT:
        case usr_PB_LTYPE_SVARINT:
            if (wire_type != usr_PB_WT_VARINT && wire_type != usr_PB_WT_PACKED)
                usr_PB_RETURN_ERROR(stream, "wrong wire type");

            return usr_pb_dec_varint(stream, field);

        case usr_PB_LTYPE_FIXED32:
            if (wire_type != usr_PB_WT_32BIT && wire_type != usr_PB_WT_PACKED)
                usr_PB_RETURN_ERROR(stream, "wrong wire type");

            return usr_pb_decode_fixed32(stream, field->pData);

        case usr_PB_LTYPE_FIXED64:
            if (wire_type != usr_PB_WT_64BIT && wire_type != usr_PB_WT_PACKED)
                usr_PB_RETURN_ERROR(stream, "wrong wire type");

#ifdef usr_PB_CONVERT_DOUBLE_FLOAT
            if (field->data_size == sizeof(float))
            {
                return usr_pb_decode_double_as_float(stream, (float*)field->pData);
            }
#endif

#ifdef usr_PB_WITHOUT_64BIT
            usr_PB_RETURN_ERROR(stream, "invalid data_size");
#else
            return usr_pb_decode_fixed64(stream, field->pData);
#endif

        case usr_PB_LTYPE_BYTES:
            if (wire_type != usr_PB_WT_STRING)
                usr_PB_RETURN_ERROR(stream, "wrong wire type");

            return usr_pb_dec_bytes(stream, field);

        case usr_PB_LTYPE_STRING:
            if (wire_type != usr_PB_WT_STRING)
                usr_PB_RETURN_ERROR(stream, "wrong wire type");

            return usr_pb_dec_string(stream, field);

        case usr_PB_LTYPE_SUBMESSAGE:
        case usr_PB_LTYPE_SUBMSG_W_CB:
            if (wire_type != usr_PB_WT_STRING)
                usr_PB_RETURN_ERROR(stream, "wrong wire type");

            return usr_pb_dec_submessage(stream, field);

        case usr_PB_LTYPE_FIXED_LENGTH_BYTES:
            if (wire_type != usr_PB_WT_STRING)
                usr_PB_RETURN_ERROR(stream, "wrong wire type");

            return usr_pb_dec_fixed_length_bytes(stream, field);

        default:
            usr_PB_RETURN_ERROR(stream, "invalid field type");
    }
}

static bool checkreturn decode_static_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field)
{
    switch (usr_PB_HTYPE(field->type))
    {
        case usr_PB_HTYPE_REQUIRED:
            return decode_basic_field(stream, wire_type, field);
            
        case usr_PB_HTYPE_OPTIONAL:
            if (field->pSize != NULL)
                *(bool*)field->pSize = true;
            return decode_basic_field(stream, wire_type, field);
    
        case usr_PB_HTYPE_REPEATED:
            if (wire_type == usr_PB_WT_STRING
                && usr_PB_LTYPE(field->type) <= usr_PB_LTYPE_LAST_PACKABLE)
            {
                /* Packed array */
                bool status = true;
                usr_pb_istream_t substream;
                usr_pb_size_t *size = (usr_pb_size_t*)field->pSize;
                field->pData = (char*)field->pField + field->data_size * (*size);

                if (!usr_pb_make_string_substream(stream, &substream))
                    return false;

                while (substream.bytes_left > 0 && *size < field->array_size)
                {
                    if (!decode_basic_field(&substream, usr_PB_WT_PACKED, field))
                    {
                        status = false;
                        break;
                    }
                    (*size)++;
                    field->pData = (char*)field->pData + field->data_size;
                }

                if (substream.bytes_left != 0)
                    usr_PB_RETURN_ERROR(stream, "array overflow");
                if (!usr_pb_close_string_substream(stream, &substream))
                    return false;

                return status;
            }
            else
            {
                /* Repeated field */
                usr_pb_size_t *size = (usr_pb_size_t*)field->pSize;
                field->pData = (char*)field->pField + field->data_size * (*size);

                if ((*size)++ >= field->array_size)
                    usr_PB_RETURN_ERROR(stream, "array overflow");

                return decode_basic_field(stream, wire_type, field);
            }

        case usr_PB_HTYPE_ONEOF:
            if (usr_PB_LTYPE_IS_SUBMSG(field->type) &&
                *(usr_pb_size_t*)field->pSize != field->tag)
            {
                /* We memset to zero so that any callbacks are set to NULL.
                 * This is because the callbacks might otherwise have values
                 * from some other union field.
                 * If callbacks are needed inside oneof field, use .proto
                 * option submsg_callback to have a separate callback function
                 * that can set the fields before submessage is decoded.
                 * usr_pb_dec_submessage() will set any default values. */
                memset(field->pData, 0, (size_t)field->data_size);

                /* Set default values for the submessage fields. */
                if (field->submsg_desc->default_value != NULL ||
                    field->submsg_desc->field_callback != NULL ||
                    field->submsg_desc->submsg_info[0] != NULL)
                {
                    usr_pb_field_iter_t submsg_iter;
                    if (usr_pb_field_iter_begin(&submsg_iter, field->submsg_desc, field->pData))
                    {
                        if (!usr_pb_message_set_to_defaults(&submsg_iter))
                            usr_PB_RETURN_ERROR(stream, "failed to set defaults");
                    }
                }
            }
            *(usr_pb_size_t*)field->pSize = field->tag;

            return decode_basic_field(stream, wire_type, field);

        default:
            usr_PB_RETURN_ERROR(stream, "invalid field type");
    }
}

#ifdef usr_PB_ENABLE_MALLOC
/* Allocate storage for the field and store the pointer at iter->pData.
 * array_size is the number of entries to reserve in an array.
 * Zero size is not allowed, use usr_pb_free() for releasing.
 */
static bool checkreturn allocate_field(usr_pb_istream_t *stream, void *pData, size_t data_size, size_t array_size)
{    
    void *ptr = *(void**)pData;
    
    if (data_size == 0 || array_size == 0)
        usr_PB_RETURN_ERROR(stream, "invalid size");
    
#ifdef __AVR__
    /* Workaround for AVR libc bug 53284: http://savannah.nongnu.org/bugs/?53284
     * Realloc to size of 1 byte can cause corruption of the malloc structures.
     */
    if (data_size == 1 && array_size == 1)
    {
        data_size = 2;
    }
#endif

    /* Check for multiplication overflows.
     * This code avoids the costly division if the sizes are small enough.
     * Multiplication is safe as long as only half of bits are set
     * in either multiplicand.
     */
    {
        const size_t check_limit = (size_t)1 << (sizeof(size_t) * 4);
        if (data_size >= check_limit || array_size >= check_limit)
        {
            const size_t size_max = (size_t)-1;
            if (size_max / array_size < data_size)
            {
                usr_PB_RETURN_ERROR(stream, "size too large");
            }
        }
    }
    
    /* Allocate new or expand previous allocation */
    /* Note: on failure the old pointer will remain in the structure,
     * the message must be freed by caller also on error return. */
    ptr = usr_pb_realloc(ptr, array_size * data_size);
    if (ptr == NULL)
        usr_PB_RETURN_ERROR(stream, "realloc failed");
    
    *(void**)pData = ptr;
    return true;
}

/* Clear a newly allocated item in case it contains a pointer, or is a submessage. */
static void initialize_pointer_field(void *pItem, usr_pb_field_iter_t *field)
{
    if (usr_PB_LTYPE(field->type) == usr_PB_LTYPE_STRING ||
        usr_PB_LTYPE(field->type) == usr_PB_LTYPE_BYTES)
    {
        *(void**)pItem = NULL;
    }
    else if (usr_PB_LTYPE_IS_SUBMSG(field->type))
    {
        /* We memset to zero so that any callbacks are set to NULL.
         * Default values will be set by usr_pb_dec_submessage(). */
        memset(pItem, 0, field->data_size);
    }
}
#endif

static bool checkreturn decode_pointer_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field)
{
#ifndef usr_PB_ENABLE_MALLOC
    usr_PB_UNUSED(wire_type);
    usr_PB_UNUSED(field);
    usr_PB_RETURN_ERROR(stream, "no malloc support");
#else
    switch (usr_PB_HTYPE(field->type))
    {
        case usr_PB_HTYPE_REQUIRED:
        case usr_PB_HTYPE_OPTIONAL:
        case usr_PB_HTYPE_ONEOF:
            if (usr_PB_LTYPE_IS_SUBMSG(field->type) && *(void**)field->pField != NULL)
            {
                /* Duplicate field, have to release the old allocation first. */
                /* FIXME: Does this work correctly for oneofs? */
                usr_pb_release_single_field(field);
            }
        
            if (usr_PB_HTYPE(field->type) == usr_PB_HTYPE_ONEOF)
            {
                *(usr_pb_size_t*)field->pSize = field->tag;
            }

            if (usr_PB_LTYPE(field->type) == usr_PB_LTYPE_STRING ||
                usr_PB_LTYPE(field->type) == usr_PB_LTYPE_BYTES)
            {
                /* usr_pb_dec_string and usr_pb_dec_bytes handle allocation themselves */
                field->pData = field->pField;
                return decode_basic_field(stream, wire_type, field);
            }
            else
            {
                if (!allocate_field(stream, field->pField, field->data_size, 1))
                    return false;
                
                field->pData = *(void**)field->pField;
                initialize_pointer_field(field->pData, field);
                return decode_basic_field(stream, wire_type, field);
            }
    
        case usr_PB_HTYPE_REPEATED:
            if (wire_type == usr_PB_WT_STRING
                && usr_PB_LTYPE(field->type) <= usr_PB_LTYPE_LAST_PACKABLE)
            {
                /* Packed array, multiple items come in at once. */
                bool status = true;
                usr_pb_size_t *size = (usr_pb_size_t*)field->pSize;
                size_t allocated_size = *size;
                usr_pb_istream_t substream;
                
                if (!usr_pb_make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left)
                {
                    if (*size == usr_PB_SIZE_MAX)
                    {
#ifndef usr_PB_NO_ERRMSG
                        stream->errmsg = "too many array entries";
#endif
                        status = false;
                        break;
                    }

                    if ((size_t)*size + 1 > allocated_size)
                    {
                        /* Allocate more storage. This tries to guess the
                         * number of remaining entries. Round the division
                         * upwards. */
                        size_t remain = (substream.bytes_left - 1) / field->data_size + 1;
                        if (remain < usr_PB_SIZE_MAX - allocated_size)
                            allocated_size += remain;
                        else
                            allocated_size += 1;
                        
                        if (!allocate_field(&substream, field->pField, field->data_size, allocated_size))
                        {
                            status = false;
                            break;
                        }
                    }

                    /* Decode the array entry */
                    field->pData = *(char**)field->pField + field->data_size * (*size);
                    initialize_pointer_field(field->pData, field);
                    if (!decode_basic_field(&substream, usr_PB_WT_PACKED, field))
                    {
                        status = false;
                        break;
                    }
                    
                    (*size)++;
                }
                if (!usr_pb_close_string_substream(stream, &substream))
                    return false;
                
                return status;
            }
            else
            {
                /* Normal repeated field, i.e. only one item at a time. */
                usr_pb_size_t *size = (usr_pb_size_t*)field->pSize;

                if (*size == usr_PB_SIZE_MAX)
                    usr_PB_RETURN_ERROR(stream, "too many array entries");
                
                if (!allocate_field(stream, field->pField, field->data_size, (size_t)(*size + 1)))
                    return false;
            
                field->pData = *(char**)field->pField + field->data_size * (*size);
                (*size)++;
                initialize_pointer_field(field->pData, field);
                return decode_basic_field(stream, wire_type, field);
            }

        default:
            usr_PB_RETURN_ERROR(stream, "invalid field type");
    }
#endif
}

static bool checkreturn decode_callback_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field)
{
    if (!field->descriptor->field_callback)
        return usr_pb_skip_field(stream, wire_type);

    if (wire_type == usr_PB_WT_STRING)
    {
        usr_pb_istream_t substream;
        size_t prev_bytes_left;
        
        if (!usr_pb_make_string_substream(stream, &substream))
            return false;
        
        do
        {
            prev_bytes_left = substream.bytes_left;
            if (!field->descriptor->field_callback(&substream, NULL, field))
                usr_PB_RETURN_ERROR(stream, "callback failed");
        } while (substream.bytes_left > 0 && substream.bytes_left < prev_bytes_left);
        
        if (!usr_pb_close_string_substream(stream, &substream))
            return false;

        return true;
    }
    else
    {
        /* Copy the single scalar value to stack.
         * This is required so that we can limit the stream length,
         * which in turn allows to use same callback for packed and
         * not-packed fields. */
        usr_pb_istream_t substream;
        usr_pb_byte_t buffer[10];
        size_t size = sizeof(buffer);
        
        if (!read_raw_value(stream, wire_type, buffer, &size))
            return false;
        substream = usr_pb_istream_from_buffer(buffer, size);
        
        return field->descriptor->field_callback(&substream, NULL, field);
    }
}

static bool checkreturn decode_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type, usr_pb_field_iter_t *field)
{
#ifdef usr_PB_ENABLE_MALLOC
    /* When decoding an oneof field, check if there is old data that must be
     * released first. */
    if (usr_PB_HTYPE(field->type) == usr_PB_HTYPE_ONEOF)
    {
        if (!usr_pb_release_union_field(stream, field))
            return false;
    }
#endif

    switch (usr_PB_ATYPE(field->type))
    {
        case usr_PB_ATYPE_STATIC:
            return decode_static_field(stream, wire_type, field);
        
        case usr_PB_ATYPE_POINTER:
            return decode_pointer_field(stream, wire_type, field);
        
        case usr_PB_ATYPE_CALLBACK:
            return decode_callback_field(stream, wire_type, field);
        
        default:
            usr_PB_RETURN_ERROR(stream, "invalid field type");
    }
}

/* Default handler for extension fields. Expects to have a usr_pb_msgdesc_t
 * pointer in the extension->type->arg field, pointing to a message with
 * only one field in it.  */
static bool checkreturn default_extension_decoder(usr_pb_istream_t *stream,
    usr_pb_extension_t *extension, uint32_t tag, usr_pb_wire_type_t wire_type)
{
    usr_pb_field_iter_t iter;

    if (!usr_pb_field_iter_begin_extension(&iter, extension))
        usr_PB_RETURN_ERROR(stream, "invalid extension");

    if (iter.tag != tag || !iter.message)
        return true;

    extension->found = true;
    return decode_field(stream, wire_type, &iter);
}

/* Try to decode an unknown field as an extension field. Tries each extension
 * decoder in turn, until one of them handles the field or loop ends. */
static bool checkreturn decode_extension(usr_pb_istream_t *stream,
    uint32_t tag, usr_pb_wire_type_t wire_type, usr_pb_extension_t *extension)
{
    size_t pos = stream->bytes_left;
    
    while (extension != NULL && pos == stream->bytes_left)
    {
        bool status;
        if (extension->type->decode)
            status = extension->type->decode(stream, extension, tag, wire_type);
        else
            status = default_extension_decoder(stream, extension, tag, wire_type);

        if (!status)
            return false;
        
        extension = extension->next;
    }
    
    return true;
}

/* Initialize message fields to default values, recursively */
static bool usr_pb_field_set_to_default(usr_pb_field_iter_t *field)
{
    usr_pb_type_t type;
    type = field->type;

    if (usr_PB_LTYPE(type) == usr_PB_LTYPE_EXTENSION)
    {
        usr_pb_extension_t *ext = *(usr_pb_extension_t* const *)field->pData;
        while (ext != NULL)
        {
            usr_pb_field_iter_t ext_iter;
            if (usr_pb_field_iter_begin_extension(&ext_iter, ext))
            {
                ext->found = false;
                if (!usr_pb_message_set_to_defaults(&ext_iter))
                    return false;
            }
            ext = ext->next;
        }
    }
    else if (usr_PB_ATYPE(type) == usr_PB_ATYPE_STATIC)
    {
        bool init_data = true;
        if (usr_PB_HTYPE(type) == usr_PB_HTYPE_OPTIONAL && field->pSize != NULL)
        {
            /* Set has_field to false. Still initialize the optional field
             * itself also. */
            *(bool*)field->pSize = false;
        }
        else if (usr_PB_HTYPE(type) == usr_PB_HTYPE_REPEATED ||
                 usr_PB_HTYPE(type) == usr_PB_HTYPE_ONEOF)
        {
            /* REPEATED: Set array count to 0, no need to initialize contents.
               ONEOF: Set which_field to 0. */
            *(usr_pb_size_t*)field->pSize = 0;
            init_data = false;
        }

        if (init_data)
        {
            if (usr_PB_LTYPE_IS_SUBMSG(field->type) &&
                (field->submsg_desc->default_value != NULL ||
                 field->submsg_desc->field_callback != NULL ||
                 field->submsg_desc->submsg_info[0] != NULL))
            {
                /* Initialize submessage to defaults.
                 * Only needed if it has default values
                 * or callback/submessage fields. */
                usr_pb_field_iter_t submsg_iter;
                if (usr_pb_field_iter_begin(&submsg_iter, field->submsg_desc, field->pData))
                {
                    if (!usr_pb_message_set_to_defaults(&submsg_iter))
                        return false;
                }
            }
            else
            {
                /* Initialize to zeros */
                memset(field->pData, 0, (size_t)field->data_size);
            }
        }
    }
    else if (usr_PB_ATYPE(type) == usr_PB_ATYPE_POINTER)
    {
        /* Initialize the pointer to NULL. */
        *(void**)field->pField = NULL;

        /* Initialize array count to 0. */
        if (usr_PB_HTYPE(type) == usr_PB_HTYPE_REPEATED ||
            usr_PB_HTYPE(type) == usr_PB_HTYPE_ONEOF)
        {
            *(usr_pb_size_t*)field->pSize = 0;
        }
    }
    else if (usr_PB_ATYPE(type) == usr_PB_ATYPE_CALLBACK)
    {
        /* Don't overwrite callback */
    }

    return true;
}

static bool usr_pb_message_set_to_defaults(usr_pb_field_iter_t *iter)
{
    usr_pb_istream_t defstream = usr_PB_ISTREAM_EMPTY;
    uint32_t tag = 0;
    usr_pb_wire_type_t wire_type = usr_PB_WT_VARINT;
    bool eof;

    if (iter->descriptor->default_value)
    {
        defstream = usr_pb_istream_from_buffer(iter->descriptor->default_value, (size_t)-1);
        if (!usr_pb_decode_tag(&defstream, &wire_type, &tag, &eof))
            return false;
    }

    do
    {
        if (!usr_pb_field_set_to_default(iter))
            return false;

        if (tag != 0 && iter->tag == tag)
        {
            /* We have a default value for this field in the defstream */
            if (!decode_field(&defstream, wire_type, iter))
                return false;
            if (!usr_pb_decode_tag(&defstream, &wire_type, &tag, &eof))
                return false;

            if (iter->pSize)
                *(bool*)iter->pSize = false;
        }
    } while (usr_pb_field_iter_next(iter));

    return true;
}

/*********************
 * Decode all fields *
 *********************/

static bool checkreturn usr_pb_decode_inner(usr_pb_istream_t *stream, const usr_pb_msgdesc_t *fields, void *dest_struct, unsigned int flags)
{
    uint32_t extension_range_start = 0;
    usr_pb_extension_t *extensions = NULL;

    /* 'fixed_count_field' and 'fixed_count_size' track position of a repeated fixed
     * count field. This can only handle _one_ repeated fixed count field that
     * is unpacked and unordered among other (non repeated fixed count) fields.
     */
    usr_pb_size_t fixed_count_field = usr_PB_SIZE_MAX;
    usr_pb_size_t fixed_count_size = 0;
    usr_pb_size_t fixed_count_total_size = 0;

    usr_pb_fields_seen_t fields_seen = {{0, 0}};
    const uint32_t allbits = ~(uint32_t)0;
    usr_pb_field_iter_t iter;

    if (usr_pb_field_iter_begin(&iter, fields, dest_struct))
    {
        if ((flags & usr_PB_DECODE_NOINIT) == 0)
        {
            if (!usr_pb_message_set_to_defaults(&iter))
                usr_PB_RETURN_ERROR(stream, "failed to set defaults");
        }
    }

    while (stream->bytes_left)
    {
        uint32_t tag;
        usr_pb_wire_type_t wire_type;
        bool eof;

        if (!usr_pb_decode_tag(stream, &wire_type, &tag, &eof))
        {
            if (eof)
                break;
            else
                return false;
        }

        if (tag == 0)
        {
          if (flags & usr_PB_DECODE_NULLTERMINATED)
          {
            break;
          }
          else
          {
            usr_PB_RETURN_ERROR(stream, "zero tag");
          }
        }

        if (!usr_pb_field_iter_find(&iter, tag) || usr_PB_LTYPE(iter.type) == usr_PB_LTYPE_EXTENSION)
        {
            /* No match found, check if it matches an extension. */
            if (extension_range_start == 0)
            {
                if (usr_pb_field_iter_find_extension(&iter))
                {
                    extensions = *(usr_pb_extension_t* const *)iter.pData;
                    extension_range_start = iter.tag;
                }

                if (!extensions)
                {
                    extension_range_start = (uint32_t)-1;
                }
            }

            if (tag >= extension_range_start)
            {
                size_t pos = stream->bytes_left;

                if (!decode_extension(stream, tag, wire_type, extensions))
                    return false;

                if (pos != stream->bytes_left)
                {
                    /* The field was handled */
                    continue;
                }
            }

            /* No match found, skip data */
            if (!usr_pb_skip_field(stream, wire_type))
                return false;
            continue;
        }

        /* If a repeated fixed count field was found, get size from
         * 'fixed_count_field' as there is no counter contained in the struct.
         */
        if (usr_PB_HTYPE(iter.type) == usr_PB_HTYPE_REPEATED && iter.pSize == &iter.array_size)
        {
            if (fixed_count_field != iter.index) {
                /* If the new fixed count field does not match the previous one,
                 * check that the previous one is NULL or that it finished
                 * receiving all the expected data.
                 */
                if (fixed_count_field != usr_PB_SIZE_MAX &&
                    fixed_count_size != fixed_count_total_size)
                {
                    usr_PB_RETURN_ERROR(stream, "wrong size for fixed count field");
                }

                fixed_count_field = iter.index;
                fixed_count_size = 0;
                fixed_count_total_size = iter.array_size;
            }

            iter.pSize = &fixed_count_size;
        }

        if (usr_PB_HTYPE(iter.type) == usr_PB_HTYPE_REQUIRED
            && iter.required_field_index < usr_PB_MAX_REQUIRED_FIELDS)
        {
            uint32_t tmp = ((uint32_t)1 << (iter.required_field_index & 31));
            fields_seen.bitfield[iter.required_field_index >> 5] |= tmp;
        }

        if (!decode_field(stream, wire_type, &iter))
            return false;
    }

    /* Check that all elements of the last decoded fixed count field were present. */
    if (fixed_count_field != usr_PB_SIZE_MAX &&
        fixed_count_size != fixed_count_total_size)
    {
        usr_PB_RETURN_ERROR(stream, "wrong size for fixed count field");
    }

    /* Check that all required fields were present. */
    {
        usr_pb_size_t req_field_count = iter.descriptor->required_field_count;

        if (req_field_count > 0)
        {
            usr_pb_size_t i;

            if (req_field_count > usr_PB_MAX_REQUIRED_FIELDS)
                req_field_count = usr_PB_MAX_REQUIRED_FIELDS;

            /* Check the whole words */
            for (i = 0; i < (req_field_count >> 5); i++)
            {
                if (fields_seen.bitfield[i] != allbits)
                    usr_PB_RETURN_ERROR(stream, "missing required field");
            }

            /* Check the remaining bits (if any) */
            if ((req_field_count & 31) != 0)
            {
                if (fields_seen.bitfield[req_field_count >> 5] !=
                    (allbits >> (uint_least8_t)(32 - (req_field_count & 31))))
                {
                    usr_PB_RETURN_ERROR(stream, "missing required field");
                }
            }
        }
    }

    return true;
}

bool checkreturn usr_pb_decode_ex(usr_pb_istream_t *stream, const usr_pb_msgdesc_t *fields, void *dest_struct, unsigned int flags)
{
    bool status;

    if ((flags & usr_PB_DECODE_DELIMITED) == 0)
    {
      status = usr_pb_decode_inner(stream, fields, dest_struct, flags);
    }
    else
    {
      usr_pb_istream_t substream;
      if (!usr_pb_make_string_substream(stream, &substream))
        return false;

      status = usr_pb_decode_inner(&substream, fields, dest_struct, flags);

      if (!usr_pb_close_string_substream(stream, &substream))
        return false;
    }
    
#ifdef usr_PB_ENABLE_MALLOC
    if (!status)
        usr_pb_release(fields, dest_struct);
#endif
    
    return status;
}

bool checkreturn usr_pb_decode(usr_pb_istream_t *stream, const usr_pb_msgdesc_t *fields, void *dest_struct)
{
    bool status;

    status = usr_pb_decode_inner(stream, fields, dest_struct, 0);

#ifdef usr_PB_ENABLE_MALLOC
    if (!status)
        usr_pb_release(fields, dest_struct);
#endif

    return status;
}

#ifdef usr_PB_ENABLE_MALLOC
/* Given an oneof field, if there has already been a field inside this oneof,
 * release it before overwriting with a different one. */
static bool usr_pb_release_union_field(usr_pb_istream_t *stream, usr_pb_field_iter_t *field)
{
    usr_pb_field_iter_t old_field = *field;
    usr_pb_size_t old_tag = *(usr_pb_size_t*)field->pSize; /* Previous which_ value */
    usr_pb_size_t new_tag = field->tag; /* New which_ value */

    if (old_tag == 0)
        return true; /* Ok, no old data in union */

    if (old_tag == new_tag)
        return true; /* Ok, old data is of same type => merge */

    /* Release old data. The find can fail if the message struct contains
     * invalid data. */
    if (!usr_pb_field_iter_find(&old_field, old_tag))
        usr_PB_RETURN_ERROR(stream, "invalid union tag");

    usr_pb_release_single_field(&old_field);

    if (usr_PB_ATYPE(field->type) == usr_PB_ATYPE_POINTER)
    {
        /* Initialize the pointer to NULL to make sure it is valid
         * even in case of error return. */
        *(void**)field->pField = NULL;
        field->pData = NULL;
    }

    return true;
}

static void usr_pb_release_single_field(usr_pb_field_iter_t *field)
{
    usr_pb_type_t type;
    type = field->type;

    if (usr_PB_HTYPE(type) == usr_PB_HTYPE_ONEOF)
    {
        if (*(usr_pb_size_t*)field->pSize != field->tag)
            return; /* This is not the current field in the union */
    }

    /* Release anything contained inside an extension or submsg.
     * This has to be done even if the submsg itself is statically
     * allocated. */
    if (usr_PB_LTYPE(type) == usr_PB_LTYPE_EXTENSION)
    {
        /* Release fields from all extensions in the linked list */
        usr_pb_extension_t *ext = *(usr_pb_extension_t**)field->pData;
        while (ext != NULL)
        {
            usr_pb_field_iter_t ext_iter;
            if (usr_pb_field_iter_begin_extension(&ext_iter, ext))
            {
                usr_pb_release_single_field(&ext_iter);
            }
            ext = ext->next;
        }
    }
    else if (usr_PB_LTYPE_IS_SUBMSG(type) && usr_PB_ATYPE(type) != usr_PB_ATYPE_CALLBACK)
    {
        /* Release fields in submessage or submsg array */
        usr_pb_size_t count = 1;
        
        if (usr_PB_ATYPE(type) == usr_PB_ATYPE_POINTER)
        {
            field->pData = *(void**)field->pField;
        }
        else
        {
            field->pData = field->pField;
        }
        
        if (usr_PB_HTYPE(type) == usr_PB_HTYPE_REPEATED)
        {
            count = *(usr_pb_size_t*)field->pSize;

            if (usr_PB_ATYPE(type) == usr_PB_ATYPE_STATIC && count > field->array_size)
            {
                /* Protect against corrupted _count fields */
                count = field->array_size;
            }
        }
        
        if (field->pData)
        {
            for (; count > 0; count--)
            {
                usr_pb_release(field->submsg_desc, field->pData);
                field->pData = (char*)field->pData + field->data_size;
            }
        }
    }
    
    if (usr_PB_ATYPE(type) == usr_PB_ATYPE_POINTER)
    {
        if (usr_PB_HTYPE(type) == usr_PB_HTYPE_REPEATED &&
            (usr_PB_LTYPE(type) == usr_PB_LTYPE_STRING ||
             usr_PB_LTYPE(type) == usr_PB_LTYPE_BYTES))
        {
            /* Release entries in repeated string or bytes array */
            void **pItem = *(void***)field->pField;
            usr_pb_size_t count = *(usr_pb_size_t*)field->pSize;
            for (; count > 0; count--)
            {
                usr_pb_free(*pItem);
                *pItem++ = NULL;
            }
        }
        
        if (usr_PB_HTYPE(type) == usr_PB_HTYPE_REPEATED)
        {
            /* We are going to release the array, so set the size to 0 */
            *(usr_pb_size_t*)field->pSize = 0;
        }
        
        /* Release main pointer */
        usr_pb_free(*(void**)field->pField);
        *(void**)field->pField = NULL;
    }
}

void usr_pb_release(const usr_pb_msgdesc_t *fields, void *dest_struct)
{
    usr_pb_field_iter_t iter;
    
    if (!dest_struct)
        return; /* Ignore NULL pointers, similar to free() */

    if (!usr_pb_field_iter_begin(&iter, fields, dest_struct))
        return; /* Empty message type */
    
    do
    {
        usr_pb_release_single_field(&iter);
    } while (usr_pb_field_iter_next(&iter));
}
#endif

/* Field decoders */

bool usr_pb_decode_bool(usr_pb_istream_t *stream, bool *dest)
{
    uint32_t value;
    if (!usr_pb_decode_varint32(stream, &value))
        return false;

    *(bool*)dest = (value != 0);
    return true;
}

bool usr_pb_decode_svarint(usr_pb_istream_t *stream, usr_pb_int64_t *dest)
{
    usr_pb_uint64_t value;
    if (!usr_pb_decode_varint(stream, &value))
        return false;
    
    if (value & 1)
        *dest = (usr_pb_int64_t)(~(value >> 1));
    else
        *dest = (usr_pb_int64_t)(value >> 1);
    
    return true;
}

bool usr_pb_decode_fixed32(usr_pb_istream_t *stream, void *dest)
{
    union {
        uint32_t fixed32;
        usr_pb_byte_t bytes[4];
    } u;

    if (!usr_pb_read(stream, u.bytes, 4))
        return false;

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN && CHAR_BIT == 8
    /* fast path - if we know that we're on little endian, assign directly */
    *(uint32_t*)dest = u.fixed32;
#else
    *(uint32_t*)dest = ((uint32_t)u.bytes[0] << 0) |
                       ((uint32_t)u.bytes[1] << 8) |
                       ((uint32_t)u.bytes[2] << 16) |
                       ((uint32_t)u.bytes[3] << 24);
#endif
    return true;
}

#ifndef usr_PB_WITHOUT_64BIT
bool usr_pb_decode_fixed64(usr_pb_istream_t *stream, void *dest)
{
    union {
        uint64_t fixed64;
        usr_pb_byte_t bytes[8];
    } u;

    if (!usr_pb_read(stream, u.bytes, 8))
        return false;

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN && CHAR_BIT == 8
    /* fast path - if we know that we're on little endian, assign directly */
    *(uint64_t*)dest = u.fixed64;
#else
    *(uint64_t*)dest = ((uint64_t)u.bytes[0] << 0) |
                       ((uint64_t)u.bytes[1] << 8) |
                       ((uint64_t)u.bytes[2] << 16) |
                       ((uint64_t)u.bytes[3] << 24) |
                       ((uint64_t)u.bytes[4] << 32) |
                       ((uint64_t)u.bytes[5] << 40) |
                       ((uint64_t)u.bytes[6] << 48) |
                       ((uint64_t)u.bytes[7] << 56);
#endif
    return true;
}
#endif

static bool checkreturn usr_pb_dec_bool(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field)
{
    return usr_pb_decode_bool(stream, (bool*)field->pData);
}

static bool checkreturn usr_pb_dec_varint(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field)
{
    if (usr_PB_LTYPE(field->type) == usr_PB_LTYPE_UVARINT)
    {
        usr_pb_uint64_t value, clamped;
        if (!usr_pb_decode_varint(stream, &value))
            return false;

        /* Cast to the proper field size, while checking for overflows */
        if (field->data_size == sizeof(usr_pb_uint64_t))
            clamped = *(usr_pb_uint64_t*)field->pData = value;
        else if (field->data_size == sizeof(uint32_t))
            clamped = *(uint32_t*)field->pData = (uint32_t)value;
        else if (field->data_size == sizeof(uint_least16_t))
            clamped = *(uint_least16_t*)field->pData = (uint_least16_t)value;
        else if (field->data_size == sizeof(uint_least8_t))
            clamped = *(uint_least8_t*)field->pData = (uint_least8_t)value;
        else
            usr_PB_RETURN_ERROR(stream, "invalid data_size");

        if (clamped != value)
            usr_PB_RETURN_ERROR(stream, "integer too large");

        return true;
    }
    else
    {
        usr_pb_uint64_t value;
        usr_pb_int64_t svalue;
        usr_pb_int64_t clamped;

        if (usr_PB_LTYPE(field->type) == usr_PB_LTYPE_SVARINT)
        {
            if (!usr_pb_decode_svarint(stream, &svalue))
                return false;
        }
        else
        {
            if (!usr_pb_decode_varint(stream, &value))
                return false;

            /* See issue 97: Google's C++ protobuf allows negative varint values to
            * be cast as int32_t, instead of the int64_t that should be used when
            * encoding. Nanopb versions before 0.2.5 had a bug in encoding. In order to
            * not break decoding of such messages, we cast <=32 bit fields to
            * int32_t first to get the sign correct.
            */
            if (field->data_size == sizeof(usr_pb_int64_t))
                svalue = (usr_pb_int64_t)value;
            else
                svalue = (int32_t)value;
        }

        /* Cast to the proper field size, while checking for overflows */
        if (field->data_size == sizeof(usr_pb_int64_t))
            clamped = *(usr_pb_int64_t*)field->pData = svalue;
        else if (field->data_size == sizeof(int32_t))
            clamped = *(int32_t*)field->pData = (int32_t)svalue;
        else if (field->data_size == sizeof(int_least16_t))
            clamped = *(int_least16_t*)field->pData = (int_least16_t)svalue;
        else if (field->data_size == sizeof(int_least8_t))
            clamped = *(int_least8_t*)field->pData = (int_least8_t)svalue;
        else
            usr_PB_RETURN_ERROR(stream, "invalid data_size");

        if (clamped != svalue)
            usr_PB_RETURN_ERROR(stream, "integer too large");

        return true;
    }
}

static bool checkreturn usr_pb_dec_bytes(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field)
{
    uint32_t size;
    size_t alloc_size;
    usr_pb_bytes_array_t *dest;
    
    if (!usr_pb_decode_varint32(stream, &size))
        return false;
    
    if (size > usr_PB_SIZE_MAX)
        usr_PB_RETURN_ERROR(stream, "bytes overflow");
    
    alloc_size = usr_PB_BYTES_ARRAY_T_ALLOCSIZE(size);
    if (size > alloc_size)
        usr_PB_RETURN_ERROR(stream, "size too large");
    
    if (usr_PB_ATYPE(field->type) == usr_PB_ATYPE_POINTER)
    {
#ifndef usr_PB_ENABLE_MALLOC
        usr_PB_RETURN_ERROR(stream, "no malloc support");
#else
        if (stream->bytes_left < size)
            usr_PB_RETURN_ERROR(stream, "end-of-stream");

        if (!allocate_field(stream, field->pData, alloc_size, 1))
            return false;
        dest = *(usr_pb_bytes_array_t**)field->pData;
#endif
    }
    else
    {
        if (alloc_size > field->data_size)
            usr_PB_RETURN_ERROR(stream, "bytes overflow");
        dest = (usr_pb_bytes_array_t*)field->pData;
    }

    dest->size = (usr_pb_size_t)size;
    return usr_pb_read(stream, dest->bytes, (size_t)size);
}

static bool checkreturn usr_pb_dec_string(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field)
{
    uint32_t size;
    size_t alloc_size;
    usr_pb_byte_t *dest = (usr_pb_byte_t*)field->pData;

    if (!usr_pb_decode_varint32(stream, &size))
        return false;

    if (size == (uint32_t)-1)
        usr_PB_RETURN_ERROR(stream, "size too large");

    /* Space for null terminator */
    alloc_size = (size_t)(size + 1);

    if (alloc_size < size)
        usr_PB_RETURN_ERROR(stream, "size too large");

    if (usr_PB_ATYPE(field->type) == usr_PB_ATYPE_POINTER)
    {
#ifndef usr_PB_ENABLE_MALLOC
        usr_PB_RETURN_ERROR(stream, "no malloc support");
#else
        if (stream->bytes_left < size)
            usr_PB_RETURN_ERROR(stream, "end-of-stream");

        if (!allocate_field(stream, field->pData, alloc_size, 1))
            return false;
        dest = *(usr_pb_byte_t**)field->pData;
#endif
    }
    else
    {
        if (alloc_size > field->data_size)
            usr_PB_RETURN_ERROR(stream, "string overflow");
    }
    
    dest[size] = 0;

    if (!usr_pb_read(stream, dest, (size_t)size))
        return false;

#ifdef usr_PB_VALIDATE_UTF8
    if (!usr_pb_validate_utf8((const char*)dest))
        usr_PB_RETURN_ERROR(stream, "invalid utf8");
#endif

    return true;
}

static bool checkreturn usr_pb_dec_submessage(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field)
{
    bool status = true;
    bool submsg_consumed = false;
    usr_pb_istream_t substream;

    if (!usr_pb_make_string_substream(stream, &substream))
        return false;
    
    if (field->submsg_desc == NULL)
        usr_PB_RETURN_ERROR(stream, "invalid field descriptor");
    
    /* Submessages can have a separate message-level callback that is called
     * before decoding the message. Typically it is used to set callback fields
     * inside oneofs. */
    if (usr_PB_LTYPE(field->type) == usr_PB_LTYPE_SUBMSG_W_CB && field->pSize != NULL)
    {
        /* Message callback is stored right before pSize. */
        usr_pb_callback_t *callback = (usr_pb_callback_t*)field->pSize - 1;
        if (callback->funcs.decode)
        {
            status = callback->funcs.decode(&substream, field, &callback->arg);

            if (substream.bytes_left == 0)
            {
                submsg_consumed = true;
            }
        }
    }

    /* Now decode the submessage contents */
    if (status && !submsg_consumed)
    {
        unsigned int flags = 0;

        /* Static required/optional fields are already initialized by top-level
         * usr_pb_decode(), no need to initialize them again. */
        if (usr_PB_ATYPE(field->type) == usr_PB_ATYPE_STATIC &&
            usr_PB_HTYPE(field->type) != usr_PB_HTYPE_REPEATED)
        {
            flags = usr_PB_DECODE_NOINIT;
        }

        status = usr_pb_decode_inner(&substream, field->submsg_desc, field->pData, flags);
    }
    
    if (!usr_pb_close_string_substream(stream, &substream))
        return false;

    return status;
}

static bool checkreturn usr_pb_dec_fixed_length_bytes(usr_pb_istream_t *stream, const usr_pb_field_iter_t *field)
{
    uint32_t size;

    if (!usr_pb_decode_varint32(stream, &size))
        return false;

    if (size > usr_PB_SIZE_MAX)
        usr_PB_RETURN_ERROR(stream, "bytes overflow");

    if (size == 0)
    {
        /* As a special case, treat empty bytes string as all zeros for fixed_length_bytes. */
        memset(field->pData, 0, (size_t)field->data_size);
        return true;
    }

    if (size != field->data_size)
        usr_PB_RETURN_ERROR(stream, "incorrect fixed length bytes size");

    return usr_pb_read(stream, (usr_pb_byte_t*)field->pData, (size_t)field->data_size);
}

#ifdef usr_PB_CONVERT_DOUBLE_FLOAT
bool usr_pb_decode_double_as_float(usr_pb_istream_t *stream, float *dest)
{
    uint_least8_t sign;
    int exponent;
    uint32_t mantissa;
    uint64_t value;
    union { float f; uint32_t i; } out;

    if (!usr_pb_decode_fixed64(stream, &value))
        return false;

    /* Decompose input value */
    sign = (uint_least8_t)((value >> 63) & 1);
    exponent = (int)((value >> 52) & 0x7FF) - 1023;
    mantissa = (value >> 28) & 0xFFFFFF; /* Highest 24 bits */

    /* Figure if value is in range representable by floats. */
    if (exponent == 1024)
    {
        /* Special value */
        exponent = 128;
        mantissa >>= 1;
    }
    else
    {
        if (exponent > 127)
        {
            /* Too large, convert to infinity */
            exponent = 128;
            mantissa = 0;
        }
        else if (exponent < -150)
        {
            /* Too small, convert to zero */
            exponent = -127;
            mantissa = 0;
        }
        else if (exponent < -126)
        {
            /* Denormalized */
            mantissa |= 0x1000000;
            mantissa >>= (-126 - exponent);
            exponent = -127;
        }

        /* Round off mantissa */
        mantissa = (mantissa + 1) >> 1;

        /* Check if mantissa went over 2.0 */
        if (mantissa & 0x800000)
        {
            exponent += 1;
            mantissa &= 0x7FFFFF;
            mantissa >>= 1;
        }
    }

    /* Combine fields */
    out.i = mantissa;
    out.i |= (uint32_t)(exponent + 127) << 23;
    out.i |= (uint32_t)sign << 31;

    *dest = out.f;
    return true;
}
#endif
