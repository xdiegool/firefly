package se.lth.cs.firefly;

public class Debug {
	public static synchronized void log(String msg) {
		System.out.println(msg);
	}

	public static synchronized void errx(String msg) {
		System.err.println(msg);
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
}
