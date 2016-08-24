/*
 * for pre-compiled library (FM legacy)
 */

#include <bt_common.h>

void *GKI_getbuf(size_t size) {
    return osi_malloc(size);
}

void GKI_freebuf(void *ptr) {
    osi_free(ptr);
}

