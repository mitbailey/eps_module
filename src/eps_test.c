#include "eps_extern.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
Commands available externally:
Ping
Reboot
Get HK
Get HK Out
Toggle LUP
Set LUP
Hard Reset
*/

#define EPS_CMD_PING 0
#define EPS_CMD_REBOOT 1
#define EPS_CMD_TLUP 2
#define EPS_CMD_SLUP 3
#define EPS_CMD_HARDRESET 4
#define EPS_CMD_GET_HK 5
#define EPS_CMD_GET_HK_OUT 6

hkparam_t hk[1];
eps_hk_out_t hkout[1];

void print_hk(hkparam_t hk)
{
    printf("Photovoltaic voltage (mV):");
    for (int i = 0; i < 3; i++)
        printf(" %u", (hk.pv[i]));
    printf("\nTotal photo current [mA]: %u", (hk.pc));
    printf("\nBattery voltage [mV]: %u", (hk.bv));
    printf("\nTotal system current [mA]: %u", (hk.sc));
    printf("\nTemp of boost converters and onboard batt (C):");
    for (int i = 0; i < 4; i++)
        printf(" %d", (hk.temp[i]));
    printf("\nExternal board batt (C):");
    for (int i = 0; i < 2; i++)
        printf(" %d", (hk.batt_temp[i]));
    printf("\nNumber of latchups:");
    for (int i = 0; i < 6; i++)
        printf(" %u", (hk.latchup[i]));
    printf("\nCause of last reset: 0x%02x", hk.reset);
    printf("\nNumber of reboots: %u", (hk.bootcount));
    printf("\nSoftware errors: %u", (hk.sw_errors));
    printf("\nPPT mode: %u", hk.ppt_mode);
    printf("\nChannel status: %u", hk.channel_status);
    printf("\n\n");
}

void print_hk_out(eps_hk_out_t hk_out)
{
    printf("LUP currents: ");
    for (int i = 0; i < 6; i++)
        printf("%u ", (hk_out.curout[i]));
    printf("\nLUP status: ");
    for (int i = 0; i < 8; i++)
        printf("%01x ", (hk_out.output[i]));
    printf("\nLUP on delay [s]: ");
    for (int i = 0; i < 8; i++)
        printf("%u ", (hk_out.output_on_delta[i]));
    printf("\nLUP off delay [s]: ");
    for (int i = 0; i < 8; i++)
        printf("%u ", (hk_out.output_off_delta[i]));
    printf("\nNumber of LUPs: ");
    for (int i = 0; i < 6; i++)
        printf("%u ", (hk_out.latchup[i]));
    printf("\n\n");
}

void printval_conf_t(eps_config_t *conf)
{
    printf("PPT Mode: %u\n", conf->ppt_mode);
    printf("Battheater Mode: %u\n", conf->battheater_mode);
    printf("Battheater low [C]: %d\n", conf->battheater_low);
    printf("Battheater high [C]: %d\n", conf->battheater_high);
    for (int i = 0; i < 8; i++)
    {
        printf("[%d] Startup normal: %u\n", i, conf->output_normal_value[i]);
        printf("[%d] Startup safe: %u\n", i, conf->output_safe_value[i]);
        printf("[%d] On delay: %u\n", i, conf->output_initial_on_delay[i]);
        printf("[%d] Off delay: %u\n", i, conf->output_initial_off_delay[i]);
        printf("\n");
    }
    for (int i = 0; i < 3; i++)
        printf("[%d] Vboost: %u\n", i, conf->vboost[i]);
    printf("\n");
}

