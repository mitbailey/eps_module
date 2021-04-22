/**
 * @file main.c
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief main() symbol of the SPACE-HAUC Flight Software.
 * @version 0.2
 * @date 2020-03-19
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#define MAIN_PRIVATE       // enable prototypes in main.h and modules in modules.h
#define DATALOGGER_PRIVATE //
#include <main.h>
#include <modules.h>
#undef MAIN_PRIVATE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

int sys_boot_count = -1;
volatile sig_atomic_t done = 0;
__thread int sys_status;

/**
 * @brief Main function executed when shflight.out binary is executed
 * 
 * @return int returns 0 on success, -1 on failure, error code on thread init failures
 */
int main(void)
{
    // Boot counter
    sys_boot_count = bootCount(); // Holds bootCount to generate a different log file at every boot
    if (sys_boot_count < 0)
    {
        fprintf(stderr, "Boot count returned negative, fatal error. Exiting.\n");
        exit(-1);
    }
    // SIGINT handler register
    struct sigaction saction;
    saction.sa_handler = &catch_sigint;
    sigaction(SIGINT, &saction, NULL);
    // initialize modules
    for (int i = 0; i < num_init; i++)
    {
        int val = module_init[i]();
        if (val < 0)
        {
            sherror("Error in initialization!");
            exit(-1);
        }
    }
    printf("Done init modules\n");

    // Initialize datalogger.
    // printf("Initializing datalogger: %d\n", dlgr_init());

    // set up threads
    int rc[num_systems];                                         // fork-join return codes
    pthread_t thread[num_systems];                               // thread containers
    pthread_attr_t attr;                                         // thread attribute
    int args[num_systems];                                       // thread arguments (thread id in this case, but can be expanded by passing structs etc)
    void *status;                                                // thread return value
    pthread_attr_init(&attr);                                    // initialize attribute
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); // create threads to be joinable

    for (int i = 0; i < num_systems; i++)
    {
        args[i] = i; // sending a pointer to i to every thread may end up with duplicate thread ids because of access times
        rc[i] = pthread_create(&thread[i], &attr, module_exec[i], (void *)(&args[i]));
        if (rc[i])
        {
            printf("[Main] Error: Unable to create thread %d: Errno %d\n", i, errno);
            exit(-1);
        }
    }

    pthread_attr_destroy(&attr); // destroy the attribute

    for (int i = 0; i < num_systems; i++)
    {
        rc[i] = pthread_join(thread[i], &status);
        if (rc[i])
        {
            printf("[Main] Error: Unable to join thread %d: Errno %d\n", i, errno);
            exit(-1);
        }
    }

    // destroy modules
    for (int i = 0; i < num_destroy; i++)
    {
        module_destroy[i]();
    }
    return 0;
}
/**
 * @brief SIGINT handler, sets the global variable `done` as 1, so that thread loops can break.
 * Wakes up sitl_comm and datavis threads to ensure they exit.
 * 
 * @param sig Receives the signal as input.
 */
void catch_sigint(int sig)
{
    done = 1;
    for (int i = 0; i < num_wakeups; i++)
        pthread_cond_broadcast(wakeups[i]);
}
/**
 * @brief Prints errors specific to shflight in a fashion similar to perror
 * 
 * @param msg Input message to print along with error description
 */
void sherror(const char *msg)
{
    switch (sys_status)
    {
    case ERROR_MALLOC:
        fprintf(stderr, "%s: Error allocating memory\n", msg);
        break;

    case ERROR_HBRIDGE_INIT:
        fprintf(stderr, "%s: Error initializing h-bridge\n", msg);
        break;

    case ERROR_MUX_INIT:
        fprintf(stderr, "%s: Error initializing mux\n", msg);
        break;

    case ERROR_CSS_INIT:
        fprintf(stderr, "%s: Error initializing CSS\n", msg);
        break;

    case ERROR_FSS_INIT:
        fprintf(stderr, "%s: Error initializing FSS\n", msg);
        break;

    case ERROR_FSS_CONFIG:
        fprintf(stderr, "%s: Error configuring FSS\n", msg);
        break;

    default:
        fprintf(stderr, "%s\n", msg);
        break;
    }
}

