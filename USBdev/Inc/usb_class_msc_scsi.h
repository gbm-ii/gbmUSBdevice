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
#define SCSI_FORMAT_UNIT                            0x04
#define SCSI_INQUIRY                                0x12
#define SCSI_MODE_SELECT6                           0x15
#define SCSI_MODE_SELECT10                          0x55
#define SCSI_MODE_SENSE6                            0x1A
#define SCSI_MODE_SENSE10                           0x5A
#define SCSI_ALLOW_MEDIUM_REMOVAL                   0x1E
#define SCSI_READ6                                  0x08
#define SCSI_READ10                                 0x28
#define SCSI_READ12                                 0xA8
#define SCSI_READ16                                 0x88

#define SCSI_READ_CAPACITY10                        0x25
#define SCSI_READ_CAPACITY16                        0x9E

#define SCSI_REQUEST_SENSE                          0x03
#define SCSI_START_STOP_UNIT                        0x1B
#define SCSI_TEST_UNIT_READY                        0x00
#define SCSI_WRITE6                                 0x0A
#define SCSI_WRITE10                                0x2A
#define SCSI_WRITE12                                0xAA
#define SCSI_WRITE16                                0x8A

#define SCSI_VERIFY10                               0x2F
#define SCSI_VERIFY12                               0xAF
#define SCSI_VERIFY16                               0x8F

#define SCSI_SEND_DIAGNOSTIC                        0x1D
#define SCSI_READ_FORMAT_CAPACITIES                 0x23

#define	SCSI_UNMAP	0x42	// gbm

#define NO_SENSE                                    0
#define RECOVERED_ERROR                             1
#define NOT_READY                                   2
#define MEDIUM_ERROR                                3
#define HARDWARE_ERROR                              4
#define ILLEGAL_REQUEST                             5
#define UNIT_ATTENTION                              6
#define DATA_PROTECT                                7
#define BLANK_CHECK                                 8
#define VENDOR_SPECIFIC                             9
#define COPY_ABORTED                                10
#define ABORTED_COMMAND                             11
#define VOLUME_OVERFLOW                             13
#define MISCOMPARE                                  14

#define INVALID_CDB                                 0x20
#define INVALID_FIELED_IN_COMMAND                   0x24
#define PARAMETER_LIST_LENGTH_ERROR                 0x1A
#define INVALID_FIELD_IN_PARAMETER_LIST             0x26
#define ADDRESS_OUT_OF_RANGE                        0x21
#define MEDIUM_NOT_PRESENT                          0x3A
#define MEDIUM_HAVE_CHANGED                         0x28
#define WRITE_PROTECTED                             0x27 
#define UNRECOVERED_READ_ERROR			    0x11
#define WRITE_FAULT				    0x03 

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

#define MSC_DATA_BUF_SIZE	512u

#define CBW_SIZE	31u
#define CSW_SIZE	13u

#define CBW_SIG 0x43425355
#define CSW_SIG 0x53425355

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

enum botstate_ {BS_CBW, BS_DATAOUT, BS_DATAIN, BS_CSW, BS_ERROR, BS_RSTRQ};

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
};

extern struct msc_bot_scsi_data_ bsdata;


void msc_bot_reset(void);
void msc_bot_out(const struct usbdevice_ *usbd, uint8_t epn, uint16_t len);

//========================================================================

#if 0
extern  uint8_t Page00_Inquiry_Data[];
extern  uint8_t Standard_Inquiry_Data[];
extern  uint8_t Standard_Inquiry_Data2[];
extern  uint8_t Mode_Sense6_data[];
extern  uint8_t Mode_Sense10_data[];
extern  uint8_t Scsi_Sense_Data[];
extern  uint8_t ReadCapacity10_Data[];
extern  uint8_t ReadFormatCapacity_Data [];
/**
  * @}
  */ 


/** @defgroup USBD_SCSI_Exported_TypesDefinitions
  * @{
  */

typedef struct _SENSE_ITEM {                
  char Skey;
  union {
    struct _ASCs {
      char ASC;
      char ASCQ;
    }b;
    unsigned int	ASC;
    char *pData;
  } w;
} USBD_SCSI_SenseTypeDef; 
/**
  * @}
  */ 

/** @defgroup USBD_SCSI_Exported_Macros
  * @{
  */ 

/**
  * @}
  */ 

/** @defgroup USBD_SCSI_Exported_Variables
  * @{
  */ 

/**
  * @}
  */ 
/** @defgroup USBD_SCSI_Exported_FunctionsPrototype
  * @{
  */ 
int8_t SCSI_ProcessCmd(USBD_HandleTypeDef  *pdev,
                           uint8_t lun, 
                           uint8_t *cmd);

void   SCSI_SenseCode(USBD_HandleTypeDef  *pdev,
                      uint8_t lun, 
                      uint8_t sKey, 
                      uint8_t ASC);

#endif

/**
  * @}
  */ 

#ifdef __cplusplus
}
#endif

#endif /* __USB_CLASS_MSC_SCSI_H */
/**
  * @}
  */ 

/**
  * @}
  */ 

/**
* @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

