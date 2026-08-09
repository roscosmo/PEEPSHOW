#ifndef PEEPSHOW_USBX_STORAGE_OPCODE_OVERRIDE_H
#define PEEPSHOW_USBX_STORAGE_OPCODE_OVERRIDE_H

/*
 * Wrapper include for USBX storage class declarations.
 *
 * Do not use include_next here: this project adds USBX/App to the include
 * search path, so include_next can resolve back to this wrapper via -I and
 * skip the real middleware declaration set.
 */
#include "../../Middlewares/ST/usbx/common/usbx_device_classes/inc/ux_device_class_storage.h"

/*
 * Force write-through mode advertisement in MODE SENSE caching page.
 *
 * Windows issues MODE SELECT(6) to clear WCE when we advertise it set.
 * Keeping WCE clear avoids that host path and prevents BOT enumeration
 * from stalling before the volume is mounted.
 */
#ifdef UX_SLAVE_CLASS_STORAGE_CACHING_MODE_PAGE_FLAG_WCE
#undef UX_SLAVE_CLASS_STORAGE_CACHING_MODE_PAGE_FLAG_WCE
#endif
#define UX_SLAVE_CLASS_STORAGE_CACHING_MODE_PAGE_FLAG_WCE (0u)

#endif /* PEEPSHOW_USBX_STORAGE_OPCODE_OVERRIDE_H */
