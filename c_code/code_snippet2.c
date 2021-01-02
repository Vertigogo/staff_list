
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

    p = str = malloc(len);
    if (!str)
        fatal("out of memory");

    va_start(ap, n);
    for (i = 0; i < n; i++) {
        char *s = va_arg(ap, char *);
        size_t slen = strlen(s);
        memcpy(p, s, slen);
        p += slen;
    }
    va_end(ap);

    *p = 0;
    return str;
}

/**
 * Read the protection key from a key agent identified by its IV.
 */
static int agent_read(uint8_t *key, const uint8_t *id);

/**
 * Serve the protection key on a key agent identified by its IV.
 */
static int agent_run(const uint8_t *key, const uint8_t *id);

#if ENCHIVE_OPTION_AGENT
#include <poll.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

/**
 * Fill ADDR with a unix domain socket name for the agent.
 */
static int
agent_addr(struct sockaddr_un *addr, const uint8_t *iv)
{
    char *dir = getenv("XDG_RUNTIME_DIR");
    if (!dir) {
        dir = getenv("TMPDIR");
        if (!dir)
            dir = "/tmp";
    }

    addr->sun_family = AF_UNIX;
    if (strlen(dir) + 1 + 16 + 1 > sizeof(addr->sun_path)) {
        warning("agent socket path too long -- %s", dir);
        return 0;
    } else {
        sprintf(addr->sun_path, "%s/%02x%02x%02x%02x%02x%02x%02x%02x", dir,
                iv[0], iv[1], iv[2], iv[3], iv[4], iv[5], iv[6], iv[7]);
        return 1;
    }
}

static int
agent_read(uint8_t *key, const uint8_t *iv)
{
    int success;
    struct sockaddr_un addr;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (!agent_addr(&addr, iv)) {
        close(fd);
        return 0;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        close(fd);
        return 0;
    }
    success = read(fd, key, 32) == 32;
    close(fd);
    return success;
}

