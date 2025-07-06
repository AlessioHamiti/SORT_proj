#include "executive.h"
#include <cassert>
#include <iostream>
#include <string>
#define VERBOSE


Executive::Executive(size_t num_tasks,unsigned int frame_length_,unsigned int unit_duration_ms)
    : tasks(num_tasks),frame_length(frame_length_),unit_time(unit_duration_ms)
{   
    //setta tutti i task in Idle
    for (auto& T : tasks) {
        std::lock_guard<std::mutex> lg(T.state_mtx);
        T.state = State::Idle;
    }
}

void Executive::set_periodic_task(size_t task_id,std::function<void()> periodic_task,unsigned int wcet)
{
    assert(task_id < tasks.size()); //task_id valido
    auto& T = tasks[task_id];
    T.function = std::move(periodic_task);
    T.wcet = wcet;

    // crea e lancia il thread
    T.thread = std::thread(&Executive::task_function, std::ref(T));
    {
        std::lock_guard<std::mutex> lg(T.state_mtx);
        T.state = State::Idle;
    }
    // priorità minima iniziale
    rt::set_priority(T.thread, rt::priority::rt_min);
}

void Executive::set_aperiodic_task(std::function<void()> aperiodic_task, unsigned int wcet) {
    ap_T.function = std::move(aperiodic_task);
    ap_T.wcet = wcet;
    ap_T.skip_count = 0;

    ap_T.thread = std::thread(&Executive::task_function, std::ref(ap_T));
    {
        std::lock_guard<std::mutex> lg(ap_T.state_mtx);
        ap_T.state = State::Idle;
    }

    rt::set_priority(ap_T.thread, rt::priority::rt_min);
}


void Executive::add_frame(std::vector<size_t> frame) {
    for (auto id : frame) {
        assert(id < tasks.size());
    }
    frames.push_back(frame);
    
    // calcola slack time per il frame
    int slack_time = frame_length;
    for (size_t j = 0; j < frame.size(); j++) {
        size_t tid = frame[j];
        auto& T = tasks[tid];
        slack_time -= T.wcet;
    }
#ifdef VERBOSE
    std::cout << "[Exec] Frame " << frames.size() - 1 << ", con slack time: " << slack_time << std::endl;
#endif
    slack_times.push_back(slack_time);
}

void Executive::start() {
    exec_thread = std::thread(&Executive::exec_function, this);
    // thread manager con priorità massima
    rt::set_priority(exec_thread, rt::priority::rt_max);
}

void Executive::wait() {
    if (exec_thread.joinable())
        exec_thread.join();
}

void Executive::ap_task_request() {
    // Segnala la presenza di una richiesta aperiodica
    {
    std::lock_guard<std::mutex> lg(ap_request_mtx);
    ap_request_pending = true;
    }
#ifdef VERBOSE
    std::cout << "[AP] Richiesta aperiodico ricevuta\n";
#endif
}
    


void Executive::task_function(TaskData& T) {
    while (true) {
        // aspetta pending
        std::unique_lock<std::mutex> lk(T.mtx);
        T.cv.wait(lk, [&]{
            std::lock_guard<std::mutex> lg(T.state_mtx);
            return T.state == State::Pending;
        });
        lk.unlock();

#ifdef VERBOSE
        rt::priority current_priority = rt::get_priority(T.thread);
        std::cout << "[Task] Running task priority: " << current_priority << std::endl;
#endif
        // esecuzione: setta running
        {
            std::lock_guard<std::mutex> lg(T.state_mtx);
            T.state = State::Running;
        }
        // esegue il task
        T.function();

        // torna idle
        {
            std::lock_guard<std::mutex> lg(T.state_mtx);
            T.state = State::Idle;
        }
    }
}

