package se.lth.cs.firefly;

import java.net.ServerSocket;
import java.util.ArrayList;
import java.net.InetAddress;
import java.net.Socket;

public class TCPConnectionMultiplexer {
	private ServerSocket ssock;
	private Reader reader;
	private FireflyApplication delegate	;

	private ArrayList<Connection> connections;

	public TCPConnectionMultiplexer(FireflyServer delegate, int port) {
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
		ssock = new ServerSocket(port);
		reader = new Reader();
		reader.start();
	}

	public TCPConnectionMultiplexer(FireflyApplication delegate) {
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
	}

	public Connection openConnection(String host, int port) {
		Socket sock = new Socket(host, port);
		TCPConnection conn = new TCPConnection(sock);
		addConnection(conn);

		return conn;
	}

	// TODO: Move.
	public void addConnection(Connection conn) {
		synchronized (connections) {
			connections.add(conn);
		}
		// TODO: Callback?
	}

	private class Reader extends Thread {
		public void run() {
			while (!interrupted()) {
				// Make udp-ish with selectors?
				Socket s = ssock.accept();
				TCPConnection c = new TCPConnection(s);
				// coannections.append(c);
				TCPConnectionMultiplexer.this.addConnection(c);
			}
		}
	}
}
