package se.lth.cs.firefly.protocol;

import se.lth.control.labcomm.Decoder;
import se.lth.control.labcomm.Encoder;
import se.lth.control.labcomm.Sample;
import se.lth.control.labcomm.SampleHandler;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * A list of LabComm types to register on a {@link Channel}.
 *
 * <p>The class keeps two lists internally, one for types to register on
 * the {@link se.lth.control.labcomm.Encoder} and one for the types (and
 * LabComm Handlers) to register on the
 * {@link se.lth.control.labcomm.Decoder}.
 *
 * <p>The class is used in two different ways, either when opening a
 * new {@link Channel}
 * (using
 * {@link se.lth.cs.firefly.transport.Connection#openChannel(TypeList) openChannel})
 * or when receiving one (in the
 * {@link se.lth.cs.firefly.FireflyApplication#channelAccept(Channel) channelAccept}
 * callback).</p>
 */
public class TypeList {
	private List<Sample> encoderTypes;
	private List<DecoderEntry> decoderTypes;

	public TypeList() {
		encoderTypes = new ArrayList<Sample>();
		decoderTypes = new ArrayList<DecoderEntry>();
	}

	/**
	 * Adds a new type to register on the Encoder.
	 */
	public void addEncoderType(Sample sample) {
		encoderTypes.add(sample);
	}

	/**
	 * Adds a new type and handler to register on the Decoder.
	 */
	public void addDecoderType(Sample sample, SampleHandler handler) {
		decoderTypes.add(new DecoderEntry(sample, handler));
	}

	/**
	 * Registers the added types to the provided Decoder. 
	 *
	 * @throws IOException if the registration fails
	 */
	public void register(Decoder decoder) throws IOException {
		for (DecoderEntry entry : decoderTypes) {
			decoder.register(entry.sample.getDispatcher(), entry.handler);
		}
	}

	/**
	 * Registers the added types to the provided Encoder. 
	 *
	 * @throws IOException if the registration fails
	 */
	public void register(Encoder encoder) throws IOException {
		for (Sample sample : encoderTypes) {
			encoder.register(sample.getDispatcher());
		}
	}

	/**
	 * Returns the number of encoder types in this TypeList.
	 *
	 * @return The number of encoder types in this TypeList.
	 */
	public int numEncoderTypes() {
		return encoderTypes.size();
	}

	/**
	 * Returns the number of decoder types in this TypeList.
	 *
	 * @return The number of decoder types in this TypeList.
	 */
	public int numDecoderTypes() {
		return decoderTypes.size();
	}

	/**
	 * Private helper class that holds a sample and its handler that is
	 * to be registered on the decoder.
	 */
	private class DecoderEntry {
		public Sample sample;
		public SampleHandler handler;

		public DecoderEntry(Sample sample, SampleHandler handler) {
			this.sample = sample;
			this.handler = handler;
		}
	}
}
