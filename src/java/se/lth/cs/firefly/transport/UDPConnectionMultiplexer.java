package se.lth.cs.firefly.transport;

import java.io.*;
import java.net.*;
import java.util.HashMap;

import se.lth.cs.firefly.protocol.*;
import se.lth.cs.firefly.util.*;

/**
 * Multiplexes UDP connections, since UDP is a connectionless protocol this
 * class receives connection or opens them but also multiplexes incoming packets
 * to the appropriate channel. This is done by writing to a stream that
 * Connection reads from. 
 * 
 */
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
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,
				new ActionQueue.Action() {
					private DatagramPacket p;

					private ActionQueue.Action init(DatagramPacket p) {
						this.p = p;
						return this;
					}

					public void doAction() throws Exception {
						InetAddress remoteAddress = p.getAddress();
						int remotePort = p.getPort();
						InetSocketAddress sa = new InetSocketAddress(
								remoteAddress, remotePort);
						BlockingAppendableInputStream stream = getStream(sa);
						byte[] toConnection = new byte[p.getLength()];
						System.arraycopy(p.getData(), 0, toConnection, 0,
								toConnection.length);
						if (stream == null) {
							if (delegate.connectionAccept(remoteAddress,
									remotePort)) {
								Debug.log("New Connection created");
								// null for version bypass
								stream = new BlockingAppendableInputStream();
								addStream(sa, stream);
								Connection conn = new Connection(new UDPLayer(
										dsock, remoteAddress, remotePort,
										stream), delegate, actionQueue);
								addConnection(conn);
								// The actual package contents are appended.
								// This must be done after connection creation
								// otherwise it reads the channel request before
								// it has registered itself to the decoders
								stream.append(toConnection);
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
		Debug.log("openTransport Connection");
		// null for version bypass
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
