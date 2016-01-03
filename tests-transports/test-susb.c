#include <aura/aura.h>
#include <unistd.h>
int main() {
        int ret; 
        int i = 32; 
        slog_init(NULL, 88);
        struct aura_node *n = aura_open("simpleusb", "../simpleusbconfigs/susb-test.conf");

        if (!n) { 
                printf("err\n");
                return -1;
        }
        aura_wait_status(n, AURA_STATUS_ONLINE);

        struct aura_buffer *retbuf; 
	
        ret = aura_call(n, "led_ctl", &retbuf, 0x1111, 0x11);
        slog(0, SLOG_DEBUG, "call ret %d", ret);
        if (0 != ret)
		exit(1);

        aura_buffer_release(retbuf); 
	
	uint32_t a = rand();
	uint32_t b = rand();

        ret = aura_call(n, "write", &retbuf, 0xa, 0xb, a, b);
        slog(0, SLOG_DEBUG, "call ret %d", ret);
        if (0 != ret)
		exit(1);

        aura_buffer_release(retbuf); 

        ret = aura_call(n, "read", &retbuf, 0x0, 0x0);
        slog(0, SLOG_DEBUG, "call ret %d", ret);
        if (0 != ret)
		exit(1);

	aura_hexdump("buffer", retbuf->data, retbuf->size);

	if (a != aura_buffer_get_u32(retbuf))
		exit(1);

	if (b != aura_buffer_get_u32(retbuf))
		exit(1);
	
	printf("TEST_OK!\n");
        aura_close(n);
        return 0;
}
