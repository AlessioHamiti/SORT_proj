#include "executive.h"
#include <cassert>
#include <iostream>
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

void Executive::add_frame(std::vector<size_t> frame) {
    for (auto id : frame) {
        assert(id < tasks.size());
    }
    frames.push_back(frame);
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
    /*
// Reset di tutti i task in Idle e priorità minima
        for (auto& T : tasks) {
            std::lock_guard<std::mutex> lg(T.state_mtx);
            T.state = State::Idle;
            rt::set_priority(T.thread, rt::priority::rt_min);
        }
*/
    while (true) {
#ifdef VERBOSE
        std::cout << "*** Frame " << frame_id << " start ***" << std::endl;
#endif
        auto frame_start = next_time;
        next_time = frame_start + frame_length * unit_time;

        

        // Attiva i task del frame con priorità decrescente
        rt::priority maxp = rt::priority::rt_max;;
        for (size_t i = 0; i < frames[frame_id].size(); ++i) {
            size_t tid = frames[frame_id][i];
            auto& T = tasks[tid];
            std::lock_guard<std::mutex> lg(T.state_mtx);
            if (T.skip_count > 0) {
                --T.skip_count;
                continue;
            }
            // calcolo prio_val = maxp - (i+1), clamped a [min+1, maxp]
            rt::priority prio_val = maxp - static_cast<int>(i + 1);
            rt::priority minp = rt::priority::rt_min + 1;
            if (prio_val < minp) prio_val = minp;
            rt::set_priority(T.thread, prio_val);

            // set release e deadline
            T.release_time = frame_start;
            T.deadline_time = frame_start + frame_length * unit_time;
            T.state = State::Pending;
            T.cv.notify_one();
        }

        // dormi fino al prossimo frame
        std::this_thread::sleep_until(next_time);

        // verifica deadline miss
        for (auto tid : frames[frame_id]) {
            auto& T = tasks[tid];
            bool idle;
            {
                std::lock_guard<std::mutex> lg(T.state_mtx);
                idle = (T.state == State::Idle);
            }
            if (!idle) {
                std::cerr << "Deadline miss: task " << tid << std::endl;
                rt::set_priority(T.thread, rt::priority::rt_min);
                if (T.state == State::Pending) {
                    // se il task è in pending, lo mettiamo in idle
                    std::lock_guard<std::mutex> lg(T.state_mtx);
                    T.state = State::Idle;
                } 
                T.skip_count = 1;
            }
        }

#ifdef VERBOSE
        std::cout << "*** Frame " << frame_id << " end ***" << std::endl;
#endif
        frame_id = (frame_id + 1) % frames.size();
    }
}