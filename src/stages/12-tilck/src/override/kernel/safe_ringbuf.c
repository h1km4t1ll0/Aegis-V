/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * WARNING: despite the name, safe_ringbuf is not SAFE in every use pattern.
 * Read at the documentation in the header file: safe_ringbuf.h.
 */

/*
 * Aegis-V: C version of Tilck's kernel/safe_ringbuf.cpp (tcc has no C++).
 * The templates __safe_ringbuf_write<n> and __safe_ringbuf_read<n> become
 * the macro-made functions __safe_ringbuf_write_<n> / __safe_ringbuf_read_<n>
 * for n = 0 (memcpy of rb->elem_size bytes), 1, 2, 4, 8 (u8 ... u64). The
 * extern "C" functions keep their names and signatures.
 */

#include <tilck/common/basic_defs.h>
#include <tilck/common/string_util.h>
#include <tilck/common/atomics.h>
#include <tilck/kernel/safe_ringbuf.h>
#include <tilck/kernel/kmalloc.h>
#include <tilck/kernel/hal.h>

static ALWAYS_INLINE bool
rb_stat_is_empty(struct generic_safe_ringbuf_stat *s)
{
   return s->read_pos == s->write_pos && !s->full;
}

bool safe_ringbuf_is_empty(struct safe_ringbuf *rb)
{
   bool res;
   ulong var;

   disable_interrupts(&var);
   {
      res = rb_stat_is_empty(&rb->s);
   }
   enable_interrupts(&var);
   return res;
}

bool safe_ringbuf_is_full(struct safe_ringbuf *rb)
{
   struct generic_safe_ringbuf_stat cs;
   cs.__raw = atomic_load(&rb->s.raw);
   return cs.full;
}

static ALWAYS_INLINE void
begin_debug_write_checks(struct safe_ringbuf *rb)
{
   DEBUG_ONLY(atomic_fetch_add(&rb->nested_writes, 1));
}

static ALWAYS_INLINE void
end_debug_write_checks(struct safe_ringbuf *rb)
{
   DEBUG_ONLY(atomic_fetch_sub(&rb->nested_writes, 1));
}

static ALWAYS_INLINE void
begin_debug_read_checks(struct safe_ringbuf *rb)
{
#if DEBUG_CHECKS

   int nw = atomic_load(&rb->nested_writes);

   if (nw)
      panic("Read from safe_ringbuf interrupted on-going write. Not supported");

#endif
}

static ALWAYS_INLINE void
end_debug_read_checks(struct safe_ringbuf *rb)
{
   /* Do nothing, at the moment */
}

void
safe_ringbuf_init(struct safe_ringbuf *rb, u16 max_elems, u16 e_size, void *buf)
{
   ASSERT(max_elems <= 32768);

   rb->max_elems = max_elems;
   rb->elem_size = e_size;
   rb->buf = (u8 *)buf;
   atomic_store(&rb->s.raw, 0);

#if DEBUG_CHECKS
   atomic_store(&rb->nested_writes, 0);
#endif
}

void safe_ringbuf_destory(struct safe_ringbuf *rb)
{
   bzero(rb, sizeof(struct safe_ringbuf));
}

/* static_elem_size = n; T = unsigned_by_size<n>::type (unused for n = 0) */
#define DEF_SAFE_RINGBUF_WRITE(static_elem_size, T)                         \
static ALWAYS_INLINE bool                                                   \
__safe_ringbuf_write_##static_elem_size(struct safe_ringbuf *rb,            \
                                        void *elem_ptr,                     \
                                        bool *was_empty)                    \
{                                                                           \
   struct generic_safe_ringbuf_stat cs, ns;                                 \
   const u16 e_size = rb->elem_size;                                        \
   bool ret = true;                                                         \
   begin_debug_write_checks(rb);                                            \
                                                                            \
   do {                                                                     \
                                                                            \
      cs.__raw = rb->s.__raw;                                               \
      ns.__raw = rb->s.__raw;                                               \
                                                                            \
      if (UNLIKELY(cs.full)) {                                              \
         *was_empty = false;                                                \
         ret = false;                                                       \
         goto out;                                                          \
      }                                                                     \
                                                                            \
      ns.write_pos = (ns.write_pos + 1) % rb->max_elems;                    \
                                                                            \
      if (ns.write_pos == ns.read_pos)                                      \
         ns.full = true;                                                    \
                                                                            \
   } while (!atomic_cas_weak(&rb->s.raw,                                    \
                                 &cs.__raw,                                 \
                                 ns.__raw));                                \
                                                                            \
   if (static_elem_size)                                                    \
      ((T *)rb->buf)[cs.write_pos] = *(T *)elem_ptr;                        \
   else                                                                     \
      memcpy(rb->buf + cs.write_pos * e_size, elem_ptr, e_size);            \
                                                                            \
   *was_empty = rb_stat_is_empty(&cs);                                      \
                                                                            \
out:                                                                        \
   end_debug_write_checks(rb);                                              \
   return ret;                                                              \
}

