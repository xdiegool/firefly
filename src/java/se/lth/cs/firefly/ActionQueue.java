package se.lth.cs.firefly;

import se.lth.cs.firefly.util.Debug;

import java.util.*;
import java.io.*;
import java.net.SocketException;

/**
* This class is mainly ment to offload threads that listen awaits packages on connectionless protocols.
* As these threads need to available and listening as fast as possible after having received, another thread
* needs to handle the package after it's been received, which is what this class does. Most of the library 
* classes are not threadsafe and this thread is used to remove the need for synchronization. 
* It consists of both the queue where actions or events are placed and the thread that "does" the action.
*
* It also supports priority in the actions, as defined in the enum Priority.
*
* An interface Action is provided as well. Actions that should be performed by the thread implement that interface. 
*/
public class ActionQueue{
	private Queue<QueueAction> queue;
	private Object lock;
	private Thread thread;


	public ActionQueue(){	
		queue = new PriorityQueue<QueueAction>(20, new Comparator<QueueAction>() {
			@Override
			public int compare(QueueAction o1, QueueAction o2) {
				return o1.p.compareTo(o2.p);
			};
		});
		lock = new Object();
		thread = new Thread(new ActionAgent());
		thread.start();
	}
	public void queue(Priority p , Action a){
		synchronized(lock){
			queue.offer(new QueueAction(p,a));
			Debug.log("Queued action: " +  a);
			lock.notifyAll();
		 }
	}
	public void stop(){
		thread.interrupt();
	}
	private class ActionAgent implements Runnable {
		@Override
		public void run() {
			Debug.log("ActionAgent starting");
			while (!Thread.currentThread().isInterrupted() || !queueIsEmpty()) { //Finish all actions before exiting
				try {
					while(!queueIsEmpty()){
						Action a = poll();
						Debug.log("ActionAgent doing: " + a);
						a.doAction();
					}
					while(queueIsEmpty()){waitOnLock();}
				} catch (EOFException e) {
					// Happens when user defined types are done
					// sending. If this happens, we only want to
					// continue listening for packets
				} catch (InterruptedException e) {
					Thread.currentThread().interrupt();
				} catch (SocketException e) {
					Debug.printStackTrace(e);
					Thread.currentThread().interrupt();
				} catch (Exception e) {
					Debug.printStackTrace(e);
				}
			}
			Debug.log("ActionAgent stopping");
		}
		private boolean queueIsEmpty(){
			boolean b;
			synchronized(lock){
				b = queue.peek() == null;
			}
			return b;
		}
		private Action poll(){
			Action a;
			synchronized(lock){
				a = queue.poll().a;
			}
			return a;
		}
		private void waitOnLock() throws InterruptedException{
			synchronized(lock){
				lock.wait();
			}
		}
	}
	public enum Priority{
		HIGH_PRIOTITY(1), MED_PRIORITY(2), LOW_PRIORITY(3);
		private int i;
		private Priority(int i){ this.i = i;}
	}
	public interface Action{
		public void doAction() throws Exception;
	}
	private class QueueAction{
		private Action a;
		private Priority p;
		private QueueAction(Priority p, Action a){
			this.a = a;
			this.p = p; 
		}
	}
	
}
