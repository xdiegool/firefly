package se.lth.cs.firefly.transport;

import java.net.ServerSocket;
import java.net.InetAddress;
import java.net.Socket;
import java.io.IOException;

import se.lth.cs.firefly.protocol.*;
import se.lth.cs.firefly.util.ActionQueue;
import se.lth.cs.firefly.util.Debug;

/**
 * Multiplexes TCP connection, since TCP is a connection protocol
 * this class only receives connection or opens them. It then wraps
 * the socket in an TransportLayerAbstraction and sends it to Connection. 
 */
public class TCPConnectionMultiplexer extends LinkLayerPort {
	private ServerSocket ssock;

	public TCPConnectionMultiplexer(FireflyApplication delegate, int port,
			ActionQueue actionQueue) throws IOException {
		super(port, delegate, actionQueue);
		ssock = new ServerSocket(port);
	}

	public TCPConnectionMultiplexer(FireflyApplication delegate,
			ActionQueue actionQueue) {
		super(delegate, actionQueue);
	}

	protected void listen() throws IOException {
		Socket s = ssock.accept();
		// Give action to action thread to reduce blocking
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY, new ActionQueue.Action() {
			private Socket s;
			private ActionQueue.Action init(Socket s) {
				this.s = s;
				return this;
			}

			public void doAction() throws Exception {
				InetAddress remoteAddress = s.getInetAddress();
				int remotePort = s.getPort();
				Connection conn = getConnection(remoteAddress, remotePort);
				if (conn == null
						&& delegate.connectionAccept(remoteAddress, remotePort)){
					Debug.log("New Connection created");
					conn = new Connection(new TCPLayer(s), delegate,
							actionQueue);
					addConnection(conn);
				}
			}
		}.init(s));
	}

	@Override
	public Connection openTransportConnection(InetAddress remoteAddress,
			int remotePort) throws IOException {
		Socket sock = new Socket(remoteAddress, remotePort);
		TCPLayer layer = new TCPLayer(sock);
		Connection conn = new Connection(layer, delegate, actionQueue);
		addConnection(conn);
		return conn;
	}

	@Override
	protected void transportClose() throws IOException {
		ssock.close();	
	}
}
