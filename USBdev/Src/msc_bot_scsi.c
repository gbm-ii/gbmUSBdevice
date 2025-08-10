/*
MSC requests:
- reset
- get max LUN

BOT protocol
https://www.usb.org/sites/default/files/usbmassbulk_10.pdf

Packet flow:
h->d	CBW
h->d or d->h Data out/in
d->h	CSW

CBW
31 bytes
3 * 4 B + 3 B + CBWCB
include 15 B header and 1..16 B CBWCB
always sent as 31-byte packet
header - little-endian ordering, long fields are size-aligned

CSW
13 B, 3*4 B + 1B

BOT states
start - ready for CBW
CBW_ok


SCSI minimal command set - documented in
https://www.usb.org/sites/default/files/usb_msc_boot_1.0.pdf

all basic commands are 12 B long
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "usb_dev.h"
#include "usb_dev_config.h"
#include "usb_class_msc_scsi.h"

struct msc_bot_scsi_data_ bsdata;

static inline uint16_t getBE16(const uint8_t *p)
{
	return p[0] << 8 | p[1];
}

static inline uint32_t getBE32(const uint8_t *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

#define nLUNs 1u

static const uint8_t inquiry_data[36] = {
		0,	// device type
		0,	// bit 7 set -> removable media
		0, 0,
		0,	// additional length
		0, 0, 0,	// reserved
		[8] = 'g', 'b', 'm', ' ', ' ', ' ', ' ', ' ',	// vendor ID
		[16] = 'M', 'a', 's', 's', ' ', 'S', 't', 'o', 'r', 'a', 'g', 'e', ' ', ' ', ' ', ' ',	// product ID
		[32] = 'A', '0', '0', '0'	// revision level
};

#define MODE_SENSE6_LEN			 8
#define MODE_SENSE10_LEN		 8
#define LENGTH_INQUIRY_PAGE00		 7
#define LENGTH_FORMAT_CAPACITIES    	20
/* USB Mass storage Page 0 Inquiry Data */
const uint8_t  MSC_Page00_Inquiry_Data[] = {//7
	0x00,
	0x00,
	0x00,
	(LENGTH_INQUIRY_PAGE00 - 4),
	0x00,
	0x80,
	0x83
};

/* USB Mass storage sense 10  Data */
const uint8_t  MSC_Mode_Sense10_data[] = {
	0x00,
	0x06,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00
};

/* USB Mass storage sense 6  Data */
const uint8_t  MSC_Mode_Sense6_data[] = {
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00
};

struct sense_data_ {
	uint8_t error_code, segment_number, sense_key,
		inf[4],	// 3..6
		asl,	// additional sense length
		csinfo[4],	// 8..11
		asc, ascq,	// 12, 13
		reserved[4];	// 14..17
};

static struct sense_data_ sense_data = {
		0x70, 0,
		0,
		.asl = sizeof(struct sense_data_) - 7
};

#define NUM_BLOCKS	128u
#define	LAST_LBA	(NUM_BLOCKS - 1u)
#define BLK_SIZE	512u

#if 1
alignas (uint32_t) static uint8_t media[NUM_BLOCKS][BLK_SIZE];

static bool media_write(uint8_t lun, uint32_t blk, const uint8_t *buf)
{
	memcpy(media[blk], buf, BLK_SIZE);
	return 0;
}

static bool media_read(uint8_t lun, uint32_t blk, uint8_t *buf)
{
	memcpy(buf, media[blk], BLK_SIZE);
	return 0;
}
#endif

static const uint8_t read_capacity_data[8] = {
		LAST_LBA >> 24, LAST_LBA >> 16 & 0xff, LAST_LBA >> 8 & 0xff, LAST_LBA & 0xff,
		BLK_SIZE >> 24, BLK_SIZE >> 16 & 0xff, BLK_SIZE >> 8 & 0xff, BLK_SIZE & 0xff
};

// get and verify data transfer parms, return 1 if ok, 0 if incorrect
static bool getparm10(void)
{
	//bsdata.scsi_lun = bsdata.cbw.CB[1] >> 5;
	bsdata.scsi_blkaddr = getBE32(&bsdata.cbw.CB[2]);
	bsdata.scsi_nblocks = getBE16(&bsdata.cbw.CB[7]);
	bsdata.devTransferLength = bsdata.scsi_nblocks * BLK_SIZE;
	bsdata.devDataTransfered = 0;
	bsdata.dbidx = 0;
	bsdata.csw.dDataResidue = bsdata.devTransferLength;
	return bsdata.scsi_nblocks
			&& bsdata.scsi_blkaddr < NUM_BLOCKS
			&& bsdata.scsi_blkaddr + bsdata.scsi_nblocks <= NUM_BLOCKS
			&& bsdata.devTransferLength == bsdata.cbw.dDataTransferLength;
}

