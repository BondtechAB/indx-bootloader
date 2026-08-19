/*
 * 1kByte USB DFU bootloader for Atmel SAMD11 microcontrollers
 *
 * Copyright (c) 2018-2020, Peter Lawrence
 * derived from https://github.com/ataradov/vcp Copyright (c) 2016, Alex Taradov
 * <alex@taradov.com> All rights reserved.
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

/*
NOTES:
- anything pointed to by udc_mem[*].*.ADDR.reg *MUST* BE IN RAM and be 32-bit
aligned... no exceptions
*/

/*- Includes ----------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nvm_data.h"
#include "same51j19a.h"
#include "usb.h"
#include "usb_descriptors.h"

/*- Definitions -------------------------------------------------------------*/
#define REBOOT_AFTER_DOWNLOAD /* comment out to prevent boot into app after it \
                                 has been downloaded */
#define USB_CMD(dir, rcpt, type)                                               \
  ((USB_##dir##_TRANSFER << 7) | (USB_##type##_REQUEST << 5) |                 \
   (USB_##rcpt##_RECIPIENT << 0))
#define SIMPLE_USB_CMD(rcpt, type)                                             \
  ((USB_##type##_REQUEST << 5) | (USB_##rcpt##_RECIPIENT << 0))

/*- Types -------------------------------------------------------------------*/
typedef struct {
  UsbDeviceDescBank out;
  UsbDeviceDescBank in;
} udc_mem_t;

/*- Variables ---------------------------------------------------------------*/
static uint32_t usb_config = 0;
static uint32_t dfu_status_choices[4] = {
    0x00000000,
    0x00000002, /* normal */
    0x00000000,
    0x00000005, /* dl */
};

static udc_mem_t udc_mem[USB_EPT_NUM];
static uint32_t udc_ctrl_in_buf[16];
static uint32_t udc_ctrl_out_buf[16];

/*- Implementations ---------------------------------------------------------*/

//-----------------------------------------------------------------------------
static void __attribute__((noinline)) udc_control_send(const uint32_t *data,
                                                       uint32_t size) {
  /* USB peripheral *only* reads valid data from 32-bit aligned RAM locations */
  udc_mem[0].in.ADDR.reg = (uint32_t)data;

  udc_mem[0].in.PCKSIZE.reg = USB_DEVICE_PCKSIZE_BYTE_COUNT(size) |
                              USB_DEVICE_PCKSIZE_MULTI_PACKET_SIZE(0) |
                              USB_DEVICE_PCKSIZE_SIZE(3 /*64 Byte*/);

  USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT1;
  USB->DEVICE.DeviceEndpoint[0].EPSTATUSSET.bit.BK1RDY = 1;

  while (0 == USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.bit.TRCPT1)
    ;
}

//-----------------------------------------------------------------------------
static void __attribute__((noinline)) udc_control_send_zlp(void) {
  udc_control_send(NULL,
                   0); /* peripheral can't read from NULL address, but size is
                          zero and this value takes less space to compile */
}

//-----------------------------------------------------------------------------
static uint32_t __attribute__((noinline)) USB_Service(void) {
  static uint32_t dfu_addr;

  if (USB->DEVICE.INTFLAG.bit.EORST) /* End Of Reset */
  {
    USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_EORST;
    USB->DEVICE.DADD.reg = USB_DEVICE_DADD_ADDEN;

    USB->DEVICE.DeviceEndpoint[0].EPCFG.reg =
        USB_DEVICE_EPCFG_EPTYPE0(1 /*CONTROL*/) |
        USB_DEVICE_EPCFG_EPTYPE1(1 /*CONTROL*/);
    USB->DEVICE.DeviceEndpoint[0].EPSTATUSSET.bit.BK0RDY = 1;
    USB->DEVICE.DeviceEndpoint[0].EPSTATUSCLR.bit.BK1RDY = 1;

    udc_mem[0].in.ADDR.reg = (uint32_t)udc_ctrl_in_buf;
    udc_mem[0].in.PCKSIZE.reg = USB_DEVICE_PCKSIZE_BYTE_COUNT(0) |
                                USB_DEVICE_PCKSIZE_MULTI_PACKET_SIZE(0) |
                                USB_DEVICE_PCKSIZE_SIZE(3 /*64 Byte*/);

    udc_mem[0].out.ADDR.reg = (uint32_t)udc_ctrl_out_buf;
    udc_mem[0].out.PCKSIZE.reg = USB_DEVICE_PCKSIZE_BYTE_COUNT(64) |
                                 USB_DEVICE_PCKSIZE_MULTI_PACKET_SIZE(0) |
                                 USB_DEVICE_PCKSIZE_SIZE(3 /*64 Byte*/);

    USB->DEVICE.DeviceEndpoint[0].EPSTATUSCLR.bit.BK0RDY = 1;
  }

  if (USB->DEVICE.DeviceEndpoint[0]
          .EPINTFLAG.bit.TRCPT0) /* Transmit Complete 0 */
  {
    if (dfu_addr) {

      // Unlock region containing this address
      NVMCTRL->ADDR.reg = dfu_addr;
      NVMCTRL->CTRLB.reg =
          NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD(NVMCTRL_CTRLB_CMD_UR);
      while (!NVMCTRL->STATUS.bit.READY)
        ;

      /* on a block boundary write, perform erase */
      if ((dfu_addr % NVMCTRL_BLOCK_SIZE) == 0) {
        NVMCTRL->ADDR.reg = dfu_addr;
        NVMCTRL->CTRLB.reg =
            NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD(NVMCTRL_CTRLB_CMD_EB);
        while (!NVMCTRL->STATUS.bit.READY)
          ;
      }

      // Clear page buffer
      NVMCTRL->CTRLB.reg =
          NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD(NVMCTRL_CTRLB_CMD_PBC);
      while (!NVMCTRL->STATUS.bit.READY)
        ;

      // Fill page buffer
      uint32_t *nvm_addr = (uint32_t *)dfu_addr;
      uint32_t *ram_addr = (uint32_t *)udc_ctrl_out_buf;
      for (unsigned i = 0; i < 16; i++)
        *nvm_addr++ = *ram_addr++;
      while (!NVMCTRL->STATUS.bit.READY)
        ;

      // Write page buffer
      NVMCTRL->ADDR.reg = dfu_addr;
      NVMCTRL->CTRLB.reg =
          NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD(NVMCTRL_CTRLB_CMD_WP);
      while (!NVMCTRL->STATUS.bit.READY)
        ;

      udc_control_send_zlp();
      dfu_addr = 0;
    }

    USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT0;
  }

  if (USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.bit.RXSTP) /* Received Setup */
  {
    USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_RXSTP;
    USB->DEVICE.DeviceEndpoint[0].EPSTATUSCLR.bit.BK0RDY = 1;

    usb_request_t *request = (usb_request_t *)udc_ctrl_out_buf;
    uint8_t type = request->wValue >> 8;
    uint16_t length = request->wLength;
    uint8_t index = request->wValue & 0xFF;
    static uint32_t *dfu_status = dfu_status_choices + 0;

    /* for these other USB requests, we must examine all fields in bmRequestType
     */
    if (USB_CMD(OUT, INTERFACE, STANDARD) == request->bmRequestType) {
      udc_control_send_zlp();
      return 0;
    }

    /* for these "simple" USB requests, we can ignore the direction and use only
     * bRequest */
    switch (request->bmRequestType & 0x7F) {
    case SIMPLE_USB_CMD(DEVICE, STANDARD):
    case SIMPLE_USB_CMD(INTERFACE, STANDARD):
      switch (request->bRequest) {
      case USB_GET_DESCRIPTOR:
        if (USB_DEVICE_DESCRIPTOR == type) {
          udc_control_send((uint32_t *)&usb_device_descriptor, length);
        } else if (USB_CONFIGURATION_DESCRIPTOR == type) {
          udc_control_send((uint32_t *)&usb_configuration_hierarchy, length);
        } else if (USB_STRING_DESCRIPTOR == type) {
          if (index == 0) {
            udc_control_send((uint32_t *)&usb_str_desc_langid, length);
          } else if (index == 1) {
            udc_control_send((uint32_t *)&usb_str_desc_manufacturer, length);
          } else if (index == 2) {
            udc_control_send((uint32_t *)&usb_str_desc_product, length);
          }
        } else {
          USB->DEVICE.DeviceEndpoint[0].EPSTATUSSET.bit.STALLRQ1 = 1;
        }
        break;
      case USB_GET_CONFIGURATION:
        udc_control_send(&usb_config, 1);
        break;
      case USB_GET_STATUS:
        udc_control_send(dfu_status_choices + 0,
                         2); /* a 32-bit aligned zero in RAM is all we need */
        break;
      case USB_SET_FEATURE:
      case USB_CLEAR_FEATURE:
        USB->DEVICE.DeviceEndpoint[0].EPSTATUSSET.bit.STALLRQ1 = 1;
        break;
      case USB_SET_ADDRESS:
        udc_control_send_zlp();
        USB->DEVICE.DADD.reg =
            USB_DEVICE_DADD_ADDEN | USB_DEVICE_DADD_DADD(request->wValue);
        break;
      case USB_SET_CONFIGURATION:
        usb_config = request->wValue;
        udc_control_send_zlp();
        break;
      }
      break;
    case SIMPLE_USB_CMD(INTERFACE, CLASS):
      switch (request->bRequest) {
      case 0x03: // DFU_GETSTATUS
        udc_control_send(&dfu_status[0], 6);
        break;
      case 0x05: // DFU_GETSTATE
        udc_control_send(&dfu_status[1], 1);
        break;
      case 0x01: // DFU_DNLOAD
        dfu_status = dfu_status_choices + 0;
        if (request->wLength) {
          dfu_status = dfu_status_choices + 2;
          dfu_addr = APP_OFFSET + request->wValue * 64;
        }
#ifdef REBOOT_AFTER_DOWNLOAD
        else {
          /* the download has now finished, so now reboot */
          WDT->CONFIG.reg = WDT_CONFIG_PER_CYC8 | WDT_CONFIG_WINDOW_CYC8;
          WDT->CTRLA.reg = WDT_CTRLA_ENABLE;
        }
#endif
        /* fall through */
      default: // DFU_UPLOAD & others
        /* 0x00 == DFU_DETACH, 0x04 == DFU_CLRSTATUS, 0x06 == DFU_ABORT, and
         * 0x01 == DFU_DNLOAD and 0x02 == DFU_UPLOAD */
        if (!dfu_addr)
          udc_control_send_zlp();
        break;
      }
      break;
    }
  }

  return 0;
}

uint32_t run_bootloader() {
  /*
  configure oscillator for crystal-free USB operation (USBCRM / USB Clock
  Recovery Mode)
  */

  OSCCTRL->DFLLCTRLB.reg = OSCCTRL_DFLLCTRLB_MODE | OSCCTRL_DFLLCTRLB_USBCRM;
  OSCCTRL->DFLLMUL.bit.MUL = 48000;

  GCLK->GENCTRL[0].reg = GCLK_GENCTRL_SRC(GCLK_SOURCE_DFLL48M) |
                         GCLK_GENCTRL_RUNSTDBY | GCLK_GENCTRL_GENEN;
  while (GCLK->SYNCBUSY.bit.GENCTRL0)
    ;

  /*
  initialize USB
  */

  PORT->Group[0].WRCONFIG.reg = PORT_WRCONFIG_HWSEL | PORT_WRCONFIG_WRPINCFG |
                                PORT_WRCONFIG_WRPMUX | PORT_WRCONFIG_PMUXEN |
                                PORT_WRCONFIG_PMUX(MUX_PA24H_USB_DM) |
                                PORT_WRCONFIG_PINMASK(0x0300);

  MCLK->APBBMASK.reg |= MCLK_APBBMASK_USB;

  GCLK->PCHCTRL[USB_GCLK_ID].reg = GCLK_PCHCTRL_GEN(0) | GCLK_PCHCTRL_CHEN;

  USB->DEVICE.CTRLA.reg = USB_CTRLA_SWRST;
  while (USB->DEVICE.SYNCBUSY.bit.SWRST)
    ;

  USB->DEVICE.PADCAL.reg = USB_PADCAL_TRANSN(NVM_READ_CAL(NVM_USB_TRANSN)) |
                           USB_PADCAL_TRANSP(NVM_READ_CAL(NVM_USB_TRANSP)) |
                           USB_PADCAL_TRIM(NVM_READ_CAL(NVM_USB_TRIM));

  USB->DEVICE.DESCADD.reg = (uint32_t)udc_mem;

  USB->DEVICE.CTRLA.reg = USB_CTRLA_MODE_DEVICE | USB_CTRLA_RUNSTDBY;
  USB->DEVICE.CTRLB.reg = USB_DEVICE_CTRLB_SPDCONF_FS;
  USB->DEVICE.CTRLA.reg |= USB_CTRLA_ENABLE;

  /*
  service USB
  */

#if (BOOT2_OFFSET + 0)
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DWT->CYCCNT = 0;
  // 1000ms timeout before we jump to second bootloader
  uint32_t sof_timeout = 48000000;
#endif

  while (1) {
    uint32_t addr = USB_Service();
    if (addr != 0)
      return addr;

#if (BOOT2_OFFSET + 0)
    if (sof_timeout != 0) {

      if (USB->DEVICE.INTFLAG.bit.SOF) {
        // SOF detected, stay in USB mode
        sof_timeout = 0;
      } else if ((int32_t)(sof_timeout - DWT->CYCCNT) < 0) {
        // SOF not detected within timeout window, go to second bootloader
        USB->DEVICE.CTRLA.reg = USB_CTRLA_SWRST;
        PORT->Group[0].WRCONFIG.reg =
            PORT_WRCONFIG_HWSEL | PORT_WRCONFIG_WRPINCFG |
            PORT_WRCONFIG_WRPMUX | PORT_WRCONFIG_PMUXEN |
            PORT_WRCONFIG_PMUX(0) | PORT_WRCONFIG_PINMASK(0x0300);
        return BOOT2_OFFSET;
      }
    }
#endif
  }
}

// Delays for ~100us, used for waiting for pin pulls to settle.
static void delay_pinwait() {
  __asm__ volatile("" : : : "memory");
  for (int i = 0; i < 1200; i++) {
    __asm__ volatile("nop" : : : "memory");
  }
  __asm__ volatile("" : : : "memory");
}

uint32_t bootloader(void) {
//#if (BOOT2_OFFSET + 0)
//  /* Configure PB31 "USB_CAN_SELECT" as mode selector with pulldown. If it is
//   * low, jump to CAN bootloader instead */
//  PORT->Group[1].PINCFG[31].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_INEN;
//  PORT->Group[1].OUTSET.reg = (1UL << 31);
//
//  delay_pinwait();

//  if (!(PORT->Group[1].IN.reg & (1UL << 31))) {
//    return BOOT2_OFFSET; /* pin grounded, so run bootloader */
//  }
//#endif
  

  /* Check for magic value bootloader request */
  volatile uint32_t *magic_loc =
      (volatile uint32_t *)(HSRAM_ADDR + HSRAM_SIZE - 4);
  if (*magic_loc == 0xF01669EF) {
    *magic_loc = 0;
    return run_bootloader();
  }

  /* Configure PB23 "CAN RESET" as bootloader entry pin with a pull-up */
  PORT->Group[1].PINCFG[23].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_INEN;
  PORT->Group[1].OUTSET.reg = (1UL << 23);

  delay_pinwait();

  if (!(PORT->Group[1].IN.reg & (1UL << 23))) {
    return run_bootloader(); /* pin grounded, so run bootloader */
  }

  /* unlock DSU */
  PAC->WRCTRL.reg = PAC_WRCTRL_PERID(ID_DSU) | PAC_WRCTRL_KEY_CLR;

  /* Read length stored in the unused pvReservedM9 vector */
  uint32_t crc_addr = *(volatile uint32_t *)(APP_OFFSET + 0x1C);

  if (crc_addr < APP_OFFSET) {
    return run_bootloader();
  }

  uint32_t len = crc_addr - APP_OFFSET;

  /* Read length is too large, firmware can't be valid. */
  if (len > FLASH_SIZE - APP_OFFSET - 4) {
    return run_bootloader();
  }

  // /* Read CRC address stored at APP_OFFSET + the len we just read */
  uint32_t crc = *(volatile uint32_t *)(crc_addr);

  DSU->STATUSA.bit.DONE = 1;
  DSU->ADDR.reg = APP_OFFSET; /* start CRC check at beginning of user app */
  DSU->LENGTH.reg = len;

  /* ask DSU to compute CRC */
  DSU->DATA.reg = 0xFFFFFFFF;
  DSU->CTRL.bit.CRC = 1;
  while (!DSU->STATUSA.bit.DONE)
    ;

  if ((~DSU->DATA.reg) != crc)
    return run_bootloader(); /* CRC failed, so run bootloader */

  return APP_OFFSET; /* we've checked everything and there is no reason to run
             the bootloader */
}
