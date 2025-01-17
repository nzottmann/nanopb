/* usr_pb_common.c: Common support functions for usr_pb_encode.c and usr_pb_decode.c.
 *
 * 2014 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "usr_pb_common.h"

static bool load_descriptor_values(usr_pb_field_iter_t *iter)
{
    uint32_t word0;
    uint32_t data_offset;
    int_least8_t size_offset;

    if (iter->index >= iter->descriptor->field_count)
        return false;

    word0 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index]);
    iter->type = (usr_pb_type_t)((word0 >> 8) & 0xFF);

    switch(word0 & 3)
    {
        case 0: {
            /* 1-word format */
            iter->array_size = 1;
            iter->tag = (usr_pb_size_t)((word0 >> 2) & 0x3F);
            size_offset = (int_least8_t)((word0 >> 24) & 0x0F);
            data_offset = (word0 >> 16) & 0xFF;
            iter->data_size = (usr_pb_size_t)((word0 >> 28) & 0x0F);
            break;
        }

        case 1: {
            /* 2-word format */
            uint32_t word1 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index + 1]);

            iter->array_size = (usr_pb_size_t)((word0 >> 16) & 0x0FFF);
            iter->tag = (usr_pb_size_t)(((word0 >> 2) & 0x3F) | ((word1 >> 28) << 6));
            size_offset = (int_least8_t)((word0 >> 28) & 0x0F);
            data_offset = word1 & 0xFFFF;
            iter->data_size = (usr_pb_size_t)((word1 >> 16) & 0x0FFF);
            break;
        }

        case 2: {
            /* 4-word format */
            uint32_t word1 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index + 1]);
            uint32_t word2 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index + 2]);
            uint32_t word3 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index + 3]);

            iter->array_size = (usr_pb_size_t)(word0 >> 16);
            iter->tag = (usr_pb_size_t)(((word0 >> 2) & 0x3F) | ((word1 >> 8) << 6));
            size_offset = (int_least8_t)(word1 & 0xFF);
            data_offset = word2;
            iter->data_size = (usr_pb_size_t)word3;
            break;
        }

        default: {
            /* 8-word format */
            uint32_t word1 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index + 1]);
            uint32_t word2 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index + 2]);
            uint32_t word3 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index + 3]);
            uint32_t word4 = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index + 4]);

            iter->array_size = (usr_pb_size_t)word4;
            iter->tag = (usr_pb_size_t)(((word0 >> 2) & 0x3F) | ((word1 >> 8) << 6));
            size_offset = (int_least8_t)(word1 & 0xFF);
            data_offset = word2;
            iter->data_size = (usr_pb_size_t)word3;
            break;
        }
    }

    if (!iter->message)
    {
        /* Avoid doing arithmetic on null pointers, it is undefined */
        iter->pField = NULL;
        iter->pSize = NULL;
    }
    else
    {
        iter->pField = (char*)iter->message + data_offset;

        if (size_offset)
        {
            iter->pSize = (char*)iter->pField - size_offset;
        }
        else if (usr_PB_HTYPE(iter->type) == usr_PB_HTYPE_REPEATED &&
                 (usr_PB_ATYPE(iter->type) == usr_PB_ATYPE_STATIC ||
                  usr_PB_ATYPE(iter->type) == usr_PB_ATYPE_POINTER))
        {
            /* Fixed count array */
            iter->pSize = &iter->array_size;
        }
        else
        {
            iter->pSize = NULL;
        }

        if (usr_PB_ATYPE(iter->type) == usr_PB_ATYPE_POINTER && iter->pField != NULL)
        {
            iter->pData = *(void**)iter->pField;
        }
        else
        {
            iter->pData = iter->pField;
        }
    }

    if (usr_PB_LTYPE_IS_SUBMSG(iter->type))
    {
        iter->submsg_desc = iter->descriptor->submsg_info[iter->submessage_index];
    }
    else
    {
        iter->submsg_desc = NULL;
    }

    return true;
}

