package example;

import se.lth.cs.firefly.FireflyServer;
import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.transport.Connection;
import se.lth.cs.firefly.transport.TCPConnectionMultiplexer;
import se.lth.cs.firefly.util.Debug;

import se.lth.control.labcomm.Encoder;
import se.lth.control.labcomm.Decoder;

import lc_gen.data;

import java.io.IOException;

public class Pong implements Runnable, FireflyServer, data.Handler {
	/**
	 * The channel used to communicate with Ping.
	 */
	private Channel chan;

	/**
	 * Flag to indicate whether we've received data from Ping or not.
	 */
	private boolean dataReceived;

	/* BEGIN FIREFLY CHANNEL CALLBACKS */
	@Override
	public boolean channelAccept(Connection connection) {
		return true;
	}

	@Override
	public synchronized void channelOpened(Channel chan) {
		this.chan = chan;
		Debug.log("Got channel.");
		notifyAll();
	}

	@Override
	public void channelClosed(Channel chan) {
	}

	@Override
	public void channelRestrict(Channel chan) {
	}

	@Override
	public void channelStatus(Channel chan) {
	}

	@Override
	public void channelError(Channel chan) {
	}
	/* END FIREFLY CHANNEL CALLBACKS */

	/* BEGIN FIREFLY CONNECTION CALLBACKS */
	@Override
	public void connectionError(Connection conn) {
	}

	@Override
	public boolean acceptConnection(String addr) {
		return true;
	}

	@Override
	public void connectionOpened() {
	}
	/* END FIREFLY CONNECTION CALLBACKS */

	@Override
	public void run() {
		try {
			internalRun();
		} catch (IOException e) {
			Debug.errx("IO broke: " + e);
		} catch (InterruptedException e) {
		}
	}

	private synchronized void waitForChannel() throws InterruptedException {
		while (chan == null) wait();
	}

	@Override
	public synchronized void handle_data(int value) throws IOException {
		data.encode(chan.getEncoder(), value);
		dataReceived = true;
		notifyAll();
	}

	private synchronized void waitForData() throws InterruptedException {
		while (!dataReceived) wait();
	}

	private void internalRun() throws IOException, InterruptedException {
		/* Start listening for connections. */
		final TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this, 8080);

		/* Wait for a channel to be opened. */
		waitForChannel();
		System.out.println("Got chan");

		/* Get encoder and decoder from channel. */
		final Decoder dec = chan.getDecoder();
		final Encoder enc = chan.getEncoder();

		/* Register data type on encoder and decoder. */
		data.register(enc);
		data.register(dec, this); // Reg. handler above.

		/* Wait for some data to arrive. */
		waitForData();
		System.out.println("Got data");
	}

	public static void main(String[] args) {
		new Pong().run();
	}
}
