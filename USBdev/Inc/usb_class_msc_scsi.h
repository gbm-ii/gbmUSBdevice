/**
 * modified by gbm for multiple LUNs and UNMAP
  ******************************************************************************
  * @file    usbd_msc_scsi.h
  * @author  MCD Application Team
  * @version V2.4.2
  * @date    11-December-2015
  * @brief   Header for the usbd_msc_scsi.c file
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_CLASS_MSC_SCSI_H
#define __USB_CLASS_MSC_SCSI_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
//#include "usbd_def.h"

/** @defgroup USBD_SCSI_Exported_Defines
  * @{
  */

// MSC subclass codes ====================================================
#define MSC_SCSI_TRANSPARENT_COMMAND_SET	0x06

#define USB_MSC_BULK_ONLY_TRANSPORT		0x50

#define SENSE_LIST_DEEPTH                           4

/* SCSI Commands */
#define SCSI_TEST_UNIT_READY                        0x00	// used
#define SCSI_REQUEST_SENSE                          0x03	// used
#define SCSI_FORMAT_UNIT                            0x04
#define SCSI_WRITE6                                 0x0A
#define SCSI_READ6                                  0x08

#define SCSI_INQUIRY                                0x12	// used
#define SCSI_MODE_SELECT6                           0x15
#define SCSI_MODE_SENSE6                            0x1A	// used
#define SCSI_START_STOP_UNIT                        0x1B
#define SCSI_SEND_DIAGNOSTIC                        0x1D
#define SCSI_ALLOW_MEDIUM_REMOVAL                   0x1E	// used

#define SCSI_READ_FORMAT_CAPACITIES                 0x23	// used
#define SCSI_READ_CAPACITY10                        0x25	// used
#define SCSI_READ10                                 0x28	// used
#define SCSI_WRITE10                                0x2A	// used
#define SCSI_VERIFY10                               0x2F

#define SCSI_MODE_SELECT10                          0x55
#define SCSI_MODE_SENSE10                           0x5A

#define SCSI_READ16                                 0x88
#define SCSI_VERIFY16                               0x8F
#define SCSI_WRITE16                                0x8A
#define SCSI_READ_CAPACITY16                        0x9E

#define SCSI_READ12                                 0xA8
#define SCSI_WRITE12                                0xAA
#define SCSI_VERIFY12                               0xAF

#define	SCSI_UNMAP	0x42	// gbm

// sense key codes returned by REQUEST_SENSE
#define SKEY_NO_SENSE                        0
//#define RECOVERED_ERROR                             1
#define SKEY_NOT_READY                                   2
//#define MEDIUM_ERROR                                3
//#define HARDWARE_ERROR                              4
#define SKEY_ILLEGAL_REQUEST                             5
//#define UNIT_ATTENTION                              6
//#define DATA_PROTECT                                7
//#define BLANK_CHECK                                 8
//#define VENDOR_SPECIFIC                             9
//#define COPY_ABORTED                                10
//#define ABORTED_COMMAND                             11
//#define VOLUME_OVERFLOW                             13
//#define MISCOMPARE                                  14

// ASC codes returned by REQUEST_SENSE
#define ASC_NO_SENSE                        0
#define ASC_WRITE_FAULT						0x03
#define ASC_UNRECOVERED_READ_ERROR			0x11
#define ASC_PARAMETER_LIST_LENGTH_ERROR		0x1A
#define ASC_INVALID_CDB						0x20
#define ASC_LBA_OUT_OF_RANGE                0x21
#define ASC_INVALID_FIELD_IN_CDB			0x24
#define ASC_INVALID_FIELD_IN_PARAMETER_LIST	0x26
#define ASC_MEDIUM_HAVE_CHANGED				0x28
#define ASC_WRITE_PROTECTED					0x27
#define ASC_MEDIUM_NOT_PRESENT              0x3A


#define READ_FORMAT_CAPACITY_DATA_LEN               0x0C
#define READ_CAPACITY10_DATA_LEN                    0x08
#define MODE_SENSE10_DATA_LEN                       0x08
#define MODE_SENSE6_DATA_LEN                        0x04
#define REQUEST_SENSE_DATA_LEN                      0x12
#define STANDARD_INQUIRY_DATA_LEN                   0x24
#define BLKVFY                                      0x04

// BOT layer defs by gbm =================================================

#define	BOTRQ_RESET	0xff
#define BOTRQ_GET_MAX_LUN	0xfe

#define MSC_DATA_BUF_SIZE	512u	// implementation-specific!

#define CBW_SIZE	31u
#define CSW_SIZE	13u

#define CBW_SIG 0x43425355
#define CSW_SIG 0x53425355

// error codes returned in CSW
#define	BOT_CMD_PASSED	0u
#define	BOT_CMD_FAILED	1u
#define	BOT_PHASE_ERROR	2u

struct botCBW_ {
	uint32_t dSignature, dTag, dDataTransferLength;
	union {
		uint8_t b;
		struct {
			uint8_t b0_6:7, DirIn:1;
		};
	} bmFlags;
	uint8_t bLUN, bCBLength;
	uint8_t CB[16];
};

struct botCSW_ {
	uint32_t dSignature, dTag, dDataResidue;
	uint8_t bStatus;
};

enum botstate_ {BS_CBW,	// waiting for CBW
	BS_DATAOUT, BS_DATAIN,	// data transfer
	BS_CSW,	// data In transfer complete, waiting for In ep to send CSW
	BS_INVCBW,
	BS_RESET	// BOT Reset request received
};

struct msc_bot_scsi_data_ {
	struct botCBW_ cbw;
	struct botCSW_ csw;
	enum botstate_ state;
	uint8_t devDirIn;	// device data direction according to command interpreted
	uint32_t devTransferLength;
	uint32_t devDataTransfered;
	uint32_t scsi_blkaddr;
	uint32_t scsi_nblocks;
	const uint8_t *txptr;
	uint8_t *rxptr;
	uint8_t databuf[MSC_DATA_BUF_SIZE];
	uint8_t outbuf[MSC_BOT_EP_SIZE];
	uint16_t dbidx;
	bool prevent_removal;
	bool in_busy;
};

extern struct msc_bot_scsi_data_ bsdata;


void msc_bot_init(const struct usbdevice_ *usbd);
void msc_bot_reset(void);
void msc_bot_out(const struct usbdevice_ *usbd, uint8_t epn, uint16_t len);
void msc_bot_in(const struct usbdevice_ *usbd, uint8_t epn);
void msc_bot_ClearEPStall(const struct usbdevice_ *usbd, uint8_t epaddr);

//========================================================================

#endif