static void advance_iterator(usr_pb_field_iter_t *iter)
{
    iter->index++;

    if (iter->index >= iter->descriptor->field_count)
    {
        /* Restart */
        iter->index = 0;
        iter->field_info_index = 0;
        iter->submessage_index = 0;
        iter->required_field_index = 0;
    }
    else
    {
        /* Increment indexes based on previous field type.
         * All field info formats have the following fields:
         * - lowest 2 bits tell the amount of words in the descriptor (2^n words)
         * - bits 2..7 give the lowest bits of tag number.
         * - bits 8..15 give the field type.
         */
        uint32_t prev_descriptor = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index]);
        usr_pb_type_t prev_type = (prev_descriptor >> 8) & 0xFF;
        usr_pb_size_t descriptor_len = (usr_pb_size_t)(1 << (prev_descriptor & 3));

        /* Add to fields.
         * The cast to usr_pb_size_t is needed to avoid -Wconversion warning.
         * Because the data is is constants from generator, there is no danger of overflow.
         */
        iter->field_info_index = (usr_pb_size_t)(iter->field_info_index + descriptor_len);
        iter->required_field_index = (usr_pb_size_t)(iter->required_field_index + (usr_PB_HTYPE(prev_type) == usr_PB_HTYPE_REQUIRED));
        iter->submessage_index = (usr_pb_size_t)(iter->submessage_index + usr_PB_LTYPE_IS_SUBMSG(prev_type));
    }
}

bool usr_pb_field_iter_begin(usr_pb_field_iter_t *iter, const usr_pb_msgdesc_t *desc, void *message)
{
    memset(iter, 0, sizeof(*iter));

    iter->descriptor = desc;
    iter->message = message;

    return load_descriptor_values(iter);
}

bool usr_pb_field_iter_begin_extension(usr_pb_field_iter_t *iter, usr_pb_extension_t *extension)
{
    const usr_pb_msgdesc_t *msg = (const usr_pb_msgdesc_t*)extension->type->arg;
    bool status;

    uint32_t word0 = usr_PB_PROGMEM_READU32(msg->field_info[0]);
    if (usr_PB_ATYPE(word0 >> 8) == usr_PB_ATYPE_POINTER)
    {
        /* For pointer extensions, the pointer is stored directly
         * in the extension structure. This avoids having an extra
         * indirection. */
        status = usr_pb_field_iter_begin(iter, msg, &extension->dest);
    }
    else
    {
        status = usr_pb_field_iter_begin(iter, msg, extension->dest);
    }

    iter->pSize = &extension->found;
    return status;
}

bool usr_pb_field_iter_next(usr_pb_field_iter_t *iter)
{
    advance_iterator(iter);
    (void)load_descriptor_values(iter);
    return iter->index != 0;
}

bool usr_pb_field_iter_find(usr_pb_field_iter_t *iter, uint32_t tag)
{
    if (iter->tag == tag)
    {
        return true; /* Nothing to do, correct field already. */
    }
    else if (tag > iter->descriptor->largest_tag)
    {
        return false;
    }
    else
    {
        usr_pb_size_t start = iter->index;
        uint32_t fieldinfo;

        if (tag < iter->tag)
        {
            /* Fields are in tag number order, so we know that tag is between
             * 0 and our start position. Setting index to end forces
             * advance_iterator() call below to restart from beginning. */
            iter->index = iter->descriptor->field_count;
        }

        do
        {
            /* Advance iterator but don't load values yet */
            advance_iterator(iter);

            /* Do fast check for tag number match */
            fieldinfo = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index]);

            if (((fieldinfo >> 2) & 0x3F) == (tag & 0x3F))
            {
                /* Good candidate, check further */
                (void)load_descriptor_values(iter);

                if (iter->tag == tag &&
                    usr_PB_LTYPE(iter->type) != usr_PB_LTYPE_EXTENSION)
                {
                    /* Found it */
                    return true;
                }
            }
        } while (iter->index != start);

        /* Searched all the way back to start, and found nothing. */
        (void)load_descriptor_values(iter);
        return false;
    }
}

