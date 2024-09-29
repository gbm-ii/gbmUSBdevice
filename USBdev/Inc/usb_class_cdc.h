/*----------------------------------------------------------------------------
 *      Definitions  based on usbcdc11.pdf (www.usb.org)
 *---------------------------------------------------------------------------*/
// Communication device class specification version 1.10
#define CDC_V1_10                               0x0110
#define CDC_V1_20                               0x0120

// Communication interface class code
// (usbcdc11.pdf, 4.2, Table 15)
#define CDC_COMMUNICATION_INTERFACE_CLASS       0x02

// Communication interface class subclass codes
// (usbcdc11.pdf, 4.3, Table 16)
#define CDC_DIRECT_LINE_CONTROL_MODEL           0x01
#define CDC_ABSTRACT_CONTROL_MODEL              0x02
#define CDC_TELEPHONE_CONTROL_MODEL             0x03
#define CDC_MULTI_CHANNEL_CONTROL_MODEL         0x04
#define CDC_CAPI_CONTROL_MODEL                  0x05
#define CDC_ETHERNET_NETWORKING_CONTROL_MODEL   0x06
#define CDC_ATM_NETWORKING_CONTROL_MODEL        0x07

// Communication interface class control protocol codes
// (usbcdc11.pdf, 4.4, Table 17)
#define CDC_PROTOCOL_COMMON_AT_COMMANDS         0x01

// Data interface class code
// (usbcdc11.pdf, 4.5, Table 18)
#define CDC_DATA_INTERFACE_CLASS                0x0A

// Data interface class protocol codes
// (usbcdc11.pdf, 4.7, Table 19)
#define CDC_PROTOCOL_ISDN_BRI                   0x30
#define CDC_PROTOCOL_HDLC                       0x31
#define CDC_PROTOCOL_TRANSPARENT                0x32
#define CDC_PROTOCOL_Q921_MANAGEMENT            0x50
#define CDC_PROTOCOL_Q921_DATA_LINK             0x51
#define CDC_PROTOCOL_Q921_MULTIPLEXOR           0x52
#define CDC_PROTOCOL_V42                        0x90
#define CDC_PROTOCOL_EURO_ISDN                  0x91
#define CDC_PROTOCOL_V24_RATE_ADAPTATION        0x92
#define CDC_PROTOCOL_CAPI                       0x93
#define CDC_PROTOCOL_HOST_BASED_DRIVER          0xFD
#define CDC_PROTOCOL_DESCRIBED_IN_PUFD          0xFE

// Type values for bDescriptorType field of functional descriptors
// (usbcdc11.pdf, 5.2.3, Table 24)
#define CDC_CS_INTERFACE                        0x24
#define CDC_CS_ENDPOINT                         0x25

// Type values for bDescriptorSubtype field of functional descriptors
// (usbcdc11.pdf, 5.2.3, Table 25)
#define CDC_HEADER                              0x00
#define CDC_CALL_MANAGEMENT                     0x01
#define CDC_ABSTRACT_CONTROL_MANAGEMENT         0x02
#define CDC_DIRECT_LINE_MANAGEMENT              0x03
#define CDC_TELEPHONE_RINGER                    0x04
#define CDC_REPORTING_CAPABILITIES              0x05
#define CDC_UNION                               0x06
#define CDC_COUNTRY_SELECTION                   0x07
#define CDC_TELEPHONE_OPERATIONAL_MODES         0x08
#define CDC_USB_TERMINAL                        0x09
#define CDC_NETWORK_CHANNEL                     0x0A
#define CDC_PROTOCOL_UNIT                       0x0B
#define CDC_EXTENSION_UNIT                      0x0C
#define CDC_MULTI_CHANNEL_MANAGEMENT            0x0D
#define CDC_CAPI_CONTROL_MANAGEMENT             0x0E
#define CDC_ETHERNET_NETWORKING                 0x0F
#define CDC_ATM_NETWORKING                      0x10

