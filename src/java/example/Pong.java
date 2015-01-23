package example;

import se.lth.cs.firefly.FireflyServer;
import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.protocol.TypeList;
import se.lth.cs.firefly.transport.Connection;
import se.lth.cs.firefly.transport.TCPConnectionMultiplexer;
import se.lth.cs.firefly.util.Debug;

import lc_gen.data;

import java.io.IOException;

public class Pong extends AbstractPingPong implements FireflyServer, data.Handler {
	private static final int TEST_CONNECTION_OPENED = 0;
	private static final int TEST_CHAN_RECEIVED     = TEST_CONNECTION_OPENED + 1;
	private static final int TEST_CHAN_OPENED       = TEST_CHAN_RECEIVED     + 1;
	private static final int TEST_CHAN_RESTRICTED   = TEST_CHAN_OPENED       + 1;
	private static final int TEST_DATA_RECEIVED     = TEST_CHAN_RESTRICTED   + 1;
	private static final int TEST_DATA_SEND         = TEST_DATA_RECEIVED     + 1;
	private static final int TEST_CHAN_CLOSED       = TEST_DATA_SEND         + 1;
	private static final int PONG_TEST_DONE         = TEST_CHAN_CLOSED       + 1;
	private static final int PONG_NBR_TESTS         = PONG_TEST_DONE         + 1;

	private TCPConnectionMultiplexer connMux;

	public Pong() {
		TEST_NAMES = new String[]{
			"Connection opened",
			"Received channel (responding party)",
			"Opened channel (responding party)",
			"Restricted channel (responding party)",
			"Received data",
			"Send data",
			"Channel closed",
			"Pong done"
		};
		testResults = new boolean[PONG_NBR_TESTS];
	}

	/* BEGIN FIREFLY CHANNEL CALLBACKS */
	@Override
	public boolean channelAccept(Channel chan) {
		final TypeList types = new TypeList();
		types.addEncoderType(new data());
		types.addDecoderType(new data(), this);

		chan.setChannelTypes(types);

		testResults[TEST_CHAN_RECEIVED] = true;

		return true;
	}

	@Override
	public synchronized void channelOpened(Channel chan) {
		this.chan = chan;
		testResults[TEST_CHAN_OPENED] = true;
		notifyAll();
	}

	@Override
	public synchronized void channelClosed(Channel chan) {
		this.chan = null;
		testResults[TEST_CHAN_CLOSED] = true;
		notifyAll();
	}

	@Override
	public boolean channelRestricted(Channel chan) {
		return true;
	}

	@Override
	public void channelRestrictInfo(Channel chan) {
		testResults[TEST_CHAN_RESTRICTED] = chan.isRestricted();
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
	public void connectionOpened(Connection conn) {
		testResults[TEST_CONNECTION_OPENED] = true;
	}
	/* END FIREFLY CONNECTION CALLBACKS */

	private synchronized void waitForChannel() throws InterruptedException {
		while (chan == null) wait();
	}

	private synchronized void waitForClose() throws InterruptedException {
		while (chan != null) wait();
	}

	@Override
	public synchronized void handle_data(int value) throws IOException {
		Debug.log("Got data! " + value);
		testResults[TEST_DATA_RECEIVED] = value == PING_DATA;
		data.encode(chan.getEncoder(), PONG_DATA);
		testResults[TEST_DATA_SEND] = true;
		dataReceived = true;
		notifyAll();
	}

	protected void internalRun() throws IOException, InterruptedException {
		/* Start listening for connections. */
		connMux = new TCPConnectionMultiplexer(this, 8080);

		/* Wait for a channel to be opened. */
		waitForChannel();

		/* Wait for some data to arrive. */
		waitForData();

		waitForClose();
		try {
			connMux.close();
		} catch (IOException e) {
			Debug.log(e.toString());
		}

		testResults[PONG_TEST_DONE] = true;
	}

	public static void main(String[] args) {
		new Pong().run();
	}
}
