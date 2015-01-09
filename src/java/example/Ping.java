package example;

import se.lth.cs.firefly.FireflyApplication;
import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.transport.Connection;
import se.lth.cs.firefly.transport.TCPConnectionMultiplexer;
import se.lth.cs.firefly.util.Debug;

import se.lth.control.labcomm.Encoder;
import se.lth.control.labcomm.Decoder;

import lc_gen.data;

import java.io.IOException;

public class Ping implements Runnable, FireflyApplication, data.Handler {
	/**
	 * The channel used to communicate with Pong.
	 */
	private Channel chan;

	/**
	 * Flag to indicate whether we've received data back from Pong or not.
	 */
	private boolean echoed;

	/* BEGIN FIREFLY CHANNEL CALLBACKS */
	// TODO: Enforce convention regarding channel set up in
	//       client-server scenario?

	@Override
	public boolean channelAccept(Connection connection) {
		return false;
	}

	@Override
	public synchronized void channelOpened(Channel chan) {
		this.chan = chan;
		notifyAll();
	}

	@Override
	public void channelClosed(Channel chan) {
		this.chan = null;
	}

	@Override
	public void channelRestrict(Channel chan) {
		// Not used.
	}

	@Override
	public void channelStatus(Channel chan) {
		// Not used.
	}

	@Override
	public void channelError(Channel chan) {
		Debug.errx("Chan. err.");
	}

	@Override
	public void connectionError(Connection conn) {
		Debug.errx("Conn. err.");
	}
	/* END FIREFLY CHANNEL CALLBACKS */

	/**
	 * Callback for LabComm data.
	 */
	@Override
	public synchronized void handle_data(int value) {
		System.out.println("Got data:" + value);
		echoed = true;
		notifyAll();
	}

	@Override
	public void run() {
		try {
			internalRun();
		} catch (InterruptedException e) {
		} catch (IOException e) {
		}
	}

	private synchronized void waitForChan() throws InterruptedException {
		while (chan == null) {
			wait();
		}
	}

	private synchronized void waitForEcho() throws InterruptedException {
		while (!echoed){
			wait();
		}
	}

	private void internalRun() throws InterruptedException, IOException {
		TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this);
		Connection conn = connMux.openConnection(null, 8080); // Loopback

		/* Open the channel and wait for it to complete. */
		conn.openChannel();
		waitForChan();

		/* Get encoder and decoder from the channel. */
		Encoder enc = chan.getEncoder();
		Decoder dec = chan.getDecoder();

		/* Register the data type on the encoder and decoder. */
		data.register(enc);
		data.register(dec, this);

		/* Send some data on the channel and wait for reply. */
		data.encode(enc, 123);
		waitForEcho();

		/* Close the channel since we're done. */
		chan.close();
	}

	public static void main(String[] args) {
		new Ping().run();
	}

}