#define DEF_SAFE_RINGBUF_READ(static_elem_size, T)                          \
static ALWAYS_INLINE bool                                                   \
__safe_ringbuf_read_##static_elem_size(struct safe_ringbuf *rb,             \
                                       void *elem_ptr /* out */)            \
{                                                                           \
   struct generic_safe_ringbuf_stat cs, ns;                                 \
   const u16 e_size = rb->elem_size;                                        \
   bool ret = true;                                                         \
                                                                            \
   if (static_elem_size)                                                    \
      ASSERT(rb->elem_size == static_elem_size);                            \
                                                                            \
   begin_debug_read_checks(rb);                                             \
                                                                            \
   do {                                                                     \
                                                                            \
      cs.__raw = rb->s.__raw;                                               \
      ns.__raw = rb->s.__raw;                                               \
                                                                            \
      if (rb_stat_is_empty(&cs)) {                                          \
         ret = false;                                                       \
         goto out;                                                          \
      }                                                                     \
                                                                            \
      if (static_elem_size)                                                 \
         *(T *)elem_ptr = ((T *)rb->buf)[cs.read_pos];                      \
      else                                                                  \
         memcpy(elem_ptr, rb->buf + cs.read_pos * e_size, e_size);          \
                                                                            \
      ns.read_pos = (ns.read_pos + 1) % rb->max_elems;                      \
      ns.full = false;                                                      \
                                                                            \
   } while (!atomic_cas_weak(&rb->s.raw,                                    \
                                 &cs.__raw,                                 \
                                 ns.__raw));                                \
                                                                            \
out:                                                                        \
   end_debug_read_checks(rb);                                               \
   return ret;                                                              \
}

DEF_SAFE_RINGBUF_WRITE(0, u8)
DEF_SAFE_RINGBUF_WRITE(1, u8)
DEF_SAFE_RINGBUF_WRITE(2, u16)
DEF_SAFE_RINGBUF_WRITE(4, u32)
DEF_SAFE_RINGBUF_WRITE(8, u64)

DEF_SAFE_RINGBUF_READ(0, u8)
DEF_SAFE_RINGBUF_READ(1, u8)
DEF_SAFE_RINGBUF_READ(2, u16)
DEF_SAFE_RINGBUF_READ(4, u32)
DEF_SAFE_RINGBUF_READ(8, u64)

bool
safe_ringbuf_write_elem(struct safe_ringbuf *rb, void *e, bool *was_empty)
{
   return __safe_ringbuf_write_0(rb, e, was_empty);
}

bool
safe_ringbuf_read_elem(struct safe_ringbuf *rb, void *elem_ptr /* out */)
{
   return __safe_ringbuf_read_0(rb, elem_ptr);
}

#define INST_WRITE_FUNC(s, n)                                                 \
   bool safe_ringbuf_write_##s(struct safe_ringbuf *rb,void *e,bool *empty) { \
      return __safe_ringbuf_write_##n(rb, e, empty);                          \
   }

#define INST_READ_FUNC(s, n)                                             \
   bool safe_ringbuf_read_##s(struct safe_ringbuf *rb, void *elem_ptr) { \
      return __safe_ringbuf_read_##n(rb, elem_ptr);                      \
   }


// For the moment, the following instantiations are NOT needed in Tilck.
// No point in adding code-bloat to the kernel. As a use-cases come out,
// un-comment the individual functions.

// INST_WRITE_FUNC(ulong, 8)
INST_WRITE_FUNC(1, 1)
// INST_WRITE_FUNC(2, 2)
// INST_WRITE_FUNC(4, 4)
// INST_WRITE_FUNC(8, 8)

// INST_READ_FUNC(ulong, 8)
INST_READ_FUNC(1, 1)
// INST_READ_FUNC(2, 2)
// INST_READ_FUNC(4, 4)
// INST_READ_FUNC(8, 8)
