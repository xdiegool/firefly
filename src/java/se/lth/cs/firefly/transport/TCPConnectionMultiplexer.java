package se.lth.cs.firefly.transport;

import se.lth.cs.firefly.FireflyApplication;
import se.lth.cs.firefly.FireflyServer;
import se.lth.cs.firefly.util.Debug;

import java.net.ServerSocket;
import java.util.ArrayList;
import java.net.InetAddress;
import java.net.Socket;
import java.io.IOException;
import java.net.UnknownHostException;

public class TCPConnectionMultiplexer {
	private ServerSocket ssock;
	private Reader reader;
	private FireflyApplication delegate;

	private ArrayList<Connection> connections;

	public TCPConnectionMultiplexer(FireflyServer delegate, int port)
		throws IOException
	{
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
		ssock = new ServerSocket(port);
		reader = new Reader(delegate);
		reader.start();
	}

	public TCPConnectionMultiplexer(FireflyApplication delegate) {
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
	}

	public Connection openConnection(String host, int port)
		throws UnknownHostException, IOException
	{
		Socket sock = new Socket(host, port);
		TCPConnection conn = new TCPConnection(sock, delegate);
		addConnection(conn);

		return conn;
	}

	// TODO: Move.
	public void addConnection(Connection conn) {
		synchronized (connections) {
			connections.add(conn);
		}
	}

	private class Reader extends Thread {
		private FireflyServer srv;

		public Reader(FireflyServer srv) {
			this.srv = srv;
		}

		public void run() {
			while (!interrupted()) {
				// Make udp-ish with selectors?
				try {
					Socket s = ssock.accept();
					boolean conf;
					String ha = s.getInetAddress().getHostAddress();
					Debug.log("Incoming " + ha);
					conf = srv.acceptConnection(ha);
					if (conf) {
						TCPConnection c = new TCPConnection(s, delegate);
						TCPConnectionMultiplexer.this.addConnection(c);
					} else {
						s.close();
					}
				} catch (IOException e) {
					Debug.errx("IO broke: " + e);
				}
			}
		}
	}
}
