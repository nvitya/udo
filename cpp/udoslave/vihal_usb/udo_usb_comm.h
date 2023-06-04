/* -----------------------------------------------------------------------------
 * This file is a part of the UDO project: https://github.com/nvitya/udo
 * Copyright (c) 2023 Viktor Nagy, nvitya
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software. Permission is granted to anyone to use this
 * software for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 * --------------------------------------------------------------------------- */
/*
 *  file:     udo_usb_comm.h
 *  brief:    USB (CDC) UDO-SL Slave Implementation for VIHAL
 *  created:  2023-06-04
 *  authors:  nvitya
*/

#ifndef UDO_USB_COMM_H_
#define UDO_USB_COMM_H_

#include "platform.h"

#include "stdint.h"

#include "usbdevice.h"
#include "usbif_cdc.h"
#include "usbfunc_cdc_uart.h"

#include "udo.h"
#include "udoslave.h"

class TUdoCdcControl : public TUsbInterface
{
private:
	typedef TUsbInterface super;

public:

  TUsbCdcDescCallUnionFunc  desc_cdc_union =
  {
    .length = 5,
    .descriptor_type = 0x24,  // 0x24 = CS_INTERFACE
    .descriptor_subtype = 6,  // 6 = union func
    .master_interface = 0,    // will be set automatically
    .slave_interface = 1      // will be set automatically
  };

  TUsbCdcDescCallManagement  desc_cdc_callmgmt =
  {
    .length = 5,
    .descriptor_type = 0x24,  // 0x24 = CS_INTERFACE
    .descriptor_subtype = 6,  // 6 = union func
    .capabilities = 0,        // 0 = no call management
    .data_interface = 0       // will be set automatically
  };

	TCdcLineCoding  linecoding;

	TUsbEndpoint    ep_manage;
	uint8_t         databuf[64] __attribute__((aligned(4)));

public: // mandatory functions
	virtual bool    InitInterface();
	virtual void    OnConfigured();
	virtual bool    HandleTransferEvent(TUsbEndpoint * aep, bool htod);
	virtual bool    HandleSetupRequest(TUsbSetupRequest * psrq);
	virtual bool    HandleSetupData(TUsbSetupRequest * psrq, void * adata, unsigned adatalen);

};

class TUdoCdcData : public TUsbInterface
{
private:
	typedef TUsbInterface super;

protected:
  uint8_t         offslen = 0;
  uint8_t         rqcmd = 0;

  uint8_t         usb_txlen = 0;
  uint16_t        usb_rxlen = 0;
  uint16_t        usb_rpos = 0;

  unsigned        rxstate = 0;
  unsigned        rxcnt = 0;
  unsigned        txstate = 0;
  unsigned        txcnt = 0;
  unsigned        tx_remaining = 0;
  unsigned        tx_dataidx = 0;

  uint8_t         rxcrc = 0;
  uint8_t         txcrc = 0;

public:

  bool            rxenabled = true;
  bool            usb_ready_to_send = true;

  unsigned        lastrecvtime = 0;
  unsigned        error_count_crc = 0;
  unsigned        error_count_timeout = 0;

  TUdoRequest     rq;

  TUsbEndpoint    ep_input;
  TUsbEndpoint    ep_output;


public: // mandatory functions
	virtual bool    InitInterface();
	virtual void    OnConfigured();
	virtual bool    HandleTransferEvent(TUsbEndpoint * aep, bool htod);

public: // help functions
  void            Reset();

  void            Run();

  void            HandleRx();  // more commands might come in one packet
  void            HandleTx();

  bool            AddTx(void * asrc, unsigned len);

protected:
  uint8_t         rwdatabuf[UDO_MAX_DATALEN]  __attribute__((aligned(4)));

  uint8_t         usb_txbuf[64] __attribute__((aligned(4)));
  uint8_t         usb_rxbuf[64] __attribute__((aligned(4)));
};

class TUsbFuncUdo : public TUsbFunction
{
private:
  typedef TUsbFunction super;

public:

  TUdoCdcControl       uif_control;
  TUdoCdcData          uif_data;

  virtual bool         InitFunction();
  virtual void         Run();

};

#endif /* UDO_USB_COMM_H_ */
