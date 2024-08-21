#include "Thread.h"
#include <set>
#include <thread>
#include <iostream>

using namespace std;
Thread::Thread_ID_Maker *Thread::threadIdMaker = new Thread::Thread_ID_Maker();

Thread::Thread(thread_entry_point entryPoint) : id(Thread::threadIdMaker->getNewID()) {
    state = State::READY;
    total_run_time = 0;
    environment = new __jmp_buf_tag;
    if (entryPoint != nullptr) {
        stack = new char[STACK_SIZE];
        setNewEntryPoint(entryPoint);
    }
    else {
        sigsetjmp(environment, 1);
        sigemptyset(&environment->__saved_mask);
    }
}

Thread::~Thread() {
    delete environment;

    if (id != 0) {
        threadIdMaker->addIDtoList(id);
        delete[] stack;
    }
}

int Thread::getId() const {
    return id;
}

State Thread::getState() const {
    return state;
}

void Thread::setState(State new_state) {
    state = new_state;
}

void Thread::removeID() {
    threadIdMaker->addIDtoList(id);
}

void Thread::incrementQuantumAmount(){
    total_run_time++;
}

int Thread::getRunTime() const {
    return total_run_time;
}

/*
 * returns lowest available id.
 */
int Thread::Thread_ID_Maker::getNewID() {
    if (available_ids->empty()) return ++last_id;
    auto lowest = available_ids->begin();
    if (*lowest < last_id) {
        *available_ids->erase(lowest);
        return *lowest;
    }
    else {
        available_ids->clear();
        return ++last_id;
    }
}

/*
 * adds id of eliminated thread to the list of available thread ids
 */
void Thread::Thread_ID_Maker::addIDtoList(int eliminated) {
    if (available_ids->count(eliminated) != 0)
        return;
    else if (eliminated == last_id)
        --last_id;
    else
        available_ids->insert(eliminated);
}


Thread::Thread_ID_Maker::Thread_ID_Maker() {
    last_id = -1;
    available_ids = new set<int>();
}

Thread::Thread_ID_Maker::~Thread_ID_Maker() {
    delete available_ids;
}

int Thread::saveRunStatus(){
    return sigsetjmp(environment, 1);
    //we do not save the masked signals
}

void Thread::restoreState() const{
    sigset_t  temp = {0};
    sigaddset(&temp, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &temp, nullptr);
    siglongjmp(environment, 1);
}

void Thread::setNewEntryPoint(thread_entry_point entry_point) {
    sigset_t  temp = {0};
    sigaddset(&temp, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &temp, nullptr);
    sigsetjmp(environment, 1);
    sigprocmask(SIG_UNBLOCK, &temp, nullptr);

    address_t sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
    auto pc = (address_t) entry_point;
    (environment->__jmpbuf)[JB_SP] = translate_address(sp);
    (environment->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&(environment->__saved_mask));
}

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t Thread::translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}




