/* usr_pb_common.h: Common support functions for usr_pb_encode.c and usr_pb_decode.c.
 * These functions are rarely needed by applications directly.
 */

#ifndef usr_PB_COMMON_H_INCLUDED
#define usr_PB_COMMON_H_INCLUDED

#include "usr_pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the field iterator structure to beginning.
 * Returns false if the message type is empty. */
bool usr_pb_field_iter_begin(usr_pb_field_iter_t *iter, const usr_pb_msgdesc_t *desc, void *message);

/* Get a field iterator for extension field. */
bool usr_pb_field_iter_begin_extension(usr_pb_field_iter_t *iter, usr_pb_extension_t *extension);

/* Same as usr_pb_field_iter_begin(), but for const message pointer.
 * Note that the pointers in usr_pb_field_iter_t will be non-const but shouldn't
 * be written to when using these functions. */
bool usr_pb_field_iter_begin_const(usr_pb_field_iter_t *iter, const usr_pb_msgdesc_t *desc, const void *message);
bool usr_pb_field_iter_begin_extension_const(usr_pb_field_iter_t *iter, const usr_pb_extension_t *extension);

/* Advance the iterator to the next field.
 * Returns false when the iterator wraps back to the first field. */
bool usr_pb_field_iter_next(usr_pb_field_iter_t *iter);

/* Advance the iterator until it points at a field with the given tag.
 * Returns false if no such field exists. */
bool usr_pb_field_iter_find(usr_pb_field_iter_t *iter, uint32_t tag);

/* Find a field with type usr_PB_LTYPE_EXTENSION, or return false if not found.
 * There can be only one extension range field per message. */
bool usr_pb_field_iter_find_extension(usr_pb_field_iter_t *iter);

#ifdef usr_PB_VALIDATE_UTF8
/* Validate UTF-8 text string */
bool usr_pb_validate_utf8(const char *s);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
