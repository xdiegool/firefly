package se.lth.cs.firefly.transport;

import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.protocol.TypeList;

import se.lth.control.labcomm.*;

import java.util.HashMap;
import java.util.Map;
import java.io.IOException;

/**
 * Abstract super class that represents a Connection in Firefly.
 *
 * A Connection is a link to another Firefly instance on top of which
 * several {@link Channel}s exists.
 */
public abstract class Connection {
	/**
	 * The channels on this Connection. The key is the channel's local ID.
	 */
	protected Map<Integer, Channel> channels;

	/**
	 * The ID of the next {@link Channel} that is created.
	 */
	protected int nextChannelID;

	/**
	 * Creates a new Connection.
	 */
	public Connection() {
		channels = new HashMap<Integer, Channel>();
	}

	/**
	 * Opens a new {@link Channel} on this Connection.
	 *
	 * @param channelTypes The list of types to register on the new Channel's
	 *                     {@link se.lth.control.labcomm.Encoder} and
	 *                     {@link se.lth.control.labcomm.Decoder}
	 * @throws IOException if there is an error sending the open request.
	 */
	public abstract void openChannel(TypeList channelTypes) throws IOException;

	/**
	 * Closes the provided {@link Channel}.
	 *
	 * @throws IOException if there is an error sending the close request.
	 */
	public abstract void closeChannel(Channel chan) throws IOException;

	/**
	 * Closes this entire Connection, including all the {@link Channel}s
	 * on it.
	 *
	 * @throws IOException if there is an error closing either the
	 *                     Connection or one of its {@link Channel}s.
	 */
	public abstract void close() throws IOException;

	/**
	 * Sends the data from the provided {@link Channel} on this Connection.
	 *
	 * @throws IOException if there is an error sending the data.
	 */
	public abstract void sendData(Channel chan, byte[] data) throws IOException;

	/**
	 * TODO: Doc
	 */
	public abstract void channelAutoRestricted(Channel chan) throws IOException;
}
