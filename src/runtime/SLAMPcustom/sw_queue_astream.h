/** ***********************************************/
/** *** SW Queue with Supporting Variable Size ****/
/** ***********************************************/
#pragma once

#include <cstdint>
#include <iostream>
#define DUALCORE

#include <cstdint>
#include <sys/mman.h>
#include <xmmintrin.h>
#include <smmintrin.h>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <thread>

#define ATTRIBUTE(x) __attribute__((x))
#define ATTRIBUTE(x) 

#include "inline.h"
#include "bitcast.h"
#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
/*#define CACHELINE_SIZE 64 */
/* Cache line size of glacier, as reported by lmbench. Other tests,
 * namely the smtx ones, vindicate this. */
#endif /* CACHELINE_SIZE */

#define QTYPE uint32_t
#ifndef QSIZE
#define QSIZE_BYTES (1 << 27) // 1 << 0 - 1 byte; 1 << 10 1KB; 1 << 20 1MB; 1 << 24 16MB; 1 << 26 64MB; 1 << 28 256MB; 1 << 30 1GB
#define QSIZE (QSIZE_BYTES / sizeof(QTYPE))
// #define QSIZE (1 << 23)
#endif /* QSIZE */

static constexpr uint64_t QSIZE_GUARD = QSIZE - 60;

#ifndef QPREFETCH
#define QPREFETCH (1 << 7)
#endif /* QPREFETCH */

#define PAD(suffix, size) char padding ## suffix [CACHELINE_SIZE - (size)]

struct UnderlyingQueue {
  volatile bool ready_to_read;
  PAD(1, sizeof(bool));
  volatile bool ready_to_write;
  PAD(2, sizeof(bool));
  uint64_t size;
  PAD(3, sizeof(uint64_t));
  uint32_t *data;

  void init(uint32_t *data){
    this->ready_to_read = false;
    this->ready_to_write = true;
    this->size = 0;
    this->data = data;
  }
};

using Queue = UnderlyingQueue;
using Queue_p = Queue *;

struct DoubleQueue {
  Queue_p qA, qB, qNow, qOther;
  uint64_t index = 0;
  uint64_t size = 0;
  uint32_t *data;

  std::mutex &m;
  std::condition_variable &cv;
  const unsigned ALL_THREADS;
  unsigned &running_threads;
  // uint32_t packet[4];
  __m128i packet;

  DoubleQueue(Queue_p dqA, Queue_p dqB, bool isConsumer, unsigned &threads,
      std::mutex &m, std::condition_variable &cv)
      : qA(dqA), qB(dqB), m(m), cv(cv), running_threads(threads), ALL_THREADS(threads) {
    this->qA = dqA;
    this->qB = dqB;

    // consumer
    if (isConsumer) {
      this->qNow = dqB;
      this->qOther = dqA;
    }
    // producer
    else {
      this->qNow = dqA;
      this->qOther = dqB;
    }

    this->data = qNow->data;
  }

  void swap(){
    if(qNow == qA){
      qNow = qB;
      qOther = qA;
    }else{
      qNow = qA;
      qOther = qB;
    }
    data = qNow->data;
  }

  void check() {
    if (index == size) {
      // only the last thread one does this
      auto lock = std::unique_lock<std::mutex>(m);
      // lock is locked
      if (running_threads == 1) {
        qNow->ready_to_read = false;
        qNow->ready_to_write = true;
        // std::cerr << "Thread " << std::this_thread::get_id() << " is waiting for queue" << std::endl;
        while (!qOther->ready_to_read) {
          // spin
          usleep(10);
        }
        qOther->ready_to_write = false;
        // std::cerr << "Thread " << std::this_thread::get_id() << " ready for queue" << std::endl;
        running_threads = ALL_THREADS;
        lock.unlock();
        // allow all other threads to continue
        cv.notify_all();
        // std::cerr << "Thread " << std::this_thread::get_id() << " is ready to go" << std::endl;
      }
      else {
        // lock is locked
        running_threads--;
        // std::cerr << "Thread " << std::this_thread::get_id() << " is waiting, threads: " << running_threads << std::endl;

        // wait fo the last thread to finish
        cv.wait(lock); // unlocks
        // std::cerr << "Thread " << std::this_thread::get_id() << " is unlocked" << std::endl;
        // lock reaquires
        lock.unlock();
        // std::cerr << "Thread " << std::this_thread::get_id() << " is ready to go" << std::endl;
      }
      swap();
      index = 0;
      size = qNow->size;
      // // make sure all pending writes are visible
      // _mm_mfence();
    }
  }