int bootCount()
{
    FILE *fp;
    int _bootCount = 0;                      // assume 0 boot
    if (access(BOOTCOUNT_FNAME, F_OK) != -1) // file exists
    {
        fp = fopen(BOOTCOUNT_FNAME, "r+");              // open file for reading
        int read_bytes = fscanf(fp, "%d", &_bootCount); // read bootcount
        if (read_bytes < 0)                             // if no bytes were read
        {
            perror("File not read"); // indicate error
            _bootCount = 0;          // reset boot count
        }
        fclose(fp); // close
    }
    // Update boot file
    fp = fopen(BOOTCOUNT_FNAME, "w"); // open for writing
    fprintf(fp, "%d", ++_bootCount);  // write 1
    fclose(fp);                       // close
    sync();                           // sync file system
    return --_bootCount;              // return 0 on first boot, return 1 on second boot etc
}

/***************************
 * 
 * DATALOGGER
 * 
 * *************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FBEGIN_SIZE 6
#define FEND_SIZE 4

char FBEGIN[6] = {'F', 'B', 'E', 'G', 'I', 'N'};
char FEND[4] = {'F', 'E', 'N', 'D'};
uint64_t logIndex;
settings_t localSettings[1];
ssize_t moduleLogSize;

// Example Directory
/* datalogger
 * - log
 * - - eps
 * - - - settings.cfg
 * - - - index.inf
 * - - - module.inf
 * - - - 0.dat
 * - - - 1.dat
 * - - - 2.dat
 * - - - 3.dat
 * - - - 4.dat
 * - - - 5.dat
 * - - acs
 * - - - settings.cfg
 * - - - index.inf
 * - - - module.inf
 * - - - 0.dat
 * - - - 1.dat
 */

// settings.cfg file format (index moved to index.inf)
/* 1. max file size (Bytes)
 * 2. max dir size (Bytes)
 */

// char* moduleName is just a placeholder. Later, we will get the
// module names from somewhere else.

