package se.lth.cs.firefly.protocol;

import java.io.IOException;
import java.net.InetAddress;
import java.net.SocketException;
import java.util.ArrayList;

import se.lth.cs.firefly.*;
import se.lth.cs.firefly.util.Debug;

public abstract class LinkLayerPort {
	protected int localPort;
	protected FireflyApplication delegate;
	private ArrayList<Connection> connections;
	protected Thread listener;
	protected ActionQueue actionQueue;

	public LinkLayerPort(int localPort, FireflyApplication delegate,
			ActionQueue actionQueue) {
		this.actionQueue = actionQueue;
		this.localPort = localPort;
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
		listener = new Thread(new Listener());
		listener.start();
	}

	public LinkLayerPort(FireflyApplication delegate, ActionQueue actionQueue) {
		this.actionQueue = actionQueue;
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
	}

	protected abstract Connection openTransportConnection(
			InetAddress remoteAddress, int remotePort) throws IOException;

	protected abstract void listen() throws IOException;

	protected abstract void transportClose() throws IOException;

	public Connection openConnection(InetAddress remoteAddress, int remotePort)
			throws IOException {
		actionQueue.queue(new ActionQueue.Action() {
			@Override
			public void doAction() throws Exception {
				if (listener == null) {
					listener = new Thread(new Listener());
					listener.start();
				}
			}
		});
		// The thread calling openTransportConnection must not hold the lock to
		// this object, since this call is blocking and will introduce a deadlock by
		// synchronizing(this) together with the listener
		Connection conn = openTransportConnection(remoteAddress, remotePort);
		addConnection(conn);
		return conn;
	}

	public synchronized void closeConnection(Connection conn) {
		connections.remove(conn);
		conn.close();
	}

	public synchronized void close() throws IOException {
		for (Connection conn : connections) {
			conn.close();
		}
		listener.interrupt();
		transportClose();
	}

	protected synchronized void addConnection(Connection conn) {
		connections.add(conn);
	}

	protected synchronized Connection getConnection(InetAddress address,
			int port) {
		for (Connection conn : connections) {
			if (conn.isTheSame(address, port)) {
				return conn;
			}
		}
		return null;
	}

	/* ###################### PRIVATE HELPER CLASSES ################### */
	private class Listener implements Runnable {
		public void run() {
			Debug.log("Listener running");
			while (!Thread.currentThread().isInterrupted()) {
				try {
					listen();
				} catch (SocketException e) {
					Thread.currentThread().interrupt(); // Socket closed
				} catch (Exception e) {
					delegate.LLPError(LinkLayerPort.this, e);
				}
			}
			Debug.log("Listener stopping");
		}
	}

}
