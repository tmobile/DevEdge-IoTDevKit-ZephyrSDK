/*
 * Copyright (c) 2023 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_TONE_PLAYER_H
#define TMO_TONE_PLAYER_H

/**
 * @brief Play a tone binary file
 * 
 * @param filename Filename of the tone binary
 * @param finished_callback Callback function for tone sequence finish
 * @return 0 on sucess, negative error on failure
 */
int play_tone_bin(char* filename, void (*finished_callback)(void));

/**
 * @brief Play a single note
 * 
 * @param freq Frequency of the note
 * @param duration Duration in milliseconds
 * @param finished_callback Callback function for note finish
 * @return 0 on sucess, negative error on failure
 */
int play_single_note(uint16_t freq, uint16_t duration, void (*finished_callback)(void));

/**
 * @brief Cancel playback of current tone sequence
 * 
 * @return 0 on sucess, negative error on failure
 */
int cancel_playback(void);

#endif /* TMO_TONE_PLAYER_H */
