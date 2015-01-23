package se.lth.cs.firefly.transport;

import genproto.*;

import se.lth.cs.firefly.FireflyApplication;
import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.protocol.ProtocolException;
import se.lth.cs.firefly.protocol.TypeList;
import se.lth.cs.firefly.util.Debug;

import se.lth.control.labcomm.EncoderChannel;
import se.lth.control.labcomm.DecoderChannel;

import java.net.Socket;
import java.net.SocketException;
import java.io.EOFException;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * Subclass of {@link Connection} that uses TCP to transmit.
 */
public class TCPConnection extends Connection
		implements ack.Handler,
				   channel_ack.Handler,
				   channel_close.Handler,
				   channel_request.Handler,
				   channel_response.Handler,
				   channel_restrict_ack.Handler,
				   channel_restrict_request.Handler,
				   data_sample.Handler {
	private Socket sock;
	private FireflyApplication delegate;
	private Reader reader;
	private Debug logger;

	private ConnectionDecoder bottomDecoder;
	private ConnectionEncoder bottomEncoder;

	public TCPConnection(Socket sock, FireflyApplication delegate) throws IOException {
		this.sock = sock;
		this.delegate = delegate;
		bottomDecoder = new ConnectionDecoder(sock.getInputStream());
		bottomEncoder = new ConnectionEncoder(sock.getOutputStream(),
											  bottomDecoder);
		logger = new Debug(TCPConnection.class.getCanonicalName());

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

		reader = new Reader(bottomDecoder);
	}

	// 1
	@Override
	public synchronized void openChannel(TypeList channelTypes) throws IOException {
		final Channel chan = new Channel(nextChannelID, this);
		chan.setChannelTypes(channelTypes);
		channels.put(nextChannelID, chan);

		final channel_request req = new channel_request();
		req.source_chan_id        = nextChannelID;
		/* Java version always uses auto restrict. */
		req.auto_restrict         = true;

		channel_request.encode(bottomEncoder, req);
		nextChannelID++;
	}

	@Override
	public void closeChannel(Channel chan) throws IOException{
		final channel_close cc = new channel_close();
		cc.source_chan_id = chan.getLocalId();
		cc.dest_chan_id = chan.getRemoteId();

		channel_close.encode(bottomEncoder, cc);
		chan.setClosing();
		delegate.channelClosed(chan);
	}

	// LabComm callbacks for generated protocol. Might move.
	@Override
	public void handle_ack(ack value) {
		// Data ack not used with TCP.
	}

	@Override
	public synchronized void handle_channel_close(channel_close req) throws IOException {
		final Channel chan = channels.get(req.dest_chan_id);
		if (chan == null) {
			throw new ProtocolException("Got close on non-existing channel.");
		}

		chan.setClosed();
		delegate.channelClosed(chan);
		channels.remove(chan.getLocalId());
	}

	@Override
	public synchronized void handle_channel_request(channel_request req) throws IOException {
		if (req.auto_restrict) {
			final channel_response resp = new channel_response();
			resp.dest_chan_id = req.source_chan_id;

			final Channel chan = new Channel(nextChannelID++, req.source_chan_id, this);
			if (delegate.channelAccept(chan)) {
				/* The application accepted the channel, create and save a new channel. */
				channels.put(chan.getLocalId(), chan);

				resp.source_chan_id = chan.getLocalId();
				resp.ack = true;
			} else {
				resp.ack = false;
			}

			channel_response.encode(bottomEncoder, resp);
		} else {
			logger.errc("Cannot accept channel request without auto restrict. Dropping request.");
		}
	}

	// 3
	@Override
	public synchronized void handle_channel_response(channel_response resp) throws IOException {
		final Channel channel = channels.get(resp.dest_chan_id);
		if (channel == null) {
			throw new ProtocolException("Got channel response on non-existing channel.");
		}

		if (resp.ack) {
			channel.setRemoteId(resp.source_chan_id);
			channel.setOpen();
		} else {
			// TODO: Error handling.
		}

		final channel_ack ack = new channel_ack();
		ack.source_chan_id = resp.dest_chan_id; // TODO: Confirm this w/ prev.
		ack.dest_chan_id = resp.source_chan_id;
		ack.ack = resp.ack;
		channel_ack.encode(bottomEncoder, ack);

		if (resp.ack) {
			delegate.channelOpened(channel);
		} else {
			// TODO: Error handling.
		}
	}

	@Override
	public void handle_channel_ack(channel_ack ack) {
		final Channel chan = channels.get(ack.dest_chan_id);
		if (chan == null) {
			logger.errc("Received ack on non-existing channel: " + ack.dest_chan_id);
		} else if (ack.ack) {
			chan.setOpen();
			delegate.channelOpened(chan);
		} else {
			delegate.channelClosed(chan);
			chan.setClosed();
			channels.remove(ack.dest_chan_id);
		}
	}

	@Override
	public void handle_channel_restrict_ack(channel_restrict_ack ack) throws IOException {
		final Channel chan = channels.get(ack.dest_chan_id);
		if (chan == null) {
			throw new ProtocolException("Received restrict ack on non-existing channel.");
		}

		if (ack.restricted) {
			if (chan.isLocallyRestricted()) {
				/* Both sides done with restrict. */
				chan.setRemoteRestricted(true);
				delegate.channelRestrictInfo(chan);
			} else {
				throw new ProtocolException("Received restrict ack without a request having been sent.");
			}
		} else if (chan.isLocallyRestricted()) {
			/* TODO: Call channelRestrictInfo callback instead. */
			throw new ProtocolException("Restriction denied by remote end.");
		} else {
		}
	}

	@Override
	public void handle_channel_restrict_request(channel_restrict_request req) throws IOException {
		logger.errc("Got unwanted channel restrict request on channel: " + req.dest_chan_id);
		// final channel_restrict_ack resp = new channel_restrict_ack();
		// final Channel chan = channels.get(req.dest_chan_id);
		// if (chan == null) {
		//     throw new ProtocolException("Received restrict request on non-existing channel (id: " +
		//                                 req.dest_chan_id + ")");
		// }

		// chan.setRemoteRestricted(req.restricted);
		// if (chan.isRemotelyRestricted()) {
		//     [> Restrict request. <]
		//     if (!chan.isLocallyRestricted()) {
		//         [> Remotely initiated restrict request, ask app delegate if accepted. <]
		//         chan.setLocalRestricted(delegate.channelRestricted(chan));
		//     } else {
		//         [> Remotely initiated restrict request on already restricted channel. <]
		//         throw new ProtocolException("Received restrict request on already restricted " +
		//                                     "channel (id: " + req.dest_chan_id + ").");
		//     }
		// } else {
		//     [> Unrestrict request. <]
		//     if (chan.isLocallyRestricted()) {
		//         [> Remotely initiated un-restrict request. <]
		//         chan.setRemoteRestricted(false);
		//         chan.setLocalRestricted(false);
		//         delegate.channelRestrictInfo(chan);
		//     } else {
		//         [> Remotely initiated un-restrict request on non-restricted channel. <]
		//         throw new ProtocolException("Received un-restrict on non-restricted channel (id: " +
		//                                     req.dest_chan_id + ").");
		//     }
		// }

		// resp.restricted = chan.isLocallyRestricted();
		// resp.dest_chan_id   = req.source_chan_id;
		// resp.source_chan_id = req.dest_chan_id;

		// channel_restrict_ack.encode(bottomEncoder, resp);
	}

	@Override
	public void handle_data_sample(data_sample ds) throws Exception {
		// logger.logc("got data sample: " + String.format("0x%x", ds.app_enc_data[0]) + "...");
		/* Get the channel this data sample references. */
		final Channel chan = channels.get(ds.dest_chan_id);
		if (chan != null) {
			chan.receivedData(ds.app_enc_data);
		} else {
			logger.errc("Got data sample on non-existing channel: " + ds.dest_chan_id);
		}
	}

	@Override
	public synchronized void close() throws IOException {
		// for (Channel channel : channels) {
		//     channel.

		// }
		sock.close();
		reader.interrupt();
	}

	@Override
	public void sendData(Channel chan, byte[] data) throws IOException {
		final data_sample dataSample = new data_sample();
		dataSample.dest_chan_id = chan.getRemoteId();
		dataSample.src_chan_id = chan.getLocalId();
		dataSample.seqno = chan.getNextSequenceNumber();
		dataSample.important = false; // TODO: Handle this better.
		dataSample.app_enc_data = data;

		// logger.logc("data_sample.encode(" + bottomEncoder + ", " + dataSample + ");");
		data_sample.encode(bottomEncoder, dataSample);
	}

	public void channelAutoRestricted(Channel chan) throws IOException {

		final channel_restrict_ack ack = new channel_restrict_ack();
		ack.dest_chan_id = chan.getRemoteId();
		ack.source_chan_id = chan.getLocalId();
		ack.restricted = true;

		channel_restrict_ack.encode(bottomEncoder, ack);
	}

	// TODO: Should Reader exist in the common Connection super class?
	private class Reader extends Thread {
		private ConnectionDecoder dec;
		private Debug logger;

		public Reader(ConnectionDecoder dec) {
			this.dec = dec;
			start();
			logger = new Debug(Reader.class.getCanonicalName());
		}

		public void run() {
			while (!isInterrupted()) {
				try {
					dec.runOne();
				} catch (EOFException e) {
					System.out.println("Decoder reached end of file.");
					return;
				} catch (SocketException e) {
					interrupt();
				} catch (Exception e) {
					logger.logc("Got random exception: " + e.toString());
					e.printStackTrace();
					// TODO: handle
				}
			}
		}
	}
}
