package se.lth.cs.firefly;

import genproto.*;

import se.lth.control.labcomm.LabCommEncoderChannel;
import se.lth.control.labcomm.LabCommDecoderChannel;

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
	public void openChannel() throws IOException {
		channel_request req = new channel_request();
		req.source_chan_id = nextChannelID++;
		synchronized (bottomEncoder) {
			channel_request.encode(bottomEncoder, req);
		}
	}

	// LabComm callbacks for generated protocol. Might move.
	public void handle_ack(ack value) {
	}

	public void handle_channel_ack(channel_ack value) {
	}

	public void handle_channel_close(channel_close value) {
	}

	// 2
	public synchronized void handle_channel_request(channel_request req) throws IOException {
		channel_response resp = new channel_response();
		resp.source_chan_id = nextChannelID;
		resp.dest_chan_id = req.source_chan_id;
		if (delegate.channelAccept(this)) {
			resp.ack = false;
			nextChannelID++;
		}
		synchronized (bottomEncoder) {
			channel_response.encode(bottomEncoder, resp);
		}
	}

	// 3
	public void handle_channel_response(channel_response resp) throws IOException {
		channel_ack ack = new channel_ack();
		ack.source_chan_id = resp.dest_chan_id; // TODO: Confirm this w/ prev.
		ack.dest_chan_id = resp.source_chan_id;
		ack.ack = resp.ack;
		synchronized (bottomEncoder) {
			channel_ack.encode(bottomEncoder, ack);
		}
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
