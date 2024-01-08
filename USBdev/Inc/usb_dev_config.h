/*
 * Application-specific USB device configuration 
 * gbm 11'2022
 */
 
#ifndef USB_DEV_CONFIG_H_
#define USB_DEV_CONFIG_H_

#define USBD_MSC 0	// not supported yet
#define USBD_CDC_CHANNELS	2
#define USBD_PRINTER	0
#define USBD_HID	0	// not supported yet

// synthesize PID from device config
#define USBD_CFG_PID	((USBD_MSC) | USBD_CDC_CHANNELS << 1 \
	| (USBD_PRINTER) << 3 | (USBD_HID) << 4 )

// Vendor and product ID
#define	USB_VID	0x25AE
#define USB_PID	(0x24AB - 6 + USBD_CFG_PID)

// endpoint sizes
#define USBD_CTRL_EP_SIZE	64u
#define MSC_BOT_EP_SIZE	64u
#define CDC_DATA_EP_SIZE	64u
#define CDC_INT_EP_SIZE	8u
#define PRN_DATA_EP_SIZE	64u

//#define USE_COMMON_CDC_INT_IN_EP

#define CDC_INT_POLLING_INTERVAL	10u	// ms

// interface numbers - start at 0
enum usbd_ifnum_ {
#if USBD_MSC
	IFNUM_MSC,
#endif

#if USBD_CDC_CHANNELS
	IFNUM_CDC0_CONTROL, IFNUM_CDC0_DATA,
#if USBD_CDC_CHANNELS > 1
	IFNUM_CDC1_CONTROL, IFNUM_CDC1_DATA,
#if USBD_CDC_CHANNELS > 2
	IFNUM_CDC2_CONTROL, IFNUM_CDC2_DATA,
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS

#if USBD_PRINTER
	IFNUM_PRN,
#endif
	USBD_NUM_INTERFACES	// number of interfaces
};

// endpoint addresses - start at 0 for OUT eps, 0x80 for IN eps
enum usbd_epaddr_ {
// Out endpoints
	CTRL_OUT_EP,	// Control OUT ep
#if USBD_MSC
	MSC_BOT_OUT_EP,
#endif

#if USBD_CDC_CHANNELS
	EMPTY0_EP,
	CDC0_DATA_OUT_EP,
#if USBD_CDC_CHANNELS > 1
#ifndef USE_COMMON_CDC_INT_IN_EP
	EMPTY1_EP,
#endif
	CDC1_DATA_OUT_EP,
#if USBD_CDC_CHANNELS > 2
#ifndef USE_COMMON_CDC_INT_IN_EP
	EMPTY2_EP,
#endif
	CDC2_DATA_OUT_EP,
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS

#if USBD_PRINTER
	PRN_DATA_OUT_EP,
#endif
	USBD_OUT_EPS,	// no. of Out endpoints
	
// In endpoints
	CTRL_IN_EP = 0x80,	// Control IN ep
#if USBD_MSC
	MSC_BOT_IN_EP,
#endif

#if USBD_CDC_CHANNELS
	CDC0_INT_IN_EP,
	CDC0_DATA_IN_EP,
#if USBD_CDC_CHANNELS > 1
#ifndef USE_COMMON_CDC_INT_IN_EP
	CDC1_INT_IN_EP,
#endif
	CDC1_DATA_IN_EP,
#if USBD_CDC_CHANNELS > 2
#ifndef USE_COMMON_CDC_INT_IN_EP
	CDC2_INT_IN_EP,
#endif
	CDC2_DATA_IN_EP,
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS
//	PRN_DATA_IN_EP,
	USBD_IN_EPS	// no. of In endpoints
};

// no of endpoint pairs used in the application
#define USBD_NUM_EPPAIRS	((USBD_IN_EPS & 0xf) > USBD_OUT_EPS ? (USBD_IN_EPS & 0xf) : USBD_OUT_EPS) 

#ifdef USE_COMMON_CDC_INT_IN_EP
#define	CDC1_INT_IN_EP CDC0_INT_IN_EP
#define	CDC2_INT_IN_EP CDC0_INT_IN_EP
#endif

#endif
