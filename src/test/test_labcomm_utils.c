#include "test/test_labcomm_utils.h"

#include <unistd.h>
#include <fcntl.h>

#include <CUnit/Basic.h>
#include <labcomm_fd_reader_writer.h>

#include "utils/firefly_errors.h"
#include "utils/cppmacros.h"

size_t read_file_to_mem(unsigned char **data, char *file_name)
{
	FILE *file = fopen(file_name, "rb");
	if (file == NULL) {
		CU_FAIL("Could not open file.\n");
	}

	int res = fseek(file, 0, SEEK_END);
	if (res != 0) {
		CU_FAIL("Could not seek file.\n");
	}
	long file_len = ftell(file);
	if (file_len == -1L) {
		perror("Ftell fcked.\n");
	}
	res = fseek(file, 0L, SEEK_SET);
	if (res != 0) {
		CU_FAIL("Could not seek file to begin.\n");
	}

	*data = calloc(1, file_len);
	if (*data == NULL) {
		CU_FAIL("Could not alloc filebuf.\n");
	}

	size_t units_read = fread(*data, file_len, 1, file);
	if (units_read != 1) {
		CU_FAIL("Did not read the whole file.\n");
	}

	fclose(file);
	return file_len;
}

void handle_labcomm_error(enum labcomm_error error_id, size_t nbr_va_args, ...)
{
	UNUSED_VAR(nbr_va_args);
	const char *err_msg = labcomm_error_get_str(error_id);
	if (err_msg == NULL) {
		err_msg = "Error with an unknown error ID occured.";
	}
	firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);

	CU_FAIL_FATAL("Labcomm error occured!\n");
}

void create_labcomm_files_general(lc_register_f reg, lc_encode_f enc, void *data)
{
	int tmpfd = open(SIG_FILE, O_RDWR|O_CREAT|O_TRUNC, 0644);
	struct labcomm_encoder *fd_encoder =
			labcomm_encoder_new(labcomm_fd_writer, &tmpfd);
	reg(fd_encoder);
	close(tmpfd);
	tmpfd = open(DATA_FILE, O_RDWR|O_CREAT|O_TRUNC, 0644);
	enc(fd_encoder, data);
	close(tmpfd);
	labcomm_encoder_free(fd_encoder);
}

void create_lc_files_name(lc_register_f reg, lc_encode_f enc, void *data,
		char *name)
{
	char *sig_name = malloc(strlen(name) + 19);
	strcpy(sig_name, "testfiles/");
	strcat(sig_name, name);
	strcat(sig_name, "_sig.enc");
	char *data_name = malloc(strlen(name) + 20);
	strcpy(data_name, "testfiles/");
	strcat(data_name, name);
	strcat(data_name, "_data.enc");
	int tmpfd = open(sig_name, O_RDWR|O_CREAT|O_TRUNC, 0644);
	struct labcomm_encoder *fd_encoder =
			labcomm_encoder_new(labcomm_fd_writer, &tmpfd);
	reg(fd_encoder);
	close(tmpfd);
	tmpfd = open(data_name, O_RDWR|O_CREAT|O_TRUNC, 0644);
	enc(fd_encoder, data);
	close(tmpfd);
	labcomm_encoder_free(fd_encoder);
}
