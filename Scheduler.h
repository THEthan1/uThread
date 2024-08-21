//
// Created by ykkosman on 4/17/23.
//

#ifndef OS_EX2_SCHEDULER_H
#define OS_EX2_SCHEDULER_H

#include "Thread.h"
#include "uthreads.h"
#include <map>
#include <list>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

class Scheduler {
private:
    Thread *running;
    std::list<Thread*> *ready;
    std::map<int,Thread*> *blocked;
    std::map<int,int> *sleeping;
    std::map<int,Thread*> *threads;
    const int quant_len;
    struct sigaction sa;
    struct itimerval timer_data;
    int total_quantum_counter;
    int next_sleep_check;
    Thread* terminated = nullptr;

    /*
     * initialize/restart the timer_data to send SIGVTALRM after a fixed amount of microseconds specified
     * in the constructor (quantum_length).
     */
    void runTimer();

    /*
     * run the next unblocked thread in line
     */
    void runNextThread();

    /*
     * remove an existing thread from the Scheduler's databases
     */
    void removeThread(Thread*);

public:
    /**
     * create a Scheduler object that enable managing new user level threads. a helper class for uthread.
     *
     * @param quantum_length the length of a cycle for running a thread
     */
    explicit Scheduler(int quantum_length, struct sigaction sa);

    /**
     * a simple destructor
     */
    ~Scheduler();

    /**
     * returns the Thread object related to the given ID
     * @return
     */
    Thread* getThreadByID(int) const;

    /**
     * sets the status of the given thread to RUNNING run it and initialize its timer_data
     * @param thread
     */
    void runThread(Thread *thread);

    /**
     * chang the status of the given time to ready, and adding it to the end of the ready line.
     */
    void setReady(Thread*);

    /**
     * sets the status of the given thread into BLOCKED, effectively preventing its running until a
     * different thread unblocks it.
     */
    void blockThread(Thread*);

    /**
     * blocks the currently running block for the given amount of quantums.
     */
    void sleepCurrentThread(int);

    /**
     * remove the BLOCK status from the given block, (not necessarily making it ready)
     */
    void unblockThread(Thread*);

    /**
     * terminate and delete the given thread
     */
    void terminateThread(Thread*);

    /**
     * reruns the currently running thread.
     * @return
     */
    Thread* getCurrentThread() const;

    /**
     * adds the given block to the Scheduler's databases.
     *
     * @return id of the newly added (but NOT newly create) thread.
     */
    int addNewThread(Thread*);

    /**
     * this function is called whenever the timer_data expires.
     * @param sig unused
     */
    void timerHandler(int sig);
    void handleSleeping();


    /**
     * returns the amount of quantums passed by know (not include the currebt)
     * @return
     */
    int getTotalQuantumCycles() const;

};

#endif //OS_EX2_SCHEDULER_H
