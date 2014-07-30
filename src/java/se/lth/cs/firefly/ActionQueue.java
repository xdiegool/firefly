package se.lth.cs.firefly;

import genproto.*;
import se.lth.control.labcomm.*;

import java.util.*;
import java.io.*;
import java.net.SocketException;


public class ActionQueue{
	private Queue<Action> queue;
	private Object lock;
	private Thread thread;
	private Connection conn;

	public ActionQueue(Connection conn){	
		this.conn = conn;
		queue = new LinkedList<Action>();
		lock = new Object();
		thread = new Thread(new ActionAgent());
		thread.start();
	}
	public void queue(Action a){
		synchronized(lock){
			queue.offer(a);
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
			while (!Thread.currentThread().isInterrupted() || !queueIsEmpty()) { //Finish all actions
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
					Thread.currentThread().interrupt(); // Socket
										// closed
				} catch (Exception e) {
					conn.exception(e);
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
				a = queue.poll();
			}
			return a;
		}
		private void waitOnLock() throws InterruptedException{
			synchronized(lock){
				lock.wait();
			}
		}
	}
	public interface Action{
		public void doAction() throws Exception;
	}
	
}