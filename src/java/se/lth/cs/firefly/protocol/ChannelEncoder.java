package se.lth.cs.firefly.protocol;

import se.lth.cs.firefly.util.Debug;

import se.lth.control.labcomm.Constant;
import se.lth.control.labcomm.EncoderChannel;
import se.lth.control.labcomm.Sample;
import se.lth.control.labcomm.Writer;

import java.io.DataInputStream;
import java.io.IOException;

/**
 * Subclass of {@link se.lth.control.labcomm.EncoderChannel} for a
 * Firefly {@link Channel}. It keeps track of when a LabComm sample is
 * done encoding (since the write() method of a
 * {@link se.lth.control.labcomm.Writer} can be called several times
 * while encoding one sample) so we know when we should send the encoded
 * bytes in a Firefly data sample.
 */
class ChannelEncoder extends EncoderChannel {
	private Channel channel;

	public ChannelEncoder(Channel channel) throws IOException {
		super(new ChannelWriter(channel));
		this.channel = channel;
	}

	@Override
	public void end(Class<? extends Sample> c) throws IOException {
		super.end(c);
		((ChannelWriter) writer).end(c != null);
	}

	private static class ChannelWriter implements Writer {
		private Channel channel;
		private byte[] data;
		private Debug logger;

		public ChannelWriter(Channel channel) {
			this.channel = channel;
			data = new byte[0];
			logger = new Debug(ChannelWriter.class.getCanonicalName());
		}

		@Override
		public void write(byte[] data) {
			byte[] tmp = new byte[this.data.length + data.length];
			System.arraycopy(this.data, 0, tmp, 0, this.data.length);
			System.arraycopy(data, 0, tmp, this.data.length, data.length);
			this.data = tmp;
		}

		/**
		 * Ends encoding of the current sample.
		 *
		 * @param isData Whether the encoding is of data of not.
		 */
		public void end(boolean isData) {
			// logger.logc("ending writer: " + String.format("0x%x...", data[0]));
			try {
				/* Only want to send if channel is open or if the encoded data is a signature. */
				if (channel.isOpen() || !isData) {
					channel.getConnection().sendData(channel, data);
					this.data = new byte[0];
				} else {
					logger.errc("Attempting to write on closed channel.");
				}
			} catch (IOException e) {
				logger.errc(e.toString());
			}
		}
	}
}
