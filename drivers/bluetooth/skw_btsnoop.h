/******************************************************************************
 *
 *  Copyright (C) 2020-2021 SeekWave Technology
 *
 *
 ******************************************************************************/


#ifndef __SKW_BTSNOOP_H__
#define __SKW_BTSNOOP_H__


#include <linux/types.h>



/* HCI Packet types */
#define HCI_COMMAND_PKT     0x01
#define HCI_ACLDATA_PKT     0x02
#define HCI_SCODATA_PKT     0x03
#define HCI_EVENT_PKT       0x04
#define HCI_ISODATA_PKT		0x05
#define HCI_EVENT_SKWLOG    0x07


void skw_btsnoop_init(void);

void skw_btsnoop_close(void);

void skw_btsnoop_capture(const unsigned char *packet, unsigned char is_received);

uint64_t skw_btsnoop_timestamp(void);



#endif
