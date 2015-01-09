package se.lth.cs.firefly;

import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.transport.Connection;

public interface FireflyApplication {
	// Reduce.
	// Want blocking ops?
	public boolean channelAccept(Connection conn);
	public void channelOpened(Channel chan);
	public void channelClosed(Channel chan);
	public void channelRestrict(Channel chan);
	public void channelStatus(Channel chan);
	public void channelError(Channel chan);
	public void connectionError(Connection conn);
}
