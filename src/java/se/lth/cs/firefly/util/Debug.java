package se.lth.cs.firefly.util;

import java.util.Date;
import java.io.*;
import java.text.*;

public class Debug {
	public static boolean debug = true;
	public static synchronized void log(String msg) {
		if(debug){
			DateFormat dateFormat = new SimpleDateFormat("HH:mm:ss:SS");
			Date date = new Date();
			String time = dateFormat.format(date);
			System.out.println(time + " : " + Thread.currentThread().getId() + ":" + msg);
		}
	}

	public static synchronized void errx(String msg) {
		Debug.warnx(msg);
		System.exit(1);
	}

	public static synchronized void warnx(String msg) {
		System.err.println(msg);
	}
	public static String byteArrayToString(byte[] data, int len){
		String s = "";
		for (int i  = 0; i< len; i++){
			s+=data[i] + ",";
		}
		s = "[" +s.substring(0, s.length()-1) + "]";
		return s;
	}
	public static void printStackTrace(Exception e){
		StringWriter sw = new StringWriter();
		e.printStackTrace(new PrintWriter(sw));
		String exceptionAsString = sw.toString();
		Debug.log(exceptionAsString);
	}
}
