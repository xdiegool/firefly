/**
 * @file
 * @brief The public API of the transport Ethernet POSIX with specific structures and
 * functions.
 */
#ifdef _GNU_SOURCE
#error "Something turned it on!"
#undef _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE (200112L)

#include "transport/firefly_transport_eth_posix_private.h"
#include <transport/firefly_transport_eth_posix.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/select.h>		// defines fd_set
#include <sys/ioctl.h>		// defines SIOCGIFINDEX
#include <unistd.h>			// defines close
#include <netinet/ether.h>	// defines ETH_P_ALL, AF_PACKET
/*#include <net/ethernet.h>*/
#include <netpacket/packet.h>	// defines sockaddr_ll
#include <arpa/inet.h>		// defines htons
/*#include <sys/socket.h>*/
/*#include <sys/types.h>*/

#include <linux/if.h>		// defines ifreq, IFNAMSIZ
/*#include <linux/net.h>		// defines SOCK_DGRAM, AF_PACKET*/

#include <utils/firefly_errors.h>
#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include "protocol/firefly_protocol_private.h"

#define ERROR_STR_MAX_LEN      (256)

struct firefly_transport_llp *firefly_transport_llp_eth_posix_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_posix on_conn_recv)
{
	int err;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	struct transport_llp_eth_posix *llp_eth;
	struct firefly_transport_llp *llp;
	llp_eth = malloc(sizeof(*llp_eth));
	
	/* Create the socket */
	err = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
	if(err < 0){
		firefly_error(FIREFLY_ERROR_SOCKET, 2,
					  "Failed in %s().\n", __FUNCTION__);
	}
	llp_eth->socket = err;

	/* Set the interface name */
	strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);

	/* Retreive the interface index of the interface and save it to
	* ifr.ifr_ifindex. */
	err = ioctl(llp_eth->socket, SIOCGIFINDEX, &ifr);
	if(err < 0){
		close(llp_eth->socket);
		firefly_error(FIREFLY_ERROR_SOCKET, 2,
					  "Failed in %s().\n", __FUNCTION__);
	}

	/* init mem of local_addr with zeros */
	memset(&addr, 0, sizeof(struct sockaddr_ll));
	/* set address parameters */
	addr.sll_family   = AF_PACKET;
	/* htons will convert protocol into correct format used by network */
	addr.sll_protocol = htons(FIREFLY_ETH_PROTOCOL);
	/*addr.sll_protocol = htons(ETH_P_ALL);*/
	addr.sll_ifindex  = ifr.ifr_ifindex;
	/* Bind socket to specified interface */
	err = bind(llp_eth->socket, (struct sockaddr *)&addr,
		    sizeof(struct sockaddr_ll));
	if(err < 0){
		close(llp_eth->socket);
		firefly_error(FIREFLY_ERROR_SOCKET, 2,
					  "Failed in %s().\n", __FUNCTION__);
	}

	llp_eth->on_conn_recv = on_conn_recv;
	llp_eth->recv_buf = NULL;
	llp_eth->recv_buf_size = 0;
	llp = malloc(sizeof(struct firefly_transport_llp));
	llp->llp_platspec = llp_eth;
	llp->conn_list = NULL;
	return llp;
}

void firefly_transport_llp_eth_posix_free(struct firefly_transport_llp **llp)
{
	struct transport_llp_eth_posix *llp_eth;
	llp_eth = (struct transport_llp_eth_posix *) (*llp)->llp_platspec;
	close(llp_eth->socket);
	struct llp_connection_list_node *head = (*llp)->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while (head != NULL) {
		tmp = head->next;
		firefly_transport_connection_udp_posix_free_event(head->conn);
		free(head);
		head = tmp;
	}
	free(llp_eth->recv_buf);
	free(llp_eth);
	free(*llp);
	*llp = NULL;
}

