/* Simple Plugin API
 * Copyright (C) 2017 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __SPA_POD_BUILDER_H__
#define __SPA_POD_BUILDER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <spa/pod.h>

typedef struct _SpaPODFrame {
  struct _SpaPODFrame *parent;
  SpaPOD               pod;
  uint32_t             ref;
} SpaPODFrame;

typedef struct _SpaPODBuilder {
  void        *data;
  uint32_t     size;
  uint32_t     offset;
  SpaPODFrame *stack;
  uint32_t    (*write) (struct _SpaPODBuilder *builder, uint32_t ref, const void *data, uint32_t size);
  bool         in_array;
  bool         first;
} SpaPODBuilder;

#define SPA_POD_BUILDER_DEREF(b,ref,type)    SPA_MEMBER ((b)->data, (ref), type)

static inline void
spa_pod_builder_init (SpaPODBuilder *builder,
                      void          *data,
                      uint32_t       size)
{
  builder->data = data;
  builder->size = size;
  builder->offset = 0;
  builder->stack = NULL;
}

static inline uint32_t
spa_pod_builder_push (SpaPODBuilder *builder,
                      SpaPODFrame   *frame,
                      const SpaPOD  *pod,
                      uint32_t       ref)
{
  frame->parent = builder->stack;
  frame->pod = *pod;
  frame->ref = ref;
  builder->stack = frame;
  builder->in_array = builder->first = (pod->type == SPA_POD_TYPE_ARRAY ||
                                        pod->type == SPA_POD_TYPE_PROP);
  return ref;
}

static inline void
spa_pod_builder_advance (SpaPODBuilder *builder, uint32_t size, bool pad)
{
  if (pad)
    size = SPA_ROUND_UP_N (size, 8);

  if (size > 0) {
    SpaPODFrame *f;

    builder->offset += size;
    for (f = builder->stack; f; f = f->parent)
      f->pod.size += size;
  }
}

static inline void
spa_pod_builder_pop (SpaPODBuilder *builder,
                     SpaPODFrame   *frame)
{
  if (frame->ref != -1) {
    if (builder->write)
      builder->write (builder, frame->ref, &frame->pod, sizeof(SpaPOD));
    else
      memcpy (builder->data + frame->ref, &frame->pod, sizeof(SpaPOD));
  }
  builder->stack = frame->parent;
  builder->in_array = (builder->stack && (builder->stack->pod.type == SPA_POD_TYPE_ARRAY ||
                                          builder->stack->pod.type == SPA_POD_TYPE_PROP));
  spa_pod_builder_advance (builder, SPA_ROUND_UP_N(builder->offset, 8) - builder->offset, false);
}

static inline uint32_t
spa_pod_builder_raw (SpaPODBuilder *builder, const void *data, uint32_t size, bool pad)
{
  uint32_t ref;

  if (builder->write) {
    ref = builder->write (builder, -1, data, size);
  } else {
    ref = builder->offset;
    if (ref + size > builder->size)
      ref = -1;
    else
      memcpy (builder->data + ref, data, size);
  }
  spa_pod_builder_advance (builder, size, pad);

  return ref;
}

static inline uint32_t
spa_pod_builder_string_body (SpaPODBuilder *builder,
                             const char    *str,
                             uint32_t       len)
{
  uint32_t out = spa_pod_builder_raw (builder, str, len, true);
  if (out != -1)
    *SPA_MEMBER (builder->data, out + len-1, char) = '\0';
  return out;
}

static inline uint32_t
spa_pod_builder_primitive (SpaPODBuilder *builder, const SpaPOD *p)
{
  const void *data;
  uint32_t size;

  if (builder->in_array && !builder->first) {
    data = SPA_POD_BODY_CONST (p);
    size = SPA_POD_BODY_SIZE (p);
  } else {
    data = p;
    size = SPA_POD_SIZE (p);
    builder->first = false;
  }
  return spa_pod_builder_raw (builder, data, size, !builder->in_array);
}

static inline uint32_t
spa_pod_builder_bool (SpaPODBuilder *builder, bool val)
{
  const SpaPODBool p = { { sizeof (uint32_t), SPA_POD_TYPE_BOOL }, val ? 1 : 0 };
  return spa_pod_builder_primitive (builder, &p.pod);
}

static inline uint32_t
spa_pod_builder_uri (SpaPODBuilder *builder, uint32_t val)
{
  const SpaPODURI p = { { sizeof (uint32_t), SPA_POD_TYPE_URI }, val };
  return spa_pod_builder_primitive (builder, &p.pod);
}

static inline uint32_t
spa_pod_builder_int (SpaPODBuilder *builder, int32_t val)
{
  const SpaPODInt p = { { sizeof (uint32_t), SPA_POD_TYPE_INT }, val };
  return spa_pod_builder_primitive (builder, &p.pod);
}

static inline uint32_t
spa_pod_builder_long (SpaPODBuilder *builder, int64_t val)
{
  const SpaPODLong p = { { sizeof (val), SPA_POD_TYPE_LONG }, val };
  return spa_pod_builder_primitive (builder, &p.pod);
}

static inline uint32_t
spa_pod_builder_float (SpaPODBuilder *builder, float val)
{
  const SpaPODFloat p = { { sizeof (val), SPA_POD_TYPE_FLOAT }, val };
  return spa_pod_builder_primitive (builder, &p.pod);
}

static inline uint32_t
spa_pod_builder_double (SpaPODBuilder *builder, double val)
{
  const SpaPODDouble p = { { sizeof (val), SPA_POD_TYPE_DOUBLE }, val };
  return spa_pod_builder_primitive (builder, &p.pod);
}

static inline uint32_t
spa_pod_builder_string_len (SpaPODBuilder *builder, const char *str, uint32_t len)
{
  const SpaPODString p = { { len, SPA_POD_TYPE_STRING } };
  uint32_t out = spa_pod_builder_raw (builder, &p, sizeof (p) , false);
  if (spa_pod_builder_string_body (builder, str, len) == -1)
    out = -1;
  return out;
}

static inline uint32_t
spa_pod_builder_string (SpaPODBuilder *builder, const char *str)
{
  uint32_t len = str ? strlen (str) : 0;
  return spa_pod_builder_string_len (builder, str ? str : "", len + 1);
}

static inline uint32_t
spa_pod_builder_bytes (SpaPODBuilder *builder, const void *bytes, uint32_t len)
{
  const SpaPODBytes p = { { len, SPA_POD_TYPE_BYTES } };
  uint32_t out = spa_pod_builder_raw (builder, &p, sizeof (p) , false);
  if (spa_pod_builder_raw (builder, bytes, len, true) == -1)
    out = -1;
  return out;
}

static inline uint32_t
spa_pod_builder_rectangle (SpaPODBuilder *builder, uint32_t width, uint32_t height)
{
  const SpaPODRectangle p = { { sizeof (SpaRectangle), SPA_POD_TYPE_RECTANGLE }, { width, height } };
  return spa_pod_builder_primitive (builder, &p.pod);
}

static inline uint32_t
spa_pod_builder_fraction (SpaPODBuilder *builder, uint32_t num, uint32_t denom)
{
  const SpaPODFraction p = { { sizeof (SpaFraction), SPA_POD_TYPE_FRACTION }, { num, denom } };
  return spa_pod_builder_primitive (builder, &p.pod);
}

static inline uint32_t
spa_pod_builder_push_array (SpaPODBuilder  *builder,
                            SpaPODFrame    *frame)
{
  const SpaPODArray p = { { sizeof (SpaPODArrayBody) - sizeof (SpaPOD), SPA_POD_TYPE_ARRAY }, { { 0, 0 } } };
  return spa_pod_builder_push (builder, frame, &p.pod,
                               spa_pod_builder_raw (builder, &p, sizeof(p) - sizeof(SpaPOD), false));
}

static inline uint32_t
spa_pod_builder_array (SpaPODBuilder *builder,
                       uint32_t       child_size,
                       uint32_t       child_type,
                       uint32_t       n_elems,
                       const void    *elems)
{
  const SpaPODArray p = {
    { (uint32_t)(sizeof (SpaPODArrayBody) + n_elems * child_size), SPA_POD_TYPE_ARRAY },
    { { child_size, child_type } }
  };
  uint32_t out = spa_pod_builder_raw (builder, &p, sizeof(p), true);
  if (spa_pod_builder_raw (builder, elems, child_size * n_elems, true) == -1)
    out = -1;
  return out;
}

static inline uint32_t
spa_pod_builder_push_struct (SpaPODBuilder  *builder,
                             SpaPODFrame    *frame)
{
  const SpaPODStruct p = { { 0, SPA_POD_TYPE_STRUCT } };
  return spa_pod_builder_push (builder, frame, &p.pod,
                               spa_pod_builder_raw (builder, &p, sizeof(p), false));
}

static inline uint32_t
spa_pod_builder_push_object (SpaPODBuilder  *builder,
                             SpaPODFrame    *frame,
                             uint32_t        id,
                             uint32_t        type)
{
  const SpaPODObject p = { { sizeof (SpaPODObjectBody), SPA_POD_TYPE_OBJECT }, { id, type } };
  return spa_pod_builder_push (builder, frame, &p.pod,
                               spa_pod_builder_raw (builder, &p, sizeof(p), false));
}

static inline uint32_t
spa_pod_builder_push_prop (SpaPODBuilder *builder,
                           SpaPODFrame   *frame,
                           uint32_t       key,
                           uint32_t       flags)
{
  const SpaPODProp p = { { sizeof (SpaPODPropBody) - sizeof(SpaPOD), SPA_POD_TYPE_PROP},
                         { key, flags | SPA_POD_PROP_RANGE_NONE, { 0, 0 } } };
  return spa_pod_builder_push (builder, frame, &p.pod,
                               spa_pod_builder_raw (builder, &p, sizeof(p) - sizeof(SpaPOD), false));
}

static inline void
spa_pod_builder_addv (SpaPODBuilder *builder,
                      uint32_t       type,
                      va_list        args)
{
  uint32_t n_values = 0;
  while (type != 0) {
    switch (type) {
      case SPA_POD_TYPE_INVALID:
        break;
      case SPA_POD_TYPE_BOOL:
        spa_pod_builder_bool (builder, va_arg (args, int));
        break;
      case SPA_POD_TYPE_URI:
        spa_pod_builder_uri (builder, va_arg (args, int32_t));
        break;
      case SPA_POD_TYPE_INT:
        spa_pod_builder_int (builder, va_arg (args, int32_t));
        break;
      case SPA_POD_TYPE_LONG:
        spa_pod_builder_long (builder, va_arg (args, int64_t));
        break;
      case SPA_POD_TYPE_FLOAT:
        spa_pod_builder_float (builder, va_arg (args, double));
        break;
      case SPA_POD_TYPE_DOUBLE:
        spa_pod_builder_double (builder, va_arg (args, double));
        break;
      case SPA_POD_TYPE_STRING:
        spa_pod_builder_string (builder, va_arg (args, char *));
        break;
      case -SPA_POD_TYPE_STRING:
      {
        char *str = va_arg (args, char *);
        uint32_t len = va_arg (args, uint32_t);
        spa_pod_builder_string_len (builder, str, len);
        break;
      }
      case SPA_POD_TYPE_RECTANGLE:
      {
        uint32_t width = va_arg (args, uint32_t), height = va_arg (args, uint32_t);
        spa_pod_builder_rectangle (builder, width, height);
        break;
      }
      case SPA_POD_TYPE_FRACTION:
      {
        uint32_t num = va_arg (args, uint32_t), denom = va_arg (args, uint32_t);
        spa_pod_builder_fraction (builder, num, denom);
        break;
      }
      case SPA_POD_TYPE_BITMASK:
        break;
      case SPA_POD_TYPE_ARRAY:
      {
        SpaPODFrame *f = va_arg (args, SpaPODFrame *);
        type = va_arg (args, uint32_t);
        n_values = va_arg (args, uint32_t);
        spa_pod_builder_push_array (builder, f);
        break;
      }
      case SPA_POD_TYPE_STRUCT:
      {
        SpaPODFrame *f = va_arg (args, SpaPODFrame *);
        spa_pod_builder_push_struct (builder, f);
        break;
      }
      case SPA_POD_TYPE_OBJECT:
      {
        SpaPODFrame *f = va_arg (args, SpaPODFrame *);
        uint32_t id = va_arg (args, uint32_t);
        uint32_t type = va_arg (args, uint32_t);
        spa_pod_builder_push_object (builder, f, id, type);
        break;
      }
      case SPA_POD_TYPE_PROP:
      {
        SpaPODFrame *f = va_arg (args, SpaPODFrame *);
        uint32_t key = va_arg (args, uint32_t);
        uint32_t flags = va_arg (args, uint32_t);
        type = va_arg (args, uint32_t);
        n_values = va_arg (args, uint32_t);
        spa_pod_builder_push_prop (builder, f, key, flags);
        break;
      }
      case -SPA_POD_TYPE_ARRAY:
      case -SPA_POD_TYPE_STRUCT:
      case -SPA_POD_TYPE_OBJECT:
      case -SPA_POD_TYPE_PROP:
      {
        SpaPODFrame *f = va_arg (args, SpaPODFrame *);
        spa_pod_builder_pop (builder, f);
        break;
      }
      case SPA_POD_TYPE_BYTES:
      {
        void *value = va_arg (args, void *);
        uint32_t size = va_arg (args, uint32_t);
        spa_pod_builder_bytes (builder, value, size);
        break;
      }
      case SPA_POD_TYPE_POD:
      {
        SpaPOD *value = va_arg (args, SpaPOD *);
        spa_pod_builder_raw (builder, value, SPA_POD_SIZE (value), true);
        break;
      }
    }
    if (n_values > 0)
      n_values--;
    else
      type = va_arg (args, uint32_t);
  }
}

static inline void
spa_pod_builder_add (SpaPODBuilder *builder,
                     uint32_t       type, ...)
{
  va_list args;

  va_start (args, type);
  spa_pod_builder_addv (builder, type, args);
  va_end (args);
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* __SPA_POD_BUILDER_H__ */
