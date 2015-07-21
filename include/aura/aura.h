#ifndef AURA_H
#define AURA_H

#define _GNU_SOURCE

#include <search.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <aura/slog.h>
#include <search.h>
#include <errno.h> 
#include <stdbool.h>

#include "list.h"
#include "format.h"
#include "endian.h"

enum aura_node_status {
	AURA_STATUS_OFFLINE,
	AURA_STATUS_ONLINE,
};

enum aura_call_status { 
	AURA_CALL_COMPLETED,
	AURA_CALL_TIMEOUT,
	AURA_CALL_TRANSPORT_FAIL,
};

enum aura_fd_action { 
	AURA_FD_ADDED,
	AURA_FD_REMOVED
};


struct aura_pollfds { 
	struct aura_node *node; 
	int fd;
	short events;
};

struct aura_node { 
	const struct aura_transport    *tr;
	struct aura_export_table *tbl;
	void                     *transport_data;
	void                     *user_data;
	enum aura_node_status     status;
	struct list_head          outbound_buffers;
	struct list_head          inbound_buffers;
	/* Synchronos calls put their stuff here */
	bool sync_call_running;
	bool need_endian_swap;
	struct aura_buffer *sync_ret_buf; 
	int sync_call_result;

	/* General callbacks */
	void *status_changed_arg;
	void (*status_changed_cb)(struct aura_node *node, int newstatus, void *arg);
	void *etable_changed_arg;
	void (*etable_changed_cb)(struct aura_node *node, void *arg);
	void *fd_changed_arg;
	void (*fd_changed_cb)(const struct aura_pollfds *fd, 
			      enum aura_fd_action act, void *arg);

	/* Event system and polling */
	int numfds; /* Currently available space for descriptors */
	int nextfd; /* Next descriptor to add */
	struct aura_pollfds *fds; /* descriptor and event array */

	void *eventsys_data; /* eventloop structure */
	unsigned int poll_timeout; 
	uint64_t last_checked;
	struct list_head eventloop_node_list;
};

struct aura_buffer {
	int               size;
	int               pos; 
	void             *userdata;
	struct list_head  qentry;
	char              data[];
};

struct aura_object { 
	int id;
	char *name;
	char *arg_fmt;
	char *ret_fmt;

	int valid; 
	char* arg_pprinted;
	char* ret_pprinted;
	int num_args; 
	int num_rets; 

	int arglen;
	int retlen;

	/* Store callbacks here. 
	   TODO: Can there be several calls of the same method pending? */
	int pending;
	void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg);
	void *arg;
};

struct aura_eventloop { 
	int poll_timeout;
	struct list_head nodelist;
	void *eventsysdata;
};


#define object_is_event(o)  (o->arg_fmt==NULL)
#define object_is_method(o) (o->arg_fmt!=NULL)

struct aura_export_table {
	int size;
	int next;
	struct aura_node *owner;
	struct hsearch_data index; 
	struct aura_object objects[];
};


#define __BIT(n) (1<< n)

struct aura_transport
{
	const char *name;
	uint32_t flags; 
	struct list_head registry;
	
	/* Additional bytes to allocate in buffers */
	int      buffer_overhead;
	/* Offset of the serialized data */
	int      buffer_offset;

	int    (*open)(struct aura_node *node, va_list ap);
	void   (*close)(struct aura_node *node);
	void   (*loop)(struct aura_node *node, const struct aura_pollfds *fd);
	
	/* Pointer ops, optional */
	void               *(*ptr_pull)(struct aura_buffer *buf);
	void                (*ptr_push)(struct aura_buffer *buf, void *ptr);

	/* Buffer ops. Implement these if you want ta take alloc in your hands or just adjust sizes*/
	struct aura_buffer *(*buffer_request)(struct aura_node *node, int size);
	void                (*buffer_release)(struct aura_node *node, struct aura_buffer *buf);

	int usage;
};


/* Serdes API */

struct aura_buffer *aura_serialize(struct aura_node *node, const char *fmt, va_list ap);
int  aura_fmt_len(struct aura_node *node, const char *fmt);
char* aura_fmt_pretty_print(const char* fmt, int *valid, int *num_args);

/* Transport Plugins API */
void aura_transport_register(struct aura_transport *tr);
void aura_transport_dump_usage();
void aura_transport_release(const struct aura_transport *tr);

static inline void  aura_set_transportdata(struct aura_node *node, void *udata)
{
	node->transport_data = udata;
}

static inline void *aura_get_transportdata(struct aura_node *node)
{
	return node->transport_data;
}

static inline void  aura_set_userdata(struct aura_node *node, void *udata)
{
	node->user_data = udata;
}

static inline void *aura_get_userdata(struct aura_node *node)
{
	return node->user_data;
}

const struct aura_transport *aura_transport_lookup(const char *name);
void aura_set_status(struct aura_node *node, int status);

