#ifndef UTIL_FIFO_H
#define UTIL_FIFO_H


#include <Arduino.h>
#include <stddef.h>

#define xt_rsil(level) (__extension__({uint32_t state; __asm__ __volatile__("rsil %0," __STRINGIFY(level) : "=a" (state)); state;}))
#define xt_wsr_ps(state)  __asm__ __volatile__("wsr %0,ps; isync" :: "a" (state) : "memory")

#define RB_ATOMIC_START do { uint32_t _savedIS = xt_rsil(1) ;
#define RB_ATOMIC_END xt_wsr_ps(_savedIS) ;} while(0);

#if defined(BOARD_TTWR) || defined(BOARD_TTWR_V1)
typedef struct FIFOBuffer
{
  uint16_t *begin;
  uint16_t *end;
  uint16_t * volatile head;
  uint16_t * volatile tail;
} FIFOBuffer;
#else
typedef struct FIFOBuffer
{
  unsigned char *begin;
  unsigned char *end;
  unsigned char * volatile head;
  unsigned char * volatile tail;
} FIFOBuffer;
#endif

inline bool fifo_isempty(const FIFOBuffer *f) {
  return f->head == f->tail;
}

inline bool fifo_isfull(const FIFOBuffer *f) {
  return ((f->head == f->begin) && (f->tail == f->end)) || (f->tail == f->head - 1);
}

#if defined(BOARD_TTWR) || defined(BOARD_TTWR_V1)
inline void fifo_push(FIFOBuffer *f, uint16_t c) {
#else
inline void fifo_push(FIFOBuffer *f, unsigned char c) {
#endif
  *(f->tail) = c;
  
  if (f->tail == f->end) {
    f->tail = f->begin;
  } else {
    f->tail++;
  }
}

#if defined(BOARD_TTWR) || defined(BOARD_TTWR_V1)
inline uint16_t fifo_pop(FIFOBuffer *f) {
#else
inline unsigned char fifo_pop(FIFOBuffer *f) {
#endif
  uint16_t c;
  if(f->head == f->end) {
    f->head = f->begin;
    return *(f->end);
  } else { 
    c=*(f->head);
    if(!(f->head==f->tail)) f->head++;
    return c;
  }
}

inline void fifo_flush(FIFOBuffer *f) {
#if defined(BOARD_TTWR) || defined(BOARD_TTWR_V1)
  f->head = f->tail = f->begin;
#else
  f->head = f->tail;
#endif
}

inline bool fifo_isempty_locked(const FIFOBuffer *f) {
  bool result;
  RB_ATOMIC_START
  {
    result = fifo_isempty(f);
  }
  RB_ATOMIC_END
  return result;
}

inline bool fifo_isfull_locked(const FIFOBuffer *f) {
  bool result;
  RB_ATOMIC_START
  {
    result = fifo_isfull(f);
   }
  RB_ATOMIC_END
  return result;
}

#if defined(BOARD_TTWR) || defined(BOARD_TTWR_V1)
inline void fifo_push_locked(FIFOBuffer *f, uint16_t c) {
#else
inline void fifo_push_locked(FIFOBuffer *f, unsigned char c) {
#endif
  RB_ATOMIC_START
  {
    fifo_push(f, c);
   }
  RB_ATOMIC_END
}

#if defined(BOARD_TTWR) || defined(BOARD_TTWR_V1)
inline uint16_t fifo_pop_locked(FIFOBuffer *f) {
#else
inline unsigned char fifo_pop_locked(FIFOBuffer *f) {
#endif
  uint16_t c;
  RB_ATOMIC_START
  {
    c = fifo_pop(f);
   }
  RB_ATOMIC_END
  return c;
}

#if defined(BOARD_TTWR) || defined(BOARD_TTWR_V1)
inline void fifo_init(FIFOBuffer *f, uint16_t *buffer, size_t size) {
#else
inline void fifo_init(FIFOBuffer *f, unsigned char *buffer, size_t size) {
#endif
  f->head = f->tail = f->begin = buffer;
  f->end = buffer + size -1;
}

inline size_t fifo_len(FIFOBuffer *f) {
  //return f->end - f->begin;
  if(f->tail>f->head){
    return f->tail-f->head;
  }else{
    return (f->end-f->head)+(f->tail-f->begin);
  }
  return 0;
}

#endif