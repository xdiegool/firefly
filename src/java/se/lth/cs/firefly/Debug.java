package se.lth.cs.firefly;

class Debug {
	public static void log(String msg) {
		System.out.println(msg);
	}

	public static void err(String msg) {
		System.err.println(msg);
	}

	public static void die() {
		System.err.println("Terminating, bye!");
		System.exit(1);
	}
}
