#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>

#include <stdio.h>
#include <stdbool.h>
#include <utils/firefly_event_queue_posix.h>
#include <utils/firefly_event_queue.h>

struct firefly_event_queue_posix_context {
	pthread_mutex_t lock;
	pthread_cond_t signal;
	pthread_t event_loop;
	bool event_loop_stop;
};

int64_t firefly_event_queue_posix_add(struct firefly_event_queue *eq,
		unsigned char prio, firefly_event_execute_f execute, void *context,
		unsigned int nbr_deps, const int64_t *deps);

struct firefly_event_queue *firefly_event_queue_posix_new(size_t pool_size)
{
	int res;
	struct firefly_event_queue_posix_context *ctx =
		malloc(sizeof(struct firefly_event_queue_posix_context));
	res = pthread_mutex_init(&ctx->lock, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
	}
	res = pthread_cond_init(&ctx->signal, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init cond variable.\n");
	}
	ctx->event_loop_stop = false;
	struct firefly_event_queue *eq =
		firefly_event_queue_new(firefly_event_queue_posix_add, pool_size, ctx);
	return eq;
}

void firefly_event_queue_posix_free(struct firefly_event_queue **eq)
{
	// make sure loop is stopped, free context and queue
	struct firefly_event_queue_posix_context *ctx =
		firefly_event_queue_get_context(*eq);

	firefly_event_queue_posix_stop(*eq);
	pthread_mutex_destroy(&ctx->lock);
	pthread_cond_destroy(&ctx->signal);
	free(ctx);
	firefly_event_queue_free(eq);
}

int64_t firefly_event_queue_posix_add(struct firefly_event_queue *eq,
		unsigned char prio, firefly_event_execute_f execute, void *context,
		unsigned int nbr_deps, const int64_t *deps)
{
	int res = 0;
	struct firefly_event_queue_posix_context *ctx =
		(struct firefly_event_queue_posix_context *)
		firefly_event_queue_get_context(eq);

	res = pthread_mutex_lock(&ctx->lock);
	if (res) {
		return res;
	}
	res = firefly_event_add(eq, prio, execute, context, nbr_deps, deps);
	if (res > 0) {
		pthread_cond_signal(&ctx->signal);
	}
	pthread_mutex_unlock(&ctx->lock);
	return res;
}

void *firefly_event_posix_thread_main(void *args)
{
	struct firefly_event_queue *eq =
		(struct firefly_event_queue *) args;
	struct firefly_event_queue_posix_context *ctx =
		firefly_event_queue_get_context(eq);
	struct firefly_event *ev = NULL;
	pthread_mutex_lock(&ctx->lock);
	int event_left = firefly_event_queue_length(eq);
	bool finish = ctx->event_loop_stop;
	pthread_mutex_unlock(&ctx->lock);

	while (!finish || event_left > 0) {
		pthread_mutex_lock(&ctx->lock);
		event_left = firefly_event_queue_length(eq);
		finish = ctx->event_loop_stop;
		while (event_left <= 0 && !finish) {
			pthread_cond_wait(&ctx->signal, &ctx->lock);
			finish = ctx->event_loop_stop;
			event_left = firefly_event_queue_length(eq);
		}
		ev = firefly_event_pop(eq);
		pthread_mutex_unlock(&ctx->lock);
		if (ev != NULL) {
			firefly_event_execute(ev);
			pthread_mutex_lock(&ctx->lock);
			firefly_event_return(eq, &ev);
			pthread_mutex_unlock(&ctx->lock);
		}
	}
	return NULL;
}

int firefly_event_queue_posix_run(struct firefly_event_queue *eq,
		pthread_attr_t *attr)
{
	int res = 0;
	struct firefly_event_queue_posix_context *ctx =
		firefly_event_queue_get_context(eq);
	res = pthread_create(
			&ctx->event_loop, attr, firefly_event_posix_thread_main, eq);
	return res;
}

int firefly_event_queue_posix_stop(struct firefly_event_queue *eq)
{
	struct firefly_event_queue_posix_context *ctx =
		firefly_event_queue_get_context(eq);
	pthread_mutex_lock(&ctx->lock);
	ctx->event_loop_stop = true;
	pthread_cond_signal(&ctx->signal);
	pthread_mutex_unlock(&ctx->lock);
	int res = pthread_join(ctx->event_loop, NULL);
	return res;
}
