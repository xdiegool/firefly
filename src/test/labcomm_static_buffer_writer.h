#ifndef _LABCOMM_DYNAMIC_BUFFER_READER_WRITER_H_
#define _LABCOMM_DYNAMIC_BUFFER_READER_WRITER_H_

#include "labcomm.h"

#define LABCOMM_IOCTL_WRITER_GET_BUFFER \
  LABCOMM_IOR('m',0,0)

#define LABCOMM_IOCTL_WRITER_RESET_BUFFER LABCOMM_IOW('r',0,0)

struct labcomm_writer *labcomm_static_buffer_writer_new();

#endif
