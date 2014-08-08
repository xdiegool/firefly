package se.lth.cs.firefly.protocol;

import se.lth.cs.firefly.*;
import se.lth.cs.firefly.util.*;
import genproto.*;

import java.util.*;
import java.io.*;
import java.net.*;

/**
 * Handles all firefly protocol callbacks and keeps track of channels.
 * 
 * It represents a connection between to firefly nodes and provides methods for
 * opening and closing channels and handles connection setup and handshake. It
 * has two helper classes, which will be explained in their respective comments.
 * 
 * TODO Connection state?
 */
public class Connection implements ack.Handler, channel_ack.Handler,
		channel_close.Handler, channel_request.Handler,
		channel_response.Handler, channel_restrict_ack.Handler,
		channel_restrict_request.Handler, data_sample.Handler {

	private final static int SEQNO_MAX = Integer.MAX_VALUE;
	private final static int SEQNO_START = 0;
	private HashMap<Integer, Channel> channels;
	private int nextChannelID;
	private int seqno;
	private ResendQueue resendQueue;
	private ActionQueue actionQueue;
	private ConnectionDecoder bottomDecoder;
	private ConnectionEncoder bottomEncoder;
	private FireflyApplication delegate;
	private TransportLayerAbstraction tla;
	private Thread reader;

	public Connection(TransportLayerAbstraction tla,
			FireflyApplication delegate, ActionQueue actionQueue)
			throws IOException {
		this.delegate = delegate;
		this.actionQueue = actionQueue;
		this.tla = tla;
		seqno = SEQNO_START;
		channels = new HashMap<Integer, Channel>();
		nextChannelID = 0;
		channels = new HashMap<Integer, Channel>();
		bottomEncoder = new ConnectionEncoder(tla.getWriter());
		bottomDecoder = new ConnectionDecoder(tla.getInputStream());
		bottomEncoder.setShortCircuit(bottomDecoder);
		resendQueue = new ResendQueue(bottomEncoder, this);

		reader = new Thread(new Reader());
		reader.start();

		data_sample.register(bottomEncoder);
		ack.register(bottomEncoder);
		channel_request.register(bottomEncoder);
		channel_response.register(bottomEncoder);
		channel_ack.register(bottomEncoder);
		channel_close.register(bottomEncoder);
		channel_restrict_request.register(bottomEncoder);
		channel_restrict_ack.register(bottomEncoder);

		data_sample.register(bottomDecoder, this);
		ack.register(bottomDecoder, this);
		channel_request.register(bottomDecoder, this);
		channel_response.register(bottomDecoder, this);
		channel_ack.register(bottomDecoder, this);
		channel_close.register(bottomDecoder, this);
		channel_restrict_request.register(bottomDecoder, this);
		channel_restrict_ack.register(bottomDecoder, this);

	}

	/* ###################### PUBLIC INTERFACE ################### */
	/**
	 * Sends a request to the connected firefly node asking to open a channel.
	 * 
	 * @throws IOException
	 */
	public void openChannel() throws IOException {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws Exception {
						Channel chan = new Channel(nextChannelID,
								Connection.this, actionQueue, resendQueue);
						channels.put(nextChannelID, chan);
						channel_request req = new channel_request();
						req.source_chan_id = nextChannelID;
						Debug.log("Sending channel request");
						channel_request.encode(bottomEncoder, req);
						resendQueue.queueChannelMsg(nextChannelID, req); 		
						nextChannelID++;
					}
				});
	}

	public void openChannelAutoRestrict(final ArrayList<UserType> list) throws IOException {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY, new ActionQueue.Action() {
					@Override
					public void doAction() throws Exception {
						// TODO Implement this
					}
		});
	}

	/**
	 * Sends a restrict request to the connected firefly node asking to close a
	 * channel. This means that no more data types may be registered. Data
	 * should not be sent before a channel is restricted.
	 * 
	 * @param chan
	 *            the channel to be restricted
	 */
	public void restrictChannel(final Channel chan) throws IOException {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						chan.setRestricted(true);
						channel_restrict_request crr = new channel_restrict_request();
						crr.source_chan_id = chan.getLocalID();
						crr.dest_chan_id = chan.getRemoteID();
						crr.restricted = true;
						Debug.log("Sending channel restrict request");
						channel_restrict_request.encode(bottomEncoder, crr);
						// Resend until ack
						resendQueue.queueChannelMsg(crr.source_chan_id, crr);
					}
				});
	}

	/*
	 * Checks whether this channel has the same remoteAddress and port as those
	 * provided.
	 */
	public boolean isTheSame(InetAddress address, int port) {
		Debug.log((tla.getRemoteHost().equals(address) && tla.getRemotePort() == port)
				+ "");
		return tla.getRemoteHost().equals(address)
				&& tla.getRemotePort() == port;
	}

	public synchronized void exception(Exception e, String msg) {
		e.printStackTrace();
		delegate.connectionError(this, (msg == null ? "" : msg), e);
	}

	public void close() {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws Exception {
						for (Channel chan : channels.values()) {
							chan.close();
						}
						reader.interrupt();
						resendQueue.stop();
						Debug.log("Close connection");
					}
				});
	}

	/* ###################### PACKAGE INTERFACE ################### */
	/**
	 * Sends a request to the connected firefly node asking to close a channel.
	 * 
	 * @param chan
	 *            the channel to be closed
	 */
	void sendCloseRequest(Channel chan) throws IOException {
		Debug.log("Sending close channel request");
		channel_close cc = new channel_close();
		cc.source_chan_id = chan.getLocalID();
		cc.dest_chan_id = chan.getRemoteID();
		channel_close.encode(bottomEncoder, cc);
		resendQueue.queueChannelMsg(cc.source_chan_id, cc); // Resend
	}

	/**
	 * Called by channels to send data via protocol.
	 * 
	 * @param data
	 *            the data to be sent
	 * @param important
	 *            if the data is important, i.e. we should resend.
	 * @param chan
	 *            the channel that want to send
	 * @return the sequence number of data data packet.
	 * @throws IOException
	 */
	int sendDataSample(byte[] data, boolean important, Channel chan)
			throws IOException {
		data_sample ds = new data_sample();
		ds.src_chan_id = chan.getLocalID();
		ds.dest_chan_id = chan.getRemoteID();
		ds.app_enc_data = data;
		if (important) {
			ds.important = true;
			ds.seqno = seqno;
			seqno = (seqno == SEQNO_MAX) ? SEQNO_START : seqno + 1; // In case
																	// we send
																	// many data
																	// samples
			resendQueue.queueData(ds.seqno, ds);
		}
		data_sample.encode(bottomEncoder, ds);
		return ds.seqno;

	}

	/* ###################### PRIVATE INTERFACE ################### */
	private void noLocalChannel(int localId, int remoteId) throws IOException {
		Debug.log("Received message to channel with ID: "
				+ localId
				+ " from channel with ID: "
				+ remoteId
				+ ", but no such channel exists, sending close request to remote.");
		channel_close cc = new channel_close();
		cc.dest_chan_id = remoteId;
		cc.source_chan_id = localId;
		channel_close.encode(bottomEncoder, cc);

	}

	/* ###################### LABCOMM CALLBACKS ################### */
	/**
	 * LabComm callback
	 * 
	 * A channel request has been received. Three cases: 1 We have received it
	 * before but delegate declined => Check again and send response 2 We have
	 * received it and delegate accepted (i.e. it exists in channels) => send
	 * positive response again 3 We haven't received it before => Check with
	 * delegate and send response
	 * 
	 * 1 and 3 have the same outcome so we can bundle them.
	 */
	public void handle_channel_request(final channel_request req)
			throws Exception {
		Debug.log("Got channel request");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						channel_response resp = new channel_response();
						resp.dest_chan_id = req.source_chan_id;
						if (channels.get(req.dest_chan_id) != null) { // 2
							resp.ack = true;
						} else { // 1 & 3
							if (delegate.channelAccept(Connection.this)) {
								Channel chan = new Channel(nextChannelID,
										Connection.this, actionQueue,
										resendQueue);
								channels.put(nextChannelID, chan);
								resp.source_chan_id = nextChannelID++;
								resp.ack = true;
							} else {
								resp.source_chan_id = Channel.CHANNEL_ID_NOT_SET;
								resp.ack = false;
							}
						}
						Debug.log("Sending channel response: " + resp.ack);
						channel_response.encode(bottomEncoder, resp);
						// Resend until ack
						resendQueue.queueChannelMsg(resp.source_chan_id, resp);
					}
				});
	}

	/**
	 * LabComm callback
	 * 
	 * A channel response has been received from remote host on an channel
	 * request from this host.
	 * 
	 * Cases: 1 We have received it before => send ack 2 We have not received it
	 * before a. It was positive => OPEN the channel that we requested. b. It
	 * was negative => remove channel from channels => Send ack regardless of a
	 * or b. Outcome of 1 is always done.
	 * 
	 * @param resp
	 *            the channel_response informing whether or not or request was
	 *            accepted by the remote node.
	 */
	public void handle_channel_response(final channel_response resp)
			throws Exception {
		Debug.log("Got channel response: " + resp.ack);
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						channel_ack ack = new channel_ack();
						ack.source_chan_id = resp.dest_chan_id;
						ack.dest_chan_id = resp.source_chan_id;
						ack.ack = resp.ack;
						Debug.log("Sending channel ack");
						channel_ack.encode(bottomEncoder, ack);
						Channel chan = channels.get(resp.dest_chan_id);
						if (chan == null) {
							noLocalChannel(resp.dest_chan_id,
									resp.source_chan_id);
						} else {
							if (!chan.isOpen()) { // 2
								if (resp.ack) { // 2a
									chan.setRemoteID(resp.source_chan_id);
									chan.setEncoder(new ChannelEncoder(chan
											.getWriter()));
									chan.setDecoder(new ChannelDecoder(
											new AppendableInputStream(null)));
									// null is for version bypass
									chan.setOpen();
									delegate.channelOpened(chan);
								} else { // 2b
									channels.remove(resp.dest_chan_id);
								}
								// Removes request
								resendQueue
										.dequeueChannelMsg(resp.dest_chan_id);
							}
						}
					}
				});

	}

	/**
	 * LabComm callback
	 * 
	 * An ack regarding channels has been received, i.e., the remote host has
	 * sent ack on a channel_response from this host.
	 * 
	 * Cases: 1 We have not received this ack before => If ack is true, we OPEN
	 * the connection, else, close it and inform delegate. 2 We have received
	 * this ack before (i.e. channel is either already open (prev_ack == true)
	 * or does not exist (prev_ack == false) => disregard
	 */
	public void handle_channel_ack(final channel_ack value) throws Exception {
		Debug.log("Got channel ack: " + value.ack);
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						Channel chan = channels.get(value.dest_chan_id);
						// We have not received this ack before
						if(chan == null){
							noLocalChannel(value.dest_chan_id, value.source_chan_id);
						}else if (!chan.isOpen()) {
							if (value.ack) {
								chan.setRemoteID(value.source_chan_id);
								chan.setEncoder(new ChannelEncoder(chan
										.getWriter()));
								chan.setDecoder(new ChannelDecoder(
										new AppendableInputStream(null)));
								chan.setOpen();
								delegate.channelOpened(chan);
							} else {
								chan.setClosed();
								delegate.channelClosed(chan);
								channels.remove(value.dest_chan_id);
							}
						}
						resendQueue.dequeueChannelMsg(value.dest_chan_id);
					}
				});
	}

	/**
	 * LabComm callback
	 * 
	 * Remote host wants to close channel or has closed channel on our request,
	 * i.e. works as both response and request.
	 * 
	 * If it's a request, close and remove channel and send response, else it's
	 * a response and we remove the channel.
	 */
	public void handle_channel_close(final channel_close req) throws Exception {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						Channel chan = channels.get(req.dest_chan_id);
						if (chan != null) {
							if (chan.isOpen()) {
								Debug.log("Got channel close request");
								// Request
								delegate.channelClosed(channels
										.get(req.dest_chan_id));
								channel_close resp = new channel_close();
								resp.dest_chan_id = req.source_chan_id;
								resp.source_chan_id = req.dest_chan_id;
								Debug.log("Sending channel close response");
								channel_close.encode(bottomEncoder, resp);
								chan.setClosed();
								channels.remove(req.dest_chan_id);
							} else if (chan.isClosed()) {
								Debug.log("Got channel close response");
								// Response
								// Clear resend messages
								channels.remove(req.dest_chan_id);
								resendQueue.dequeueChannelMsg(req.dest_chan_id);
							}
						} // Already closed, silently discard
					}
				});
	}

	/**
	 * LabComm callback An ack on our own restrict request has been received.
	 */
	public void handle_channel_restrict_ack(final channel_restrict_ack ack)
			throws Exception {
		Debug.log("Got channel restrict ack");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						Channel chan = channels.get(ack.dest_chan_id);
						if (chan == null) {
							noLocalChannel(ack.dest_chan_id, ack.source_chan_id);
						} else {
							resendQueue.dequeueChannelMsg(ack.dest_chan_id);
							if (ack.restricted) {
								if (chan.isRestricted() && !chan.isRemoteRestricted()) {
									chan.setRemoteRestricted(true);
									delegate.channelRestrictStateChange(chan, FireflyApplication.RestrictState.RESTRICTED);
								}else if(!chan.isRestricted()){
									Debug.log("Inconsistent state: Received restrict ack on non locally restricted channel");
								}
							}else{
								if(chan.isRestricted()){
									// We sent restrict request and was denied
									delegate.channelRestrictStateChange(chan, FireflyApplication.RestrictState.RESTRICT_REFUSED);
								}else{
									// We send unrestrict request and was accepted
									delegate.channelRestrictStateChange(chan, FireflyApplication.RestrictState.UNRESTRICTED);
								}
								chan.setRestricted(false);
							}
						}
					}
				});
	}

	/**
	 * LabComm callback We have received a restrict request, i.e. remote host
	 * says its done registering types and wants to start sending data.
	 * 
	 */
	public void handle_channel_restrict_request(
			final channel_restrict_request crr) throws Exception {
		Debug.log("Got channel restrict request");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						Channel chan = channels.get(crr.dest_chan_id);
						if (chan == null) {
							noLocalChannel(crr.dest_chan_id, crr.source_chan_id);
						} else {
							channel_restrict_ack ack = new channel_restrict_ack();
							ack.dest_chan_id = crr.source_chan_id;
							ack.source_chan_id = crr.dest_chan_id;
							boolean remoteRestricted = crr.restricted;
							chan.setRemoteRestricted(remoteRestricted);
							if(remoteRestricted){
								if (!chan.isRestricted()) { // Remotely initiated
									boolean accepted = delegate.restrictAccept(chan);
									ack.restricted = accepted;
									chan.setRestricted(accepted);
									if (accepted) {
										delegate.channelRestrictStateChange(chan, FireflyApplication.RestrictState.RESTRICTED);
									}
								}// else received restrict request on restricted channel
							}else{
								if(chan.isRestricted()){
									delegate.channelRestrictStateChange(chan, FireflyApplication.RestrictState.UNRESTRICTED);
									chan.setRestricted(false);
								}// else we received unrestrict request on unrestricted channel
							}
							
							Debug.log("Sending channel restrict ack: "
									+ ack.restricted);
							channel_restrict_ack.encode(bottomEncoder, ack);
						}
					}
				});
	}

	/**
	 * LabComm callback
	 * 
	 * We have received a data sample. If it is important, we send an ack back,
	 * regardless, we give it to the channel decoder. Only important messages
	 * are checked for ordering.
	 */
	public void handle_data_sample(final data_sample ds) throws Exception {
		Debug.log("Received data sample");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws Exception {
						Channel chan = channels.get(ds.dest_chan_id);
						if (chan == null) {
							noLocalChannel(ds.dest_chan_id, ds.src_chan_id);
						} else {
							int expected_seqno = chan.getRemoteSeqno() + 1;
							if (ds.important) {
								ack dataAck = new ack();
								dataAck.dest_chan_id = ds.src_chan_id;
								dataAck.src_chan_id = ds.dest_chan_id;
								dataAck.seqno = ds.seqno;
								Debug.log("Sending data ack on seqno: "
										+ ds.seqno);
								ack.encode(bottomEncoder, dataAck);
							}
							if (!ds.important || expected_seqno == ds.seqno) {
								chan.getDecoder().decodeData(ds.app_enc_data);
								if (ds.important) {
									chan.setRemoteSeqno(ds.seqno);
								}
							} else if (ds.important
									&& expected_seqno != ds.seqno) {
								throw new IOException(
										"Received unexpected sequence number in data sample marked important.");
							}
						}
					}
				});
	}

	/**
	 * LabComm callback
	 * 
	 * We have received a data ack, dequeue relate data_sample.
	 * 
	 * @throws IOException
	 */
	public void handle_ack(final ack msg) throws IOException {
		Debug.log("Received data ack");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						Channel chan = channels.get(msg.dest_chan_id);
						if (chan == null) {
							noLocalChannel(msg.dest_chan_id, msg.src_chan_id);
						} else if (chan.receivedAck(msg)) { // returns true if
															// it was an awaited
															// ack
							resendQueue.dequeueData(msg.seqno);
						}
					}
				});
	}

	/* ###################### PUBLIC HELPER CLASSES ################### */
	/**
	 * Decodes incoming data from the stream, essentially "runs" the decoder.
	 */
	public class Reader implements Runnable {
		public void run() {
			while (!Thread.currentThread().isInterrupted()) {
				try {
					bottomDecoder.runOne();
				} catch (EOFException e) {
					// Ignore, this is only because ChannelDecoders can't handle
					// non blocking i/o on type registration
					// TODO Test if this is still an issue
				} catch (SocketException e) {
					Thread.currentThread().interrupt(); // Socket closed
				} catch (Exception e) {
					Debug.printStackTrace(e);
					Debug.errx("");
				}
			}
		}
	}

}
