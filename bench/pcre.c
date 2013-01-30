
/*
 * Copyright 2012 Yichun "agentzh" Zhang
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */


#include <pcre.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>


static void usage(int rc);
static void run_engines(pcre *re, unsigned engine_types, int ncaps,
    const char *input, size_t len);


enum {
    ENGINE_DEFAULT = (1 << 0),
    ENGINE_JIT     = (1 << 1),
    ENGINE_DFA     = (1 << 2)
};


int
main(int argc, char **argv)
{
    int                  flags = 0;
    unsigned             engine_types = 0;
    unsigned             i;
    int                  err_offset = -1;
    pcre                *re;
    int                  ncaps;
    char                *input;
    FILE                *f;
    size_t               len;
    long                 rc;
    const char          *errstr;

    if (argc < 3) {
        usage(1);
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            break;
        }

        if (strncmp(argv[i], "--default",
                    sizeof("--default") - 1) == 0)
        {
            engine_types |= ENGINE_DEFAULT;

        } else if (strncmp(argv[i], "--jit", sizeof("--jit") - 1)
                   == 0)
        {
            engine_types |= ENGINE_JIT;

        } else if (strncmp(argv[i], "--dfa", sizeof("--dfa") - 1)
                   == 0)
        {
            engine_types |= ENGINE_DFA;

        } else if (strncmp(argv[i], "-i", 2) == 0) {
            flags |= PCRE_CASELESS;

        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            exit(1);
        }
    }

    if (engine_types == 0) {
        fprintf(stderr, "No engine specified.\n");
        exit(1);
    }

    if (argc - i != 2) {
        usage(1);
    }

    re = pcre_compile(argv[i++], flags, &errstr, &err_offset, NULL);
    if (re == NULL) {
        fprintf(stderr, "[error] pos %d: %s\n", err_offset, errstr);
        return 2;
    }

    if (pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &ncaps) < 0) {
        fprintf(stderr, "failed to get capture count.\n");
        return 2;
    }

    errno = 0;

    f = fopen(argv[i], "rb");
    if (f == NULL) {
        perror("open file");
        return 1;
    }

    if (fseek(f, 0L, SEEK_END) != 0) {
        perror("seek to file end");
        return 1;
    }

    rc = ftell(f);
    if (rc == -1) {
        perror("get file offset by ftell");
        return 1;
    }

    len = (size_t) rc;

    if (fseek(f, 0L, SEEK_SET) != 0) {
        perror("seek to file beginning");
        return 1;
    }

    input = malloc(len);
    if (input == NULL) {
        fprintf(stderr, "failed to allocate %ld bytes.\n", len);
        return 1;
    }

    if (fread(input, 1, len, f) < len) {
        if (feof(f)) {
            fprintf(stderr, "file truncated.\n");
            return 1;

        } else {
            perror("read file");
        }
    }

    if (fclose(f) != 0) {
        perror("close file");
        return 1;
    }

    run_engines(re, engine_types, ncaps, input, len);

    return 0;
}


static void
run_engines(pcre *re, unsigned engine_types, int ncaps,
    const char *input, size_t len)
{
    int                  i, n;
    int                  rc;
    int                 *ovector;
    size_t               ovecsize;
    pcre_extra          *extra;
    clock_t              begin, elapsed;
    const char          *errstr = NULL;

    if (engine_types & ENGINE_DEFAULT) {

        ovecsize = (ncaps + 1) * 3;
        ovector = malloc(ovecsize * sizeof(int));
        assert(ovector);

        printf("pcre default ");

        extra = pcre_study(re, 0, &errstr);
        if (errstr != NULL) {
            fprintf(stderr, "failed to study the regex: %s", errstr);
            exit(2);
        }

        begin = clock();
        if (begin == -1) {
            perror("clock");
            exit(2);
        }

        rc = pcre_exec(re, extra, input, len, 0, 0, ovector, ovecsize);

        elapsed = clock() - begin;
        if (elapsed < 0) {
            perror("clock");
            exit(2);
        }

        if (rc == 0) {
            fprintf(stderr, "capture size too small");
            exit(2);
        }

        if (rc == PCRE_ERROR_NOMATCH) {
            printf("no match");
        }

        if (rc < 0) {
            printf("error: %d\n", rc);
            exit(2);
        }

        if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", ovector[n], ovector[n + 1]);
            }
        }

        printf(": %ld clock units elapsed.\n", (long) elapsed);
    }

    if (engine_types & ENGINE_JIT) {

        ovecsize = (ncaps + 1) * 3;
        ovector = malloc(ovecsize * sizeof(int));
        assert(ovector);

        printf("pcre JIT ");

        extra = pcre_study(re, PCRE_STUDY_JIT_COMPILE, &errstr);
        if (errstr != NULL) {
            fprintf(stderr, "failed to study the regex: %s", errstr);
            exit(2);
        }

        begin = clock();
        if (begin == -1) {
            perror("clock");
            exit(2);
        }

        rc = pcre_exec(re, extra, input, len, 0, 0, ovector, ovecsize);

        elapsed = clock() - begin;
        if (elapsed < 0) {
            perror("clock");
            exit(2);
        }

        if (rc == 0) {
            fprintf(stderr, "capture size too small");
            exit(2);
        }

        if (rc == PCRE_ERROR_NOMATCH) {
            printf("no match");
        }

        if (rc < 0) {
            printf("error: %d\n", rc);
            exit(2);
        }

        if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", ovector[n], ovector[n + 1]);
            }
        }

        printf(": %ld clock units elapsed.\n", (long) elapsed);
    }

    if (engine_types & ENGINE_DFA) {
        int ws[100];

        ovecsize = 2;
        ovector = malloc(ovecsize * sizeof(int));
        assert(ovector);

        printf("pcre DFA ");

        extra = pcre_study(re, 0, &errstr);
        if (errstr != NULL) {
            fprintf(stderr, "failed to study the regex: %s", errstr);
            exit(2);
        }

        begin = clock();
        if (begin == -1) {
            perror("clock");
            exit(2);
        }

        rc = pcre_dfa_exec(re, extra, input, len, 0, 0, ovector, ovecsize,
                           ws, sizeof(ws)/sizeof(ws[0]));

        elapsed = clock() - begin;
        if (elapsed < 0) {
            perror("clock");
            exit(2);
        }

        if (rc == 0) {
            rc = 1;
        }

        if (rc == PCRE_ERROR_NOMATCH) {
            printf("no match");
        }

        if (rc < 0) {
            printf("error: %d\n", rc);
            exit(2);
        }

        if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", ovector[n], ovector[n + 1]);
            }
        }

        printf(": %ld clock units elapsed.\n", (long) elapsed);
    }

    printf("(1 clock unit is %lf sec)\n", (double) 1.0 / CLOCKS_PER_SEC);
}


static void
usage(int rc)
{
    fprintf(stderr, "usage: sregex [options] <regexp> <file>\n"
            "options:\n"
            "   -i                  use case insensitive matching\n"
            "   --default           use the default PCRE engine\n"
            "   --dfa               use the PCRE DFA engine\n"
            "   --jit               use the PCRE JIT engine\n");
    exit(rc);
}