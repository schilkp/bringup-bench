#include "libmin.h"
#include "libtarg.h"

void *
libmin_memcpy(void *dest, const void *src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;

	for (; n; n--) *d++ = *s++;
	return dest;
}

#ifdef TARGET_CVA6_DCHECK
// Standard library function alias for GCC-generated calls
void *memcpy(void *dest, const void *src, size_t n) {
    return libmin_memcpy(dest, src, n);
}
#endif
