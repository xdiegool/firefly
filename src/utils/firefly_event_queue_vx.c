#define _VX_C_SOURCE (200112L)

#include <semLib.h>
#include <taskLib.h>
#include <stdio.h>
#include <utils/firefly_event_queue_vx.h>
#include <utils/firefly_event_queue.h>
#include <utils/firefly_event_queue_private.h>

struct firefly_event_queue_vx_context {
	SEM_ID lock;
	SEM_ID signal;				/* New items in queue. */
	SEM_ID dead;
	int tid_event_loop;
	bool event_loop_stop;
};

int64_t firefly_event_queue_vx_add(struct firefly_event_queue *eq,
		unsigned char prio, firefly_event_execute_f execute, void *context,
		unsigned int nbr_deps, const int64_t *deps);

struct firefly_event_queue *firefly_event_queue_vx_new(size_t pool_size)
{
	struct firefly_event_queue_vx_context *ctx = NULL;
	struct firefly_event_queue *eq = NULL;

	if ((ctx = calloc(1, sizeof(*ctx)))) {
		ctx->lock = semMCreate(SEM_INVERSION_SAFE | SEM_Q_PRIORITY);
		if (!ctx->lock) {
			printf("fail to create msem: ");
			switch (errno) {
			case S_semLib_INVALID_OPTION:
				printf("invalid option");
				break;
			case S_memLib_NOT_ENOUGH_MEMORY:
				printf("no mem");
				break;
			default:
				printf("other");
			}
			printf("\n");
			goto fail;
		}
		ctx->signal = semCCreate(0, 0);
		if (!ctx->signal) {
			printf("fail to create csem.\n");
			goto fail;
		}
		ctx->dead = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
		if (!ctx->dead) {
			printf("fail to create bsem.\n");
			goto fail;
		}
		ctx->event_loop_stop = 0;
		eq = firefly_event_queue_new(firefly_event_queue_vx_add, pool_size, ctx);
		if (!eq)
			goto fail;
	}

	return eq;
 fail:
	if (ctx->lock)
		semDelete(ctx->lock);
	if (ctx->signal)
		semDelete(ctx->signal);
	if (ctx->dead)
		semDelete(ctx->dead);
	free(ctx);
	if (eq)
		firefly_event_queue_vx_free(&eq);
	return NULL;
}

void firefly_event_queue_vx_free(struct firefly_event_queue **eq)
{
	struct firefly_event_queue_vx_context *ctx;

	ctx = firefly_event_queue_get_context(*eq);

	/* firefly_event_queue_vx_stop(*eq); */
	semDelete(ctx->lock);
	semDelete(ctx->signal);
	free(ctx);
	firefly_event_queue_free(eq);
}

int64_t firefly_event_queue_vx_add(struct firefly_event_queue *eq,
		unsigned char prio, firefly_event_execute_f execute, void *context,
		unsigned int nbr_deps, const int64_t *deps)
{
	int res = 0;
	struct firefly_event_queue_vx_context *ctx;

	ctx = firefly_event_queue_get_context(eq);
	semTake(ctx->lock, WAIT_FOREVER);
	res = firefly_event_add(eq, prio, execute, context, nbr_deps, deps);
	semGive(ctx->lock);
	if (res > 0) {
		semGive(ctx->signal);
	}

	return res;
}

void *firefly_event_vx_thread_main(void *args)
{
	struct firefly_event_queue *eq;
	struct firefly_event_queue_vx_context *ctx;
	struct firefly_event *ev = NULL;
	int event_left;
	bool finish;

	eq = args;
	ctx = firefly_event_queue_get_context(eq);
	semTake(ctx->lock, WAIT_FOREVER);
	event_left = firefly_event_queue_length(eq);
	finish = ctx->event_loop_stop;
	semGive(ctx->lock);

	while (!finish || event_left > 0) {
		/* TODO: Clean up. Posix version was weird. */
		semTake(ctx->lock, WAIT_FOREVER);
		event_left = firefly_event_queue_length(eq);
		finish = ctx->event_loop_stop;
		while (event_left <= 0 && !finish) {
			semGive(ctx->lock);
			semTake(ctx->signal, WAIT_FOREVER);
			semTake(ctx->lock, WAIT_FOREVER);
			finish = ctx->event_loop_stop;
			event_left = firefly_event_queue_length(eq);
		}
		ev = firefly_event_pop(eq);
		semGive(ctx->lock);
		if (ev) {
			/* TODO: Retval can indicate badly contructed event, or 
			 * failed execution. Should this be handled?
			 */
			firefly_event_execute(ev);
			semTake(ctx->lock, WAIT_FOREVER);
			firefly_event_return(eq, &ev);
			semGive(ctx->lock);
		}
	}

	/* semTake(ctx->lock, WAIT_FOREVER); */
	semGive(ctx->dead);
	/* semGive(ctx->lock); */
	printf("EVENT THREAD TERMINATING\n");

	return NULL;
}

int firefly_event_queue_vx_run(struct firefly_event_queue *eq)
{
	int res;
	struct firefly_event_queue_vx_context *ctx;

	ctx = firefly_event_queue_get_context(eq);
	res = taskSpawn("ff_event_task", 254, 0, 20000,
					(FUNCPTR)firefly_event_vx_thread_main,
					(int) eq, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (res != ERROR) {
		ctx->tid_event_loop = res;
		return 0;
	}

	return -1;
}

int firefly_event_queue_vx_stop(struct firefly_event_queue *eq)
{
	struct firefly_event_queue_vx_context *ctx;
	int ret;

	ctx = firefly_event_queue_get_context(eq);

	semTake(ctx->lock, WAIT_FOREVER);
	ctx->event_loop_stop = true;
	semGive(ctx->lock);
	semGive(ctx->signal);	/* Signal ev. task to die. */

	printf("eq not done yet...\n");
	semTake(ctx->dead, WAIT_FOREVER);
	printf("eq done!\n");
	ret = taskDelete(ctx->tid_event_loop);
	if (ret == ERROR) {
		printf("Failed to delete eq task.\n");
		return -1;
	}

	return 0;
}
