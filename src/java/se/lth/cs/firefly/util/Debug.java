package se.lth.cs.firefly.util;

public class Debug {
	public static void log(String msg) {
		System.out.println(msg);
	}

	public static void errx(String msg) {
		System.err.println(msg);
		System.exit(1);
	}

	public static void warnx(String msg) {
		System.err.println(msg);
	}
}