static bool chkparm(void)
{
	return 0;
}

// bitmap for SCSI commands recording
volatile uint8_t rq[32];

static void enable_out_ep(const struct usbdevice_ *usbd)
{

}

static void prepare_for_cbw(const struct usbdevice_ *usbd)
{
	;
}

void msc_bot_init(const struct usbdevice_ *usbd)
{
	bsdata = (struct msc_bot_scsi_data_){0};
	// enable out ep
}

void msc_bot_reset(void)
{
	// does not change data toggle nor ep stalls
//	bsdata.state = BS_CBW;
	bsdata.state = BS_RESET;
	// allow receive
}

void msc_bot_ClearEPStall(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	if (bsdata.state == BS_ERROR)
	{
		// stall IN
		bsdata.state = BS_CBW;
	}
	else if ((epaddr & 0x80) && bsdata.state != BS_RESET)
	{
		// send failed command CSW
	}
}

static void msc_bot_abort(const struct usbdevice_ *usbd)
{
	if (bsdata.cbw.bmFlags.b == 0 && bsdata.cbw.dDataTransferLength && bsdata.state < BS_ERROR)
	{
		// stall out
	}
	// stall in

	if (bsdata.state == BS_ERROR)
	{
		// allow receive
	}

}

static void msc_scsi_sense(uint8_t lun, uint8_t erc)
{
	;
}
		//SCSI_SENSE_ILLEGAL_REQUEST, INVALID_CDB, 0)

static void scsi_error(uint8_t sKey, uint8_t ASC)
{
	sense_data.sense_key = sKey;
	sense_data.asc = ASC;
}

static void bot_send_csw(const struct usbdevice_ *usbd)
{
	USBdev_SendData(usbd, MSC_BOT_IN_EP, (const uint8_t *)&bsdata.csw, CSW_SIZE, 0);
	bsdata.state = BS_CBW;
}

// send packet of data read from mass storage device
static void scsi_read_xfer(const struct usbdevice_ *usbd)
{
	if (bsdata.dbidx == 0)
	{
		// buffer empty - read from media
		media_read(bsdata.cbw.bLUN, bsdata.scsi_blkaddr, bsdata.databuf);
	}
	uint16_t txlen = bsdata.devTransferLength < MSC_BOT_EP_SIZE
			? bsdata.devTransferLength : MSC_BOT_EP_SIZE;
	USBdev_SendData(usbd, MSC_BOT_IN_EP, bsdata.txptr, txlen, 0);
	bsdata.dbidx += txlen;
	bsdata.devTransferLength -= txlen;
	if (bsdata.dbidx == MSC_DATA_BUF_SIZE)
	{
		bsdata.dbidx = 0;
		++bsdata.scsi_blkaddr;
		--bsdata.scsi_nblocks;
	}
	if (bsdata.devTransferLength == 0)
	{
		bsdata.state = BS_CSW;
	}
}

// send SCSI command response as single packet
static void scsi_resp_xfer(const struct usbdevice_ *usbd, const uint8_t *data, uint16_t len)
{
	if (bsdata.cbw.bmFlags.DirIn)
	{
		bsdata.txptr = data;
		bsdata.devTransferLength = len;
		// TODO: support response > ep_size
		uint16_t txlen = bsdata.devTransferLength < MSC_BOT_EP_SIZE
				? bsdata.devTransferLength : MSC_BOT_EP_SIZE;
		USBdev_SendData(usbd, MSC_BOT_IN_EP, bsdata.txptr, txlen, 0);
		bsdata.state = BS_CSW;
	}
	else
	{
		// bad parameters
		scsi_error(SKEY_ILLEGAL_REQUEST, ASC_INVALID_CDB);
	}
}

