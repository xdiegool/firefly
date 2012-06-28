// Protocol for starting and stopping a session with the ADC-card (server).
#ifndef FIREFLY_PROTOCOL_H
#define FIREFLY_PROTOCOL_H

#define FIREFLY_UDP_SERENITY_PORT 		(1667)			// Source from server.
#define FIREFLY_UDP_ARIEL_PORT 			(1668)			// Port that client listens to.

// BNF-syntax used here:
// ::=			assignment.
// <name>		a syntax symbol.
// NAME			a syntax symbol and also a CPP macro. 
// ()			grouping.
// *			repeats preceding symbol 0 or many times.
// ?			preceding symbol is optional.
// |			or symbol.
// Nx			N pc of the next symbol.
//
//
// <packet>			::= FIREFLY_PROTOCOL_SIGNATURE <packet_type>
#define FIREFLY_PROTOCOL_SIGNATURE    (0x1337) 						// If raw ethernet protocol is specified in ethtype field in Ethernet II, http://standards.ieee.org/develop/regauth/ethertype/eth.txt If UDP this is specified in the two first bytes in the data.
// <packet_type>		::= <setup-ariel> | <setup-serenity> | <data_stream> | FIREFLY_TEARDOWN | FIREFLY_BEAT
// <setup-ariel>		::= FIRELFY_SETUP_ARIEL firefly_data_type (<coordinate system>)? <beat_period> 
#define FIRELFY_SETUP_ARIEL		    (0x01)
#define FIREFLY_SETUP_ARIEL_LEN    (4)
// <setup-serenity>		::= FIREFLY_SETUP_SERENITY <labcomm_signature> 
#define FIREFLY_SETUP_SERENITY		    (0x02)
// <labcomm_signature>		::= (FIREFLY_LABCOMM_SIGN_LEN)x FIREFLY_BYTE
#define FIREFLY_LABCOMM_SIGN_LEN	    (32)			// Size in bytes for a LabComm signature. In Ethernet II (with out IEEE802.1Q, https://en.wikipedia.org/wiki/Ethernet_frame#cite_note-3) the payload can not be less than 46B so the rest of the frame has to be padded by the user.
// <data_stream>		::= FIREFLY_DATA_STREAM <data_len> <labcomm_data>
#define FIREFLY_DATA_STREAM		    (0x12)
// <data_len>			::= 2x FIREFLY_BYTE
// <labcomm_data>		::= (FIREFLY_LABCOMM_DATA_LEN)x FIREFLY_BYTE
#define FIREFLY_LABCOMM_DATA_LEN	    (16)						// Find dynamic deifne
#define FIREFLY_TEARDOWN	    	    (0x23)
enum firefly_data_type {FIREFLY_DATA_TYPE_RAW};
// <coordinate system> 		Here some pre specified transformation to coordinate systems can be specified.
// 				Possibly the client can dynamically specify a new coordinate system.
#define FIREFLY_BEAT		    (0x06)						// Only client sends beat, it is not acked by server.
// <beat_period>			::= FIREFLY_BEAT_PERIOD 2x FIREFLY_BYTE					// Beat period in ms.
#define FIREFLY_BEAT_PERIOD		    (0x07)
#define FIREFLY_BYTE 		 	    (unsigned char)
//
//
// A typical session:
// Ariel -> Serenity: PROTOCOL_SIGNATURE SETUP RAW BEAT_PERIOD BYTE BYTE
// Serenity -> Ariel: PROTOCOL_SIGNATURE ACK
// Serenity -> Ariel: PROTOCOL_SIGNATURE DATA_STREAM BYTE BYTE BYTE 		// Sends one byte of data.
// (data stream)
// (data stream)
// Ariel -> Serenity: PROTOCOL_SIGNATURE BEAT
// (data stream)
// (data stream)
// Ariel -> Serenity: PROTOCOL_SIGNATURE TEARDOWN
// Serenity -> Ariel: PROTOCOL_SIGNATURE ACK

enum ff_result_type {
	FIREFLY_RESULT_OK,
	FIREFLY_NULL_BUFFER,
	FIREFLY_INVALID_BEAT_RATE
};

struct ff_result {
	enum ff_result_type type;
	char *msg;
};


#endif
