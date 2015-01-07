package se.lth.cs.firefly;

import genproto.*;

import se.lth.control.labcomm.EncoderChannel;
import se.lth.control.labcomm.DecoderChannel;

import java.net.Socket;
import java.io.OutputStream;
import java.io.InputStream;
import java.io.IOException;

public class TCPConnection extends Connection implements
							   ack.Handler,
							   channel_ack.Handler,
							   channel_close.Handler,
							   channel_request.Handler,
							   channel_response.Handler,
							   channel_restrict_ack.Handler,
							   channel_restrict_request.Handler,
							   data_sample.Handler
{
	private Socket sock;
	private FireflyApplication delegate;
	private Reader reader;

	private ConnectionDecoder bottomDecoder;
	private ConnectionEncoder bottomEncoder;

	public TCPConnection(Socket sock, FireflyApplication delegate)
		throws IOException
	{
		this.sock = sock;
		this.delegate = delegate;
		bottomDecoder = new ConnectionDecoder(sock.getInputStream());
		bottomEncoder = new ConnectionEncoder(sock.getOutputStream(),
											  bottomDecoder);

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
	public synchronized void openChannel() throws IOException {
		Channel chan = new Channel(nextChannelID, this);
		channel_request req = new channel_request();
		req.source_chan_id = nextChannelID;
		channel_request.encode(bottomEncoder, req);
		nextChannelID++;
	}

	public void closeChannel(Channel chan) throws IOException{
		channel_close cc = new channel_close();
		cc.source_chan_id = chan.getLocalID();
		cc.dest_chan_id = chan.getRemoteID();
		channel_close.encode(bottomEncoder, cc);
	}

	// LabComm callbacks for generated protocol. Might move.
	public void handle_ack(ack value) {
		// Data ack not used with TCP.
	}

	public void handle_channel_ack(channel_ack value) {
		Channel chan = channels.get(value.dest_chan_id);
		if (value.ack) {
			chan.setOpen();
			delegate.channelOpened(chan);
		} else {
			delegate.channelClosed(chan);
			chan.setClosed();
			channels.remove (value.dest_chan_id);
		}
	}

	public synchronized void handle_channel_close(channel_close req)
		throws IOException
	{
		Channel chan = channels.get(req.dest_chan_id);
		if (chan.isOpen()) {
			// Request
			delegate.channelClosed(channels.get(req.dest_chan_id));
			channel_close resp = new channel_close();
			resp.dest_chan_id = req.source_chan_id;
			resp.source_chan_id = req.dest_chan_id;
			channel_close.encode(bottomEncoder, resp);
			chan.setClosed();
			channels.remove(req.dest_chan_id);
		} else if (chan.isClosed()) {
			// Response
			channels.remove(req.dest_chan_id);
		}
	}

	// 2
	public synchronized void handle_channel_request(channel_request req)
		throws IOException
	{
		channel_response resp = new channel_response();
		resp.dest_chan_id = req.source_chan_id;
		if (delegate.channelAccept(this)) {
			Channel chan = new Channel(nextChannelID, this);
			resp.source_chan_id = nextChannelID;
			channels.put(nextChannelID, chan);
			resp.ack = true;
			nextChannelID++;
		} else {
			resp.ack = false;
		}
		channel_response.encode(bottomEncoder, resp);
	}

	// 3
	public synchronized void handle_channel_response(channel_response resp)
		throws IOException
	{
		channel_ack ack = new channel_ack();
		ack.source_chan_id = resp.dest_chan_id; // TODO: Confirm this w/ prev.
		ack.dest_chan_id = resp.source_chan_id;
		ack.ack = resp.ack;
		if (resp.ack) {
			channels.get(resp.dest_chan_id).setOpen();
		}
		channel_ack.encode(bottomEncoder, ack);
	}

	public void handle_channel_restrict_ack(channel_restrict_ack value) {
	}

	public void handle_channel_restrict_request(channel_restrict_request value) {
	}

	public void handle_data_sample(data_sample ds) throws Exception {
		Channel chan = channels.get(ds.dest_chan_id);
		chan.receivedData(ds.app_enc_data);
	}

	private class Reader extends Thread {
		private ConnectionDecoder dec;

		public Reader(ConnectionDecoder dec) {
			this.dec = dec;
			start();
		}

		public void run() {
			while (!interrupted()) {
				try {
					dec.runOne();
					} catch (java.io.EOFException e) {
						System.out.println("Decoder reached end of file.");
					} catch (Exception e) {
					// TODO: handle
				}
			}
		}

	}
}
