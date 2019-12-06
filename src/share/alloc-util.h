/* SPDX-License-Identifier: Apache-2.0
 * Copyright © 2019 VMware, Inc.
 */

#pragma once

#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define new(t, n) ((t *) g_new(t, n))
#define new0(t, n) ((t*) g_new0(t, n))

static inline void *mfree(void *memory) {
        g_free(memory);

        return NULL;
}

static inline void freep(void *p) {
        g_free(*(void **) p);
}

#define DEFINE_CLEANUP_FUNC(type, func)                         \
        static inline void func##p(type *p) {                   \
                if (*p)                                         \
                        func(*p);                               \
        }                                                       \
        struct __useless_struct_to_allow_trailing_semicolon__

#define _cleanup_(x) __attribute__((cleanup(x)))

static inline void strv_free(char **strv) {
        if (strv && *strv)
                g_strfreev(strv);
}

static inline void g_string_unrefp(GString **s) {
        if (s && *s)
                g_string_free(*s, false);
}

static inline void close_fdp(int *fd) {
        if (fd && *fd && *fd >= 0)
                close(*fd);
}

DEFINE_CLEANUP_FUNC(FILE *, fclose);
DEFINE_CLEANUP_FUNC(FILE *, pclose);
DEFINE_CLEANUP_FUNC(int *, close_fdp);
DEFINE_CLEANUP_FUNC(GString **, g_string_unrefp);
DEFINE_CLEANUP_FUNC(char **, strv_free);
DEFINE_CLEANUP_FUNC(GHashTable *, g_hash_table_unref);

#define _cleanup_free_ _cleanup_(freep)
#define _cleanup_fclose_ _cleanup_(fclosep)
#define _cleanup_close_ _cleanup_(close_fdp)
#define _cleanup_pclose_ _cleanup_(pclosep)
#define _cleanup_closedir_ _cleanup_(closedirp)
#define _cleanup_hash_ _cleanup_(g_hash_table_unrefp)
#define _cleanup_strv_ _cleanup_(strv_freep)
#define _cleanup_gstring_ _cleanup_(strv_freep)

#define steal_pointer(ptr)                      \
        ({                                      \
                typeof(ptr) _ptr_ = (ptr);      \
                (ptr) = NULL;                   \
                _ptr_;                          \
        })

#define steal_fd(fd)                    \
        ({                              \
                int _fd_ = (fd);        \
                (fd) = -1;              \
                _fd_;                   \
        })