int dlgr_init()
{
    char *moduleName = NULL;

    // Check if log directory exists. If not, create it.
    const char directory[] = "log";
    struct stat sb;

    if (!(stat(directory, &sb) == 0 && S_ISDIR(sb.st_mode)))
    {
        mkdir(directory, S_IRUSR | S_IWUSR);
    }

    printf("(eps_module datalogger) DEBUG: Successful mkdir.");
    fflush(stdout);

    for (int i = 0; i < num_modname; i++)
    {
        char dirname[128] = {
            0x0,
        };
        moduleName = module_name[i];

        snprintf(dirname, sizeof(dirname), "%s/%s", directory, moduleName);
        // Check if log/%s, moduleName directory exists. If not, create it.
        if (!(stat(moduleName, &sb) == 0 && S_ISDIR(sb.st_mode)))
        {
            mkdir(moduleName, S_IRUSR | S_IWUSR);
        }

        printf("(eps_module datalogger) DEBUG: Successful %s mkdir.", dirname);
        fflush(stdout);

        // Set the module log size to -1 until we know how large one is.
        moduleLogSize = -1;
#define MODULE_FNAME_SZ 128
        // Get the size of this module's log if one was already written.
        char moduleFile[MODULE_FNAME_SZ] = {
            0x0,
        };
        snprintf(moduleFile, sizeof(moduleFile), "%s/%s/module.inf", directory, moduleName);

        FILE *modu = NULL;

        char *sLogSize;
        // If a module.inf file does not exist, create one and put in this module's log's size.
        if (access(moduleFile, F_OK | R_OK) == 0)
        {
            modu = fopen(moduleFile, "r");
            fgets(sLogSize, 20, (FILE *)index);
            moduleLogSize = atoi(sLogSize);
            fclose(modu);
        }
        else
        {
            modu = fopen(moduleFile, "w");
            fprintf(modu, "%d", moduleLogSize);
            fclose(modu);
        }

        printf("(eps_module datalogger) DEBUG: %s: Checkpoint 1.", dirname);
        fflush(stdout);

        if (localSettings == NULL)
        {
            return ERR_INIT;
        }

        // Create index.inf, with a single 0, for each module.
        // Create an initial log file, 0.dat, for each module.
        // TODO: No list of module names yet.

        // Creates index.inf
        char indexFile[MODULE_FNAME_SZ] = {
            0x0,
        };
        snprintf(indexFile, sizeof(indexFile), "log/%s/index.inf", moduleName);

        FILE *index = NULL;

        // Note: access() returns 0 on success.
        // If an index file exists, update our index to match.
        // Otherwise, make an index file with index = 0.
        if (access(indexFile, F_OK | R_OK) == 0)
        {
            // An index file already exists.
            // Set our logIndex to the value in index.inf
            index = fopen(indexFile, "r");
            char sIndex[20];
            fgets(sIndex, 20, (FILE *)index);
            logIndex = atoi(sIndex);
            fclose(index);
        }
        else
        {
            index = fopen(indexFile, "w");
            fprintf(index, "0\n");
            fclose(index);
            sync();
        }

        printf("(eps_module datalogger) DEBUG: %s: Checkpoint 2.", dirname);
        fflush(stdout);

        // Creates an initial log file, 0.dat
        char dataFile[MODULE_FNAME_SZ] = {
            0x0,
        };
        snprintf(dataFile, sizeof(dataFile), "log/%s/0.log", moduleName);

        FILE *data = NULL;

        if (access(dataFile, F_OK | R_OK) != 0)
        {
            // An initial data file does not already exist.
            data = fopen(dataFile, "wb");
            fclose(data);
        }

        // Creates an initial settings file, or syncs localSettings if one already exists.
        char settingsFile[MODULE_FNAME_SZ] = {
            0x0,
        };
        snprintf(settingsFile, sizeof(settingsFile), "log/%s/0.log", moduleName);

        FILE *settings = NULL;

        // If exists, sync settings, else create a new one.
        if (access(settingsFile, F_OK | R_OK) == 0)
        {
            settings = fopen(settingsFile, "r");

            if (settings == NULL)
            {
                return ERR_SETTINGS_OPEN;
            }

            char sMaxFileSize[10];
            char sMaxDirSize[10];

            // Gets the index, maxSizes
            if (fgets(sMaxFileSize, 10, (FILE *)settings) == NULL || fgets(sMaxDirSize, 10, (FILE *)settings) == NULL)
            {
                return ERR_SETTINGS_ACCESS;
            }

            // Converts the retrieved string to an int.
            localSettings->maxFileSize = atoi(sMaxFileSize);
            localSettings->maxDirSize = atoi(sMaxDirSize);

            fclose(settings);
        }
        else
        {
            // No settings exists, create one and fill it with defaults.
            settings = fopen(settingsFile, "w");
            fprintf(settings, "8192");    // 8KB
            fprintf(settings, "4194304"); // 4 MB
            fprintf(settings, "1");
            fclose(settings);
            sync();
        }
    }
    printf("(eps_module datalogger) DEBUG: Successful initialization.");
    fflush(stdout);
    return 1;
}

