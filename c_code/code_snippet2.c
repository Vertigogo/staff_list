
#include "../config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "docs.h"
#include "sha256.h"
#include "chacha.h"
#include "optparse.h"

int curve25519_donna(uint8_t *p, const uint8_t *s, const uint8_t *b);

/* Global options. */
static char *global_pubkey = 0;
static char *global_seckey = 0;

#if ENCHIVE_AGENT_DEFAULT_ENABLED
static int global_agent_timeout = ENCHIVE_AGENT_TIMEOUT;
#else
static int global_agent_timeout = 0;
#endif

#if ENCHIVE_PINENTRY_DEFAULT_ENABLED
static char *pinentry_path = STR(ENCHIVE_PINENTRY_DEFAULT);
#else
static char *pinentry_path = 0;
#endif

static const char enchive_suffix[] = STR(ENCHIVE_FILE_EXTENSION);

static struct {
    char *name;
    FILE *file;
} cleanup[2];

/**
 * Register a file for deletion should fatal() be called.
 */
static void
cleanup_register(FILE *file, char *name)
{
    if (file) {
        unsigned i;
        for (i = 0; i < sizeof(cleanup) / sizeof(*cleanup); i++) {
            if (!cleanup[i].name) {
                cleanup[i].name = name;
                cleanup[i].file = file;
                return;
            }
        }
    }
    abort();
}

/**
 * Update cleanup registry to indicate FILE has been closed.
 */
static void
cleanup_closed(FILE *file)
{
    unsigned i;
    for (i = 0; i < sizeof(cleanup) / sizeof(*cleanup); i++) {
        if (file == cleanup[i].file) {
            cleanup[i].file = 0;
            return;
        }
    }
    abort();
}

/**