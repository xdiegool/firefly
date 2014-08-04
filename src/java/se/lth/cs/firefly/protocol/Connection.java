package se.lth.cs.firefly.protocol;

import se.lth.control.labcomm.*;
import se.lth.cs.firefly.*;
import se.lth.cs.firefly.ActionQueue.Action;
import se.lth.cs.firefly.ActionQueue.Priority;
import se.lth.cs.firefly.util.*;
import genproto.*;

import java.util.*;
import java.io.*;
import java.net.*;

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

	public boolean isTheSame(InetAddress address, int port) {
		Debug.log((tla.getRemoteHost().equals(address) && tla.getRemotePort() == port)
				+ "");
		return tla.getRemoteHost().equals(address)
				&& tla.getRemotePort() == port;
	}

	public synchronized void exception(Exception e) {
		e.printStackTrace();
		delegate.connectionError(this);
	}

	public void close() {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws Exception {
						for (Channel chan : channels.values()) {
							closeChannel(chan);
						}
						reader.interrupt();
						resendQueue.stop();
						Debug.log("Close connection");
					}
				});
	}

	/* ###################### CHANNEL HANDLING ################### */
	/**
	 * Sends a request to the connected firefly node asking to open a channel.
	 */
	public final void openChannel() throws IOException {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws Exception {
						Channel chan = new Channel(nextChannelID,
								Connection.this);
						channels.put(nextChannelID, chan);
						channel_request req = new channel_request();
						req.source_chan_id = nextChannelID;
						Debug.log("Sending channel request");
						channel_request.encode(bottomEncoder, req);
						resendQueue.queueChannelMsg(nextChannelID, req); // Resend
																			// until
																			// response
						nextChannelID++;
					}
				});
	}

	/**
	 * Sends a request to the connected firefly node asking to close a channel.
	 * 
	 * @param chan
	 *            the channel to be closed
	 */
	public final void closeChannel(final Channel chan) throws IOException {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						chan.setClosed();
						Debug.log("Sending close channel request");
						channel_close cc = new channel_close();
						cc.source_chan_id = chan.getLocalID();
						cc.dest_chan_id = chan.getRemoteID();
						channel_close.encode(bottomEncoder, cc);
						resendQueue.queueChannelMsg(cc.source_chan_id, cc); // Resend
																			// until
																			// ack
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
	public final void restrictChannel(final Channel chan) throws IOException {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						channel_restrict_request crr = new channel_restrict_request();
						crr.source_chan_id = chan.getLocalID();
						crr.dest_chan_id = chan.getRemoteID();
						crr.restricted = true;
						Debug.log("Sending channel restrict request");
						channel_restrict_request.encode(bottomEncoder, crr);
						resendQueue.queueChannelMsg(crr.source_chan_id, crr); // Resend
																				// until
																				// ack
					}
				});
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
	public final void handle_channel_request(final channel_request req)
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
										Connection.this);
								channels.put(nextChannelID, chan);
								resp.source_chan_id = nextChannelID++;
								resp.ack = true;
							} else {
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
	 * or b Outcome of 1 is always done.
	 * 
	 * @param resp
	 *            the channel_response informing whether or not or request was
	 *            accepted by the remote node.
	 */
	public final void handle_channel_response(final channel_response resp)
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
						if (chan != null && !chan.isOpen()) { // 2
							if (resp.ack) { // 2a
								chan.setRemoteID(resp.source_chan_id);
								chan.setEncoder(new ChannelEncoder(
										new channelToConnectionWriter(
												resp.source_chan_id)));
								chan.setDecoder(new ChannelDecoder(
										new AppendableInputStream(null)));
								chan.setOpen();
								delegate.channelOpened(chan);
							} else { // 2b
								channels.remove(resp.dest_chan_id);
							}
							resendQueue.dequeueChannelMsg(resp.dest_chan_id); // Removes
																				// request
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
	 * the connection, else, close it and inform delegate. 2 We jave received
	 * this ack before (i.e. channel is either already open (prev_ack == true)
	 * or does not exist (prev_ack == false) => disregard
	 */
	public final void handle_channel_ack(final channel_ack value)
			throws Exception {
		Debug.log("Got channel ack: " + value.ack);
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						Channel chan = channels.get(value.dest_chan_id);
						if (chan != null && !chan.isOpen()) { // We have not
																// received
																// this ack
																// before,
							if (value.ack) {
								chan.setRemoteID(value.source_chan_id);
								chan.setEncoder(new ChannelEncoder(
										new channelToConnectionWriter(
												value.dest_chan_id)));
								chan.setDecoder(new ChannelDecoder(
										new AppendableInputStream(null)));
								chan.setOpen();
								delegate.channelOpened(chan);
							}

						} else {
							chan.setClosed();
							delegate.channelClosed(chan);
							channels.remove(value.dest_chan_id);
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
	public final void handle_channel_close(final channel_close req)
			throws Exception {
		// TODO some duplicate code here, fix
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
								channels.remove(req.dest_chan_id);
								resendQueue.dequeueChannelMsg(req.dest_chan_id);
							}
						} else {
							channel_close resp = new channel_close();
							resp.dest_chan_id = req.source_chan_id;
							resp.source_chan_id = req.dest_chan_id;
							Debug.log("Sending channel close response");
							channel_close.encode(bottomEncoder, resp);
						}
					}
				});
	}

	/**
	 * LabComm callback
	 */
	public final void handle_channel_restrict_ack(final channel_restrict_ack ack)
			throws Exception {
		Debug.log("Got channel restrict ack");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						Channel chan = channels.get(ack.dest_chan_id);
						if (ack.restricted) {
							resendQueue.dequeueChannelMsg(ack.dest_chan_id);
							if (!chan.isRestricted()) {
								chan.setRestricted(true);
								delegate.channelRestricted(chan);
							}
						}
					}
				});
	}

	/**
	 * LabComm callback
	 * 
	 * 
	 */
	public final void handle_channel_restrict_request(
			final channel_restrict_request crr) throws Exception {
		Debug.log("Got channel restrict request");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						Debug.log("Nbr of channels: " + channels.size());
						Channel chan = channels.get(crr.dest_chan_id);
						if (chan == null) {
							throw new NullPointerException(
									"Can't find channel with local id: "
											+ crr.dest_chan_id
											+ " when handling channel restrict request");
						}
						channel_restrict_ack ack = new channel_restrict_ack();
						ack.dest_chan_id = crr.source_chan_id;
						ack.source_chan_id = crr.dest_chan_id;
						boolean accepted = delegate.restrictAccept(chan);
						if (accepted) {
							ack.restricted = true;
							if (!chan.isRestricted()) { // We have not received
														// this
														// before
								chan.setRestricted(true);
								delegate.channelRestricted(chan);
							}
						} else {
							ack.restricted = false;
						}
						Debug.log("Sending channel restrict ack: "
								+ ack.restricted);
						channel_restrict_ack.encode(bottomEncoder, ack);
					}
				});
	}

	/**
	 * LabComm callback
	 * 
	 * We have received a data sample. This is not related to the channel
	 * handling protocol so we just send it to the right channel instead.
	 */
	public final void handle_data_sample(final data_sample ds) throws Exception {
		Debug.log("Received data sample");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws Exception {
						if (ds.important) {
							ack dataAck = new ack();
							dataAck.dest_chan_id = ds.src_chan_id;
							dataAck.src_chan_id = ds.dest_chan_id;
							dataAck.seqno = ds.seqno;
							Debug.log("Sending data ack on seqno: " + ds.seqno);
							ack.encode(bottomEncoder, dataAck);
						}
						Channel chan = channels.get(ds.dest_chan_id);
						if (chan != null) {
							chan.getDecoder().decodeData(ds.app_enc_data);
						}
					}
				});
	}

	/**
	 * LabComm callback
	 * 
	 * We have received a data ack.
	 */
	public final synchronized void handle_ack(final ack value) {
		Debug.log("Received data ack");
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					public void doAction() throws IOException {
						resendQueue.dequeueData(value.seqno);
					}
				});
	}

	/* ###################### PUBLIC HELPER CLASSES ################### */

	public class channelToConnectionWriter implements LabCommWriter {
		private int origin;

		private channelToConnectionWriter(int origin) {
			this.origin = origin;
		}

		public void write(final byte[] data) throws IOException {
			actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
					new ActionQueue.Action() {
						public void doAction() throws Exception {
							data_sample ds = new data_sample();
							Channel chan = channels.get(origin);
							ds.src_chan_id = chan.getLocalID();
							ds.dest_chan_id = chan.getRemoteID();
							ds.seqno = seqno;
							seqno = (seqno == SEQNO_MAX) ? SEQNO_START
									: seqno + 1;
							ds.app_enc_data = data;
							if (chan.shouldAckOnData()
									|| data[0] == LabComm.SAMPLE
									|| data[0] == LabComm.TYPEDEF) {
								ds.important = true;
								resendQueue.queueData(ds.seqno, ds);
							}
							data_sample.encode(bottomEncoder, ds);
						}
					});
		}
	}

	/* ###################### PRIVATE HELPER CLASSES ################### */
	public class Reader implements Runnable {
		public void run() {
			while (!Thread.currentThread().isInterrupted()) {
				try {
					bottomDecoder.runOne();
				} catch (EOFException e) {
					// Ignore, this is only because ChannelDecoders can't handle
					// non blocking i/o on type registration
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
