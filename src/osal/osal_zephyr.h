/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_OSAL_FREERTOS_H_
#define TUSB_OSAL_FREERTOS_H_

// // FreeRTOS Headers
// #include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,FreeRTOS.h)
// #include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,semphr.h)
// #include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,queue.h)
// #include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,task.h)

#include "osal.h"
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+

static k_timeout_t msec2wait(uint32_t msec)
{
	k_timeout_t timeout = K_NO_WAIT;

	if (msec == OSAL_TIMEOUT_WAIT_FOREVER) {
		// timeout = K_FOREVER; // 永久等待业务逻辑无法正常运行
    timeout = K_MSEC(10);
	} else if (msec == OSAL_TIMEOUT_NOTIMEOUT) {
		timeout = K_NO_WAIT;
	} else {
		timeout = K_MSEC(msec);
	}

	return timeout;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_task_delay(uint32_t msec) {
  k_sleep(msec2wait(msec));
}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+

typedef struct k_sem  osal_semaphore_def_t;
typedef struct k_sem* osal_semaphore_t;

TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t *semdef) {

  // 计数设置为0并将其限制设置为1来将其配置为二进制信号量。
  return (k_sem_init(semdef, 0, 1) == 0) ? (osal_semaphore_t) semdef : NULL;

}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_delete(osal_semaphore_t semd_hdl) {
  (void) semd_hdl;
  return true; // nothing to do
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr) {

  (void) in_isr;

  k_sem_give(sem_hdl);

  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec) {
  return k_sem_take(sem_hdl, msec2wait(msec))==0;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_semaphore_reset(osal_semaphore_t const sem_hdl) {
  k_sem_reset(sem_hdl);
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+

typedef struct k_mutex osal_mutex_def_t;
typedef struct k_mutex* osal_mutex_t;

TU_ATTR_ALWAYS_INLINE static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t *mdef) {
  return (k_mutex_init(mdef) == 0) ? (osal_mutex_t) mdef : NULL;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_delete(osal_mutex_t mutex_hdl) {
  (void) mutex_hdl;
  return true; // nothing to do
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_lock(osal_mutex_t mutex_hdl,
							 uint32_t msec)
{
	return k_mutex_lock(mutex_hdl, msec2wait(msec)) == 0;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl) {
  return k_mutex_unlock(mutex_hdl)==0;
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+


typedef struct
{
  uint32_t depth; // 深度
  uint32_t item_sz; // 每个项目的大小
  void*    buf;

  struct k_msgq msgq;

} osal_queue_def_t;

// _int_set is not used with an RTOS
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
  static _type _name##_##buf[_depth];\
  osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf}

typedef osal_queue_def_t* osal_queue_t;

TU_ATTR_ALWAYS_INLINE static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef) {
	if (0 != k_msgq_alloc_init(&qdef->msgq, qdef->item_sz, qdef->depth)) {
		return NULL;
	}

	return (osal_queue_t)qdef;

	// // 使用malloc分配内存
	// char *msgq_buffer = (char *)malloc(qdef->depth * qdef->item_sz);
	// // 释放内存
	// free(msgq_buffer);
	// k_msgq_init(&qdef->msgq, msgq_buffer, qdef->item_sz, qdef->depth);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_delete(osal_queue_t qhdl) {
  return k_msgq_cleanup(&qhdl->msgq)==0; 
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec) {
   return k_msgq_get(&qhdl->msgq, data, msec2wait(msec))==0;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_send(osal_queue_t qhdl, void const *data, bool in_isr) {
  return k_msgq_put(&qhdl->msgq, data, K_NO_WAIT)==0;

}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_empty(osal_queue_t qhdl) {
  return k_msgq_num_used_get(&qhdl->msgq) == 0;
}

// //--------------------------------------------------------------------+
// // dcache API
// //--------------------------------------------------------------------+

// #include <zephyr/cache.h>

// bool hcd_dcache_clean(void const *addr, uint32_t data_size) {
//   (void) addr;
//   (void) data_size;

//   sys_cache_data_flush_range((void *) addr, data_size);

//   return true;
// }

// bool hcd_dcache_invalidate(void const *addr, uint32_t data_size) {
//   (void) addr;
//   (void) data_size;

//   sys_cache_data_invd_range((void *) addr, data_size);

//   return true;
// }

// bool hcd_dcache_clean_invalidate(void const *addr,
//                                  uint32_t data_size) {
//   (void) addr;
//   (void) data_size;

//   sys_cache_data_flush_and_invd_range((void *) addr, data_size);

//   return true;
// }


#ifdef __cplusplus
}
#endif

#endif
