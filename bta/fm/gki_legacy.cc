/*
 * for pre-compiled library (FM legacy)
 */

#include <bt_common.h>

extern "C" void *GKI_getbuf(size_t size) {
    return osi_malloc(size);
}

extern "C" void GKI_freebuf(void *ptr) {
    osi_free(ptr);
}

