package se.lth.cs.firefly;

import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.transport.Connection;

/**
 * The interface that applications need to implement to communicate
 * with other Firefly applications.
 */
public interface FireflyApplication {
	// Want blocking ops?

	/**
	 * Called when a new {@link Channel} is received.
	 *
	 * The application that receives the {@link Channel} MUST register
	 * the types it wants to use on the channel in this callback.
	 *
	 * @param chan The {@link Channel} that has been received.
	 * @return A boolean indicating whether the channel is accepted by the
	 *         application or not.
	 */
	public boolean channelAccept(Channel chan);

	/**
	 * Called when a {@link Channel} is open. This notifies the application
	 * that it is ready to send on. An application MUST NOT send data on
	 * the channel before this.
	 *
	 * @param chan The {@link Channel} that now is open.
	 */
	public void channelOpened(Channel chan);

	/**
	 * Called when a {@link Channel} is closed. An application MUST NOT to
	 * send any data on the channel when this callback is received.
	 *
	 * @param chan The channel that is now open.
	 */
	public void channelClosed(Channel chan);

	/**
	 * TODO: Move to RestrictDelegate interface?
	 */
	public boolean channelRestricted(Channel chan);

	/**
	 * TODO: Move to RestrictDelegate interface?
	 * TODO: Add restrict state info.
	 */
	public void channelRestrictInfo(Channel chan);

	/**
	 * TODO: What is this for? Remove?
	 */
	public void channelStatus(Channel chan);

	/**
	 * TODO: Write javadoc.
	 */
	public void channelError(Channel chan);

	/**
	 * TODO: Write javadoc.
	 */
	public void connectionError(Connection conn);
}
