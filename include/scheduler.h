#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include "shell.h"

// 进程状态
enum class ProcessState {
    READY,
    RUNNING,
    TERMINATED
};

// 进程控制块 (PCB)
struct Process {
    int pid;
    std::string command;
    ProcessState state;
    int burst_time;
    int remaining_time;
    int waiting_time = 0;
    int turnaround_time = 0;

    Process(int id, std::string cmd, int burst)
        : pid(id), command(std::move(cmd)), state(ProcessState::READY),
          burst_time(burst), remaining_time(burst) {}
};

// 调度算法类型
enum class SchedulingAlgorithm {
    FCFS, // 先来先服务
    RR,   // 轮转 (Round Robin)
    SJF   // 短作业优先 (非抢占式)
};

class Scheduler {
public:
    Scheduler(Shell* sh);

    void addProcess(const std::string& command);
    void tick();
    void setAlgorithm(SchedulingAlgorithm algo);
    SchedulingAlgorithm getAlgorithm() const;
    const std::vector<std::shared_ptr<Process>>& getProcessList() const;
    const std::shared_ptr<Process> getRunningProcess() const;

private:
    Shell* shell_instance;
    std::deque<std::shared_ptr<Process>> ready_queue;
    std::vector<std::shared_ptr<Process>> process_list;
    std::shared_ptr<Process> running_process = nullptr;

    SchedulingAlgorithm current_algorithm = SchedulingAlgorithm::FCFS;
    int next_pid = 1;
    int time_slice = 4;
    int current_slice = 0;

    void schedule();
};

#endif // SCHEDULER_H