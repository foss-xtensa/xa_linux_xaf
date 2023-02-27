

#ifndef _CIRCULARBUFFER_H_
#define _CIRCULARBUFFER_H_

#include <stdint.h>


struct circular_buf {
	uint8_t *buffer;
	size_t head;
	size_t tail;
	size_t max; //of the buffer
	size_t filled;
};

typedef struct circular_buf circular_buf_t;


/*
 * @Desc    Create and initialize a circular buffer structure
 */
circular_buf_t *circularbuf_create(uint32_t size);

/*
 * @Desc    Destroy and free Circular buffer structure
 */
void circularbuf_destroy(circular_buf_t *cb);


/*
 * @Desc    Empty circular buffer structure
 */
void circularbuf_clear(circular_buf_t *cb);

/*
 * @Desc    Return the number of bytes filled in the circular buffer
 *
 */
uint32_t circularbuf_get_filled(circular_buf_t *cb);


/*
 * @Desc    Write data to circular buffer
 *
 */
uint32_t circularbuf_write(circular_buf_t *rb, uint8_t *data, uint32_t size);

/*
 * @Desc    Read data from Circular buffer
 *
 */
uint32_t circularbuf_read(circular_buf_t *rb, uint8_t *data, uint32_t size);



#endif /* _CIRCULARBUFFER_H_ */
