#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <labcomm_fd_reader_writer.h>
#include <labcomm_mem_reader.h>
#include "gen/firefly_sample.h"

#define FILENAME "firefly_sample_encoded.txt"

#define FILENAME_FULL "firefly_full.enc"
#define FILENAME_SIGNATURE "firefly_sign.enc"
#define FILENAME_SAMPLE "firefly_sample.enc"

static void handle_ft_sample_data(firefly_sample_data  *samp_data, void *context)
{
	printf("Recieved ft_sample_data[]:\n");
	size_t i;
	for (i = 0; i < 6; ++i) {
		printf("ft_sample_data[%u] = %hi\n", i, samp_data->a[i]);
	}
}

static int test_mem(int argc, char *argv[]){
	struct labcomm_decoder *decoder;
	void  *context = NULL;
	labcomm_mem_reader_context_t rc;
	size_t file_len;
	FILE *file;
	unsigned char *buffer;

	char *sign_filename;
	char *sample_filename;
	char *full_filename;
	if (argc == 4) {
		sign_filename = argv[1];
		sample_filename = argv[2];
		full_filename = argv[3];
	} else {
		sign_filename = FILENAME_SIGNATURE;
		sample_filename = FILENAME_SAMPLE;
		full_filename = FILENAME_FULL;
	}
	/* Get data to send */
	file = fopen(sign_filename, "rb");
	//Get file length
	fseek(file, 0, SEEK_END);
	file_len = ftell(file);
	fseek(file, 0, SEEK_SET);

	//Allocate memory
	buffer = (unsigned char *)malloc(file_len);
	rc.enc_data = buffer;
	rc.size = file_len;

	//Read file contents into buffer
	fread(buffer, file_len, 1, file);
	fclose(file);

	decoder = labcomm_decoder_new(labcomm_mem_reader, &rc);
	labcomm_decoder_register_firefly_sample_data(decoder, handle_ft_sample_data, context);

	printf("Decoding from memory:\n");
	labcomm_decoder_decode_one(decoder);

	free(buffer);

	/* Get signature to send */
	file = fopen(sample_filename, "rb");
	//Get file length
	fseek(file, 0, SEEK_END);
	file_len = ftell(file);
	fseek(file, 0, SEEK_SET);

	//Allocate memory
	buffer = (unsigned char *)malloc(file_len);
	rc.enc_data = buffer;
	rc.size = file_len;

	//Read file contents into buffer
	fread(rc.enc_data, file_len, 1, file);
	fclose(file);
	labcomm_decoder_decode_one(decoder);

	free(buffer);

	/* Get signature to send */
	file = fopen(full_filename, "rb");
	//Get file length
	fseek(file, 0, SEEK_END);
	file_len = ftell(file);
	fseek(file, 0, SEEK_SET);

	//Allocate memory
	buffer = (unsigned char *)malloc(file_len);
	rc.enc_data = buffer;
	rc.size = file_len;

	//Read file contents into buffer
	fread(buffer, file_len, 1, file);
	fclose(file);

	labcomm_decoder_decode_one(decoder);
	free(buffer);

	printf("--- End Of File ---:\n");
	labcomm_decoder_free(decoder);

	return 0;
}

int test_fd(int argc, char *argv[]){
	int fd;
	struct labcomm_decoder *decoder;
	void  *context = NULL;

	char *sign_filename;
	char *sample_filename;
	char *full_filename;
	if (argc == 4) {
		sign_filename = argv[1];
		sample_filename = argv[2];
		full_filename = argv[3];
	} else {
		sign_filename = FILENAME_SIGNATURE;
		sample_filename = FILENAME_SAMPLE;
		full_filename = FILENAME_FULL;
	}

	printf("C decoder reading from %s and %s\n", sign_filename, sample_filename);
	fd = open(sign_filename, O_RDONLY);
	//fd = open(full_filename, O_RDONLY);
	decoder = labcomm_decoder_new(labcomm_fd_reader, &fd);
	if (!decoder) { 
		printf("Failed to allocate decoder %s:%d\n", __FUNCTION__, __LINE__);
		return 1;
	}

	labcomm_decoder_register_firefly_sample_data(decoder, handle_ft_sample_data, context);

	printf("Decoding from file:\n");
	labcomm_decoder_decode_one(decoder);
	close(fd);
	fd = open(sample_filename, O_RDONLY);
	labcomm_decoder_decode_one(decoder);
	close(fd);
	fd = open(sample_filename, O_RDONLY);
	labcomm_decoder_decode_one(decoder);
	close(fd);

	printf("--- End Of File ---:\n");
	labcomm_decoder_free(decoder);

	return 0;
}

int main(int argc, char *argv[])
{
	return test_mem(argc, argv);
	return test_fd(argc, argv);
}