static int
agent_run(const uint8_t *key, const uint8_t *iv)
{
    struct pollfd pfd = {-1, POLLIN, 0};
    struct sockaddr_un addr;
    pid_t pid;

    pfd.fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (pfd.fd == -1) {
        warning("could not create agent socket");
        return 0;
    }

    if (!agent_addr(&addr, iv))
        return 0;

    pid = fork();
    if (pid == -1) {
        warning("could not fork() agent -- %s", strerror(errno));
        return 0;
    } else if (pid != 0) {
        return 1;
    }
    close(0);
    close(1);

    umask(~(S_IRUSR | S_IWUSR));

    if (unlink(addr.sun_path))
        if (errno != ENOENT)
            fatal("failed to remove existing socket -- %s", strerror(errno));

    if (bind(pfd.fd, (struct sockaddr *)&addr, sizeof(addr))) {
        if (errno != EADDRINUSE)
            warning("could not bind agent socket %s -- %s",
                    addr.sun_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (listen(pfd.fd, SOMAXCONN)) {
        if (errno != EADDRINUSE)
            fatal("could not listen on agent socket -- %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    close(2);
    for (;;) {
        int cfd;
        int r = poll(&pfd, 1, global_agent_timeout * 1000);
        if (r < 0) {
            unlink(addr.sun_path);
            fatal("agent poll failed -- %s", strerror(errno));
        }
        if (r == 0) {
            unlink(addr.sun_path);
            fputs("info: agent timeout\n", stderr);
            close(pfd.fd);
            break;
        }
        cfd = accept(pfd.fd, 0, 0);
        if (cfd != -1) {
            if (write(cfd, key, 32) != 32)
                warning("agent write failed");
            close(cfd);
        }
    }
    exit(EXIT_SUCCESS);
}

#else
static int
agent_read(uint8_t *key, const uint8_t *id)
{
    (void)key;
    (void)id;
    return 0;
}

static int
agent_run(const uint8_t *key, const uint8_t *id)
{
    (void)key;
    (void)id;
    return 0;
}
#endif

/**
 * Prepend the system user config directory to a filename, creating
 * the directory if necessary. Calls fatal() on any error.
 */
static char *storage_directory(char *file);

#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/**
 * Return non-zero if path exists and is a directory.
 */
static int
dir_exists(const char *path)
{
    struct stat info;
    return !stat(path, &info) && S_ISDIR(info.st_mode);
}

/* Use $XDG_CONFIG_HOME/enchive, or $HOME/.config/enchive. */
static char *
storage_directory(char *file)
{
    static const char enchive[] = "/enchive/";
    static const char config[] = "/.config";
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    char *path, *s;

    if (!xdg_config_home) {
        char *home = getenv("HOME");
        if (!home)
            fatal("no $HOME or $XDG_CONFIG_HOME, giving up");
        if (home[0] != '/')
            fatal("$HOME is not absolute");
        path = joinstr(4, home, config, enchive, file);
    } else {
        if (xdg_config_home[0] != '/')
            fatal("$XDG_CONFIG_HOME is not absolute");
        path = joinstr(3, xdg_config_home, enchive, file);
    }

    s = strchr(path + 1, '/');
    while (s) {
        *s = 0;
        if (dir_exists(path) || !mkdir(path, 0700)) {
            DIR *dir = opendir(path);
            if (dir)
                closedir(dir);
            else
                fatal("opendir(%s) -- %s", path, strerror(errno));
        } else {
            fatal("mkdir(%s) -- %s", path, strerror(errno));
        }
        *s = '/';
        s = strchr(s + 1, '/');
    }

    return path;
}

#elif defined(_WIN32)
#include <windows.h>

/* Use %APPDATA% */
static char *
storage_directory(char *file)
{
    char *parent;
    static const char enchive[] = "\\enchive\\";
    char *appdata = getenv("APPDATA");
    if (!appdata)
        fatal("$APPDATA is unset");

    parent = joinstr(2, appdata, enchive);
    if (!CreateDirectory(parent, 0)) {
        if (GetLastError() == ERROR_PATH_NOT_FOUND) {
            fatal("$APPDATA directory doesn't exist");
        } else { /* ERROR_ALREADY_EXISTS */
            DWORD attr = GetFileAttributes(parent);
            if ((attr == INVALID_FILE_ATTRIBUTES) ||
                !(attr & FILE_ATTRIBUTE_DIRECTORY))
                fatal("%s is not a directory", parent);
        }
    }
    free(parent);

    return joinstr(3, appdata, enchive, file);
}

#endif /* _WIN32 */

/**
 * Read a passphrase directly from the keyboard without echo.
 */
static void get_passphrase(char *buf, size_t len, char *prompt);

/**
 * Read a passphrase without any fanfare (fallback).
 */
static void
get_passphrase_dumb(char *buf, size_t len, char *prompt)
{
    size_t passlen;
    warning("reading passphrase from stdin with echo");
    fputs(prompt, stderr);
    fflush(stderr);
    if (!fgets(buf, len, stdin))
        fatal("could not read passphrase");
    passlen = strlen(buf);
    if (buf[passlen - 1] < ' ')
        buf[passlen - 1] = 0;
}

#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

static void
pinentry_decode(char *buf, size_t blen, const char *str)
{
    size_t i, j;

    for (i = 0, j = 0; str[i] && str[i] != '\n' && j < blen - 1; i++) {
        int c = str[i];
        if (c == '%') {
            static const char *hex = "0123456789ABCDEF";
            char *nibh, *nibl;
            if (!str[i + 1] || !str[i + 2])
                fatal("invalid data from pinentry");
            nibh = memchr(hex, str[i + 1], 16);
            nibl = memchr(hex, str[i + 2], 16);
            if (!nibh || !nibl)
                fatal("invalid data from pinentry");
            buf[j++] = (nibh - hex) * 16 + (nibl - hex);
            i += 2;
        } else {
            buf[j++] = c;
        }
    }
    buf[j] = 0;
}

static void
invoke_pinentry(char *buf, size_t len, char *prompt)
{
    int pin[2];
    int pout[2];
    pid_t pid;

    if (pipe(pin) != 0)
        fatal("could not start pinentry -- %s", strerror(errno));
    if (pipe(pout) != 0)
        fatal("could not start pinentry -- %s", strerror(errno));

    pid = fork();
    if (pid == -1)
        fatal("pinentry fork() failed -- %s", strerror(errno));
    if (pid) {
        FILE *pfi, *pfo;
        char line[ENCHIVE_PASSPHRASE_MAX * 3 + 32];

        close(pin[0]);
        close(pout[1]);

        if (!(pfi = fdopen(pin[1], "w")))
            fatal("fdopen() input -- %s", strerror(errno));
        if (!(pfo = fdopen(pout[0], "r")))
            fatal("fdopen() output -- %s", strerror(errno));

        if (!fgets(line, sizeof(line), pfo))
            /* Likely caused by exec() failure, so exit quietly. */
            exit(EXIT_FAILURE);
        if (strncmp(line, "OK", 2) != 0)
            fatal("pinentry startup failure");

        if (fprintf(pfi, "SETPROMPT %s\n", prompt) < 0 || fflush(pfi) < 0)
            fatal("pinentry write() -- %s", strerror(errno));

        if (!fgets(line, sizeof(line), pfo))
            fatal("pinentry read() -- %s", strerror(errno));
        if (strncmp(line, "OK", 2) != 0)
            fatal("pinentry protocol failure");

        if (fprintf(pfi, "GETPIN\n") < 0 || fflush(pfi) < 0)
            fatal("pinentry write() -- %s", strerror(errno));

        if (!fgets(line, sizeof(line), pfo))
            fatal("pinentry read() -- %s", strerror(errno));
        if (strncmp(line, "ERR ", 4) == 0)
            fatal("passphrase entry canceled");
        else if (strncmp(line, "OK", 2) == 0)
            buf[0] = 0;
        else if (strncmp(line, "D ", 2) == 0)
            pinentry_decode(buf, len, line + 2);
        else
            fatal("pinentry protocol failure");

        fclose(pfo);
        fclose(pfi);
    } else {
        close(pin[1]);
        close(pout[0]);
        dup2(pin[0], STDIN_FILENO);
        dup2(pout[1], STDOUT_FILENO);
        if (execlp(pinentry_path, pinentry_path, (char *)0))
            fatal("exec(\"%s\") failed -- %s",
                  pinentry_path, strerror(errno));
    }
}

static void
get_passphrase(char *buf, size_t len, char *prompt)
{
    int tty;

    if (pinentry_path) {
        invoke_pinentry(buf, len, prompt);
        return;
    }

    tty = open("/dev/tty", O_RDWR);
    if (tty == -1) {
        get_passphrase_dumb(buf, len, prompt);
    } else {
        char newline = '\n';
        size_t i = 0;
        struct termios old, new;
        if (write(tty, prompt, strlen(prompt)) == -1)
            fatal("error asking for passphrase");
        tcgetattr(tty, &old);
        new = old;
        new.c_lflag &= ~ECHO;
        tcsetattr(tty, TCSANOW, &new);
        errno = 0;
        while (i < len - 1 && read(tty, buf + i, 1) == 1) {
            if (buf[i] == '\n' || buf[i] == '\r')
                break;
            i++;
        }
        buf[i] = 0;
        tcsetattr(tty, TCSANOW, &old);
        if (write(tty, &newline, 1) == -1)
            fatal("error asking for passphrase");
        close(tty);
        if (errno)
            fatal("could not read passphrase from /dev/tty");
    }
}

#elif defined(_WIN32)
#include <windows.h>

static void
get_passphrase(char *buf, size_t len, char *prompt)
{
    DWORD orig;
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(in, &orig)) {
        get_passphrase_dumb(buf, len, prompt);
    } else {
        size_t passlen;
        SetConsoleMode(in, orig & ~ENABLE_ECHO_INPUT);
        fputs(prompt, stderr);
        if (!fgets(buf, len, stdin))
            fatal("could not read passphrase");
        fputc('\n', stderr);
        passlen = strlen(buf);
        if (buf[passlen - 1] < ' ')
            buf[passlen - 1] = 0;
    }
}

#else
static void
get_passphrase(char *buf, size_t len, char *prompt)
{
    get_passphrase_dumb(buf, len, prompt);
}
#endif

/**
 * Create/truncate a file with paranoid permissions using OS calls.
 */
static FILE *secure_creat(const char *file);

#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
#include <unistd.h>

static FILE *
secure_creat(const char *file)
{
    int fd = open(file, O_CREAT | O_WRONLY, 00600);
    if (fd == -1)
        return 0;
    return fdopen(fd, "wb");
}

#else
static FILE *
secure_creat(const char *file)
{
    return fopen(file, "wb");
}
#endif

/**
 * Initialize a SHA-256 context for HMAC-SHA256.
 * All message data will go into the resulting context.
 */
static void
hmac_init(SHA256_CTX *ctx, const uint8_t *key)
{
    int i;
    uint8_t pad[SHA256_BLOCK_SIZE];
    sha256_init(ctx);
    for (i = 0; i < SHA256_BLOCK_SIZE; i++)
        pad[i] = key[i] ^ 0x36U;
    sha256_update(ctx, pad, sizeof(pad));
}

/**
 * Compute the final HMAC-SHA256 MAC.
 * The key must be the same as used for initialization.
 */
static void
hmac_final(SHA256_CTX *ctx, const uint8_t *key, uint8_t *hash)
{
    int i;
    uint8_t pad[SHA256_BLOCK_SIZE];
    sha256_final(ctx, hash);
    sha256_init(ctx);
    for (i = 0; i < SHA256_BLOCK_SIZE; i++)
        pad[i] = key[i] ^ 0x5cU;
    sha256_update(ctx, pad, sizeof(pad));
    sha256_update(ctx, hash, SHA256_BLOCK_SIZE);
    sha256_final(ctx, hash);
}

/**
 * Derive a 32-byte key from null-terminated passphrase into buf.
 * Optionally provide an 8-byte salt.
 */
static void
key_derive(const char *passphrase, uint8_t *buf, int iexp, const uint8_t *salt)
{
    uint8_t salt32[SHA256_BLOCK_SIZE] = {0};
    SHA256_CTX ctx[1];
    unsigned long i;
    unsigned long memlen = 1UL << iexp;
    unsigned long mask = memlen - 1;
    unsigned long iterations = 1UL << (iexp - 5);
    uint8_t *memory, *memptr, *p;

    memory = malloc(memlen + SHA256_BLOCK_SIZE);
    if (!memory)
        fatal("not enough memory for key derivation");

    if (salt)
        memcpy(salt32, salt, 8);
    hmac_init(ctx, salt32);
    sha256_update(ctx, (uint8_t *)passphrase, strlen(passphrase));
    hmac_final(ctx, salt32, memory);

    for (p = memory + SHA256_BLOCK_SIZE;
         p < memory + memlen + SHA256_BLOCK_SIZE;
         p += SHA256_BLOCK_SIZE) {
        sha256_init(ctx);
        sha256_update(ctx, p - SHA256_BLOCK_SIZE, SHA256_BLOCK_SIZE);
        sha256_final(ctx, p);
    }

    memptr = memory + memlen - SHA256_BLOCK_SIZE;
    for (i = 0; i < iterations; i++) {
        unsigned long offset;
        sha256_init(ctx);
        sha256_update(ctx, memptr, SHA256_BLOCK_SIZE);
        sha256_final(ctx, memptr);
        offset = ((unsigned long)memptr[3] << 24 |
                  (unsigned long)memptr[2] << 16 |
                  (unsigned long)memptr[1] <<  8 |
                  (unsigned long)memptr[0] <<  0);
        memptr = memory + (offset & mask);
    }

    memcpy(buf, memptr, SHA256_BLOCK_SIZE);
    free(memory);
}

/**
 * Get secure entropy suitable for key generation from OS.
 * Abort the program if the entropy could not be retrieved.
 */
static void secure_entropy(void *buf, size_t len);

#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
static void
secure_entropy(void *buf, size_t len)
{
    FILE *r = fopen("/dev/urandom", "rb");
    if (!r)
        fatal("failed to open %s", "/dev/urandom");
    if (!fread(buf, len, 1, r))
        fatal("failed to gather entropy");
    fclose(r);
}

#elif defined(_WIN32)
#include <windows.h>

static void
secure_entropy(void *buf, size_t len)
{
    HCRYPTPROV h = 0;
    DWORD type = PROV_RSA_FULL;
    DWORD flags = CRYPT_VERIFYCONTEXT | CRYPT_SILENT;
    if (!CryptAcquireContext(&h, 0, 0, type, flags) ||
        !CryptGenRandom(h, len, buf))
        fatal("failed to gather entropy");
    CryptReleaseContext(h, 0);
}
#endif

/**
 * Generate a brand new Curve25519 secret key from system entropy.
 */
static void
generate_secret(uint8_t *s)
{
    secure_entropy(s, 32);
    s[0] &= 248;
    s[31] &= 127;
    s[31] |= 64;
}

/**
 * Generate a Curve25519 public key from a secret key.
 */
static void
compute_public(uint8_t *p, const uint8_t *s)
{
    static const uint8_t b[32] = {9};
    curve25519_donna(p, s, b);
}

/**
 * Compute a shared secret from our secret key and their public key.
 */
static void
compute_shared(uint8_t *sh, const uint8_t *s, const uint8_t *p)
{
    curve25519_donna(sh, s, p);
}

/**
 * Encrypt from file to file using key/iv, aborting on any error.
 */
static void
symmetric_encrypt(FILE *in, FILE *out, const uint8_t *key, const uint8_t *iv)
{
    static uint8_t buffer[2][CHACHA_BLOCKLENGTH * 1024];
    uint8_t mac[SHA256_BLOCK_SIZE];
    SHA256_CTX hmac[1];
    chacha_ctx ctx[1];

    chacha_keysetup(ctx, key, 256);
    chacha_ivsetup(ctx, iv);
    hmac_init(hmac, key);

    for (;;) {
        size_t z = fread(buffer[0], 1, sizeof(buffer[0]), in);
        if (!z) {
            if (ferror(in))
                fatal("error reading plaintext file");
            break;
        }
        sha256_update(hmac, buffer[0], z);
        chacha_encrypt(ctx, buffer[0], buffer[1], z);
        if (!fwrite(buffer[1], z, 1, out))
            fatal("error writing ciphertext file");
        if (z < sizeof(buffer[0]))
            break;
    }

    hmac_final(hmac, key, mac);

    if (!fwrite(mac, sizeof(mac), 1, out))
        fatal("error writing checksum to ciphertext file");
    if (fflush(out))
        fatal("error flushing to ciphertext file -- %s", strerror(errno));
}

/**
 * Decrypt from file to file using key/iv, aborting on any error.
 */
static void
symmetric_decrypt(FILE *in, FILE *out, const uint8_t *key, const uint8_t *iv)
{
    static uint8_t buffer[2][CHACHA_BLOCKLENGTH * 1024 + SHA256_BLOCK_SIZE];
    uint8_t mac[SHA256_BLOCK_SIZE];
    SHA256_CTX hmac[1];
    chacha_ctx ctx[1];

    chacha_keysetup(ctx, key, 256);
    chacha_ivsetup(ctx, iv);
    hmac_init(hmac, key);

    /* Always keep SHA256_BLOCK_SIZE bytes in the buffer. */
    if (!(fread(buffer[0], SHA256_BLOCK_SIZE, 1, in))) {
        if (ferror(in))
            fatal("cannot read ciphertext file");
        else
            fatal("ciphertext file too short");
    }

    for (;;) {
        uint8_t *p = buffer[0] + SHA256_BLOCK_SIZE;
        size_t z = fread(p, 1, sizeof(buffer[0]) - SHA256_BLOCK_SIZE, in);
        if (!z) {
            if (ferror(in))
                fatal("error reading ciphertext file");
            break;
        }
        chacha_encrypt(ctx, buffer[0], buffer[1], z);
        sha256_update(hmac, buffer[1], z);
        if (!fwrite(buffer[1], z, 1, out))
            fatal("error writing plaintext file");

        /* Move last SHA256_BLOCK_SIZE bytes to the front. */
        memmove(buffer[0], buffer[0] + z, SHA256_BLOCK_SIZE);

        if (z < sizeof(buffer[0]) - SHA256_BLOCK_SIZE)
            break;
    }

    hmac_final(hmac, key, mac);
    if (memcmp(buffer[0], mac, sizeof(mac)) != 0)
        fatal("checksum mismatch!");
    if (fflush(out))
        fatal("error flushing to plaintext file -- %s", strerror(errno));

}

/**
 * Return the default public key file.
 */
static char *
default_pubfile(void)
{
    return storage_directory("enchive.pub");
}

/**
 * Return the default secret key file.
 */
static char *
default_secfile(void)
{
    return storage_directory("enchive.sec");
}

/**
 * Dump the public key to a file, aborting on error.
 */
static void
write_pubkey(char *file, uint8_t *key)
{
    FILE *f = fopen(file, "wb");
    if (!f)
        fatal("failed to open key file for writing '%s' -- %s",
              file, strerror(errno));
    cleanup_register(f, file);
    if (!fwrite(key, 32, 1, f))
        fatal("failed to write key file '%s'", file);
    cleanup_closed(f);
    if (fclose(f))
        fatal("failed to flush key file '%s' -- %s", file, strerror(errno));
}

/* Layout of secret key file */
#define SECFILE_IV            0
#define SECFILE_ITERATIONS    8
#define SECFILE_VERSION       9
#define SECFILE_PROTECT_HASH  12
#define SECFILE_SECKEY        32

/**
 * Write the secret key to a file, encrypting it if necessary.
 */
static void
write_seckey(char *file, const uint8_t *seckey, int iexp)
{
    FILE *secfile;
    chacha_ctx cha[1];
    SHA256_CTX sha[1];
    uint8_t buf[8 + 1 + 3 + 20 + 32] = {0}; /* entire file contents */
    uint8_t protect[32];

    uint8_t *buf_iv           = buf + SECFILE_IV;
    uint8_t *buf_iterations   = buf + SECFILE_ITERATIONS;
    uint8_t *buf_version      = buf + SECFILE_VERSION;
    uint8_t *buf_protect_hash = buf + SECFILE_PROTECT_HASH;
    uint8_t *buf_seckey       = buf + SECFILE_SECKEY;

    buf_version[0] = ENCHIVE_FORMAT_VERSION;

    if (iexp) {
        /* Prompt for a passphrase. */
        char pass[2][ENCHIVE_PASSPHRASE_MAX];
        get_passphrase(pass[0], sizeof(pass[0]),
                       "protection passphrase (empty for none): ");
        if (!pass[0][0]) {
            /* Nevermind. */
            iexp = 0;
        }  else {
            get_passphrase(pass[1], sizeof(pass[0]),
                           "protection passphrase (repeat): ");
            if (strcmp(pass[0], pass[1]) != 0)
                fatal("protection passphrases don't match");

            /* Generate an IV to double as salt. */
            secure_entropy(buf_iv, 8);

            key_derive(pass[0], protect, iexp, buf_iv);
            buf_iterations[0] = iexp;

            sha256_init(sha);
            sha256_update(sha, protect, sizeof(protect));
            sha256_final(sha, buf_protect_hash);
        }
    }

    if (iexp) {
        /* Encrypt using key derived from passphrase. */
        chacha_keysetup(cha, protect, 256);
        chacha_ivsetup(cha, buf_iv);