/*
 * Copyright 2013-2019 Software Radio Systems Limited
 *
 * This file is part of srsLTE.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srslte/asn1/liblte_mme.h"
#include "srslte/common/log_filter.h"
#include "srsue/hdr/stack/upper/tft_packet_filter.h"
#include <iostream>
#include <memory>
#include <srslte/common/buffer_pool.h>
#include <srslte/common/int_helpers.h>
#include <srslte/srslte.h>

#define TESTASSERT(cond)                                                                                               \
  {                                                                                                                    \
    if (!(cond)) {                                                                                                     \
      std::cout << "[" << __FUNCTION__ << "][Line " << __LINE__ << "]: FAIL at " << (#cond) << std::endl;              \
      return -1;                                                                                                       \
    }                                                                                                                  \
  }
using namespace srsue;
using namespace srslte;

// IP test message 1
// Protocol UDP
// Source port 2222, Destination port 2001
uint8_t ip_tst_message1[] = {
    0x45, 0x00, 0x00, 0x5c, 0x22, 0xa1, 0x40, 0x00, 0x40, 0x11, 0x19, 0xee, 0x7f, 0x00, 0x00, 0x01, 0x7f, 0x00, 0x00,
    0x01, 0x08, 0xae, 0x07, 0xd1, 0x00, 0x48, 0xfe, 0x5b, 0xd8, 0xf8, 0xd5, 0x4d, 0x9a, 0x9d, 0x26, 0xc7, 0xbd, 0xb4,
    0xcc, 0x90, 0xe0, 0x21, 0x0b, 0x07, 0x74, 0x00, 0xcb, 0x2b, 0xf8, 0x09, 0xa1, 0x55, 0xa8, 0xf8, 0xfc, 0x93, 0xee,
    0x4c, 0x67, 0x60, 0xb6, 0xa0, 0x1c, 0x79, 0x29, 0x45, 0x59, 0x96, 0xe6, 0x9b, 0x70, 0xc7, 0x34, 0xb0, 0x2f, 0xf5,
    0x0e, 0x0f, 0xcb, 0x45, 0xf1, 0xae, 0x97, 0x46, 0x0c, 0xbe, 0x9f, 0xd7, 0xfa, 0xe5, 0xec, 0x99};
uint32_t ip_message_len1 = sizeof(ip_tst_message1);

// IP test message 2
// Protocol UDP
// Source port 8000, Destination Port 2001
uint8_t ip_tst_message2[] = {
    0x45, 0x00, 0x00, 0x5c, 0x1c, 0x0e, 0x40, 0x00, 0x40, 0x11, 0x20, 0x81, 0x7f, 0x00, 0x00, 0x01, 0x7f, 0x00, 0x00,
    0x01, 0x1f, 0x40, 0x07, 0xd1, 0x00, 0x48, 0xfe, 0x5b, 0xb8, 0x1a, 0x56, 0x0d, 0xd2, 0xa3, 0xf9, 0x11, 0xd5, 0x56,
    0xb6, 0x95, 0x60, 0x07, 0x2d, 0x95, 0xe2, 0x53, 0x6b, 0x8f, 0x90, 0xb5, 0x48, 0xd1, 0x71, 0x24, 0xe8, 0x6e, 0x2d,
    0x56, 0xec, 0xf1, 0xe5, 0x85, 0xa5, 0x79, 0xc6, 0x5c, 0x90, 0xd6, 0x72, 0x87, 0x20, 0x99, 0x94, 0xfa, 0x82, 0x0d,
    0x2a, 0x2c, 0xdf, 0x02, 0x60, 0xef, 0x80, 0x07, 0xe6, 0xe1, 0xef, 0x4f, 0x40, 0x9a, 0x0a, 0xbc};

uint32_t ip_message_len2 = sizeof(ip_tst_message2);

int tft_filter_test_single_local_port()
{
  srslte::log_filter log1("TFT");
  log1.set_level(srslte::LOG_LEVEL_DEBUG);
  log1.set_hex_limit(128);

  srslte::byte_buffer_pool *pool = srslte::byte_buffer_pool::get_instance();
  srslte::unique_byte_buffer_t ip_msg1, ip_msg2;
  ip_msg1 = allocate_unique_buffer(*pool);
  ip_msg2 = allocate_unique_buffer(*pool);

  // Filter length: 3 bytes
  // Filter type:   Single local port
  // Local port:    2222
  uint8_t filter_message[3];
  filter_message[0] = SINGLE_LOCAL_PORT_TYPE;
  srslte::uint16_to_uint8(2222, &filter_message[1]);


  // Set IP test message
  ip_msg1->N_bytes = ip_message_len1;
  memcpy(ip_msg1->msg, ip_tst_message1, ip_message_len1);
  log1.info_hex(ip_msg1->msg, ip_msg1->N_bytes, "IP test message\n");


  // Set IP test message
  ip_msg2->N_bytes = ip_message_len2;
  memcpy(ip_msg2->msg, ip_tst_message2, ip_message_len1);
  log1.info_hex(ip_msg2->msg, ip_msg2->N_bytes, "IP test message\n");

  // Packet filter
  LIBLTE_MME_PACKET_FILTER_STRUCT packet_filter;

  packet_filter.dir = LIBLTE_MME_TFT_PACKET_FILTER_DIRECTION_BIDIRECTIONAL;
  packet_filter.id = 1;
  packet_filter.eval_precedence = 0;
  packet_filter.filter_size = 3;
  memcpy(packet_filter.filter, filter_message, 3);
 
  srsue::tft_packet_filter_t filter(packet_filter);

  // Check filter
  TESTASSERT(filter.match(ip_msg1));
  TESTASSERT(!filter.match(ip_msg2));

  printf("Test NAS Activate Dedicated EPS Bearer Context Request successfull\n");
  return 0;
}

int tft_filter_test_single_remote_port()
{
  srslte::log_filter log1("TFT");
  log1.set_level(srslte::LOG_LEVEL_DEBUG);
  log1.set_hex_limit(128);

  srslte::byte_buffer_pool *pool = srslte::byte_buffer_pool::get_instance();
  srslte::unique_byte_buffer_t ip_msg1, ip_msg2;
  ip_msg1 = allocate_unique_buffer(*pool);
  ip_msg2 = allocate_unique_buffer(*pool);

  // Filter length: 3 bytes
  // Filter type:   Single local port
  // Local port:    2222
  uint8_t filter_message[3];
  filter_message[0] = SINGLE_REMOTE_PORT_TYPE;
  srslte::uint16_to_uint8(2001, &filter_message[1]);

  // Set IP test message
  ip_msg1->N_bytes = ip_message_len1;
  memcpy(ip_msg1->msg, ip_tst_message1, ip_message_len1);
  log1.info_hex(ip_msg1->msg, ip_msg1->N_bytes, "IP test message\n");


  // Set IP test message
  ip_msg2->N_bytes = ip_message_len2;
  memcpy(ip_msg2->msg, ip_tst_message2, ip_message_len1);
  log1.info_hex(ip_msg2->msg, ip_msg2->N_bytes, "IP test message\n");

  // Packet filter
  LIBLTE_MME_PACKET_FILTER_STRUCT packet_filter;

  packet_filter.dir = LIBLTE_MME_TFT_PACKET_FILTER_DIRECTION_BIDIRECTIONAL;
  packet_filter.id = 1;
  packet_filter.eval_precedence = 0;
  packet_filter.filter_size = 3;
  memcpy(packet_filter.filter, filter_message, 3);
 
  srsue::tft_packet_filter_t filter(packet_filter);

  // Check filter
  TESTASSERT(filter.match(ip_msg1));
  TESTASSERT(!filter.match(ip_msg2));

  printf("Test NAS Activate Dedicated EPS Bearer Context Request successfull\n");
  return 0;
}

int main(int argc, char **argv)
{
  srslte::byte_buffer_pool::get_instance();
  if (tft_filter_test_single_local_port()) {
    return -1;
  }
  if (tft_filter_test_single_remote_port()) {
    return -1;
  }
  srslte::byte_buffer_pool::cleanup();
}
