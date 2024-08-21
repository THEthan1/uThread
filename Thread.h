#ifndef OS_EX2_THREAD_H
#define OS_EX2_THREAD_H

#include <set>
#include <csetjmp>
#include <thread>
#include <signal.h>
#include "uthreads.h"

enum State {READY, RUNNING, BLOCKED, SLEEPING, TERMINATED};
typedef void (*thread_entry_point)(void);
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

class Thread{
public:
    // we pass nullptr when creating main thread
    explicit Thread(thread_entry_point = nullptr);
    ~Thread();
    int getId() const;
    State getState() const;
    void setState(State);

    /**
     * add one to quantum count
     */
    void incrementQuantumAmount();

    int getRunTime() const;

    /**
     * save the current state of the thraed (to stack)
     */
    int saveRunStatus();

    /**
     * continue to run this thread
     */
    void restoreState() const;

    /**
     * adds this ID to removed ID list
     */
    void removeID();

    /**
     * internal class for providing lowest possible thread ID number
     */
    class Thread_ID_Maker {
        std::set<int> *available_ids;
        int last_id;

    public:
        Thread_ID_Maker();

        ~Thread_ID_Maker();

        /**
        * returns lowest available id.
        */
        int getNewID();

        /**
        * adds id of eliminated thread to the list of available thread ids
        */
        void addIDtoList(int);
    };

private:
    __jmp_buf_tag* environment;
    const int id;
    State state;
    int total_run_time;
    static Thread_ID_Maker *threadIdMaker;
    char *stack;

    /**
     * Prepares for sigsetjmp thread switch.
     * @param entry_point the entry point for the thread about to be run
     */
    void setNewEntryPoint(thread_entry_point entry_point);

    /**
     * Black Box: A translation is required when using an address of a variable.
     * @param addr the address to translate
     * @return the translated address
     */
    static address_t translate_address(address_t addr);


};

#endif //OS_EX2_THREAD_H