bool usr_pb_field_iter_find_extension(usr_pb_field_iter_t *iter)
{
    if (usr_PB_LTYPE(iter->type) == usr_PB_LTYPE_EXTENSION)
    {
        return true;
    }
    else
    {
        usr_pb_size_t start = iter->index;
        uint32_t fieldinfo;

        do
        {
            /* Advance iterator but don't load values yet */
            advance_iterator(iter);

            /* Do fast check for field type */
            fieldinfo = usr_PB_PROGMEM_READU32(iter->descriptor->field_info[iter->field_info_index]);

            if (usr_PB_LTYPE((fieldinfo >> 8) & 0xFF) == usr_PB_LTYPE_EXTENSION)
            {
                return load_descriptor_values(iter);
            }
        } while (iter->index != start);

        /* Searched all the way back to start, and found nothing. */
        (void)load_descriptor_values(iter);
        return false;
    }
}

static void *usr_pb_const_cast(const void *p)
{
    /* Note: this casts away const, in order to use the common field iterator
     * logic for both encoding and decoding. The cast is done using union
     * to avoid spurious compiler warnings. */
    union {
        void *p1;
        const void *p2;
    } t;
    t.p2 = p;
    return t.p1;
}

bool usr_pb_field_iter_begin_const(usr_pb_field_iter_t *iter, const usr_pb_msgdesc_t *desc, const void *message)
{
    return usr_pb_field_iter_begin(iter, desc, usr_pb_const_cast(message));
}

bool usr_pb_field_iter_begin_extension_const(usr_pb_field_iter_t *iter, const usr_pb_extension_t *extension)
{
    return usr_pb_field_iter_begin_extension(iter, (usr_pb_extension_t*)usr_pb_const_cast(extension));
}

bool usr_pb_default_field_callback(usr_pb_istream_t *istream, usr_pb_ostream_t *ostream, const usr_pb_field_t *field)
{
    if (field->data_size == sizeof(usr_pb_callback_t))
    {
        usr_pb_callback_t *pCallback = (usr_pb_callback_t*)field->pData;

        if (pCallback != NULL)
        {
            if (istream != NULL && pCallback->funcs.decode != NULL)
            {
                return pCallback->funcs.decode(istream, field, &pCallback->arg);
            }

            if (ostream != NULL && pCallback->funcs.encode != NULL)
            {
                return pCallback->funcs.encode(ostream, field, &pCallback->arg);
            }
        }
    }

    return true; /* Success, but didn't do anything */

}

#ifdef usr_PB_VALIDATE_UTF8

/* This function checks whether a string is valid UTF-8 text.
 *
 * Algorithm is adapted from https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
 * Original copyright: Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/> 2005-03-30
 * Licensed under "Short code license", which allows use under MIT license or
 * any compatible with it.
 */

bool usr_pb_validate_utf8(const char *str)
{
    const usr_pb_byte_t *s = (const usr_pb_byte_t*)str;
    while (*s)
    {
        if (*s < 0x80)
        {
            /* 0xxxxxxx */
            s++;
        }
        else if ((s[0] & 0xe0) == 0xc0)
        {
            /* 110XXXXx 10xxxxxx */
            if ((s[1] & 0xc0) != 0x80 ||
                (s[0] & 0xfe) == 0xc0)                        /* overlong? */
                return false;
            else
                s += 2;
        }
        else if ((s[0] & 0xf0) == 0xe0)
        {
            /* 1110XXXX 10Xxxxxx 10xxxxxx */
            if ((s[1] & 0xc0) != 0x80 ||
                (s[2] & 0xc0) != 0x80 ||
                (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) ||    /* overlong? */
                (s[0] == 0xed && (s[1] & 0xe0) == 0xa0) ||    /* surrogate? */
                (s[0] == 0xef && s[1] == 0xbf &&
                (s[2] & 0xfe) == 0xbe))                 /* U+FFFE or U+FFFF? */
                return false;
            else
                s += 3;
        }
        else if ((s[0] & 0xf8) == 0xf0)
        {
            /* 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx */
            if ((s[1] & 0xc0) != 0x80 ||
                (s[2] & 0xc0) != 0x80 ||
                (s[3] & 0xc0) != 0x80 ||
                (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) ||    /* overlong? */
                (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) /* > U+10FFFF? */
                return false;
            else
                s += 4;
        }
        else
        {
            return false;
        }
    }

    return true;
}

#endif
