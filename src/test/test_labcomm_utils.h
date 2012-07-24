#include "labcomm.h"

#define DATA_FILE ("data.enc")	// File where the encoded data can be written.
#define SIG_FILE ("sig.enc")	// File where the encoded data can be written.

size_t read_file_to_mem(unsigned char **data, char *file_name);

void handle_labcomm_error(enum labcomm_error error_id, size_t nbr_va_args, ...);

typedef void (* lc_register_f)(struct labcomm_encoder *enc);
typedef void (* lc_encode_f)(struct labcomm_encoder *enc, void *data);
void create_labcomm_files_general(lc_register_f reg, lc_encode_f enc, void *data);
