#include <setjmp.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "tinythreads.h"

#define NULL            0
#define DISABLE()       cli()
#define ENABLE()        sei()
#define STACKSIZE       80
#define NTHREADS        4
#define SETSTACK(buf,a) *((unsigned int *)(buf)+8) = (unsigned int)(a) + STACKSIZE - 4; \
                        *((unsigned int *)(buf)+9) = (unsigned int)(a) + STACKSIZE - 4

struct thread_block {
    void (*function)(int);   // code to run
    int arg;                 // argument to the above
    thread next;             // for use in linked lists
    jmp_buf context;         // machine state
    char stack[STACKSIZE];   // execution stack space
};

struct thread_block threads[NTHREADS];

struct thread_block initp;

thread freeQ   = threads;
thread readyQ  = NULL;
thread current = &initp;

uint8_t milliseconds = 0;

int initialized = 0;

static void initialize(void) {
    int i;
    for (i=0; i<NTHREADS-1; i++)
        threads[i].next = &threads[i+1];
    threads[NTHREADS-1].next = NULL;

	//PORTB = (1<<PB7);
	// Pin Change Enable Mask (PCINT15)
	PCMSK1 = (1<<PCINT15);
	// External Interrupt Mask Register (EIMSK)
	EIMSK = (1<<PCIE1);
	// Timer 1 with 1024 prescaler with CTC (WGM13, WGM12)
	TCCR1B = (0<<WGM13) | (1<<WGM12) | (1<<CS12) | (0<<CS11) | (1<<CS10);

	/* 8 Mhz = 8 000 000
	 * 8 000 000 / 1024 = 7812,5
	 * 7812,5 = 1 second
	 * 7812,5 / 1000 * 50 = 390,625
	 * 391 = 50 ms
	 * 391 = 0b110000111
	 */
	// Set Timer1 Output Compare A
	TIMSK1 = (1<<OCIE1A);
	// Set Output Compare Register 1 A to 391 in binary
	//OCR1A = 0b110000111;
	//OCR1A = 391; // Decimals works too!
	//OCR1A = 7812;
	OCR1A = 3906;
	// Start the timer on value 0
	TCNT1 = 0;

    initialized = 1;
}

static void enqueue(thread p, thread *queue) {
    p->next = NULL;
    if (*queue == NULL) {
	    *queue = p;
	} else {
	    //thread q = *queue;
	    //while (q->next)
	    //q = q->next;
	    //q->next = p;
		p->next = *queue;
		*queue = p;
    }
	// Do something smart with the queue so newly created threads start first

}

static thread dequeue(thread *queue) {
    thread p = *queue;
    if (*queue) {
        *queue = (*queue)->next;
    } else {
        // Empty queue, kernel panic!!!
        while (1) ;  // not much else to do...
    }
    return p;
}

static void dispatch(thread next) {
    if (setjmp(current->context) == 0) {
        current = next;
        longjmp(next->context,1);
    }
}

void spawn(void (* function)(int), int arg) {
    thread newp;

    DISABLE();
    if (!initialized) initialize();

    newp = dequeue(&freeQ);
    newp->function = function;
    newp->arg = arg;
    newp->next = NULL;
    if (setjmp(newp->context) == 1) {
	    ENABLE();
	    current->function(current->arg);
	    DISABLE();
	    enqueue(current, &freeQ);
	    dispatch(dequeue(&readyQ));
    }
    SETSTACK(&newp->context, &newp->stack);

    enqueue(newp, &readyQ);
    ENABLE();
}

void yield(void) {
	// Put the current thread into the readyQ
	thread temp = dequeue(&readyQ);
	enqueue(current, &readyQ);
	// Start another thread from readyQ
	enqueue(temp, &readyQ);
	dispatch(dequeue(&readyQ));
}

void lock(mutex *m) {
	// If the mutex is not locked, then lock it
	if(m->locked == 0) {
		m->locked = 1;
	// Otherwise enqueue the current thread that wants mutex and start another thread from readyQ
	} else {
		enqueue(current, &m->waitQ);
		dispatch(dequeue(&readyQ));
	}
}

void unlock(mutex *m) {
	// If there is a thread waiting in waitQ in the mutex, enqueue the current thread and start 
	// the thread in waitQ that is stored in mutex
	if(m->waitQ) {
		enqueue(current, &readyQ);
		dispatch(dequeue(&m->waitQ));
	// Otherwise unlock the mutex
	} else {
		m->locked = 0;
	}
}

void resetMilliseconds() {
	milliseconds = 0;
}

int readMilliseconds() {
	return milliseconds;
}

