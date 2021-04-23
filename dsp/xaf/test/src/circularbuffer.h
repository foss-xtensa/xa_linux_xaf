/*
* Copyright (c) 2015-2020 Cadence Design Systems Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


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
