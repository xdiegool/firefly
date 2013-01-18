/**
 * @file
 * @brief General transport related data structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_H
#define FIREFLY_TRANSPORT_H

#include <protocol/firefly_protocol.h>
#include <protocol/firefly_protocol_private.h>
#include <stdbool.h>

/**
 * @brief A opaque general data structure representing a \c link \c layer \c port on the
 * transport layer.
 */
struct firefly_transport_llp;

/**
 * @breif Should normally not be used except when testing *only* transport layers.
 */
void replace_protocol_data_received_cb(struct firefly_transport_llp *llp,
		protocol_data_received_f protocol_data_received_cb);

#endif
