/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TMO_BUZZER_H
#define TMO_BUZZER_H

extern const struct device *buzzer;

int pwm_set_frequency(const struct device *dev, uint32_t pwm, uint16_t hz);
int tmo_play_tone(int frequency_hz, int duration_msecs);
void tmo_play_jingle(void);
void tmo_play_ramp(void);
void tmo_buzzer_set_volume(int);

#define TMO_PITCH_D3  147 /* This is the lowest pitch the buzzer can play */
#define TMO_PITCH_Ds3 156
#define TMO_PITCH_E3  165
#define TMO_PITCH_F3  175
#define TMO_PITCH_Fs3 185
#define TMO_PITCH_G3  196
#define TMO_PITCH_Gs3 208
#define TMO_PITCH_A3  220
#define TMO_PITCH_As3 233
#define TMO_PITCH_B3  247

#define TMO_PITCH_C4  262
#define TMO_PITCH_Cs4 277
#define TMO_PITCH_D4  294
#define TMO_PITCH_Ds4 311
#define TMO_PITCH_E4  330
#define TMO_PITCH_F4  349
#define TMO_PITCH_Fs4 370
#define TMO_PITCH_G4  392
#define TMO_PITCH_Gs4 415
#define TMO_PITCH_A4  440
#define TMO_PITCH_As4 466
#define TMO_PITCH_B4  494

#define TMO_PITCH_C5  523
#define TMO_PITCH_Cs5 554
#define TMO_PITCH_D5  587 
#define TMO_PITCH_Ds5 622
#define TMO_PITCH_E5  659
#define TMO_PITCH_F5  698
#define TMO_PITCH_Fs5 740
#define TMO_PITCH_G5  784
#define TMO_PITCH_Gs5 831
#define TMO_PITCH_A5  880
#define TMO_PITCH_As5 932
#define TMO_PITCH_B5  988

#define TMO_PITCH_C6  1047
#define TMO_PITCH_Cs6 1109
#define TMO_PITCH_D6  1175
#define TMO_PITCH_Ds6 1245
#define TMO_PITCH_E6  1319
#define TMO_PITCH_F6  1397
#define TMO_PITCH_Fs6 1480
#define TMO_PITCH_G6  1568
#define TMO_PITCH_Gs6 1661
#define TMO_PITCH_A6  1760
#define TMO_PITCH_As6 1865
#define TMO_PITCH_B6  1976

#define TMO_PITCH_C7  2093
#define TMO_PITCH_Cs7 2217
#define TMO_PITCH_D7  2349
#define TMO_PITCH_Ds7 2489
#define TMO_PITCH_E7  2637
#define TMO_PITCH_F7  2794
#define TMO_PITCH_Fs7 2960
#define TMO_PITCH_G7  3136
#define TMO_PITCH_Gs7 3322
#define TMO_PITCH_A7  3520
#define TMO_PITCH_As7 3729
#define TMO_PITCH_B7  3951

#define TMO_PITCH_C8  4186
#define TMO_PITCH_Cs8 4435
#define TMO_PITCH_D8  4699
#define TMO_PITCH_Ds8 4978
#define TMO_PITCH_E8  5274
#define TMO_PITCH_F8  5588
#define TMO_PITCH_Fs8 5920
#define TMO_PITCH_G8  6272
#define TMO_PITCH_Gs8 6645
#define TMO_PITCH_A8  7040
#define TMO_PITCH_As8 7459
#define TMO_PITCH_B8  7902

#define TMO_PITCH_C9  8372
#define TMO_PITCH_Cs9 8870
#define TMO_PITCH_D9  9397
#define TMO_PITCH_Ds9 9956
#define TMO_PITCH_E9  10548
#define TMO_PITCH_F9  11175
#define TMO_PITCH_Fs9 11840
#define TMO_PITCH_G9  12544
#define TMO_PITCH_Gs9 13290
#define TMO_PITCH_A9  14080
#define TMO_PITCH_As9 14917
#define TMO_PITCH_B9  15804

#define TMO_TUNE_PITCH_LOW TMO_PITCH_C6
#define TMO_TUNE_PITCH_HIGH TMO_PITCH_E6

#endif
