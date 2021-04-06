/**
 * @file eps.c
 * @author Mit Bailey (mitbailey99@gmail.com)
 * @brief Implementation of EPS command handling functions.
 * @version 0.3
 * @date 2021-03-17
 *
 * @copyright Copyright (c) 2021
 *
 */

#define EPS_P31U_PRIVATE
#include "eps_p31u/p31u.h"
#undef EPS_P31U_PRIVATE
#include "eps.h"
#include <main.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

/* Variable allocation for EPS */

/**
  * @brief The EPS object pointer.
  *
  */
static p31u eps[1];

/**
 * @brief Thread local variable that enqueue waits upon
 * 
 */
__thread pthread_cond_t eps_cmd_wait[1];
/**
 * @brief Mutex for the queue wait
 * 
 */
__thread pthread_mutex_t eps_cmd_wait_m[1];

int eps_ping()
{
    if (eps == NULL)
    {
        return -1;
    }

    return eps_p31u_ping(eps);
}

int eps_reboot()
{
    if (eps == NULL)
    {
        return -1;
    }

    return eps_p31u_reboot(eps);
}

int eps_get_hk(hkparam_t *hk)
{
    if (eps == NULL)
    {
        return -1;
    }

    return eps_p31u_get_hk(eps, hk);
}

int eps_get_hk_out(eps_hk_out_t *hk_out)
{
    if (eps == NULL)
    {
        return -1;
    }

    return eps_p31u_get_hk_out(eps, hk_out);
}

int eps_tgl_lup(eps_lup_idx lup)
{
    if (eps == NULL)
    {
        return -1;
    }

    return eps_p31u_tgl_lup(eps, lup);
}

int eps_lup_set(eps_lup_idx lup, int pw)
{
    if (eps == NULL)
    {
        return -1;
    }

    return eps_p31u_lup_set(eps, lup, (int)pw);
}

int eps_hardreset()
{
    if (eps == NULL)
    {
        return -1;
    }

    return eps_p31u_hardreset(eps);
}

// Initializes the EPS and ping-tests it.
int eps_init()
{
    // Check if malloc was successful.
    if (eps == NULL)
    {
        return -1;
    }

    // Initializes the EPS component while checking if successful.
    if (eps_p31u_init(eps, 1, 0x1b) <= 0)
    {
        return -1;
    }

    // If we can't successfully ping the EPS then something has gone wrong.
    if (eps_p31u_ping(eps) < 0)
    {
        return -2;
    }
}

int eps_get_conf(eps_config_t *conf)
{
    return eps_p31u_get_conf(eps, conf);
}

int eps_set_conf(eps_config_t *conf)
{
    return eps_p31u_set_conf(eps, conf);
}

void *eps_thread(void *tid)
{
    while (!done)
    {
        // Reset the watch-dog timer.
        eps_reset_wdt(eps);
        // add other things here


        sleep(EPS_LOOP_TIMER);
    }

    pthread_exit(NULL);
}

// Frees eps memory and destroys the EPS object.
void eps_destroy()
{
    // Destroy / free the eps.
    eps_p31u_destroy(eps);
}