int dlgr_LogData(char *moduleName, ssize_t size, void *dataIn)
{
    // Note: Directory will probably be accessed some other way eventually.
    printf("(eps_module datalogger) DEBUG: dlgr_LogData called...");
    fflush(stdout);

    if (moduleLogSize < 1)
    {
        return ERR_MAXLOGSIZE_NOT_SET;
    }

    if (moduleLogSize > size)
    {
        return ERR_MAXLOGSIZE_EXCEEDED;
    }

    // Constructs the index.inf directory => indexFile.
    char indexFile[MODULE_FNAME_SZ] = {
        0x0,
    };
    snprintf(indexFile, sizeof(indexFile), "log/%s/index.inf", moduleName);

    // Constructs the n.dat directory.
    char dataFile[MODULE_FNAME_SZ] = {
        0x0,
    };
    snprintf(dataFile, sizeof(dataFile), "log/%s/%d.dat", moduleName, logIndex);

    // Constructs the n+1.dat directory.
    char dataFileNew[MODULE_FNAME_SZ] = {
        0x0,
    };
    snprintf(dataFileNew, sizeof(dataFileNew), "log/%s/%d.dat", moduleName, logIndex + 1);

    // Open the current data (.dat) file in append-mode.
    FILE *data = NULL;
    data = fopen(dataFile, "ab");

    if (data == NULL)
    {
        return ERR_DATA_OPEN;
    }

    // Fetch the current file size.
    long int fileSize = ftell(data);

    // For the adjacent if statement.
    FILE *index = NULL;
    // This file has reached its maximum size.
    // Make a new one and iterate the index.
    if (fileSize >= localSettings->maxFileSize)
    { // 16KB
        index = fopen(indexFile, "w");

        if (index == NULL)
        {
            return ERR_SETTINGS_OPEN;
        }

        // Iterate the index.
        logIndex++;

        // Rewrite the index file.
        fprintf(index, "%d\n", logIndex);

        fclose(data);
        fclose(index);
        sync();

        // Replace data pointer to the new data file.
        data = fopen(dataFileNew, "wb");

        // Delete an old file.
        char dataFileOld[MODULE_FNAME_SZ] = {
            0x0,
        };
        snprintf(dataFileOld, sizeof(dataFileOld), "log/%s/%d.dat", moduleName, logIndex - (localSettings->maxDirSize / localSettings->maxFileSize));

        if (remove(dataFileOld) != 0)
        {
            return ERR_DATA_REMOVE;
        }
    }

    // Write FBEGIN, the data, and then FEND to the binary .dat file.
    fwrite(FBEGIN, 6, 1, data);
    fwrite(dataIn, sizeof(dataIn), 1, data);

    // Data padding if size < moduleLogSize.
    for (; size < moduleLogSize; size++)
    {
        fwrite(dataIn, 1, 1, 0);
    }

    fwrite(FEND, 4, 1, data);

    fclose(data);
    sync();

    printf("(eps_module datalogger) DEBUG: Successful data log.");
    fflush(stdout);
    return 1;
}

int dlgr_RetrieveData(char *moduleName, char *output, int numRequestedLogs)
{
    /*
     * This should return sets of data from the binary .dat files irregardless of what file its in,
     * for as much as is requested.
     *
     */

    // Assume:      The size of every item within a .dat file is the same size. Note that,
    //              for instance, eps and acs will not have equally sized data.
    // Consider:    A header at the start of the file containing the size of each item.
    // Remember:    .dat binary files look like...

    /* FBEGIN
     * < Some module-specific data of size n >
     * FEND
     */

    printf("(eps_module datalogger) DEBUG: dlgr_RetrieveData called...");
    fflush(stdout);

    // Check if the module log size is already defined.
    // If not, nothing has been logged yet, so we cannot retrieve.
    if (moduleLogSize < 0)
    {
        return ERR_LOG_SIZE;
    }

    // To keep track of the number of logged structures we've added to the output.
    int numReadLogs = 0;

    // How many files we have to look 'back'.
    int indexOffset = 0;

    int errorCheck = 0;

    // Where the magic happens.
    while (numReadLogs < numRequestedLogs)
    {
        errorCheck = dlgr_retrieve(moduleName, output, numRequestedLogs - numReadLogs, indexOffset);
        if (errorCheck < 0)
        {
            return errorCheck;
        }
        numReadLogs += errorCheck;
        indexOffset++;
    }

    // Check if we got the right amount of logs.
    if (numReadLogs != numRequestedLogs)
    {
        return ERR_READ_NUM;
    }

    printf("(eps_module datalogger) DEBUG: Successful data retrieval.");
    fflush(stdout);
    return 1;
}

