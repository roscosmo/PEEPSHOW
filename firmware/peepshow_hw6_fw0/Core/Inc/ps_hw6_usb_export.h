#ifndef PS_HW6_USB_EXPORT_H
#define PS_HW6_USB_EXPORT_H

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

void PS_HW6_UsbExport_Reset(void);
UINT PS_HW6_UsbExport_StartDevice(void);
UINT PS_HW6_UsbExport_StopDevice(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_USB_EXPORT_H */
