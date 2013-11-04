class Debug {

	public static void err(String msg) {
		System.err.println(msg);
	}

	public static void die() {
		System.err.println("Terminating, bye!");
		System.exit(1);
	}
}
