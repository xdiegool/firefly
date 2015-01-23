package example;

import se.lth.cs.firefly.FireflyApplication;
import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.protocol.TypeList;
import se.lth.cs.firefly.transport.Connection;
import se.lth.cs.firefly.transport.TCPConnectionMultiplexer;
import se.lth.cs.firefly.util.Debug;

import lc_gen.data;

import java.io.IOException;

public class Ping extends AbstractPingPong implements FireflyApplication, data.Handler {
	private static final int TEST_CHAN_OPENED     = 0;
	private static final int TEST_CHAN_RESTRICTED = TEST_CHAN_OPENED     + 1;
	private static final int TEST_DATA_SEND       = TEST_CHAN_RESTRICTED + 1;
	private static final int TEST_DATA_RECEIVE    = TEST_DATA_SEND       + 1;
	private static final int TEST_CHAN_CLOSE      = TEST_DATA_RECEIVE    + 1;
	private static final int PING_TEST_DONE       = TEST_CHAN_CLOSE      + 1;
	private static final int PING_NBR_TESTS       = PING_TEST_DONE       + 1;

	/* BEGIN FIREFLY CHANNEL CALLBACKS */
	// TODO: Enforce convention regarding channel set up in
	//       client-server scenario?

	public Ping() {
		TEST_NAMES = new String[]{
			"Open channel (requesting party)",
			"Restrict channel (requesting party)",
			"Send data",
			"Receive data",
			"Close channel",
			"Ping done"
		};
		testResults = new boolean[PING_NBR_TESTS];
	}

	@Override
	public boolean channelAccept(Channel chan) {
		System.err.println("Someone tried to open channel to ping, shouldn't happen!");
		System.exit(1);
		return false;
	}

	@Override
	public synchronized void channelOpened(Channel chan) {
		testResults[TEST_CHAN_OPENED] = true;
		this.chan = chan;
	}

	@Override
	public void channelClosed(Channel chan) {
		testResults[TEST_CHAN_CLOSE] = true;
		this.chan = null;
	}

	@Override
	public boolean channelRestricted(Channel chan) {
		return true;
	}

	@Override
	public synchronized void channelRestrictInfo(Channel chan) {
		testResults[TEST_CHAN_RESTRICTED] = chan.isRestricted();
		notifyAll();
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
		System.out.println("PING: Got data: " + value);
		testResults[TEST_DATA_RECEIVE] = value == PONG_DATA;
		dataReceived = true;
		notifyAll();
	}

	@Override
	protected void internalRun() throws InterruptedException, IOException {
		final TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this);
		final Connection conn = connMux.openConnection(null, 8080); // Loopback

		final TypeList types = new TypeList();
		types.addEncoderType(new data());
		types.addDecoderType(new data(), this);

		/* Open the channel and wait for it to complete. */
		conn.openChannel(types);
		waitForRestrict();

		/* Send some data on the channel and wait for reply. */
		data.encode(chan.getEncoder(), PING_DATA);
		testResults[TEST_DATA_SEND] = true;
		waitForData();

		/* Close the channel since we're done. */
		chan.close();
		conn.close();

		testResults[PING_TEST_DONE] = true;
	}

	public static void main(String[] args) {
		new Ping().run();
	}

}
