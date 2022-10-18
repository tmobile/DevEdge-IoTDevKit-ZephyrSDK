/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_GNSS_H
#define TMO_GNSS_H

#define GNSS_RSTn 8
#define MAXFIELDS 32
/* set timeout to be one second (milliseconds/10 millisecond delay) */
#define MAXTIME 100
//#define DEBUG

struct tmo_gnss_struct {
        double time;
        double lat;
        double lon;
        double alt;
        double hdop;
	char lonchar;
	char latchar;
	uint32_t sats;
	uint32_t timeToFix;
	uint32_t pps1Count;
	char version[32];
};

void setup_gnss(void);
void readGNSSData(void);
void ln_buf_gen(void);
int gnss_version(void);

#ifdef TMO_GNSS
/* variables for tmo_gnss.c */
struct drv_data {
	struct gpio_callback gpio_cb;
	struct gpio_callback gpio_cb1pps;
	gpio_flags_t mode;
	int index;
	int aux;
};

const struct device *cxd5605;

struct tmo_gnss_struct gnss_values;
bool running = false;
bool getGNSSData = false;
uint8_t rd_data[82];
//char to_send[32];
bool las_notify = false;
uint8_t ln_las_buf[13];
uint8_t ln_quality_buf[6];
// See GATT Specification Supplement V6 - page 161
uint32_t ln_ln_feature = BIT(2) | BIT (3) | BIT(11) | BIT(12) | BIT(15);
//char message[74];
bool restart_setup = false;
#else
/* variables for other files as necessary */

/** Location Service Data */
#define UUID_SERVICE_LOCATION_DATA \
	uuid128(0xa4e649f4, 0x4be5, 0x11e5, 0x885d, 0xfeff819cdc9f)
#define UUID_CHARACTERISTIC_ACCELERATION \
	uuid128(0xc4c1f6e2, 0x4be5, 0x11e5, 0x885d, 0xfeff819cdc9f)

extern struct tmo_gnss_struct gnss_values;
extern bool getGNSSData;
extern bool running;
extern bool las_notify;
extern uint8_t ln_las_buf[13];
extern uint32_t ln_ln_feature;
extern uint8_t ln_quality_buf[6];
extern bool restart_setup;
int cxd5605_init(void);
void gnss_enable_hardware(void);
#endif

#endif