void recv_buf_resize(struct transport_llp_eth_posix *llp_eth, size_t new_size)
{
	if (new_size > llp_eth->recv_buf_size) {
		llp_eth->recv_buf = realloc(llp_eth->recv_buf, new_size);
		if (llp_eth->recv_buf == NULL) {
			llp_eth->recv_buf_size = 0;
			firefly_error(FIREFLY_ERROR_ALLOC, 3,
					"Failed in %s() on line %d.\n", __FUNCTION__,
					__LINE__);
		} else {
			llp_eth->recv_buf_size = new_size;
		}
	}
}

void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr)
{
	char temp_addr[18];
	ether_ntoa_r((void*)addr->sll_addr, temp_addr);
	char *tmp_cnt = temp_addr;
	char *mac_cnt = mac_addr;
	while(*tmp_cnt) {
		if (*tmp_cnt != ':' && (*(tmp_cnt+1) != ':' && *(tmp_cnt+1) != '\0' )) {
			*mac_cnt = *tmp_cnt;
			*(mac_cnt+1) = *(tmp_cnt+1);
			*(mac_cnt+2) = *(tmp_cnt+2);
			tmp_cnt += 3;
		} else if (*tmp_cnt != ':' && (*(tmp_cnt+1) == ':' || *(tmp_cnt+1) == '\0' )) {
			*mac_cnt = '0';
			*(mac_cnt+1) = *tmp_cnt;
			*(mac_cnt+2) = *(tmp_cnt+1);
			tmp_cnt += 2;
		} else {
			tmp_cnt++;
		}
		mac_cnt += 3;
	}
}

void firefly_transport_eth_posix_read(struct firefly_transport_llp *llp)
{
	struct sockaddr_ll from_addr;
	int res;
	struct transport_llp_eth_posix *llp_eth = (struct transport_llp_eth_posix *) llp->llp_platspec;
	fd_set fs;
	FD_ZERO(&fs);
	FD_SET(llp_eth->socket, &fs);
	res = select(llp_eth->socket + 1, &fs, NULL, NULL, NULL);
	if (res == -1) {
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s() on line %d.\nFailed to select.", __FUNCTION__,
				__LINE__);
	}
	
	// Read data from socket, = 0 is crucial due to ioctl only sets the
	// first 32 bits of pkg_len
	size_t pkg_len = 0;
	res = ioctl(llp_eth->socket, FIONREAD, &pkg_len);
	if (res == -1) {
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s() on line %d.\nCould not get next packet size.", __FUNCTION__,
				__LINE__);
		pkg_len = 0;
	}
	recv_buf_resize(llp_eth, pkg_len); /* Scaleback and such. */
	socklen_t len = sizeof(struct sockaddr_ll);
	res = recvfrom(llp_eth->socket, llp_eth->recv_buf, llp_eth->recv_buf_size, 0,//MSG_WAITALL,
			(struct sockaddr *) &from_addr, &len);
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s.\n%s()\nCould not read from socket.", __FUNCTION__, err_buf);
	}
	struct firefly_connection *conn = find_connection(llp, &from_addr, connection_comp_addr);
	if (conn == NULL) {// || !((struct protocol_connection_udp_posix *)
				//conn->transport_conn_platspec)->open) {
		if (llp_eth->on_conn_recv != NULL) {
			char mac_addr[18];
			get_mac_addr(&from_addr, mac_addr);
			conn = llp_eth->on_conn_recv(llp, mac_addr);
		} else {
			firefly_error(FIREFLY_ERROR_MISSING_CALLBACK, 4,
				      "Cannot accept incoming connection.\n"
				      "Callback 'on_conn_recv' not set"
				      "on llp.\n (in %s() at %s:%d)",
				      __FUNCTION__, __FILE__, __LINE__);
		}
	}
	// Existing or newly created conn. Passing data to procol layer.
	if (conn != NULL) {
		protocol_data_received(conn, llp_eth->recv_buf, pkg_len);
	}

}

int connection_comp_addr(struct firefly_connection *conn, void *context)
{
	return 0;
}

struct firefly_connection *find_connection(struct firefly_transport_llp *llp,
		void *context, check_conn_f check_conn)
{
	return NULL;
}