// CDC class-specific request codes
// (usbcdc11.pdf, 6.2, Table 46)
// see Table 45 for info about the specific requests.
#define CDCRQ_SEND_ENCAPSULATED_COMMAND         0x00
#define CDCRQ_GET_ENCAPSULATED_RESPONSE         0x01
#define CDCRQ_SET_COMM_FEATURE                  0x02
#define CDCRQ_GET_COMM_FEATURE                  0x03
#define CDCRQ_CLEAR_COMM_FEATURE                0x04
#define CDC_SET_AUX_LINE_STATE                  0x10
#define CDC_SET_HOOK_STATE                      0x11
#define CDC_PULSE_SETUP                         0x12
#define CDC_SEND_PULSE                          0x13
#define CDC_SET_PULSE_TIME                      0x14
#define CDC_RING_AUX_JACK                       0x15
#define CDCRQ_SET_LINE_CODING                   0x20
#define CDCRQ_GET_LINE_CODING                   0x21
#define CDCRQ_SET_CONTROL_LINE_STATE            0x22
#define CDCRQ_SEND_BREAK                        0x23
#define CDC_SET_RINGER_PARMS                    0x30
#define CDC_GET_RINGER_PARMS                    0x31
#define CDC_SET_OPERATION_PARMS                 0x32
#define CDC_GET_OPERATION_PARMS                 0x33
#define CDC_SET_LINE_PARMS                      0x34
#define CDC_GET_LINE_PARMS                      0x35
#define CDC_DIAL_DIGITS                         0x36
#define CDC_SET_UNIT_PARAMETER                  0x37
#define CDC_GET_UNIT_PARAMETER                  0x38
#define CDC_CLEAR_UNIT_PARAMETER                0x39
#define CDC_GET_PROFILE                         0x3A
#define CDC_SET_ETHERNET_MULTICAST_FILTERS      0x40
#define CDC_SET_ETHERNET_PMP_FILTER             0x41
#define CDC_GET_ETHERNET_PMP_FILTER             0x42
#define CDC_SET_ETHERNET_PACKET_FILTER          0x43
#define CDC_GET_ETHERNET_STATISTIC              0x44
#define CDC_SET_ATM_DATA_FORMAT                 0x50
#define CDC_GET_ATM_DEVICE_STATISTICS           0x51
#define CDC_SET_ATM_DEFAULT_VC                  0x52
#define CDC_GET_ATM_VC_STATISTICS               0x53

// Communication feature selector codes
// (usbcdc11.pdf, 6.2.2..6.2.4, Table 47)
#define CDC_ABSTRACT_STATE                      0x01
#define CDC_COUNTRY_SETTING                     0x02

// Feature Status returned for ABSTRACT_STATE Selector
// (usbcdc11.pdf, 6.2.3, Table 48)
#define CDC_IDLE_SETTING                        (1u << 0)
#define CDC_DATA_MULTPLEXED_STATE               (1u << 1)

// Control signal bitmap values for the SetControlLineState request
// (usbcdc11.pdf, 6.2.14, Table 51)
#define CDC_DTE_PRESENT                         (1u << 0)
#define CDC_ACTIVATE_CARRIER                    (1u << 1)

// CDC class-specific notification codes
// (usbcdc11.pdf, 6.3, Table 68)
// see Table 67 for Info about class-specific notifications
#define CDC_NOTIFICATION_NETWORK_CONNECTION     0x00
#define CDC_RESPONSE_AVAILABLE                  0x01
#define CDC_AUX_JACK_HOOK_STATE                 0x08
#define CDC_RING_DETECT                         0x09
#define CDC_NOTIFICATION_SERIAL_STATE           0x20
#define CDC_CALL_STATE_CHANGE                   0x28
#define CDC_LINE_STATE_CHANGE                   0x29
#define CDC_CONNECTION_SPEED_CHANGE             0x2A

