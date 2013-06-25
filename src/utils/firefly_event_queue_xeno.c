#include <stdio.h>
#include <stdbool.h>
#include <signal.h>

// Xenomai includes
#include <native/task.h>
#include <native/mutex.h>
#include <native/sem.h>

#include <utils/firefly_event_queue_xeno.h>
#include <utils/firefly_event_queue.h>

struct firefly_event_queue_xeno_context {
	RT_MUTEX lock;
	RT_SEM event_count;
	RT_TASK event_loop;
	RTIME timeout;
	bool event_loop_stop;
};

int firefly_event_queue_xeno_add(struct firefly_event_queue *eq,
		unsigned char prio, firefly_event_execute_f execute, void *context);

struct firefly_event_queue *firefly_event_queue_xeno_new(size_t pool_size,
		int prio, RTIME timeout)
{
	int res;
	struct firefly_event_queue_xeno_context *ctx =
		malloc(sizeof(struct firefly_event_queue_xeno_context));
	res = rt_mutex_create(&ctx->lock, "event_queue_mutex");
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
	}
	res = rt_sem_create(&ctx->event_count, "event_queue_sem", 0, S_PRIO);
	if (res) {
		fprintf(stderr, "ERROR: init cond variable.\n");
	}
	res = rt_task_create(&ctx->event_loop, "event_queue_task", 0, prio,
			T_FPU|T_JOINABLE);
	if (res) {
		fprintf(stderr, "ERROR: Could not create event task.\n");
	}
	ctx->event_loop_stop = false;
	ctx->timeout = timeout;
	struct firefly_event_queue *eq =
		firefly_event_queue_new(firefly_event_queue_xeno_add, pool_size, ctx);
	return eq;
}

void firefly_event_queue_xeno_free(struct firefly_event_queue **eq)
{
	// make sure loop is stopped, free context and queue
	struct firefly_event_queue_xeno_context *ctx =
		firefly_event_queue_get_context(*eq);

	firefly_event_queue_xeno_stop(*eq);
	rt_mutex_delete(&ctx->lock);
	rt_sem_delete(&ctx->event_count);
	free(ctx);
	firefly_event_queue_free(eq);
}

int firefly_event_queue_xeno_add(struct firefly_event_queue *eq,
		unsigned char prio, firefly_event_execute_f execute, void *context)
{
	int res = 0;
	struct firefly_event_queue_xeno_context *ctx =
		(struct firefly_event_queue_xeno_context *)
		firefly_event_queue_get_context(eq);

	res = rt_mutex_acquire(&ctx->lock, ctx->timeout);
	if (res == -ETIMEDOUT) {
		// TODO error
		return -ETIMEDOUT;
	} else if (res) {
		fprintf(stderr, "Could not get event lock. ");
		switch (res) {
			case -EPERM:
				fprintf(stderr, "Incorrect context.\n");
				break;
			default:
				fprintf(stderr, "Unknown\n");
		}
		return res;
	}
	res = firefly_event_add(eq, prio, execute, context);
	if (!res) {
		rt_sem_v(&ctx->event_count);
	} else {
		fprintf(stderr, "Could not add event.\n");
	}
	rt_mutex_release(&ctx->lock);
	return res;
}

void firefly_event_xeno_thread_main(void *args)
{
	int err;
	struct firefly_event_queue *eq;
	struct firefly_event_queue_xeno_context *ctx;
	struct firefly_event *ev;
	int events_left;
	bool stop;
	bool pop;

	eq = args;
	ctx = firefly_event_queue_get_context(eq);
	ev = NULL;
	err = rt_mutex_acquire(&ctx->lock, ctx->timeout);
	if (err) {
		return;
	}
	events_left = firefly_event_queue_length(eq);
	stop = ctx->event_loop_stop;
	rt_mutex_release(&ctx->lock);
	while (!stop || events_left > 0) {
		pop = true;
		err = rt_sem_p(&ctx->event_count, TM_INFINITE);
		if (err == -EINTR) {
			pop = false;
		} else if (err) {
			// TODO error
			return;
		}
		err = rt_mutex_acquire(&ctx->lock, ctx->timeout);
		if (err == -EINTR) {
		} else if (err == -ETIMEDOUT) {
			// TODO timeout error
			return;
		} else if (err) {
			// TODO error
			return;
		} else if (pop) {
			ev = firefly_event_pop(eq);
			rt_mutex_release(&ctx->lock);
			if (ev != NULL) {
				firefly_event_execute(ev);
				err = rt_mutex_acquire(&ctx->lock, ctx->timeout);
				if (err == -EINTR) {
					continue;
				} else if (err == -ETIMEDOUT) {
					// TODO timeout error
					return;
				} else if (err) {
					// TODO error
					return;
				} else {
					firefly_event_return(eq, &ev);
					rt_mutex_release(&ctx->lock);
				}
			}
		}
		err = rt_mutex_acquire(&ctx->lock, ctx->timeout);
		if (err) {
			return;
		}
		events_left = firefly_event_queue_length(eq);
		stop = ctx->event_loop_stop;
		rt_mutex_release(&ctx->lock);
	}
	return;
}

int firefly_event_queue_xeno_run(struct firefly_event_queue *eq)
{
	int res = 0;
	struct firefly_event_queue_xeno_context *ctx =
		firefly_event_queue_get_context(eq);
	res = rt_task_start(
			&ctx->event_loop, firefly_event_xeno_thread_main, eq);
	return res;
}

int firefly_event_queue_xeno_stop(struct firefly_event_queue *eq)
{
	struct firefly_event_queue_xeno_context *ctx =
		firefly_event_queue_get_context(eq);
	ctx->event_loop_stop = true;
	rt_task_unblock(&ctx->event_loop);
	int res = rt_task_join(&ctx->event_loop);
	return res;
}

RT_TASK *firefly_event_queue_xeno_loop(struct firefly_event_queue *eq)
{
	struct firefly_event_queue_xeno_context *ctx =
		firefly_event_queue_get_context(eq);
	return &ctx->event_loop;
}
