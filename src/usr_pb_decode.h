/* usr_pb_decode.h: Functions to decode protocol buffers. Depends on usr_pb_decode.c.
 * The main function is usr_pb_decode. You also need an input stream, and the
 * field descriptions created by nanousr_pb_generator.py.
 */

#ifndef usr_PB_DECODE_H_INCLUDED
#define usr_PB_DECODE_H_INCLUDED

#include "usr_pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Structure for defining custom input streams. You will need to provide
 * a callback function to read the bytes from your storage, which can be
 * for example a file or a network socket.
 * 
 * The callback must conform to these rules:
 *
 * 1) Return false on IO errors. This will cause decoding to abort.
 * 2) You can use state to store your own data (e.g. buffer pointer),
 *    and rely on usr_pb_read to verify that no-body reads past bytes_left.
 * 3) Your callback may be used with substreams, in which case bytes_left
 *    is different than from the main stream. Don't use bytes_left to compute
 *    any pointers.
 */
struct usr_pb_istream_s
{
#ifdef usr_PB_BUFFER_ONLY
    /* Callback pointer is not used in buffer-only configuration.
     * Having an int pointer here allows binary compatibility but
     * gives an error if someone tries to assign callback function.
     */
    int *callback;
#else
    bool (*callback)(usr_pb_istream_t *stream, usr_pb_byte_t *buf, size_t count);
#endif

    void *state; /* Free field for use by callback implementation */
    size_t bytes_left;
    
#ifndef usr_PB_NO_ERRMSG
    const char *errmsg;
#endif
};

#ifndef usr_PB_NO_ERRMSG
#define usr_PB_ISTREAM_EMPTY {0,0,0,0}
#else
#define usr_PB_ISTREAM_EMPTY {0,0,0}
#endif

/***************************
 * Main decoding functions *
 ***************************/
 
/* Decode a single protocol buffers message from input stream into a C structure.
 * Returns true on success, false on any failure.
 * The actual struct pointed to by dest must match the description in fields.
 * Callback fields of the destination structure must be initialized by caller.
 * All other fields will be initialized by this function.
 *
 * Example usage:
 *    MyMessage msg = {};
 *    uint8_t buffer[64];
 *    usr_pb_istream_t stream;
 *    
 *    // ... read some data into buffer ...
 *
 *    stream = usr_pb_istream_from_buffer(buffer, count);
 *    usr_pb_decode(&stream, MyMessage_fields, &msg);
 */
bool usr_pb_decode(usr_pb_istream_t *stream, const usr_pb_msgdesc_t *fields, void *dest_struct);

/* Extended version of usr_pb_decode, with several options to control
 * the decoding process:
 *
 * usr_PB_DECODE_NOINIT:         Do not initialize the fields to default values.
 *                           This is slightly faster if you do not need the default
 *                           values and instead initialize the structure to 0 using
 *                           e.g. memset(). This can also be used for merging two
 *                           messages, i.e. combine already existing data with new
 *                           values.
 *
 * usr_PB_DECODE_DELIMITED:      Input message starts with the message size as varint.
 *                           Corresponds to parseDelimitedFrom() in Google's
 *                           protobuf API.
 *
 * usr_PB_DECODE_NULLTERMINATED: Stop reading when field tag is read as 0. This allows
 *                           reading null terminated messages.
 *                           NOTE: Until nanopb-0.4.0, usr_pb_decode() also allows
 *                           null-termination. This behaviour is not supported in
 *                           most other protobuf implementations, so usr_PB_DECODE_DELIMITED
 *                           is a better option for compatibility.
 *
 * Multiple flags can be combined with bitwise or (| operator)
 */
#define usr_PB_DECODE_NOINIT          0x01U
#define usr_PB_DECODE_DELIMITED       0x02U
#define usr_PB_DECODE_NULLTERMINATED  0x04U
bool usr_pb_decode_ex(usr_pb_istream_t *stream, const usr_pb_msgdesc_t *fields, void *dest_struct, unsigned int flags);

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define usr_pb_decode_noinit(s,f,d) usr_pb_decode_ex(s,f,d, usr_PB_DECODE_NOINIT)
#define usr_pb_decode_delimited(s,f,d) usr_pb_decode_ex(s,f,d, usr_PB_DECODE_DELIMITED)
#define usr_pb_decode_delimited_noinit(s,f,d) usr_pb_decode_ex(s,f,d, usr_PB_DECODE_DELIMITED | usr_PB_DECODE_NOINIT)
#define usr_pb_decode_nullterminated(s,f,d) usr_pb_decode_ex(s,f,d, usr_PB_DECODE_NULLTERMINATED)

