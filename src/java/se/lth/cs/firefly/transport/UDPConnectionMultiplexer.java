package se.lth.cs.firefly.transport;

import java.io.*;
import java.net.*;
import java.util.HashMap;

import se.lth.cs.firefly.ActionQueue;
import se.lth.cs.firefly.FireflyApplication;
import se.lth.cs.firefly.protocol.*;
import se.lth.cs.firefly.util.*;

public class UDPConnectionMultiplexer extends LinkLayerPort {
	private DatagramSocket dsock;
	private static final int UDP_BUFFER_SIZE = 64000;
	private HashMap<InetSocketAddress, BlockingAppendableInputStream> streamMap;

	public UDPConnectionMultiplexer(int localPort, FireflyApplication delegate,
			ActionQueue actionQueue) throws SocketException {
		super(localPort, delegate, actionQueue);
		dsock = new DatagramSocket(localPort);
		streamMap = new HashMap<InetSocketAddress, BlockingAppendableInputStream>();

	}

	public UDPConnectionMultiplexer(FireflyApplication delegate,
			ActionQueue actionQueue) throws SocketException {
		super(delegate, actionQueue);
		dsock = new DatagramSocket();
		streamMap = new HashMap<InetSocketAddress, BlockingAppendableInputStream>();
	}

	private synchronized void addStream(InetSocketAddress sa,
			BlockingAppendableInputStream is) {
		streamMap.put(sa, is);
	}

	private synchronized BlockingAppendableInputStream getStream(
			InetSocketAddress sa) {
		return streamMap.get(sa);
	}

	@Override
	protected void listen() throws IOException {
		byte[] inc = new byte[UDP_BUFFER_SIZE];
		DatagramPacket p = new DatagramPacket(inc, inc.length);
		dsock.receive(p);
		Debug.log("Received packet: "
				+ Debug.byteArrayToString(p.getData(), p.getLength()));
		// Give action to action thread to reduce blocking
		actionQueue.queue(new ActionQueue.Action() {
			private DatagramPacket p;

			private ActionQueue.Action init(DatagramPacket p) {
				this.p = p;
				return this;
			}

			public void doAction() throws Exception {
				InetAddress remoteAddress = p.getAddress();
				int remotePort = p.getPort();
				InetSocketAddress sa = new InetSocketAddress(remoteAddress,
						remotePort);
				BlockingAppendableInputStream stream = getStream(sa);
				byte[] toConnection = new byte[p.getLength()];
				System.arraycopy(p.getData(), 0, toConnection, 0,
						toConnection.length);
				if (stream == null) {
					if (delegate.acceptConnection(remoteAddress, remotePort)) {
						Debug.log("New Connection created");
						stream = new BlockingAppendableInputStream(toConnection);
						addStream(sa, stream);
						Connection conn = new Connection(new UDPLayer(dsock,
								remoteAddress, remotePort, stream), delegate,
								actionQueue);
						addConnection(conn);
					}// else discard
				} else {
					Debug.log("Appending to connection");
					stream.append(toConnection);
				}
			}
		}.init(p));
	}

	@Override
	public Connection openTransportConnection(InetAddress remoteAddress,
			int remotePort) throws IOException {
		// This could have potentially used it's own local port but that would
		// require some extra code to create a listener on that new port.
		// TODO Check if this is viable.
		Debug.log("openTransport Connection");
		BlockingAppendableInputStream is = new BlockingAppendableInputStream();
		addStream(new InetSocketAddress(remoteAddress, remotePort), is);
		return new Connection(
				new UDPLayer(dsock, remoteAddress, remotePort, is), delegate,
				actionQueue);
	}

	@Override
	protected void transportClose() throws IOException {
		dsock.close();
	}
}