void msc_bot_out(const struct usbdevice_ *usbd, uint8_t epn, uint16_t len)
{
	switch (bsdata.state)
	{
	case BS_CBW:
		memcpy(&bsdata.cbw, bsdata.outbuf, CBW_SIZE);
		// check if CBW valid
		if (len == CBW_SIZE && bsdata.cbw.dSignature == CBW_SIG)
		{
			// CBW valid
			bsdata.csw.dTag = bsdata.cbw.dTag;
			bsdata.csw.dDataResidue = 0;
			// check if CBW meaningful
			if ((bsdata.cbw.bmFlags.b0_6) == 0
				&& bsdata.cbw.bLUN < nLUNs
				&& bsdata.cbw.bCBLength > 0 && bsdata.cbw.bCBLength <= 16)
			{
				// CBW meaningful
				//uint8_t sLUN = bsdata.cbw.CB[1] >> 5;
				bsdata.devTransferLength = 0;
				bsdata.csw.bStatus = BOT_CMD_PASSED;
				
				rq[bsdata.cbw.CB[0] >> 3] |= 1u << (bsdata.cbw.CB[0] & 7);	// record unhandled rq

				switch (bsdata.cbw.CB[0])
				{
				case SCSI_INQUIRY:	// return 36 B of Inq info
					scsi_resp_xfer(usbd, inquiry_data, sizeof inquiry_data);
					break;

				case SCSI_READ10:
					if (bsdata.cbw.bmFlags.DirIn && getparm10())
					{
						// command ok
						bsdata.dbidx = 0;
						scsi_read_xfer(usbd);
						bsdata.state = BS_DATAIN;
					}
					else
					{
						// bad parameters
						scsi_error(SKEY_ILLEGAL_REQUEST, ASC_INVALID_CDB);
					}
					break;

				case SCSI_REQUEST_SENSE:	// required if error may be reported
					bsdata.devTransferLength = bsdata.cbw.CB[4];
					if (bsdata.devTransferLength > 18)
						bsdata.devTransferLength = 18;
					scsi_resp_xfer(usbd, (const uint8_t *)&sense_data, sizeof sense_data);
					sense_data.sense_key = SKEY_NO_SENSE;
					sense_data.asc = ASC_NO_SENSE;
					break;

				case SCSI_TEST_UNIT_READY:	// just return GOOD status
					if (bsdata.cbw.dDataTransferLength == 0)
					{
						bot_send_csw(usbd);
					}
					else
					{
						scsi_error(SKEY_ILLEGAL_REQUEST, ASC_INVALID_CDB);
					}
					break;
#if 0
				case SCSI_MODE_SENSE10:	// not required according to https://docs.silabs.com/protocol-usb/1.2.0/protocol-usb-msc-scsi/
					// FDMP support required - not implemented
					scsi_resp_xfer(usbd, (const uint8_t *)&MSC_Mode_Sense10_data,
							bsdata.cbw.CB[4] < sizeof MSC_Mode_Sense10_data ? bsdata.cbw.CB[4] : sizeof MSC_Mode_Sense10_data);
					break;
#endif
				case SCSI_MODE_SENSE6:	// not required according to https://docs.silabs.com/protocol-usb/1.2.0/protocol-usb-msc-scsi/
					// FDMP support required - not implemented
					scsi_resp_xfer(usbd, (const uint8_t *)&MSC_Mode_Sense6_data, sizeof MSC_Mode_Sense6_data);
					break;

				case SCSI_READ_CAPACITY10:	// return 8 B: lastLBA, block size (32-bit BE)
					scsi_resp_xfer(usbd, read_capacity_data, sizeof read_capacity_data);
					break;

				case SCSI_WRITE10:
					if (!bsdata.cbw.bmFlags.DirIn && getparm10())
					{
						// command ok
						bsdata.state = BS_DATAOUT;
					}
					else
					{
						// bad parameters
						scsi_error(SKEY_ILLEGAL_REQUEST, ASC_INVALID_CDB);
					}
					break;

				default:
					if (bsdata.cbw.dDataTransferLength == 0)
					{
						// send bad command csw
					}
					else
					{
						scsi_error(SKEY_ILLEGAL_REQUEST, ASC_INVALID_CDB);
						// abort
					}
				}
			}
			else
			{
				// CBW not meaningful
				bsdata.csw.bStatus = BOT_CMD_FAILED;

				scsi_error(SKEY_ILLEGAL_REQUEST, ASC_INVALID_CDB);
			}
		}
		else
		{
			// CBW not valid
			// stall In, stall Out
			bsdata.state = BS_ERROR;
		}
		break;

	case BS_DATAOUT:	// data to be written to a device
		switch (len)
		{
		case 0:	// zlp
			bsdata.state = BS_CSW;
			break;

		case MSC_BOT_EP_SIZE: // full packet, process write
			memcpy(&bsdata.databuf[bsdata.dbidx], bsdata.outbuf, MSC_BOT_EP_SIZE);
			if ((bsdata.dbidx += MSC_BOT_EP_SIZE) == MSC_DATA_BUF_SIZE)
			{
				bsdata.dbidx = 0;
				media_write(bsdata.cbw.bLUN, bsdata.scsi_blkaddr, bsdata.databuf);
				++bsdata.scsi_blkaddr;
				if (--bsdata.scsi_nblocks == 0)
				{
					bsdata.state = BS_CSW;
				}
			}
			break;

		default:	// incomplete packet
			//bsdata.state = ;
			break;
		}
		break;

	default:	// phase error
	}
}

void msc_bot_in(const struct usbdevice_ *usbd, uint8_t epn)
{
	switch (bsdata.state)
	{
	case BS_DATAIN:
		scsi_read_xfer(usbd);
		break;

	case BS_CSW:
		bot_send_csw(usbd);
		break;

	default:	// in xfer complete, nothing to send
		bsdata.in_busy = 0;
	}
}

