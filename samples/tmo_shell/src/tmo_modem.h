#include <zephyr/drivers/modem/murata-1sc.h>

int fcntl_ptr(int sock, int cmd, const void* ptr);

int tmo_modem_get_atcmd_resp(enum mdmdata_e cmd);

/**
 * @brief Wakes the modem up
 *
 * @return int 0 on success (including modem already connected), -err on failure
 */
int tmo_modem_wake(void);

/**
 * @brief Puts the modem to sleep
 *
 * @return int 0 on success (including modem already asleep), -err on failure
 */
int tmo_modem_sleep(void);

/**
 * @brief Gets the modem sleep state
 *
 * @return int 1 on connected, 0 on disconnected/asleep, -err on failure (or unknown state)
 */
int tmo_modem_get_state(void);

/**
 * @brief Gets the IMEI from the modem. Writes the result to res as an integer in string format
 *
 * @param res A buffer to write into. Always writes 16 bytes.
 * @param res_len The length of the buffer
 * @return int 0 on success, -err on failure
 */
int tmo_modem_get_imei(char* res, int res_len);

/**
 * @brief Gets the ICCID from the modem. Writes the result to res
 *
 * @param res  A buffer to write into. Always writes 32 bytes.
 * @param res_len The length of the buffer
 * @return int
 */
int tmo_modem_get_iccid(char* res, int res_len);

/**
 * @brief Gets the IMSI from the modem. Writes the result to res
 *
 * @param res  A buffer to write into. Always writes 16 bytes.
 * @param res_len The length of the buffer
 * @return int
 */
int tmo_modem_get_imsi(char* res, int res_len);

/**
 * @brief Gets the MSISDN from the modem. Writes the result to res
 *
 * @param res  A buffer to write into. Always writes 16 bytes.
 * @param res_len The length of the buffer
 * @return int
 */
int tmo_modem_get_msisdn(char* res, int res_len);
