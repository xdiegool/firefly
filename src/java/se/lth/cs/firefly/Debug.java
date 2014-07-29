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
}
