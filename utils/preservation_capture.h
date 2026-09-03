#ifndef PDW_PRESERVATION_CAPTURE_H
#define PDW_PRESERVATION_CAPTURE_H

#include <stddef.h>

void PreservationCaptureMessage(
    const char *capcode,
    const char *time,
    const char *date,
    const char *mode,
    const char *type,
    const char *bitrate,
    const char *message,
    const char *mobitex,
    size_t max_field_bytes);

#endif
