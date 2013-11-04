package se.lth.cs.firefly;

import genproto.*;

import se.lth.control.labcomm.LabCommEncoderChannel;
import se.lth.control.labcomm.LabCommDecoderChannel;

import java.net.Socket;
import java.io.OutputStream;
import java.io.InputStream;

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
	private Reader reader;

	// Between topcoder and socket.
	private ConnectionDecoder bottomDecoder;
	private ConnectionEncoder bottomEncoder;

	// Between application and bottomcoder.
	// private LabCommDecoderChannel topDecoder;
	// private LabCommEncoderChannel topEncoder;

	public TCPConnection(Socket sock) {
		this.sock = sock;
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

	// LabComm callbacks for generated protocol. Might move.
		public void handle_ack(ack value) {
	}

	public void handle_channel_ack(channel_ack value) {
	}

	public void handle_channel_close(channel_close value) {
	}

	public void handle_channel_request(channel_request value) {
	}

	public void handle_channel_response(channel_response value) {
	}

	public void handle_channel_restrict_ack(channel_restrict_ack value) {
	}

	public void handle_channel_restrict_request(channel_restrict_request value) {
	}

	public void handle_data_sample(data_sample value) {
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
					System.out.println("Running decoder.");
					dec.runOne();
				} catch (java.io.EOFException e) {
					System.out.println("Decoder reached end of file.");
				}
			}

		}
	}
}