  // consume a 128 bit packet and return the opcode
  uint32_t consumePacket() {
    // load four 32 bit integers into uint32_t[4]
    // packet[0] = data[index++];
    // packet[1] = data[index++];
    // packet[2] = data[index++];
    // packet[3] = data[index++];

    // for (int i = 0; i < 16; i++) {
    //   std::cout << data[index + i] << " ";
    //   if (i % 4 == 3) {
    //     std::cout << std::endl;
    //   }
    // }

    packet = _mm_load_si128((__m128i *) &data[index]);
    // packet = _mm_stream_load_si128((__m128i *) &data[index]);
    index += 4;
    uint32_t v = _mm_extract_epi32(packet, 0);
    // get the least significant 8 bits
    return v & 0xFF;

    // return packet[0];
  }

  void unpack_32(uint32_t &a) {
    a = _mm_extract_epi32(packet, 1);
  }

  void unpack_64(uint64_t &c) {
    c = _mm_extract_epi64(packet, 1);
  }

  void unpack_24_32_64(uint32_t &a, uint32_t &b, uint64_t &c) {
    uint32_t tmp = _mm_extract_epi32(packet, 0);
    a = tmp >> 8;
    b = _mm_extract_epi32(packet, 1);
    c = _mm_extract_epi64(packet, 1);
  }

  void unpack_32_32(uint32_t &a, uint32_t &b) {
    // a = packet[1];
    // b = packet[2];
    a = _mm_extract_epi32(packet, 1);
    b = _mm_extract_epi32(packet, 2);
  }

  void unpack_32_64(uint32_t &a, uint64_t &c) {
    // std::cout << packet[0] << " " << packet[1] << " " << packet[2] << " " << packet[3] << std::endl;
    // a = packet[1];
    // c = ((uint64_t) packet[3] << 32) | packet[2];
    // c = ((uint64_t) packet[2] << 32) | packet[3];
    // c = *(uint64_t*) &packet[2];
    a = _mm_extract_epi32(packet, 1);
    c = _mm_extract_epi64(packet, 1);
  }

};

struct DoubleQueue_Producer {
  Queue_p qA, qB, qNow, qOther;
  uint64_t index = 0;
  uint64_t size = 0;
  uint32_t *data;

  // uint32_t packet[4];
  __m128i packet;

  DoubleQueue_Producer(Queue_p dqA, Queue_p dqB)
      : qA(dqA), qB(dqB) {
    this->qA = dqA;
    this->qB = dqB;

    // Producer
    this->qNow = dqA;
    this->qOther = dqB;

    this->data = qNow->data;
  }

  void swap(){
    if(qNow == qA){
      qNow = qB;
      qOther = qA;
    }else{
      qNow = qA;
      qOther = qB;
    }
    data = qNow->data;
  }

  void flush() {
    qNow->size = index;
    qNow->ready_to_read = true;
    qNow->ready_to_write = false;
  }

  void produce_wait() ATTRIBUTE(noinline){
    flush();
    while (!qOther->ready_to_write){
      // spin
      usleep(10);
    }
    swap();
    qNow->ready_to_read = false;
    index = 0;
    // total_swapped++;
  }

  // the packet is always 128bit, pad  with 0
  void produce_32(uint32_t x) ATTRIBUTE(noinline){
#ifdef MM_STREAM
    // _mm_stream_si128((__m128i *)(dq_data + dq_index), _mm_set_epi32(0, 0, 0, x));
    _mm_stream_si32((int *) &data[dq_index], x);
#else
    data[index] = x;
    // dq_data[dq_index + 1] = 0;
    // dq_data[dq_index + 2] = 0;
    // dq_data[dq_index + 3] = 0;
#endif
    index += 4;

    if (index >= QSIZE_GUARD) [[unlikely]] {
      produce_wait();
    }
  }

  void produce_8(uint8_t x) ATTRIBUTE(always_inline) {
    uint32_t tmp = x;
    produce_32(tmp);
  }

