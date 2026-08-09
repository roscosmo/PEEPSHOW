/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */ 
/** USBX Component                                                        */ 
/**                                                                       */
/**   Device Storage Class                                                */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

#define UX_SOURCE_CODE


/* Include necessary system files.  */

#include "ux_api.h"
#include "ux_device_class_storage.h"
#include "ux_device_stack.h"


/**************************************************************************/ 
/*                                                                        */ 
/*  FUNCTION                                               RELEASE        */ 
/*                                                                        */ 
/*    _ux_device_class_storage_mode_select                PORTABLE C      */ 
/*                                                           6.1.10       */
/*  AUTHOR                                                                */
/*                                                                        */
/*    Chaoqiong Xiao, Microsoft Corporation                               */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */ 
/*    This function performs a MODE_SELECT SCSI command. It is not        */ 
/*    supported in this release.                                          */ 
/*                                                                        */ 
/*  INPUT                                                                 */ 
/*                                                                        */ 
/*    storage                               Pointer to storage class      */ 
/*    endpoint_in                           Pointer to IN endpoint        */
/*    endpoint_out                          Pointer to OUT endpoint       */
/*    cbwcb                                 Pointer to CBWCB              */ 
/*                                                                        */ 
/*  OUTPUT                                                                */ 
/*                                                                        */ 
/*    Completion Status                                                   */ 
/*                                                                        */ 
/*  CALLS                                                                 */ 
/*                                                                        */ 
/*    _ux_device_stack_endpoint_stall       Stall endpoint                */ 
/*    _ux_device_class_storage_csw_send     Send CSW                      */
/*                                                                        */ 
/*  CALLED BY                                                             */ 
/*                                                                        */ 
/*    Device Storage Class                                                */
/*                                                                        */ 
/*  RELEASE HISTORY                                                       */ 
/*                                                                        */ 
/*    DATE              NAME                      DESCRIPTION             */ 
/*                                                                        */ 
/*  05-19-2020     Chaoqiong Xiao           Initial Version 6.0           */
/*  09-30-2020     Chaoqiong Xiao           Modified comment(s),          */
/*                                            optimized command logic,    */
/*                                            resulting in version 6.1    */
/*  12-31-2020     Chaoqiong Xiao           Modified comment(s),          */
/*                                            fixed USB CV test issues,   */
/*                                            resulting in version 6.1.3  */
/*  01-31-2022     Chaoqiong Xiao           Modified comment(s),          */
/*                                            added standalone support,   */
/*                                            resulting in version 6.1.10 */
/*                                                                        */
/**************************************************************************/
UINT  _ux_device_class_storage_mode_select(UX_SLAVE_CLASS_STORAGE *storage, ULONG lun, 
                                            UX_SLAVE_ENDPOINT *endpoint_in,
                                            UX_SLAVE_ENDPOINT *endpoint_out, UCHAR * cbwcb)
{
UINT                    status;
ULONG                   expected_length;
ULONG                   host_length;
ULONG                   remaining;
ULONG                   transfer_length;
UX_SLAVE_TRANSFER       *transfer_request;
UCHAR                   opcode;

    /* If trace is enabled, insert this event into the trace buffer.  */
    UX_TRACE_IN_LINE_INSERT(UX_TRACE_DEVICE_CLASS_STORAGE_MODE_SELECT, storage, lun, 0, 0, UX_TRACE_DEVICE_CLASS_EVENTS, 0, 0)

    host_length = storage -> ux_slave_class_storage_host_length;
    opcode = cbwcb[0];

    /* MODE SELECT(6): parameter list length is byte 4.
       MODE SELECT(10): parameter list length is bytes 7..8.  */
    if (opcode == 0x55U)
        expected_length = (ULONG)_ux_utility_short_get_big_endian(cbwcb + 7);
    else
        expected_length = (ULONG)cbwcb[4];

    /* MODE SELECT is host-to-device only. */
    if ((storage -> ux_slave_class_storage_cbw_flags & 0x80U) != 0U)
    {
#if !defined(UX_DEVICE_STANDALONE)
        _ux_device_stack_endpoint_stall(endpoint_in);
#else
        UX_PARAMETER_NOT_USED(endpoint_in);
        UX_PARAMETER_NOT_USED(endpoint_out);
#endif
        storage -> ux_slave_class_storage_csw_status = UX_SLAVE_CLASS_STORAGE_CSW_PHASE_ERROR;
        return(UX_ERROR);
    }

#if !defined(UX_DEVICE_STANDALONE)
    transfer_request = &endpoint_out -> ux_slave_endpoint_transfer_request;
    remaining = host_length;
    while (remaining > 0U)
    {
        transfer_length = remaining;
        if (transfer_length > UX_SLAVE_CLASS_STORAGE_BUFFER_SIZE)
            transfer_length = UX_SLAVE_CLASS_STORAGE_BUFFER_SIZE;

        status = _ux_device_stack_transfer_request(transfer_request, transfer_length, transfer_length);
        if (status != UX_SUCCESS)
        {
            _ux_device_stack_endpoint_stall(endpoint_out);
            storage -> ux_slave_class_storage_csw_status = UX_SLAVE_CLASS_STORAGE_CSW_FAILED;
            storage -> ux_slave_class_storage_csw_residue = remaining;
            return(status);
        }

        remaining -= transfer_length;
    }
#else
    UX_PARAMETER_NOT_USED(endpoint_in);
    UX_PARAMETER_NOT_USED(endpoint_out);
#endif

    /* Accept MODE SELECT as a no-op.
       Report host overrun only when host declared more than command list length.  */
    if (host_length > expected_length)
        storage -> ux_slave_class_storage_csw_residue = host_length - expected_length;
    else
        storage -> ux_slave_class_storage_csw_residue = 0U;
    storage -> ux_slave_class_storage_lun[lun].ux_slave_class_storage_request_sense_status = 0;
    storage -> ux_slave_class_storage_csw_status = UX_SLAVE_CLASS_STORAGE_CSW_PASSED;
    return(UX_SUCCESS);
}    

