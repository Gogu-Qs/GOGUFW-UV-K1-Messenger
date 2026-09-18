/* Host-only regression tests: production USB core/port with mock registers.
 * No firmware build uses this file. */
#include <assert.h>
#include <stdio.h>
#include "usb_py32_reg.h"
#include "usbd_core.h"

static USB_TypeDef mock_usb;
#undef USB
#define USB (&mock_usb)
#undef USB_NOCACHE_RAM_SECTION
#define USB_NOCACHE_RAM_SECTION

#include "../../Middlewares/CherryUSB/port/usb_dc_py32.c"
#include "../../Middlewares/CherryUSB/core/usbd_core.c"

void usbd_configure_done_callback(void) {}

static unsigned completions;
static void completed(uint8_t ep, uint32_t nbytes)
{
    assert((ep == 0x81 || ep == 0x02 || ep == 0x83) && nbytes == 0);
    completions++;
}

int main(void)
{
    static const uint8_t desc[] = {0};
    memset(&usbd_core_cfg, 0xa5, sizeof(usbd_core_cfg));
    usbd_desc_register(desc);
    assert(usbd_core_cfg.descriptors == desc);
    assert(!usbd_core_cfg.configured && !usbd_core_cfg.intf_offset);
    assert(!usbd_core_cfg.ep0_data_buf && !usbd_core_cfg.zlp_flag);
    for (unsigned i = 0; i < sizeof(usbd_core_cfg.req_data); i++)
        assert(usbd_core_cfg.req_data[i] == 0);
    for (unsigned i = 0; i < CONFIG_USBDEV_EP_COUNT; i++)
        assert(!usbd_core_cfg.in_ep_cb[i] && !usbd_core_cfg.out_ep_cb[i]);

    /* Every byte address, including reserved address bits, reaches the actual
     * port APIs and core callback registration/completion paths. */
    for (unsigned ep = 0; ep <= 255; ep++) {
        if (USB_EP_GET_IDX(ep) < CONFIG_USBDEV_EP_COUNT) continue;
        struct usbd_endpoint_cfg cfg = {.ep_addr = ep, .ep_mps = 64, .ep_type = 2};
        struct usbd_endpoint cb = {.ep_addr = ep, .ep_cb = completed};
        uint8_t data[64] = {0}, stalled = 0x55;
        mock_usb.INDEX = 0x5a;
        assert(usbd_ep_open(&cfg) < 0);
        assert(usbd_ep_close(ep) < 0);
        assert(usbd_ep_set_stall(ep) < 0);
        assert(usbd_ep_clear_stall(ep) < 0);
        assert(usbd_ep_is_stalled(ep, &stalled) < 0 && stalled == 0x55);
        assert(usbd_ep_start_read(ep, data, sizeof(data)) < 0);
        assert(usbd_ep_start_write(ep, data, sizeof(data)) < 0);
        assert(mock_usb.INDEX == 0x5a);
        usbd_add_endpoint(&cb);
        usbd_event_ep_in_complete_handler(ep, 0);
        usbd_event_ep_out_complete_handler(ep, 0);
    }
    assert(completions == 0);
    for (unsigned i = 0; i < CONFIG_USBDEV_EP_COUNT; i++)
        assert(!usbd_core_cfg.in_ep_cb[i] && !usbd_core_cfg.out_ep_cb[i]);

    usbd_core_cfg.configured = true;
    usbd_core_cfg.configuration = 1;
    uint8_t reply[2], *reply_ptr = reply;
    uint32_t reply_len;
    struct usb_setup_packet setup = {.wValue = USB_FEATURE_ENDPOINT_HALT};
    const uint8_t requests[] = {USB_REQUEST_GET_STATUS, USB_REQUEST_SET_FEATURE, USB_REQUEST_CLEAR_FEATURE};
    /* All 16-bit wIndex values for all supported endpoint requests. */
    for (unsigned r = 0; r < sizeof(requests); r++) {
        setup.bRequest = requests[r];
        for (unsigned index = 0; index <= 65535; index++) {
            setup.wIndex = index;
            reply_len = 2;
            bool valid = (index & ~0x80u) < CONFIG_USBDEV_EP_COUNT;
            assert(usbd_std_endpoint_req_handler(&setup, &reply_ptr, &reply_len) == valid);
        }
    }

    const uint8_t valid_eps[] = {0x81, 0x02, 0x83};
    for (unsigned i = 0; i < sizeof(valid_eps); i++) {
        struct usbd_endpoint cb = {.ep_addr = valid_eps[i], .ep_cb = completed};
        usbd_add_endpoint(&cb);
        if (valid_eps[i] & 0x80) usbd_event_ep_in_complete_handler(valid_eps[i], 0);
        else usbd_event_ep_out_complete_handler(valid_eps[i], 0);
    }
    assert(completions == 3);

    /* Even if unsupported IRQ bits are enabled, they must not walk past EP3. */
    memset(&mock_usb, 0, sizeof(mock_usb));
    mock_usb.INT_IN1 = mock_usb.INT_IN1E = 0xf0;
    mock_usb.INT_OUT1 = mock_usb.INT_OUT1E = 0xf1;
    mock_usb.INDEX = 2;
    USBD_IRQHandler();
    assert(mock_usb.INDEX == 2 && completions == 3);

    /* A real in-range IN completion still dispatches through the shortened table. */
    mock_usb.INT_IN1 = mock_usb.INT_IN1E = 2;
    mock_usb.INT_OUT1 = mock_usb.INT_OUT1E = 0;
    uint8_t byte = 0;
    g_pyusb_udc.in_ep[1].xfer_buf = &byte;
    g_pyusb_udc.in_ep[1].ep_mps = 64;
    USBD_IRQHandler();
    assert(completions == 4);

    puts("USB: NOLOAD initialization, 256 endpoint addresses, 65536 host indices, callbacks and IRQ bounds PASS");
    return 0;
}