void Executive::exec_function() {
    size_t frame_id = 0;
    auto next_time = std::chrono::steady_clock::now();
    bool ap_request = false;
    bool ap_running = false;
    State ap_state;

    while (true) {
#ifdef VERBOSE
        std::cout << "\e[0;34m" <<"*** Frame " << frame_id << " start ***" << "\033[0m" << std::endl;
#endif
        // controllo task ancora in Running da frame precedente
        for (size_t tid = 0; tid < tasks.size(); ++tid) {
            auto& T = tasks[tid];
            bool was_running;
            {
                std::lock_guard<std::mutex> lg(T.state_mtx);
                was_running = (T.state == State::Running);
            }
            if (was_running) {
#ifdef VERBOSE
         std::cout << "\e[0;32m"<<"[Exec] Task " << tid << " riprende da frame precedente" << "\033[0m" << std::endl;
#endif
            }
        }
    
        auto frame_start = next_time;
        next_time = frame_start + frame_length * unit_time;

        // Gestione richieste aperiodiche
        {
            std::lock_guard<std::mutex> lg_request(ap_request_mtx);
            ap_request = ap_request_pending;
            ap_request_pending = false;  // Reset della richiesta
        }

        if (ap_request) {
            // Quando ricevo una richiesta aperiodica, controllo se c'è un task aperiodico in esecuzione o pending
            std::lock_guard<std::mutex> lg_ap(ap_T.state_mtx);
            ap_state = ap_T.state;
            
            if (ap_state == State::Running || ap_state == State::Pending) {
                std::cerr << "[AP] Deadline miss: richiesta ignorata perché il task aperiodico è ancora in esecuzione\n";
                ap_T.skip_count = 1; 
            } else {
                ap_T.skip_count = 0;
                ap_T.state = State::Pending;
            }
            ap_running = true;
            ap_request = false;
        }

        // Gestione task aperiodico
        if (!ap_request) {
            std::lock_guard<std::mutex> lg_ap(ap_T.state_mtx);
            ap_state = ap_T.state;
            
            if (ap_state != State::Idle && ap_T.skip_count == 0) {
                ap_T.release_time = frame_start;
                
                if (slack_times[frame_id] > 0) {
                    // Se c'è slack time, priorità massima-1 (inferiore all'executive)
                    rt::set_priority(ap_T.thread, rt::priority::rt_max - 1);
#ifdef VERBOSE
                    std::cout << "[AP] Attivo aperiodico con priorità alta (slack disponibile)\n";
#endif
                } else {
                    // Se non c'è slack time, priorità minima ma comunque schedulato
                    rt::set_priority(ap_T.thread, rt::priority::rt_min);
#ifdef VERBOSE
                    std::cout << "[AP] Attivo aperiodico con priorità minima (senza slack)\n";
#endif
                }
                ap_T.cv.notify_one();
            }
            else if (ap_state == State::Idle) {
                ap_running = false;
            }
        }
        
        // Gestione skip_count per task aperiodico
        {
            std::lock_guard<std::mutex> lg_ap(ap_T.state_mtx);
            if (ap_T.skip_count > 0) {
                ap_T.skip_count = 0;
                ap_T.state = State::Idle;
            }
        }

        // Attiva i task del frame con priorità decrescente
        rt::priority maxp = rt::priority::rt_max;;
        rt::priority prio_val;
        for (size_t i = 0; i < frames[frame_id].size(); ++i) {
            size_t tid = frames[frame_id][i];
            auto& T = tasks[tid];
            std::lock_guard<std::mutex> lg(T.state_mtx);
            if (T.skip_count > 0) {
                --T.skip_count;
                continue;
            }
            if (ap_running){
                prio_val = maxp - static_cast<int>(i + 2);
                #ifdef VERBOSE
                std::cout << "[AP] ATTIVO, quindi priorità task decreased: " <<prio_val <<"\n";
                #endif
            }
            else {
                prio_val = maxp - static_cast<int>(i + 1);
                #ifdef VERBOSE
                std::cout << "[AP] INATTIVO, quindi priorità task normal: "<<prio_val <<"\n";
                #endif
            }
            // calcolo prio_val = maxp - (i+1), clamped a [min+1, maxp]
            //rt::priority prio_val = maxp - static_cast<int>(i + 1);
            rt::priority minp = rt::priority::rt_min + 1;
            if (prio_val < minp) prio_val = minp;
            rt::set_priority(T.thread, prio_val);

            // set release e deadline
            T.release_time = frame_start;
            T.deadline_time = frame_start + frame_length * unit_time;
            T.state = State::Pending;
            T.cv.notify_one();
        }

        if (ap_running){
            auto slack = frame_start + slack_times[frame_id] * unit_time;
            #ifdef VERBOSE
            std::cout << "[AP] Dormo\n";
#endif
            std::this_thread::sleep_until(slack);
            {
            std::lock_guard<std::mutex> lg_ap(ap_T.state_mtx);
            rt::set_priority(ap_T.thread, rt::priority::rt_min);
            ap_T.cv.notify_one();
            }
#ifdef VERBOSE
            std::cout << "[AP] Task aperiodico in attesa fino allo slack time, torno a dormire\n";
#endif
        }
        // dormi fino al prossimo frame
        std::this_thread::sleep_until(next_time);

        // verifica deadline miss
        int tid = 0;
        for (auto& T : tasks) {
            bool idle;
            {
                std::lock_guard<std::mutex> lg(T.state_mtx);
                idle = (T.state == State::Idle);
            }
            if (!idle) {
                std::cerr << "\e[0;31m" << "Deadline miss" << "\033[0m" << ": task " << tid << std::endl;
                rt::set_priority(T.thread, rt::priority::rt_min);

                bool running;
                {
                    std::lock_guard<std::mutex> lg(T.state_mtx);
                    running = (T.state == State::Running);
                }

                if (!running) {
                    std::lock_guard<std::mutex> lg(T.state_mtx);
                    T.state = State::Idle;
                } 
                T.skip_count += 1;
            }
            ++tid;
        }


#ifdef VERBOSE
        std::cout << "\e[0;34m" << "*** Frame " << frame_id << " end ***" << "\033[0m" << std::endl;
#endif
        frame_id = (frame_id + 1) % frames.size();
    }
}