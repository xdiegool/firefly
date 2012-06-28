#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <labcomm_fd_reader_writer.h>
#include "gen/firefly_sample.h"

#define FILENAME_FULL "firefly_full.enc"
#define FILENAME_SIGNATURE "firefly_sign.enc"
#define FILENAME_SAMPLE "firefly_sample.enc"

int main(int argc, char *argv[])
{
	int fd;
	struct labcomm_encoder *encoder;
	char *filename;

	firefly_sample_data samp_data;
	size_t i;

	for (i = 0; i < 6; ++i) {
		samp_data.a[i] = i;
	}

	if (argc >= 2)
		filename = argv[1];
	else
		filename = FILENAME_SIGNATURE;
	printf("C encoder writing signature to %s\n", filename);
	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	encoder = labcomm_encoder_new(labcomm_fd_writer, &fd);
	labcomm_encoder_register_firefly_sample_data(encoder);
	
	close(fd);
	if (argc >= 3)
		filename = argv[2];
	else
		filename = FILENAME_SAMPLE;

	printf("C encoder writing encoded data to %s\n", filename);
	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	encoder = labcomm_encoder_new(labcomm_fd_writer, &fd);
	labcomm_encoder_register_firefly_sample_data(encoder);
	close(fd);
	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	labcomm_encode_firefly_sample_data(encoder, &samp_data);
	close(fd);

	if (argc >= 4)
		filename = argv[3];
	else
		filename = FILENAME_FULL;

	printf("C encoder writing encoded message to %s\n", filename);
	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	encoder = labcomm_encoder_new(labcomm_fd_writer, &fd);
	labcomm_encoder_register_firefly_sample_data(encoder);
	labcomm_encode_firefly_sample_data(encoder, &samp_data);
	close(fd);

	return 0;
}