// UART state bitmap values (Serial state notification).
// (usbcdc11.pdf, 6.3.5, Table 69)
#define CDC_SERIAL_STATE_CTS                	(1u << 7)  // not covered by USB PSTN spec, supported by Windows
// these 5 should reset after the notification is sent
#define CDC_SERIAL_STATE_OVERRUN                (1u << 6)  // receive data overrun error has occurred
#define CDC_SERIAL_STATE_PARITY                 (1u << 5)  // parity error has occurred
#define CDC_SERIAL_STATE_FRAMING                (1u << 4)  // framing error has occurred
#define CDC_SERIAL_STATE_RING                   (1u << 3)  // state of ring signal detection
#define CDC_SERIAL_STATE_BREAK                  (1u << 2)  // state of break detection
// the notification should be sent whenever these change
#define CDC_SERIAL_STATE_TX_CARRIER             (1u << 1)  // state of transmission carrier, DSR
#define CDC_SERIAL_STATE_RX_CARRIER             (1u << 0)  // state of receiver carrier, DCD

enum uart_stopbits_ {STOPBITS_1, STOPBITS_1_5, STOPBITS_2};
enum uart_parity_ {PARITY_NONE, PARITY_ODD, PARITY_EVEN, PARITY_MARK, PARITY_SPACE};

// ControlLineState bit masks
#define CDC_CTL_DTR	(1u << 0)
#define	CDC_CTL_RTS	(1u << 1)
#define	CDC_CTL_RTR	CDC_CTL_RTS

// Abstract Control Management Functional Descriptor capabilities
#define CDCACM_FDCAP_COMMFEAT	1u	// CommFeature support
#define CDCACM_FDCAP_LC_LS		2u	// LineCoding, ControlLineState, SerialState support
#define CDCACM_FDCAP_SENDBREAK	4u	// ...
#define CDCACM_FDCAP_NETCONN	8u	// Network Connection notification support

// gbmUSBdevice stuff ====================================================
#include <stdint.h>
#include <stdbool.h>

#define AUTONUL_TOUT	5u
//========================================================================
struct cdc_linecoding_ {
	uint32_t dwDTERate;	// Data terminal rate, in bits per second*/
	uint8_t bCharFormat;	/* Value | Stop bits
                                   0 - 1 Stop bit
                                   1 - 1.5 Stop bits
                               	   2 - 2 Stop bits */
	uint8_t bParityType;	// |  1   | Number | Parity
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
	uint8_t bDataBits;	//  Data bits (5, 6, 7, 8 or 16)
};

// Serial state notification =============================================
struct cdc_SerialStateNotif_ {
    USB_RequestType bmRequestType;
	uint8_t bNotification;
	uint16_t wValue, wIndex, wLength;
	uint16_t wSerialState;
};

// application-specific CDC channel data

// data that should be reset whenever the USB connection is established
struct cdc_session_ {
	volatile bool connected;
	bool signon_rq;
	bool prompt_rq;
	uint8_t connstart_timer;
	bool autonul;
	uint8_t autonul_timer;
	// change Len and Idx members to uint16_t for HS support
	volatile uint8_t RxLength;
	volatile uint8_t RxIdx;
	volatile uint8_t TxLength;
	uint8_t TxTout;
};

// persisent data
struct cdc_data_ {
	struct cdc_linecoding_ LineCoding;
	uint16_t ControlLineState;	// bit 0 - DTE ready, bit 1 - CD/RTS/RTR
	uint16_t SerialState;		// current SerialState
	uint16_t SerialStateSent;	// last SerialState successfully sent
	bool LineCodingChanged;
	bool ControlLineStateChanged;
	uint8_t RxData[CDC_DATA_EP_SIZE];
	uint8_t TxData[CDC_DATA_EP_SIZE];
	struct cdc_session_ session;
};

struct cdc_services_ {
	void (*SetLineCoding)(const struct usbdevice_ *usbd, uint8_t idx);
	void (*SetControlLineState)(const struct usbdevice_ *usbd, uint8_t idx);
};

//========================================================================
