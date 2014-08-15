package se.lth.cs.firefly.util;

import genproto.*;
import se.lth.control.labcomm.*;
import se.lth.cs.firefly.protocol.*;

import java.util.*;
import java.io.*;
import java.net.*;
/**
* Responsible for the resending of packets. It consists of two queues, one for data, i.e. user defined types, and one for channel messages. These queues 
* are synchronized on the same lock. It also houses the actual resender thread as a private class. The queues are fifo queues. 
* It is meant to be specific to each connection.
* TODO This is not needed for e.g. tcp. Make stub for protocols handling resending by themselves?
*/
public class ResendQueue {
	private LinkedHashMap<Integer, LabCommSample> channelQueue;
	private LinkedHashMap<Integer, data_sample> dataQueue;
	private Connection conn;
	private Object lock;
	private ConnectionEncoder encoder;
	private Thread res;

	public ResendQueue(ConnectionEncoder encoder, Connection conn) {
		channelQueue = new LinkedHashMap<Integer, LabCommSample>();
		dataQueue = new LinkedHashMap<Integer, data_sample>();
		this.conn = conn;
		this.encoder = encoder;
		lock = new Object();
		res = new Thread(new Resender());
		res.start();
	}

	public void queueData(int index, data_sample s) {
		Debug.log("Queueing data_sample: " + "@" + index);
		synchronized (lock) {
			dataQueue.put(index, s);
			lock.notifyAll();
		}
	}

	public void queueChannelMsg(int index, LabCommSample s) {
		Debug.log("Queueing: " + s.getClass().getSimpleName() + " @" + index);
		synchronized (lock) {
			channelQueue.put(index, s);
			lock.notifyAll();
		}

	}

	public void dequeueData(int index) {
		synchronized (lock) {
			data_sample s = dataQueue.remove(index);
			String printString = (s == null) ? "Already dequeued" : s
					.getClass().getSimpleName();
			Debug.log("Dequeueing: " + printString + " @" + index);
			lock.notifyAll();
		}
	}

	public void dequeueChannelMsg(int index) {
		synchronized (lock) {
			LabCommSample s = channelQueue.remove(index);
			String printString = (s == null) ? "Already dequeued" : s
					.getClass().getSimpleName();
			Debug.log("Dequeueing: " + printString + " @" + index);
			lock.notifyAll();
		}
	}

	public void stop() {
		res.interrupt();
	}

	private class Resender implements Runnable {
		private static final int sleepTime = 300;

		public void run() {
			Debug.log("ResendThread starting");
			while (!Thread.currentThread().isInterrupted()) {
				long t0 = System.currentTimeMillis();
				long t = t0;
				while ((sleepTime - (t - t0) > 0 || queuesAreEmpty())
						&& !Thread.currentThread().isInterrupted()) {
					try {
						waitOnLock(sleepTime - (t - t0));
					} catch (InterruptedException e) {
						Thread.currentThread().interrupt();
						break;
					} catch (Exception e) {
						conn.exception(e, "Error in ResendQueue");
						break;
					}
					t = queuesAreEmpty() ? t0 : System.currentTimeMillis();
				}
				synchronized (lock) {
					try {
						for (LabCommSample s : channelQueue.values()) {
							resendSample(s);
						}
						for (LabCommSample s : dataQueue.values()) {
							resendSample(s);
						}
					} catch (SocketException e) {
						Thread.currentThread().interrupt(); // Socket closed
						clearQueues();
					} catch (Exception e) {
						conn.exception(e, "Error in ResendQueue");
					}
				}
			}
			Debug.log("ResendThread stopping");
		}

		private void clearQueues() {
			Debug.log("Clearing Resend Queue");
			synchronized (lock) {
				dataQueue.clear();
				channelQueue.clear();
			}
			
		}

		private boolean queuesAreEmpty() {
			boolean b;
			synchronized (lock) {
				b = dataQueue.isEmpty() &&  channelQueue.isEmpty();
			}
			return b;
		}

		private void waitOnLock(long t) throws InterruptedException {
			synchronized (lock) {
				lock.wait(t);
			}
		}

		private void resendSample(LabCommSample s) throws IOException {
			// Ugly but working. TODO Fix this
			Debug.log("Resending " + s.getClass().getSimpleName());
			switch (s.getClass().getSimpleName()) {
			case "channel_request":
				channel_request.encode(encoder, (channel_request) s);
				break;
			case "channel_response":
				channel_response.encode(encoder, (channel_response) s);
				break;
			case "channel_ack":
				channel_ack.encode(encoder, (channel_ack) s);
				break;
			case "channel_close":
				channel_close.encode(encoder, (channel_close) s);
				break;
			case "channel_restrict_ack":
				channel_restrict_ack.encode(encoder, (channel_restrict_ack) s);
				break;
			case "channel_restrict_request":
				channel_restrict_request.encode(encoder,
						(channel_restrict_request) s);
				break;
			case "ack":
				ack.encode(encoder, (ack) s);
				break;
			case "data_sample":
				data_sample.encode(encoder, (data_sample) s);
				break;
			default:
				Debug.log("No type matched in resend sample");
			}
		}
	}
}
