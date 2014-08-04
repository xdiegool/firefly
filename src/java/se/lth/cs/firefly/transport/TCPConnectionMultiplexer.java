package se.lth.cs.firefly.transport;

import java.net.ServerSocket;
import java.util.ArrayList;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.Socket;
import java.io.IOException;
import java.net.UnknownHostException;

import se.lth.cs.firefly.ActionQueue;
import se.lth.cs.firefly.FireflyApplication;
import se.lth.cs.firefly.protocol.Connection;
import se.lth.cs.firefly.protocol.LinkLayerPort;
import se.lth.cs.firefly.util.BlockingAppendableInputStream;
import se.lth.cs.firefly.util.Debug;

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
						&& delegate.acceptConnection(remoteAddress, remotePort)){
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
