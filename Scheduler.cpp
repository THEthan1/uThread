#include <iostream>
#include "Scheduler.h"

Scheduler::Scheduler(int quantum_length, struct sigaction sa) : quant_len(quantum_length), sa(sa){
    //initialize data bases
    ready = new std::list<Thread*>();
    blocked = new std::map<int,Thread*>();
    sleeping = new std::map<int,int>();
    threads = new std::map<int,Thread*>();
    next_sleep_check = 0;

    // configure the timer_data to expire every 0 sec after that. (meaning the timer_data will go off only once)
    timer_data = {0, 0, 0, quant_len};

    //add the main thread to the Scheduler's database and run it manually.
    auto *main = new Thread(nullptr);
    main->setState(RUNNING);
    threads->insert({0,main});
    running = main;
    total_quantum_counter = 0;
    runTimer();
}

Scheduler::~Scheduler() {
    Thread *main;
    for (auto iter : *threads) {
        if (iter.first == 0)
            main = iter.second;
        else
            delete iter.second;
    }

    delete main;
    delete threads;
    delete ready;
    delete blocked;
    delete sleeping;
    delete terminated;
}

Thread* Scheduler::getThreadByID(int id) const{
    auto iter = threads->find(id);
    if (iter == threads->end()) return nullptr;
    return iter->second;
}

Thread* Scheduler::getCurrentThread() const{
    return running;
}


int Scheduler::addNewThread(Thread* thread) {
    if (threads->size() == MAX_THREAD_NUM) {
        return 0; // error
    }

    threads->insert({thread->getId(),thread});
    setReady(thread);

    return thread->getId();
}

void Scheduler::runThread(Thread *thread) {
    thread->setState(RUNNING);
    running = thread;
    runTimer();
    thread->restoreState();
}


void Scheduler::setReady(Thread *thread) {
    thread->setState(READY);
    ready->push_back(thread);
}

void Scheduler::blockThread(Thread *thread) {
    // for threads sent here from sleep function, we dont want to change their state.
    // but when a block is added to an already sleeping (blocked) function, then we DO want to set state to BLOCKED.
    if (thread->getState() != SLEEPING || blocked->count(thread->getId()) != 0)
        thread->setState(BLOCKED);
    blocked->insert({thread->getId(),thread});
    if (running == thread){
        sigset_t  temp = {0};
        sigprocmask(SIG_UNBLOCK, &temp, nullptr);
        int status = running->saveRunStatus();
        if (!running->saveRunStatus()) {
            sigprocmask(SIG_BLOCK, &temp, nullptr);
            runNextThread();
            return;
        }
    }
    else {
        ready->remove(thread);
    }
}

void Scheduler::unblockThread(Thread* thread) {
    if (thread->getState() != BLOCKED) // dont unblock thread that is meant to be sleeping
        return;
    else if (sleeping->count(thread->getId())) {
        thread->setState(SLEEPING);
        return;
    }

    blocked->erase(thread->getId());
    setReady(thread);
}

void Scheduler::sleepCurrentThread(int num_quants) {
    int wake_up_time = total_quantum_counter + num_quants;
    if (next_sleep_check == 0 || wake_up_time < next_sleep_check)
        next_sleep_check = wake_up_time;
    sleeping->insert({running->getId(),wake_up_time});
    running->setState(SLEEPING);
    blockThread(running);
}

void Scheduler::terminateThread(Thread *thread) {
    if (thread->getId() == running->getId()){
        //mask signals from timer_data until we restart it
        sigaddset(&sa.sa_mask, SIGVTALRM);
        if (terminated != nullptr && terminated != running) {
            removeThread(terminated);
            delete terminated;
        }

        terminated = running;
        terminated->setState(TERMINATED);
        removeThread(thread);
        thread->removeID();
        runNextThread();
    }
    else {
        removeThread(thread);
        delete thread;
    }
}

void Scheduler::runNextThread() {
    Thread *next_in_line = ready->front();
    ready->pop_front();
    runThread(next_in_line);
}

void Scheduler::runTimer() {
    running->incrementQuantumAmount();
    total_quantum_counter++;

    //we reached this function only if the thread ran a full quantum
    if (total_quantum_counter == next_sleep_check) {
        Scheduler::handleSleeping();
    }

    if (setitimer(ITIMER_VIRTUAL, &timer_data, nullptr)) {
        std::cerr << "system error: failed to start timer_data\n";
        delete this;
        exit(1);
    }
}

void Scheduler::timerHandler(int sig) {
    sigset_t  temp = {0};
    sigaddset(&temp, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &temp, nullptr);
    if (terminated != nullptr) {
        delete terminated;
        terminated = nullptr;
    }

    setReady(running);

    sigprocmask(SIG_UNBLOCK, &temp, nullptr);

    if (!running->saveRunStatus()) {
        sigprocmask(SIG_BLOCK, &temp, nullptr);
        runNextThread();
        return;
    }
}

void Scheduler::handleSleeping() {
    int next_check = 0;
    auto iter = sleeping->begin();
    while (iter != sleeping->end()) {
        if (iter->second == next_sleep_check) {
            int id = iter->first;
            iter = sleeping->erase(iter);
            Thread *thread = blocked->at(id);
            if (thread->getState() == SLEEPING) {
                thread->setState(BLOCKED);
                unblockThread(thread);
            }
        }
        else {
            if (next_check == 0 || iter->second < next_check)
                next_check = iter->second;
            ++iter;
        }
    }

    next_sleep_check = next_check;
}

int Scheduler::getTotalQuantumCycles() const {
    return total_quantum_counter;
}

/*
 * remove an existing thread from the Scheduler's databases
 */
void Scheduler::removeThread(Thread *thread) {
    if (thread == nullptr)
        return;

    int id = thread->getId();
    ready->remove(thread);
    blocked->erase(id);
    sleeping->erase(id);
    threads->erase(id);
}





