#include <cstdlib>
#include "uthreads.h"
#include "Scheduler.h"
#include <iostream>


typedef void (*thread_entry_point)(void);

/* External interface */

static Scheduler *scheduler;
static struct sigaction sa = {0};

static void timerHandler(int sig) {
    scheduler->timerHandler(sig);
}

static void manage_signal(int action) {
    sigset_t  temp = {0};
    sigaddset(&temp, SIGVTALRM);
    sigprocmask(action, &temp, nullptr);
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
    /*
     * course of actions:
     * we add the main (current) thread to a static something who can manage a line
     * and set an alarm, so it'll automatically will use the handler after quantum time.
     *
     */
    manage_signal(SIG_BLOCK);

    if (quantum_usecs <= 0) {
        std::cerr << "thread library error: Non-positive value sent to uthread_init function\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }

    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = timerHandler;
    sa.sa_flags = SA_NODEFER;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        std::cerr << "system error: failed to start timer_data\n";
        exit(1);
    }

    scheduler = new Scheduler(quantum_usecs, sa);
    manage_signal(SIG_UNBLOCK);
    return 0;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point){
    /*
     * course of actions:
     * if we call this we create ea environment for a new thread and add it that
     * hypothetical static data base to the end.
     * you can specify from where that thread will start.
     * note that the os alocate an id to each thread, that is NOT the id we will return,
     * but rather an id we generate using OUR genius code.
     */
    manage_signal(SIG_BLOCK);
    if (!entry_point) {
        std::cerr << "thread library error: Null entry point sent to uthread_spawn function\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }
    auto * new_thread = new Thread(entry_point);
    if (!scheduler->addNewThread(new_thread)) {
        std::cerr << "thread library error: Maximum number of threads reached ("
                        + std::to_string(MAX_THREAD_NUM) + ")\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }

    manage_signal(SIG_UNBLOCK);
    return new_thread->getId();
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid) {
    //the timer signal should be blocked in case the terminated thread is next inline
    manage_signal(SIG_BLOCK);

    if (tid == 0) {
        delete scheduler;
        exit(0);
    }

    Thread *thread = scheduler->getThreadByID(tid);
    if (!thread) {
        std::cerr << "thread library error: Invalid thread ID sent to uthread_terminate function\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }

    scheduler->terminateThread(thread);
    manage_signal(SIG_UNBLOCK);
    return 0;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    manage_signal(SIG_BLOCK);
    //timer signal should be blocked in case the blocked thread is next in line
    // (but ot if the blocked thread is the current, think why)

    if (tid == 0){
        std::cerr << "thread library error: Main thread ID was sent to uthread_block function\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }

    Thread *thread = scheduler->getThreadByID(tid);
    if (!thread || thread->getState() == TERMINATED) {
        std::cerr << "thread library error: Invalid thread ID sent to uthread_block function\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }

    scheduler->blockThread(thread);
    manage_signal(SIG_UNBLOCK);
    return 0;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    manage_signal(SIG_BLOCK);
    Thread *thread = scheduler->getThreadByID(tid);
    if (!thread || thread->getState() == TERMINATED) {
        std::cerr << "thread library error: Invalid thread ID sent to uthread_resume function\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }

    scheduler->unblockThread(thread);
    manage_signal(SIG_UNBLOCK);
    return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {
    manage_signal(SIG_BLOCK);
    //timer signal should be blocked in case it will pop out in the middle of running this function and the
    // slipping quantum amount will be disrupted
    Thread *thread = scheduler->getCurrentThread();
    if (thread->getId() == 0) {
        std::cerr << "thread library error: Cannot call uthread_sleep function on main thread\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }

    scheduler->sleepCurrentThread(num_quantums);
    manage_signal(SIG_UNBLOCK);
    return 0;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() {
    manage_signal(SIG_BLOCK);
    int id = scheduler->getCurrentThread()->getId();
    manage_signal(SIG_UNBLOCK);
    return id;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() {
    manage_signal(SIG_BLOCK);
    int num = scheduler->getTotalQuantumCycles();
    manage_signal(SIG_UNBLOCK);
    return num;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {
    manage_signal(SIG_BLOCK);
    Thread *thread = scheduler->getThreadByID(tid);
    if (!thread || thread->getState() == TERMINATED) {
        std::cerr << "thread library error: Invalid thread ID sent to uthread_get_quantums function\n";
        manage_signal(SIG_UNBLOCK);
        return -1;
    }
    int time = thread->getRunTime();
    manage_signal(SIG_UNBLOCK);
    return time;
}
