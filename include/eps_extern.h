/**
 * @file eps_extern.h
 * @author Mit Bailey (mitbailey99@gmail.com)
 * @brief Externally accessible EPS command handling functions.
 * @version 0.3
 * @date 2021-03-17
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef EPS_EXTERN_H
#define EPS_EXTERN_H

#include <pthread.h>
#include <stdarg.h>
#include "eps_p31u/p31u.h"

#ifndef EPS_H
extern __thread pthread_cond_t eps_cmd_wait[1];
extern __thread pthread_mutex_t eps_cmd_wait_m[1];
#endif

/**
  * @brief Pings the EPS.
  *
  * @return int Positive on success, negative on failure.
  */
int eps_ping();

/**
  * @brief Reboots the EPS.
  *
  * @return int Value for i2c read / write.
  */
int eps_reboot();

/**
  * @brief Gets basic housekeeping data.
  *
  *
  * @param hk Pointer to hkparam_t object.
  * @return int Value for i2c read / write.
  */
int eps_get_hk(hkparam_t *hk);

/**
  * @brief Gets basic housekeeping data.
  *
  * @param hk_out Pointer to eps_hk_out_t object.
  * @return int Value for i2c read / write.
  */
int eps_get_hk_out(eps_hk_out_t *hk_out);

/**
  * @brief Toggle EPS latch up.
  *
  * @param lup ID of latchup. 
  * @return int Variable tracking latchup status on success, value for i2c read / write on failure.
  */
int eps_tgl_lup(eps_lup_idx lup);

/**
  * @brief Set EPS latch up.
  *
  * @param lup ID of latchup.
  * @param pw Desired state; 1 for on, 0 for off.
  * @return int Value for eps_p31u_cmd
  */
int eps_lup_set(eps_lup_idx lup, int pw);

/**
 * @brief Gets the EPS configuration.
 * 
 * @param conf Pointer to eps_config_t object for output.
 * @return int 
 */
int eps_get_conf(eps_config_t *conf);

/**
 * @brief Sets the EPS configuration.
 * 
 * @param conf Pointer to eps_config_t object for input.
 * @return int 
 */
int eps_set_conf(eps_config_t *conf);

/**
  * @brief Power cycles all power lines including battery rails.
  *
  * @return int Value for i2c read / write.
  */
int eps_hardreset();

#endif // EPS_EXTERN_H