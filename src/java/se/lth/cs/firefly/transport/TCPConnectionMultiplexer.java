package se.lth.cs.firefly.transport;

import se.lth.cs.firefly.FireflyApplication;
import se.lth.cs.firefly.FireflyServer;
import se.lth.cs.firefly.util.Debug;

import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;

public class TCPConnectionMultiplexer {
	/**
	 * The server socket used to accept connections.
	 */
	private Reader reader;
	/**
	 * The application delegate.
	 */
	private FireflyApplication delegate;
	/**
	 * The list of open connections.
	 */
	private List<Connection> connections;

	private Debug logger;

	/**
	 * Creates a new instance for accepting incoming connections.
	 *
	 * @param delegate The application server delegate.
	 * @param port The port to listen for {@link Connection}s on.
	 * @throws IOException if there is an error when opening the
	 *                     listening socket.
	 */
	public TCPConnectionMultiplexer(FireflyServer delegate, int port) throws IOException {
		this(delegate);
		logger = new Debug(getClass().getCanonicalName());
		reader = new Reader(delegate, port);
		reader.start();
	}

	/**
	 * Creates a new instance for opening connections.
	 *
	 * A TCPConnectionMultiplexer cannot be used to accept incoming
	 * connections when this is constructor is used.
	 *
	 * @param delegate The application delegate.
	 */
	public TCPConnectionMultiplexer(FireflyApplication delegate) {
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
	}

	/**
	 * Opens a new {@link Connection} to the given host and port.
	 *
	 * @param host The host to connect to.
	 * @param port The port on the remote host to connect to.
	 */
	public Connection openConnection(String host, int port)
			throws UnknownHostException, IOException {
		final Socket sock = new Socket(host, port);
		final TCPConnection conn = new TCPConnection(sock, delegate);
		addConnection(conn);

		return conn;
	}

	// TODO: Move.
	private void addConnection(Connection conn) {
		synchronized (connections) {
			connections.add(conn);
		}
	}

	/**
	 * Closes this TCPConnectionMultiplexer. Closing it will close all
	 * {@link Connection}s opened on it.
	 *
	 * @throws IOException if there is an error while closing.
	 */
	public void close() throws IOException {
		synchronized (connections) {
			/* Close all the connections */
			for (Connection c : connections) {
				c.close();
			}
		}
		reader.interrupt();
		reader.ssock.close();
	}

	/**
	 * Subclass of {@link java.lang.Thread} that accepts new
	 * connections.
	 */
	private class Reader extends Thread {
		/**
		 * The server socket used to accept incoming connections.
		 */
		private final ServerSocket ssock;
		private final FireflyServer delegate;

		public Reader(FireflyServer delegate, int port) throws IOException {
			this.delegate = delegate;
			ssock = new ServerSocket(port);
			/* TODO: Remove? Or do we want this? */
			ssock.setReuseAddress(true);
		}

		public void run() {
			boolean run = true;

			while (run && !isInterrupted()) {
				try {
					final Socket client = ssock.accept();
					final String address = client.getInetAddress().getHostAddress();

					/* Ask application if the connection is accepted or not. */
					if (delegate.acceptConnection(address)) {
						final Connection conn = new TCPConnection(client, delegate);
						addConnection(conn);
						delegate.connectionOpened(conn);
					} else {
						client.close();
					}
				} catch (SocketException e) {
					run = false;
				} catch (IOException e) {
					logger.errc("IO broke: " + e.toString());
				}
			}
		}
	}
}