int dlgr_retrieve(char *moduleName, char *output, int numRequestedLogs, int indexOffset)
{

    int numReadLogs = 0;

    // First, construct the directories.
    char dataFile[MODULE_FNAME_SZ] = {
        0x0,
    };
    snprintf(dataFile, sizeof(dataFile), "log/%s/%d.dat", moduleName, logIndex);

    FILE *data = NULL;
    data = fopen(dataFile, "rb");

    if (data == NULL)
    {
        return ERR_DATA_OPEN;
    }

    // Find the total size of the .dat file.
    fseek(data, 0, SEEK_END);
    ssize_t fileSize = ftell(data);
    fseek(data, 0, SEEK_SET);

    // Create a memory buffer of size fileSize
    char *buffer = NULL;
    buffer = malloc(fileSize + 1);

    if (buffer == NULL)
    {
        return ERR_MALLOC;
    }

    // Read the entire file into memory buffer. One byte a time for sizeof(buffer) bytes.
    if (fread(buffer, 0x1, fileSize, data) != 1)
    {
        return ERR_DATA_READ;
    }

    // Create a cursor to mark our current position in the buffer. Pull it one structure from the back.
    char *bufferCursor = buffer + fileSize;
    bufferCursor -= moduleLogSize - FBEGIN_SIZE + FEND_SIZE;

    // If we havent reached the beginning of the buffer, keep getting structures.
    while (buffer - bufferCursor != 0 && numReadLogs < numRequestedLogs)
    {
        // Copies one structure to the output. Contains delimiters and trace amounts of tree nuts.
        memcpy(output, buffer, moduleLogSize + FBEGIN_SIZE + FEND_SIZE);
        numReadLogs++;
    }

    fclose(data);
    free(buffer);

    return numReadLogs;
}

ssize_t dlgr_QueryMemorySize(char *moduleName, int numRequestedLogs)
{
    return numRequestedLogs * (moduleLogSize + FBEGIN_SIZE + FEND_SIZE);
}

int dlgr_EditSettings(char *moduleName, int value, int setting)
{
    // Change localSettings values.
    // Overwrite settings.cfg file and write new values in.

    // Change localSettings values.
    switch (setting)
    {
    case MAX_FILE_SIZE:
        if (value > SIZE_FILE_HARDLIMIT || value < 1)
        {
            return ERR_SETTINGS_SET;
        }

        localSettings->maxFileSize = value;

        break;
    case MAX_DIR_SIZE:
        // Memory amount out-of-bounds.
        if (value > SIZE_DIR_HARDLIMIT || value < 1)
        {
            return ERR_SETTINGS_SET;
        }

        localSettings->maxDirSize = value;

        break;
    default:
        return ERR_DEFAULT_CASE;
    }

    // Constructs the settings.cfg directory into settingsFile.
    char settingsFile[MODULE_FNAME_SZ] = {
        0x0,
    };
    snprintf(settingsFile, sizeof(settingsFile), "log/%s/settings.cfg", moduleName);

    // Open the settings file and update numbers.
    FILE *settings = NULL;
    settings = fopen(settingsFile, "w");

    if (settings == NULL)
    {
        return ERR_SETTINGS_OPEN;
    }

    // Write everything back into the file.
    fprintf(settings, "%d\n", localSettings->maxFileSize);
    fprintf(settings, "%d\n", localSettings->maxDirSize);

    fclose(settings);
    sync();

    return 1;
}

int dlgr_RegisterMaxLogSize(char *moduleName, ssize_t max_size)
{
    if (max_size < 1)
    {
        return ERR_INVALID_INPUT;
    }
    else if (moduleLogSize > 0)
    { // <-- We already have a log size.
        return ERR_REREGISTER;
    }

    moduleLogSize = max_size;

    char moduleFile[MODULE_FNAME_SZ] = {
        0x0,
    };
    snprintf(moduleFile, sizeof(moduleFile), "log/%s/module.inf", moduleName);

    FILE *modu = NULL;

    modu = fopen(moduleFile, "w");
    fprintf(modu, "%d", moduleLogSize);
    fclose(modu);

    return 1;
}

void dlgr_destroy()
{
    // TODO: Add frees
}

// Helper functions

int dlgr_indexOf(char *buffer, ssize_t bufferSize, char *token, ssize_t tokenSize)
{
    int i = 0;
    while (i + tokenSize < bufferSize)
    {
        int score = 0;
        for (int ii = 0; ii < tokenSize; ii++)
        {
            if (buffer[i + ii] == token[ii])
            {
                score++;
            }
        }
        if (score == tokenSize)
        {
            return i;
        }
    }
    return ERR_MISC;
}