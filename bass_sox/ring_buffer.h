#include <windows.h>

typedef struct {
	void* buffer;
	size_t length;
	size_t head;
	size_t tail;
	BOOL full;
} RING_BUFFER;

RING_BUFFER* ring_buffer_create(size_t length);

size_t ring_buffer_size(RING_BUFFER* ring_buffer);

size_t ring_buffer_read_capacity(RING_BUFFER* ring_buffer);

size_t ring_buffer_write_capacity(RING_BUFFER* ring_buffer);

size_t ring_buffer_read(RING_BUFFER* ring_buffer, void* buffer, size_t length);

size_t ring_buffer_write(RING_BUFFER* ring_buffer, void* buffer, size_t length);

void ring_buffer_clear(RING_BUFFER* ring_buffer);

void ring_buffer_free(RING_BUFFER* ring_buffer);