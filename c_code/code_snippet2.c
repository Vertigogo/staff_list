
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
 * Free resources held by the cleanup registry.
 */
static void
cleanup_free(void)
{
    size_t i;
    for (i = 0; i < sizeof(cleanup) / sizeof(*cleanup); i++)
        free(cleanup[i].name);
}

/**
 * Print a message, cleanup, and exit the program with a failure code.
 */
static void
fatal(const char *fmt, ...)
{
    unsigned i;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "enchive: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    for (i = 0; i < sizeof(cleanup) / sizeof(*cleanup); i++) {
        if (cleanup[i].file)
            fclose(cleanup[i].file);
        if (cleanup[i].name)
            remove(cleanup[i].name);
    }
    va_end(ap);
    exit(EXIT_FAILURE);
}

/**
 * Print a non-fatal warning message.
 */
static void
warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "warning: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/**
 * Return a copy of S, which may be NULL.
 * Abort the program if out of memory.
 */
static char *
dupstr(const char *s)
{
    char *copy = 0;
    if (s) {
        size_t len = strlen(s) + 1;
        copy = malloc(len);
        if (!copy)
            fatal("out of memory");
        memcpy(copy, s, len);
    }
    return copy;
}

/**
 * Concatenate N strings as a new string.
 * Abort the program if out of memory.
 */
static char *
joinstr(int n, ...)
{
    int i;
    va_list ap;
    char *p, *str;
    size_t len = 1;

    va_start(ap, n);
    for (i = 0; i < n; i++) {
        char *s = va_arg(ap, char *);
        len += strlen(s);
    }
    va_end(ap);
