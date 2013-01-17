#ifndef TEST_PROTO_CONN_H
#define TEST_PROTO_CONN_H

int init_suit_proto_conn();

int clean_suit_proto_conn();

void test_conn_close_empty();
void test_conn_close_open_chan();
void test_conn_close_send_data();
void test_conn_close_send_first();
void test_conn_close_mult_chans();
void test_conn_close_recv_any();
void test_conn_close_recv_chan_req_first();

/*
 * what if a connection without channels gets a channel open request when
 * closing. The connection will be freed at the same time as it is closed.
 *
 * sequence of events: rt - read thread, et - event thread, at - app thread
 * 1. at runs close conn, event pushed to stack
 * 2. rt receives data on that conn
 * 3. rt runs protocol_data_received
 * 4. rt is suspended after checking closed state on conn.
 * 5. et executes close conn event which pushes free event
 * 6. et executes free event
 * 7. rt pushes data received event on conn
 * 8. et executes data received event and gets segfault
 *
 * possible solutions:
 * - Never free entire conn and allways leave the open attribute. Each event
 *   which depends on a connection must check the opened state of that
 *   connection.
 * - Add close conn call to transport layer before pushing free event to stack.
 *   The transp layer may use mutex to lock the connection and hence make sure
 *   any data received which is about to be pushed will be pushed before free
 *   event is executed. This solution depends on free event has lower prio than
 *   data received event.
 * - Move free instruction to transport layer to the close event instead. Will
 *   make sure nothing happends between close and free from the transport layer.
 *
 *   I, Oscar, decided solution 3 i preferred.
 */
#endif
