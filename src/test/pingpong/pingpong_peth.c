#include <transport/firefly_transport_eth_posix.h>

void *reader_thread_main(void *args)
{
	struct firefly_transport_llp *llp = (struct firefly_transport_llp *) args;
	// TODO add something here to make it stop or something
	while (1) {
		firefly_transport_eth_posix_read(llp);
	}

}