#ifdef usr_PB_ENABLE_MALLOC
/* Release any allocated pointer fields. If you use dynamic allocation, you should
 * call this for any successfully decoded message when you are done with it. If
 * usr_pb_decode() returns with an error, the message is already released.
 */
void usr_pb_release(const usr_pb_msgdesc_t *fields, void *dest_struct);
#else
/* Allocation is not supported, so release is no-op */
#define usr_pb_release(fields, dest_struct) usr_PB_UNUSED(fields); usr_PB_UNUSED(dest_struct);
#endif


/**************************************
 * Functions for manipulating streams *
 **************************************/

/* Create an input stream for reading from a memory buffer.
 *
 * msglen should be the actual length of the message, not the full size of
 * allocated buffer.
 *
 * Alternatively, you can use a custom stream that reads directly from e.g.
 * a file or a network socket.
 */
usr_pb_istream_t usr_pb_istream_from_buffer(const usr_pb_byte_t *buf, size_t msglen);

/* Function to read from a usr_pb_istream_t. You can use this if you need to
 * read some custom header data, or to read data in field callbacks.
 */
bool usr_pb_read(usr_pb_istream_t *stream, usr_pb_byte_t *buf, size_t count);


/************************************************
 * Helper functions for writing field callbacks *
 ************************************************/

/* Decode the tag for the next field in the stream. Gives the wire type and
 * field tag. At end of the message, returns false and sets eof to true. */
bool usr_pb_decode_tag(usr_pb_istream_t *stream, usr_pb_wire_type_t *wire_type, uint32_t *tag, bool *eof);

/* Skip the field payload data, given the wire type. */
bool usr_pb_skip_field(usr_pb_istream_t *stream, usr_pb_wire_type_t wire_type);

/* Decode an integer in the varint format. This works for enum, int32,
 * int64, uint32 and uint64 field types. */
#ifndef usr_PB_WITHOUT_64BIT
bool usr_pb_decode_varint(usr_pb_istream_t *stream, uint64_t *dest);
#else
#define usr_pb_decode_varint usr_pb_decode_varint32
#endif

/* Decode an integer in the varint format. This works for enum, int32,
 * and uint32 field types. */
bool usr_pb_decode_varint32(usr_pb_istream_t *stream, uint32_t *dest);

/* Decode a bool value in varint format. */
bool usr_pb_decode_bool(usr_pb_istream_t *stream, bool *dest);

/* Decode an integer in the zig-zagged svarint format. This works for sint32
 * and sint64. */
#ifndef usr_PB_WITHOUT_64BIT
bool usr_pb_decode_svarint(usr_pb_istream_t *stream, int64_t *dest);
#else
bool usr_pb_decode_svarint(usr_pb_istream_t *stream, int32_t *dest);
#endif

/* Decode a fixed32, sfixed32 or float value. You need to pass a pointer to
 * a 4-byte wide C variable. */
bool usr_pb_decode_fixed32(usr_pb_istream_t *stream, void *dest);

#ifndef usr_PB_WITHOUT_64BIT
/* Decode a fixed64, sfixed64 or double value. You need to pass a pointer to
 * a 8-byte wide C variable. */
bool usr_pb_decode_fixed64(usr_pb_istream_t *stream, void *dest);
#endif

#ifdef usr_PB_CONVERT_DOUBLE_FLOAT
/* Decode a double value into float variable. */
bool usr_pb_decode_double_as_float(usr_pb_istream_t *stream, float *dest);
#endif

/* Make a limited-length substream for reading a usr_PB_WT_STRING field. */
bool usr_pb_make_string_substream(usr_pb_istream_t *stream, usr_pb_istream_t *substream);
bool usr_pb_close_string_substream(usr_pb_istream_t *stream, usr_pb_istream_t *substream);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
