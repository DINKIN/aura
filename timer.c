#include <stdbool.h>
#include <time.h>
#include <aura/aura.h>
#include <aura/timer.h>
#include <aura/eventloop.h>

void aura_timer_update(struct aura_timer *tm, aura_timer_cb_fn timer_cb_fn, void *arg)
{
	struct aura_eventloop *loop = aura_node_eventloop_get(tm->node);
	if (!loop)
		BUG(tm->node, "Internal bug: Node has no associated eventsystem");
	if (loop->module != tm->module)
		BUG(tm->node, "Please, call aura_eventloop_module_select() BEFORE anything else.");

	tm->callback = timer_cb_fn;
	tm->callback_arg = arg;
}

struct aura_timer *aura_timer_create(struct aura_node *node, aura_timer_cb_fn timer_cb_fn, void *arg)
{
	struct aura_eventloop *loop = aura_node_eventloop_get_autocreate(node);
	struct aura_timer *tm = calloc(1, loop->module->timer_size);
	if (!tm)
		BUG(node, "FATAL: Memory allocation failure");
	tm->module = loop->module; /* Store current eventloop module in the name of sanity */
	tm->callback = timer_cb_fn;
	tm->callback_arg = arg;
	tm->node = node;
	loop->module->timer_create(loop, tm);
	aura_timer_update(tm, timer_cb_fn, arg);
	return tm;
}

void aura_timer_start(struct aura_timer *tm, int flags, struct timeval *tv)
{
	struct aura_eventloop *loop = aura_node_eventloop_get(tm->node);
	if (tm->is_active) {
		slog(0, SLOG_WARN, "Tried to activate a timer that's already active. Stop it first.");
		return;
	}
	if (!loop)
		BUG(tm->node, "Internal bug: Node has no associated eventsystem");
	tm->flags = flags;
	tm->tv = *tv;
	tm->module->timer_start(loop, tm);
	tm->is_active = true;
}

void aura_timer_stop(struct aura_timer *timer)
{
	struct aura_eventloop *loop = aura_node_eventloop_get(timer->node);
	if (!loop)
		BUG(timer->node, "Internal bug: Node has no associated eventsystem");
	timer->module->timer_stop(loop, timer);
	timer->is_active = false;
}

void aura_timer_destroy(struct aura_timer *timer)
{
	struct aura_eventloop *loop = aura_node_eventloop_get(timer->node);
	if (!loop)
		BUG(timer->node, "Internal bug: Node has no associated eventsystem");
	if (timer->is_active)
		aura_timer_stop(timer);
	timer->module->timer_destroy(loop, timer);
	free(timer);
}
