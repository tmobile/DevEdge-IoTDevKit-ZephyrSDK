/*
 * Copyright (c) 2023 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include "tmo_buzzer.h"

LOG_MODULE_REGISTER(tmo_tone_player, LOG_LEVEL_DBG);

struct note_s { 
    uint16_t freq;
    uint16_t dur_ms;
};

static struct note_s next_note;
static struct fs_file_t tone_bin;
static bool timer_running;
static void (*song_finished_callback)(void);

static void set_tone(uint16_t freq)
{
    pwm_set_frequency(buzzer, 0, freq);
}

static void next_note_f(struct k_timer *timer_id)
{
    set_tone(next_note.freq);

    if (next_note.dur_ms) {
        k_timer_start(timer_id, K_MSEC(next_note.dur_ms), K_FOREVER);
    } else {
        timer_running = false;
        if (song_finished_callback)
            song_finished_callback();
        song_finished_callback = NULL;
        return;
    }

    if (fs_read(&tone_bin, &next_note, sizeof(next_note)) < 4) {
        fs_close(&tone_bin);
        memset(&next_note, 0, sizeof(next_note));
        return;
    }

    next_note.freq = sys_be16_to_cpu(next_note.freq);
    next_note.dur_ms = sys_be16_to_cpu(next_note.dur_ms);
}

K_TIMER_DEFINE(next_tone, next_note_f, NULL);

int play_tone_bin(char* filename, void (*finished_callback)(void))
{
    if (timer_running) {
        k_timer_stop(&next_tone);
        fs_close(&tone_bin);
    }

    song_finished_callback = finished_callback;

    fs_file_t_init(&tone_bin);

    if (fs_open(&tone_bin, filename, FS_O_READ)) {
        LOG_ERR("Failed to open tone file");
        return -EIO;
    }

    if (fs_read(&tone_bin, &next_note, sizeof(next_note)) < 4) {
        LOG_ERR("Failed to read tone file");
        fs_close(&tone_bin);
        return -EIO;
    }

    next_note.freq = sys_be16_to_cpu(next_note.freq);
    next_note.dur_ms = sys_be16_to_cpu(next_note.dur_ms);
    timer_running = true;
    next_note_f(&next_tone);

    return 0;
}

int play_single_note(uint16_t freq, uint16_t duration, void (*finished_callback)(void))
{
    if (timer_running) {
        return -EBUSY;
    }
    fs_close(&tone_bin);
    next_note.freq = 0;
    next_note.dur_ms = 0;

    song_finished_callback = finished_callback;

    set_tone(next_note.freq);

    k_timer_start(&next_tone, K_MSEC(duration), K_FOREVER);

    return 0;
}

int cancel_playback(void)
{
    if (timer_running) {
        set_tone(0);
        k_timer_stop(&next_tone);
        fs_close(&tone_bin);
    } else {
        return -EALREADY;
    }
    return 0;
}
