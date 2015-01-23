package example;

import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.util.Debug;

import java.io.IOException;

public abstract class AbstractPingPong implements Runnable {
	public static final int PING_DATA = 1111;
	public static final int PONG_DATA = 2222;

	/**
	 * The channel used to communicate with Pong.
	 */
	protected Channel chan;
	protected boolean dataReceived;
	protected String[] TEST_NAMES;
	protected boolean[] testResults;

	protected abstract void internalRun() throws InterruptedException, IOException;

	protected synchronized void waitForRestrict() throws InterruptedException {
		while (chan == null || !chan.isRestricted()) {
			wait();
		}
	}

	protected synchronized void waitForData() throws InterruptedException {
		while (!dataReceived){
			wait();
		}
	}

	private void printTestResults() {
		System.out.println("====== Ping test results =======");
		int numFailes = 0;
		for (int i = 0; i < testResults.length; i++) {
			System.out.print(String.format("Phase %d: %s...", i, TEST_NAMES[i]));
			if (!testResults[i]) {
				System.out.println("failed!");
				numFailes++;
			} else {
				System.out.println("passed");
			}
		}

		System.out.println(String.format("Summary: %d/%d succeeded. %d failures",
										 testResults.length - numFailes, testResults.length,
										 numFailes));
	}


	@Override
	public final void run() {
		try {
			internalRun();
			printTestResults();
		} catch (InterruptedException e) {
			Debug.errx("Interrupted: " + e);
		} catch (IOException e) {
			Debug.errx("IO broke: " + e);
		}
	}
}
