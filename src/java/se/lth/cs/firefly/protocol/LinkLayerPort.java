package se.lth.cs.firefly.protocol;

import java.io.IOException;
import java.net.InetAddress;
import java.net.SocketException;
import java.util.ArrayList;

import se.lth.cs.firefly.*;
import se.lth.cs.firefly.util.ActionQueue;
import se.lth.cs.firefly.util.Debug;
/**
 * Class to abstract common logic for protocol specific connection multiplexors.
 * Subclasses to this implement protocol specifics of accepting network connections and
 * possibly multiplexing incoming messages to the right firefly connection.
 *
 */
public abstract class LinkLayerPort {
	protected int localPort;
	protected FireflyApplication delegate;
	private ArrayList<Connection> connections;
	protected Thread listener;
	protected ActionQueue actionQueue;
	
	/**
 	* Constructor for applications awaiting connections
 	*
 	* 
 	*/ 
	public LinkLayerPort(int localPort, FireflyApplication delegate,
			ActionQueue actionQueue) {
		this.actionQueue = actionQueue;
		this.localPort = localPort;
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
		listener = new Thread(new Listener());
		listener.start();
	}
	/**
 	* Constructor for application that initiate connections.
 	*
 	*/
	public LinkLayerPort(FireflyApplication delegate, ActionQueue actionQueue) {
		this.actionQueue = actionQueue;
		this.delegate = delegate;
		connections = new ArrayList<Connection>();
	}
	/**
 	* Does all the protocol specific setup for a connection to the host
 	* at remoteAddress at port remotePort to work.
 	* 
 	* @returns The created connection with the protocol specific transport layer abstraction
 	*/	 
	protected abstract Connection openTransportConnection(
			InetAddress remoteAddress, int remotePort) throws IOException;
	/**
 	* Listening for new connections, is called be listener thread. Also handles
 	* packet to connection multiplexing if its not done by the udnerlying
 	* protocol.
 	*
 	*
 	*/
	protected abstract void listen() throws IOException;
	
	/**
 	* Closes the network resources that were created when calling openTransportConnection.
 	*
 	*/ 
	protected abstract void transportClose() throws IOException;

	/**
 	* Opens a new connection to the specified host and port, also starts
 	* the listener if it hasn't been started before.
 	*
 	*/ 
	public Connection openConnection(InetAddress remoteAddress, int remotePort)
			throws IOException {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY,new ActionQueue.Action() {
			@Override
			public void doAction() throws Exception {
				if (listener == null) {
					listener = new Thread(new Listener());
					listener.start();
				}
			}
		});
		// The thread calling openTransportConnection must not hold the lock to
		// this object, since this call is blocking and will introduce a
		// deadlock by
		// synchronizing(this) together with the listener
		Connection conn = openTransportConnection(remoteAddress, remotePort);
		addConnection(conn);
		return conn;
	}

	public void closeConnection(final Connection conn) {
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY, new ActionQueue.Action() {
			public void doAction() throws Exception {
				connections.remove(conn);
				conn.close();
			}
		});
	}

	public void close() {
		// A bit ugly but is needed for all channels to close and send close requests before closing network resources.
		actionQueue.queue(ActionQueue.Priority.MED_PRIORITY, new ActionQueue.Action() {
			public void doAction() throws Exception {
				for (Connection conn : connections) {
					conn.close();
				}
				listener.interrupt();
			}
		});
		// This needs low prio so that it doesn't close sockets etc before all close reqs are sent
		actionQueue.queue(ActionQueue.Priority.LOW_PRIORITY, new ActionQueue.Action() { 
			public void doAction() throws Exception {
				transportClose();
			}
		});
		
	}

	protected void addConnection(Connection conn) {
		connections.add(conn);
	}

	protected Connection getConnection(InetAddress address, int port) {
		for (Connection conn : connections) {
			if (conn.isTheSame(address, port)) {
				return conn;
			}
		}
		return null;
	}

	/* ###################### PRIVATE HELPER CLASSES ################### */
	/**
 	* Listens to the underlying network protocol, be it socket or anything else, 
 	* to accept new connections and possibly multiplex incoming messages. Exactly
 	* what is done is determined in subclasses.
 	*
 	*/
	
	private class Listener implements Runnable {
		public void run() {
			Debug.log("Listener running");
			while (!Thread.currentThread().isInterrupted()) {
				try {
					listen();
				} catch (SocketException e) {
					Thread.currentThread().interrupt(); // Socket closed
				} catch (Exception e) {
					Debug.log("Error in network listener in LinkLayerPort");
					Debug.printStackTrace(e);
				}
			}
			Debug.log("Listener stopping");
		}
	}

}
