/*
 * test_fifo.c
 *
 *  Created on: Jan 15, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#include "unity.h"
#include "fifo.h"

#define FIFO_SIZE 10
static fifo_t ff;
static uint8_t buffer[FIFO_SIZE];

void setUp(void)
{
  fifo_init(&ff, buffer, FIFO_SIZE, 0);
}

void tearDown(void)
{
  memset(&ff, 0, sizeof(fifo_t));
}
void test_create_null(void)
{
  memset(&ff, 0, sizeof(fifo_t)); // clear fifo to test null created
  TEST_ASSERT_FALSE( fifo_init(&ff, buffer, 0, 0) );
  TEST_ASSERT_TRUE( fifo_init(&ff, buffer, 1, 0) );
}

void test_normal(void)
{
  uint8_t i;

  for(i=0; i < FIFO_SIZE; i++)
  {
    fifo_write(&ff, i);
  }

  for(i=0; i < FIFO_SIZE; i++)
  {
    uint8_t c;
    fifo_read(&ff, &c);
    TEST_ASSERT_EQUAL(i, c);
  }
}

void test_is_empty(void)
{
  TEST_ASSERT_TRUE(fifo_is_empty(&ff));
  fifo_write(&ff, 1);
  TEST_ASSERT_FALSE(fifo_is_empty(&ff));
}

void test_is_full(void)
{
  uint8_t i;

  TEST_ASSERT_FALSE(fifo_is_full(&ff));

  for(i=0; i < FIFO_SIZE; i++)
  {
    fifo_write(&ff, i);
  }

  TEST_ASSERT_TRUE(fifo_is_full(&ff));
}