void getval_conf_t(eps_config_t *conf)
{
    char buf[10];
    printf("Edit EPS Configuration\nPress <.> to skip\n");
    printf("PPT Mode: %u > ", conf->ppt_mode);
    scanf(" %s", buf);
    if (strncmp(buf, ".", 1))
        conf->ppt_mode = atoi(buf);
    printf("Battheater Mode: %u > ", conf->battheater_mode);
    scanf(" %s", buf);
    if (strncmp(buf, ".", 1))
        conf->battheater_mode = atoi(buf);
    printf("Battheater low [C]: %d > ", conf->battheater_low);
    scanf(" %s", buf);
    if (strncmp(buf, ".", 1))
        conf->battheater_low = atoi(buf);
    printf("Battheater high [C]: %d > ", conf->battheater_high);
    scanf(" %s", buf);
    if (strncmp(buf, ".", 1))
        conf->battheater_high = atoi(buf);
    for (int i = 0; i < 8; i++)
    {
        printf("[%d] Startup normal: %u > ", i, conf->output_normal_value[i]);
        scanf(" %s", buf);
        if (strncmp(buf, ".", 1))
            conf->output_normal_value[i] = atoi(buf);
        printf("[%d] Startup safe: %u > ", i, conf->output_safe_value[i]);
        scanf(" %s", buf);
        if (strncmp(buf, ".", 1))
            conf->output_safe_value[i] = atoi(buf);
        printf("[%d] On delay: %u > ", i, conf->output_initial_on_delay[i]);
        scanf(" %s", buf);
        if (strncmp(buf, ".", 1))
            conf->output_initial_on_delay[i] = atoi(buf);
        printf("[%d] Off delay: %u > ", i, conf->output_initial_off_delay[i]);
        scanf(" %s", buf);
        if (strncmp(buf, ".", 1))
            conf->output_initial_off_delay[i] = atoi(buf);
        printf("\n");
    }
    for (int i = 0; i < 3; i++)
    {
        printf("[%d] Vboost: %u> ", i, conf->vboost[i]);
        scanf(" %s", buf);
        if (strncmp(buf, ".", 1))
            conf->vboost[i] = atoi(buf);
    }
    printf("\n");
}

void *eps_test(void *tid)
{
    hkparam_t hk;
    eps_hk_out_t hk_out;
    eps_config_t conf[1];
    char c = 0x0;
    int lup = 0;
#ifdef NO_LUP_TGL
    int pw_stat[6] = {0, 0, 0, 0, 0, 0};
#endif
    while (!done)
    {
        printf("[p]ing, [k]ill eps, get [h]ousekeeping, [c]onfig, [r]eboot, toggle [l]atchup, [q]uit, get [d]ata log: ");
        c = getchar();
        fflush(stdin);
        printf("\n");
        switch (c)
        {
        case 'p':
        case 'P':
            eps_ping();
            break;
        case 'h':
        case 'H':
            memset(&hk, 0x0, sizeof(hkparam_t));
            eps_get_hk(&hk);
            memset(&hk_out, 0x0, sizeof(eps_hk_out_t));
            eps_get_hk_out(&hk_out);
            print_hk(hk);
            print_hk_out(hk_out);

            break;
        case 'c':
        case 'C':
            printf("Select command:\n"
                   "\t1: Get config\n"
                   "\t2: Set config\n"
                   "Enter response (1 -- 2): ");
            int confsel = 0;
            if (scanf(" %d", &confsel) < 0)
                break;
            if (confsel == 1)
            {
                eps_get_conf(conf);
                printval_conf_t(conf);
            }
            else if (confsel == 2)
            {
                eps_get_conf(conf);
                getval_conf_t(conf);
                eps_set_conf(conf);
            }
            break;
        case 'l':
        case 'L':
            printf("Select latch up:\n"
                   "\t1: 5V1\n"
                   "\t2: 5V2\n"
                   "\t3: 5V3\n"
                   "\t4: 3V1\n"
                   "\t5: 3V2\n"
                   "\t6: 3V3\nEnter response (1 -- 6): ");
            if (scanf(" %d", &lup) < 0)
                break;
#ifndef NO_LUP_TGL
            eps_tgl_lup(lup - 1);
#else
            if (pw_stat[lup - 1] == 0)
            {
                eps_lup_set(lup - 1, 1);
                pw_stat[lup - 1] = 1;
            }
            else
            {
                eps_lup_set(lup - 1, 0);
                pw_stat[lup - 1] = 0;
            }
#endif
            break;
        case 'r':
        case 'R':
            printf("Resetting EPS...\n");
            eps_reboot();
            sleep(2);
            break;
        case 'k':
        case 'K':
            printf("Hard resetting EPS...\n");
            sleep(1);
            eps_hardreset();
            break;
        case 'q':
        case 'Q':
            printf("main: quitting...");
            done = 1;
            printf("\tdone = %d\n", done);
            break;
        case 'd':
        case 'D':
            printf("Getting latest data log ... \n");
            // Get a log from the log file.
            char *logOut;

            dlgr_RegisterMaxLogSize("eps", sizeof(eps_hk_t));
            logOut = malloc(dlgr_QueryMemorySize("eps", 1));
            dlgr_RetrieveData("eps", logOut, 1);

            print_hk(*(hkparam_t *)logOut);

            free(logOut);
            done = 1;
        default:
            break;
        }
    }
    done = 1;
    return NULL;
}
