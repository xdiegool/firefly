#ifndef _LABCOMM_STATIC_BUFFER__READER_H_
#define _LABCOMM_STATIC_BUFFER__READER_H_

#include "labcomm.h"

#define LABCOMM_IOCTL_READER_SET_BUFFER \
  LABCOMM_IOWN('m',0,2)
#define LABCOMM_IOCTL_READER_SET_MEM_FUNC \
	LABCOMM_IOWN('m',1,2)

struct labcomm_reader *labcomm_static_buffer_reader_new(struct labcomm_memory *m);

struct labcomm_reader *labcomm_static_buffer_reader_mem_new(void *context,
		void *(*m)(void *c, size_t s),
		void (*f)(void *c,void *p));

#endif
