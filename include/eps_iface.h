/**
 * @file eps_iface.h
 * @author Mit Bailey (mitbailey99@gmail.com)
 * @brief
 * @version 0.3
 * @date 2021-03-17
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef EPS_IFACE_H
#define EPS_IFACE_H

#include <pthread.h>

/**
 * @brief Initializes the devices required to run the electronic power supply.
 *
 * This function initializes everything necessary for the EPS to operate.
 *
 * @return int 1 on success, error codes defined in SH_ERRORS on error.
 */
int eps_init();

/**
 * @brief EPS thread.
 *
 * @param tid Pointer to an integer containing the thread ID.
 * @return Void pointer.
 */
void *eps_thread(void *tid);

/**
 * @brief Frees and destroys.
 *
 */
void eps_destroy();

#endif // EPS_IFACE_H