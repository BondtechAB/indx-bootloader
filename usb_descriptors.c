/*
 * Copyright (c) 2016, Alex Taradov <alex@taradov.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*- Includes ----------------------------------------------------------------*/
#include "usb_descriptors.h"
#include "usb.h"

/* Bondtech AB */
#define USB_STR_MANUFACTURER                                                   \
  "\x18\x03\x42\x00\x6f\x00\x6e\x00\x64\x00\x74\x00\x65\x00\x63\x00"           \
  "\x68\x00\x20\x00\x41\x00\x42\x00"
/* INDX Toolboard Bootloader */
#define USB_STR_PRODUCT                                                        \
  "\x34\x03\x49\x00\x4e\x00\x44\x00\x58\x00\x20\x00\x54\x00\x6f\x00"           \
  "\x6f\x00\x6c\x00\x62\x00\x6f\x00\x61\x00\x72\x00\x64\x00\x20\x00\x42\x00"   \
  "\x6f\x00\x6f\x00\x74\x00\x6c\x00\x6f\x00\x61\x00\x64\x00\x65\x00\x72\x00"

/*- Variables ---------------------------------------------------------------*/
usb_device_descriptor_t usb_device_descriptor
    __attribute__((aligned(4))) = /* MUST BE IN RAM for USB peripheral */
    {.bLength = sizeof(usb_device_descriptor_t),
     .bDescriptorType = USB_DEVICE_DESCRIPTOR,

     .bcdUSB = 0x0100,
     .bDeviceClass = 254,
     .bDeviceSubClass = 1, /* DFU */
     .bDeviceProtocol = 0,

     .bMaxPacketSize0 = 64,
     .idVendor = 0x04d8,  /* Microchip */
     .idProduct = 0xe483, /* Bondtech INDX */
     .bcdDevice = 0x0107,

     .iManufacturer = USB_STR_MANUFACTURER_ID,
     .iProduct = USB_STR_PRODUCT_ID,
     .iSerialNumber = USB_STR_ZERO,

     .bNumConfigurations = 1};

usb_configuration_hierarchy_t usb_configuration_hierarchy
    __attribute__((aligned(4))) = /* MUST BE IN RAM for USB peripheral */
    {
        .configuration =
            {
                .bLength = sizeof(usb_configuration_descriptor_t),
                .bDescriptorType = USB_CONFIGURATION_DESCRIPTOR,
                .wTotalLength = sizeof(usb_configuration_hierarchy_t),
                .bNumInterfaces = 1,
                .bConfigurationValue = 1,
                .iConfiguration = USB_STR_ZERO,
                .bmAttributes = 0x80,
                .bMaxPower = 50, // 100 mA
            },

        .interface =
            {
                .bLength = sizeof(usb_interface_descriptor_t),
                .bDescriptorType = USB_INTERFACE_DESCRIPTOR,
                .bInterfaceNumber = 0,
                .bAlternateSetting = 0,
                .bNumEndpoints = 0,
                .bInterfaceClass = 254,
                .bInterfaceSubClass = 1,
                .bInterfaceProtocol = 2,
                .iInterface = USB_STR_PRODUCT_ID,
            },

        .dfu =
            {
                .bLength = sizeof(usb_dfu_descriptor_t),
                .bDescriptorType = 33,
                .bmAttributes = 3,
                .wDetachTimeout = 0,
                .wTransferSize = 64,
                .bcdDFU = 0x100,
            },
};

usb_strings_descriptor_t usb_str_desc_langid
    __attribute__((aligned(4))) = {.bLength = 4,
                                   .bDescriptorType = USB_STRING_DESCRIPTOR,
                                   .data = USB_LANGID_ENGLISH_US};

uint8_t usb_str_desc_manufacturer[] __attribute__((aligned(4))) =
    USB_STR_MANUFACTURER;
uint8_t usb_str_desc_product[] __attribute__((aligned(4))) = USB_STR_PRODUCT;
