package se.lth.cs.firefly;

public interface FireflyApplication {
	// Reduce.
	// Want blocking ops?
	public boolean channelAccept(Connection connection);
	public void channelOpened();
	public void channelClosed(Channel chan);
	public void channelRestrict();
	public void channelStatus();
	public void channelError();

	public void connectionError();


}