#define AURA_TRANSPORT(s)					   \
	void __attribute__((constructor (101))) do_reg_##s(void) { \
		aura_transport_register(&s);			   \
	}

enum aura_endianness aura_get_host_endianness();
void aura_hexdump (char *desc, void *addr, int len);
void aura_set_node_endian(struct aura_node *node, enum aura_endianness en);

void aura_queue_buffer(struct list_head *list, struct aura_buffer *buf);
struct aura_buffer *aura_dequeue_buffer(struct list_head *head);
void aura_requeue_buffer(struct list_head *head, struct aura_buffer *buf);


/* AURA export table API */ 
struct aura_export_table *aura_etable_create(struct aura_node *owner, int n);
void aura_etable_add(struct aura_export_table *tbl, 
		    const char *name, 
		    const char *argfmt, 
		     const char *retfmt);
void aura_etable_activate(struct aura_export_table *tbl);
struct aura_object *aura_etable_find(struct aura_export_table *tbl, 
				     const char *name);
struct aura_object *aura_etable_find_id(struct aura_export_table *tbl, 
					int id);
void aura_etable_destroy(struct aura_export_table *tbl);

/* Aura core API */ 

struct aura_node *aura_open(const char* name, ...); 
void aura_close(struct aura_node *dev); 


static inline int aura_get_status(struct aura_node *node) 
{
	return node->status;
}


int aura_queue_call(struct aura_node *node, 
		    int id,
		    void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg),
		    void *arg,
		    struct aura_buffer *buf);

int aura_start_call_raw(
	struct aura_node *dev, 
	int id,
	void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg),
	void *arg,
	...);

int aura_start_call(
	struct aura_node *dev, 
	const char *name,
	void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg),
	void *arg,
	...);

int aura_call_raw(
	struct aura_node *dev, 
	int id,
	struct aura_buffer **ret,
	...);

int aura_call(
	struct aura_node *dev, 
	const char *name,
	struct aura_buffer **ret,
	...);



/* Buffer.h */
struct aura_buffer *aura_buffer_request(struct aura_node *nd, int size);
void aura_buffer_release(struct aura_node *nd, struct aura_buffer *buf);
struct aura_buffer *aura_buffer_internal_get(int size);
static inline void aura_buffer_rewind(struct aura_node *node, struct aura_buffer *buf) { 
	buf->pos = node->tr->buffer_offset;
}

void aura_panic(struct aura_node *node); 


/* retparse */ 
uint8_t  aura_buffer_get_u8 (struct aura_buffer *buf);
uint16_t aura_buffer_get_u16(struct aura_buffer *buf);
uint32_t aura_buffer_get_u32(struct aura_buffer *buf);
uint64_t aura_buffer_get_u64(struct aura_buffer *buf);

int8_t  aura_buffer_get_s8 (struct aura_buffer *buf);
int16_t aura_buffer_get_s16(struct aura_buffer *buf);
int32_t aura_buffer_get_s32(struct aura_buffer *buf);
int64_t aura_buffer_get_s64(struct aura_buffer *buf);

void *aura_buffer_get_bin(struct aura_buffer *buf, int len);

void aura_etable_changed_cb(struct aura_node *node, 
			    void (*cb)(struct aura_node *node, void *arg),
			    void *arg);
void aura_status_changed_cb(struct aura_node *node, 
			    void (*cb)(struct aura_node *node, int newstatus, void *arg),
			    void *arg);
void aura_fd_changed_cb(struct aura_node *node, 
			 void (*cb)(const struct aura_pollfds *fd, enum aura_fd_action act, void *arg),
			 void *arg);

void aura_add_pollfds(struct aura_node *node, int fd, short events);
void aura_del_pollfds(struct aura_node *node, int fd);
int aura_get_pollfds(struct aura_node *node, const struct aura_pollfds **fds);


/* event system data access functions */
void *aura_eventsys_get_data(struct aura_node *node);
void aura_eventsys_set_data(struct aura_node *node, void *data);

#define aura_eventloop_create(...) \
	aura_eventloop_create__(0, ##__VA_ARGS__, NULL)

void *aura_eventloop_vcreate(va_list ap);
void *aura_eventloop_create__(int dummy, ...);
void aura_handle_events(struct aura_eventloop *loop);
void aura_handle_events_timeout(struct aura_eventloop *loop, int timeout_ms);


/* Event-System Backend */
void *aura_eventsys_backend_create();
void aura_eventsys_backend_destroy(void *backend);
int aura_eventsys_backend_wait(void *backend, int timeout_ms);
void aura_eventsys_backend_fd_action(void *backend, const struct aura_pollfds *ap, int action);


void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd);

uint64_t aura_platform_timestamp();

static inline void aura_wait_status(struct aura_node *node, int status) 
{ 
	struct aura_eventloop *loop = aura_eventsys_get_data(node);
	while (node->status != status) 
		aura_handle_events(loop);
}

#endif

