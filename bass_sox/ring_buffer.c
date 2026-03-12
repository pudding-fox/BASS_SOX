#include "ring_buffer.h"

RING_BUFFER* ring_buffer_create(size_t length) {
	RING_BUFFER* ring_buffer = calloc(sizeof(RING_BUFFER), 1);
	if (!ring_buffer) {
		return NULL;
	}
	ring_buffer->buffer = malloc(length);
	if (!ring_buffer->buffer) {
		ring_buffer_free(ring_buffer);
		return NULL;
	}
	ring_buffer->length = length;
	return ring_buffer;
}

size_t ring_buffer_size(RING_BUFFER* ring_buffer) {
	return ring_buffer->length;
}

size_t ring_buffer_read_capacity(RING_BUFFER* ring_buffer) {
	if (ring_buffer->full) {
		return ring_buffer->length;
	}
	if (ring_buffer->head >= ring_buffer->tail)
	{
		return ring_buffer->head - ring_buffer->tail;
	}
	return (ring_buffer->length + ring_buffer->head) - ring_buffer->tail;
}

size_t ring_buffer_write_capacity(RING_BUFFER* ring_buffer) {
	return ring_buffer->length - ring_buffer_read_capacity(ring_buffer);
}

static void ring_buffer_advance_tail(RING_BUFFER* ring_buffer, size_t length) {
	ring_buffer->tail += length;
	if (ring_buffer->tail > ring_buffer->length) {
		ring_buffer->tail -= ring_buffer->length;
	}
	ring_buffer->full = FALSE;
}

size_t ring_buffer_read(RING_BUFFER* ring_buffer, void* buffer, size_t length) {
	size_t capacity = ring_buffer_read_capacity(ring_buffer);
	size_t offset = 0;
	if (capacity == 0) {
		return 0;
	}
	else if (length == 0) {
		return 0;
	}
	else if (length < capacity) {
		capacity = length;
	}
	if (ring_buffer->tail >= ring_buffer->head) {
		offset = ring_buffer->length - ring_buffer->tail;
		if (offset > 0) {
			if (offset > capacity) {
				offset = capacity;
			}
			memcpy(buffer, (BYTE*)ring_buffer->buffer + ring_buffer->tail, offset);
		}
	}
	else if (ring_buffer->tail < ring_buffer->head) {
		offset = ring_buffer->head - ring_buffer->tail;
		if (offset > 0) {
			if (offset > capacity) {
				offset = capacity;
			}
			memcpy(buffer, (BYTE*)ring_buffer->buffer + ring_buffer->tail, offset);
		}
	}
	if (offset < capacity) {
		memcpy((BYTE*)buffer + offset, ring_buffer->buffer, capacity - offset);
	}
	ring_buffer_advance_tail(ring_buffer, capacity);
	return capacity;
}

static void ring_buffer_advance_head(RING_BUFFER* ring_buffer, size_t length) {
	ring_buffer->head += length;
	if (ring_buffer->head > ring_buffer->length) {
		ring_buffer->head -= ring_buffer->length;
	}
	if (ring_buffer->tail == ring_buffer->head) {
		ring_buffer->full = TRUE;
	}
	else {
		ring_buffer->full = FALSE;
	}
}

size_t ring_buffer_write(RING_BUFFER* ring_buffer, void* buffer, size_t length) {
	size_t capacity = ring_buffer_write_capacity(ring_buffer);
	size_t offset = 0;
	if (capacity == 0) {
		return 0;
	}
	else if (length == 0) {
		return 0;
	}
	else if (length < capacity) {
		capacity = length;
	}
	if (ring_buffer->head >= ring_buffer->tail) {
		offset = ring_buffer->length - ring_buffer->head;
		if (offset > 0) {
			if (offset > capacity) {
				offset = capacity;
			}
			memcpy((BYTE*)ring_buffer->buffer + ring_buffer->head, buffer, offset);
		}
	}
	else if (ring_buffer->head < ring_buffer->tail) {
		offset = ring_buffer->tail - ring_buffer->head;
		if (offset > 0) {
			if (offset > capacity) {
				offset = capacity;
			}
			memcpy((BYTE*)ring_buffer->buffer + ring_buffer->head, buffer, offset);
		}
	}
	if (offset < capacity) {
		memcpy(ring_buffer->buffer, (BYTE*)buffer + offset, capacity - offset);
	}
	ring_buffer_advance_head(ring_buffer, capacity);
	return capacity;
}

void ring_buffer_clear(RING_BUFFER* ring_buffer) {
	ring_buffer->head = 0;
	ring_buffer->tail = 0;
	ring_buffer->full = FALSE;
}

void ring_buffer_free(RING_BUFFER* ring_buffer) {
	if (ring_buffer) {
		if (ring_buffer->buffer) {
			free(ring_buffer->buffer);
			ring_buffer->buffer = NULL;
		}
		free(ring_buffer);
	}
}