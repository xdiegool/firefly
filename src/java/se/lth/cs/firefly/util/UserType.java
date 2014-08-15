package se.lth.cs.firefly.util;

/**
 * Possibly to be as parameter for open auto restrict method in connection.
 */
public interface UserType {
	
	/**
	 * 
	 * @return the number of types that should be restricted
	 */
	public int getNbrOfTypes();
	/**
	 * Registers all the handlers,encoders and decoders.
	 */
	public void registerAll();

}
