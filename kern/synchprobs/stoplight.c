/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/* Synchronization primitives & shared globals */
struct lock *quadrant_mutexes[4];
struct semaphore *cars_allowed_sem;

/*
 * Called by the driver during initialization.
 */

void
stoplight_init() {
	char buf[16];
	for (size_t i = 0; i < 4; ++i) {
		snprintf(buf, 16, "quadrant_lock_%d", i);
		quadrant_mutexes[i] = lock_create(buf);
		if (quadrant_mutexes[i] == NULL) {
			panic("sp2: lock_create failed\n");
		}
	}

	/* Allow atmost 3 cars on the intersection to avoid deadlock. (Refer The Dining Philosopher's problem) */
	cars_allowed_sem = sem_create("cars_allowed", 3);
}

/*
 * Called by the driver during teardown.
 */

void
stoplight_cleanup() {
	sem_destroy(cars_allowed_sem);

	for (size_t i = 0; i < 4; ++i) {
		lock_destroy(quadrant_mutexes[i]);
	}
}

uint32_t
gostraight_quadrant(uint32_t quadrant) {
	return (quadrant + 3) % 4;
}

uint32_t
goleft_quadrant(uint32_t quadrant) {
	return (quadrant + 2) % 4;
}

void
turnright(uint32_t direction, uint32_t index)
{
	P(cars_allowed_sem);
	lock_acquire(quadrant_mutexes[direction]);
	inQuadrant(direction, index);
	leaveIntersection(index);
	lock_release(quadrant_mutexes[direction]);
	V(cars_allowed_sem);
}

void
gostraight(uint32_t direction, uint32_t index)
{
	P(cars_allowed_sem);
	lock_acquire(quadrant_mutexes[direction]);
	inQuadrant(direction, index);

	uint32_t target_quadrant = gostraight_quadrant(direction);
	lock_acquire(quadrant_mutexes[target_quadrant]);
	inQuadrant(target_quadrant, index);
	lock_release(quadrant_mutexes[direction]);

	leaveIntersection(index);
	lock_release(quadrant_mutexes[target_quadrant]);
	V(cars_allowed_sem);
}

void
turnleft(uint32_t direction, uint32_t index)
{
	P(cars_allowed_sem);
	lock_acquire(quadrant_mutexes[direction]);
	inQuadrant(direction, index);

	/* Going straight twice makes a left turn */
	uint32_t cur_quadrant = direction;
	for (size_t i = 0; i < 2; ++i) {
		uint32_t target_quadrant = gostraight_quadrant(cur_quadrant);
		lock_acquire(quadrant_mutexes[target_quadrant]);
		inQuadrant(target_quadrant, index);
		lock_release(quadrant_mutexes[cur_quadrant]);
		cur_quadrant = target_quadrant;
	}

	leaveIntersection(index);
	lock_release(quadrant_mutexes[cur_quadrant]);
	V(cars_allowed_sem);
}
