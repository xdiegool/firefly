package se.lth.cs.firefly.util;

public class Debug {
	private String className;

	public Debug(String className) {
		this.className = className;
	}

	public static void log(String msg) {
		System.out.println(msg);
	}

	public void logc(String msg) {
		System.out.println("DEBUG(" + className + "): " + msg);
	}

	public void errc(String msg) {
		System.err.println("ERROR(" + className + "): " + msg);
	}

	public static void errx(String msg) {
		System.err.println(msg);
		System.exit(1);
	}

	public static void warnx(String msg) {
		System.err.println(msg);
	}
}
