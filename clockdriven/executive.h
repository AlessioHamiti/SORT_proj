#ifndef EXECUTIVE_H
#define EXECUTIVE_H

#include <vector>
#include <functional>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "rt/priority.h"

class Executive {
public:
    enum class State { Idle, Pending, Running };

    /* [INIT] Inizializza l'executive, impostando i parametri di scheduling:
			num_tasks: numero totale di task presenti nello schedule;
			frame_length: lunghezza del frame (in quanti temporali);
			unit_duration: durata dell'unita di tempo, in millisecondi (default 10ms).
	*/
	Executive(size_t num_tasks, unsigned int frame_length, unsigned int unit_duration = 10);

	/* [INIT] Imposta il task periodico di indice "task_id" (da invocare durante la creazione dello schedule):
		task_id: indice progressivo del task, nel range [0, num_tasks);
		periodic_task: funzione da eseguire al rilascio del task;
		wcet: tempo di esecuzione di caso peggiore (in quanti temporali).
	*/
	void set_periodic_task(size_t task_id, std::function<void()> periodic_task, unsigned int wcet);
	
	/* [INIT] Imposta il task aperiodico (da invocare durante la creazione dello schedule):
		aperiodic_task: funzione da eseguire al rilascio del task;
		wcet: tempo di esecuzione di caso peggiore (in quanti temporali).
	*/
	void set_aperiodic_task(std::function<void()> aperiodic_task, unsigned int wcet);
	
	/* [INIT] Lista di task da eseguire in un dato frame (da invocare durante la creazione dello schedule):
		frame: lista degli id corrispondenti ai task da eseguire nel frame, in sequenza
	*/
	void add_frame(std::vector<size_t> frame);

	/* [RUN] Lancia l'applicazione */
	void start();

	/* [RUN] Attende (all'infinito) finchè gira l'applicazione */
	void wait();

	/* [RUN] Richiede il rilascio del task aperiodico (da invocare durante l'esecuzione).
	*/
	void ap_task_request();

private:
    struct TaskData {
        std::function<void()> function;
        std::thread thread;
        std::mutex mtx;
        std::condition_variable cv;
        std::mutex state_mtx;
        State state{State::Idle};
        std::chrono::steady_clock::time_point release_time;
        std::chrono::steady_clock::time_point deadline_time;
        unsigned int wcet{0};
        unsigned int skip_count{0};
    };

    std::vector<TaskData> tasks;
	TaskData ap_T;
    std::thread exec_thread;
    std::vector<std::vector<size_t>> frames;
	std::vector<int> slack_times;
    unsigned int frame_length;
    std::chrono::milliseconds unit_time;
    
    // Variabile per segnalare richieste aperiodiche
    bool ap_request_pending{false};
    std::mutex ap_request_mtx;

    static void task_function(TaskData& T);
    void exec_function();
};

#endif // EXECUTIVE_H