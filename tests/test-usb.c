#include <aura/aura.h>

int main() {
	int ret; 
	int i = 32; 
	init_slog(NULL, 88);
	struct aura_node *n = aura_open("usb", 0x1d50, 0x6032, "www.ncrmnt.org", NULL, NULL);
	aura_wait_status(n, AURA_STATUS_ONLINE);

	struct aura_buffer *retbuf; 
	ret = aura_call(n, "turnTheLedOn", &retbuf, 0x1);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (0 == ret) {
		printf("====> buf pos %d len %d\n", retbuf->pos, retbuf->size);
		ret = aura_buffer_get_u8(retbuf);
	}
	printf("====> GOT %d from device\n", ret);
	aura_buffer_release(n, retbuf); 
	while(i--) {
		aura_loop_once(n);
		usleep(10000);
	}
	aura_close(n);
	return 0;
}