  void produce_8_24_32_64(uint8_t x, uint32_t y, uint32_t z, uint64_t w)
      ATTRIBUTE(noinline) {
    uint32_t xy = (y << 8) | x;
#ifdef MM_STREAM
    // _mm_stream_si128((__m128i *)(dq_data + dq_index), _mm_set_epi32(z, w, y,
    // x));
    _mm_stream_si32((int *)&dq_data[dq_index], xy);
    _mm_stream_si32((int *)&dq_data[dq_index + 1], z);
    _mm_stream_si64((long long *)&dq_data[dq_index + 2], w);
#else
    data[index] = xy;
    data[index + 1] = z;
    *((uint64_t *)&data[index + 2]) = w;
#endif
    index += 4;

    if (index >= QSIZE_GUARD) [[unlikely]] {
      produce_wait();
    }
  }

  void produce_32_32(uint32_t x, uint32_t y) ATTRIBUTE(noinline){
#ifdef MM_STREAM
    // _mm_stream_si128((__m128i *)(data + index), _mm_set_epi32(0, 0, y, x));
    _mm_stream_si32((int *) &data[index], x);
    _mm_stream_si32((int *) &data[index + 1], y);
#else
    data[index] = x;
    data[index + 1] = y;
    // data[index + 2] = 0;
    // data[index + 3] = 0;
#endif
    index += 4;

    if (index >= QSIZE_GUARD) [[unlikely]] {
      produce_wait();
    }
  }

  void produce_64_64(const uint64_t x, const uint64_t y) ATTRIBUTE(noinline){
#ifdef MM_STREAM
    _mm_stream_si128((__m128i *)(data + index), _mm_set_epi64x(y, x));
    // _mm_stream_si64((long long *) &data[index], x);
    // _mm_stream_si64((long long *) &data[index + 2], y);
#else
    *((uint64_t *) &data[index]) = x;
    *((uint64_t *) &data[index + 2]) = y;
#endif
    index += 4;

    if (index >= QSIZE_GUARD) [[unlikely]] {
      produce_wait();
    }
  }

  void produce_8_32(uint8_t x, uint32_t y) ATTRIBUTE(always_inline) {
    uint32_t x_tmp = x;
    produce_32_32(x_tmp, y);
  }

  void produce_8_64(uint8_t x, uint64_t y) ATTRIBUTE(always_inline) {
    uint32_t x_tmp = x;
    produce_64_64(x_tmp, y);
  }

  void produce_8_32_32(uint8_t x, uint32_t y, uint32_t z) ATTRIBUTE(always_inline) {
    uint32_t x_tmp = x;
    produce_32_32_32(x_tmp, y, z);
  }
  void produce_8_32_64(uint8_t x, uint32_t y, uint64_t z) ATTRIBUTE(always_inline) {
    uint32_t x_tmp = x;
    produce_32_32_64(x_tmp, y, z);
  }



  // static void produce_32_32_64(uint32_t x, uint32_t y, uint64_t z) {
  void produce_32_32_64(uint32_t x, uint32_t y, uint64_t z) ATTRIBUTE(noinline) {
#ifdef MM_STREAM
    // FIXME: set 32bit x, 32bit y, 64bit z, small endian
    _mm_stream_si32((int *) &data[index], x);
    _mm_stream_si32((int *) &data[index + 1], y);
    _mm_stream_si64((long long *) &data[index + 2], z);
    // _mm_stream_si128((__m128i *)(data + index), _mm_set_epi32( z >> 32, z & 0xFFFFFFFF, y, x));
#else
    data[index] = x;
    data[index+1] = y;
    *(uint64_t*)&data[index+2] = z;
#endif
    index += 4;
    if (index >= QSIZE_GUARD) [[unlikely]] {
      produce_wait();
    }
  }

  void produce_32_32_32(uint32_t x, uint32_t y, uint32_t z) ATTRIBUTE(noinline) {
#ifdef MM_STREAM
    _mm_stream_si128((__m128i *)(data + index), _mm_set_epi32(0, z, y, x));

    // _mm_stream_si32((int *) &data[index], x);
    // _mm_stream_si32((int *) &data[index+1], y);
    // _mm_stream_si32((int *) &data[index+2], z);
#else
    data[index] = x;
    data[index+1] = y;
    data[index+2] = z;
    data[index+3] = 0;
#endif
    index += 4;
    if (index >= QSIZE_GUARD) [[unlikely]] {
      produce_wait();
    }
  }
};
