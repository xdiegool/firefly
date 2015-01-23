package se.lth.cs.firefly.protocol;

import se.lth.cs.firefly.util.AppendableByteArrayInputStream;
import se.lth.cs.firefly.util.Debug;

import se.lth.control.labcomm.Constant;
import se.lth.control.labcomm.DecoderChannel;
import se.lth.control.labcomm.SampleDispatcher;
import se.lth.control.labcomm.SampleHandler;

import java.io.DataInputStream;
import java.io.IOException;

/**
 * Subclass of {@link se.lth.control.labcomm.DecoderChannel} to decode
 * data unpacked from a Firefly data_sample.
 */
class ChannelDecoder extends DecoderChannel {
	private AppendableByteArrayInputStream is;
	private Debug logger;
	private int numTypes;

	public ChannelDecoder() throws IOException {
		// Bypass version assert. TODO: What does this mean?
		this(new AppendableByteArrayInputStream());
		logger = new Debug(ChannelDecoder.class.getCanonicalName());
		this.numTypes = 0;
	}

	/*
	 * Private constructor which is needed to let us save the is parameter
	 * in this.is.
	 */
	private ChannelDecoder(AppendableByteArrayInputStream is) throws IOException {
		super(is);
		this.is = is;
	}

	@Override
	public void register(SampleDispatcher dispatcher, SampleHandler handler) throws IOException {
		super.register(dispatcher, handler);
		numTypes++;
	}

	public void decodeData(byte[] data) throws Exception {
		is.append(data);
		// is.reset();
		try {
			runOne();
			// runOne() succeeded since no exception, clear input stream for next data.
			is.clear();
		} catch (Exception e) {
			// TODO: Failed to parse something.
			logger.errc(e.toString() + e.getMessage());
		}
	}

	public int getNumDecoderTypes() {
		return numTypes;
	}
}
