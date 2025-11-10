#include "scheduler.h"
#include <iostream>
#include <algorithm>

Scheduler::Scheduler(Shell *sh) : shell_instance(sh) {}

void Scheduler::addProcess(const std::string &command)
{
    // 模拟：根据命令类型估算 burst_time
    int burst = 5 + (rand() % 10); // 随机给一个 5-14 的时间
    if (command.find("cat") != std::string::npos)
        burst += 5;
    if (command.find("echo") != std::string::npos)
        burst += 3;

    auto p = std::make_shared<Process>(next_pid++, command, burst);
    ready_queue.push_back(p);
    process_list.push_back(p);
    std::cout << "New Process " << p->pid << ": '" << p->command << "' added. Burst=" << burst << std::endl;
}

void Scheduler::setAlgorithm(SchedulingAlgorithm algo)
{
    current_algorithm = algo;
}

SchedulingAlgorithm Scheduler::getAlgorithm() const
{
    return current_algorithm;
}

void Scheduler::tick()
{
    // 1. 更新所有等待进程的等待时间
    for (auto &p : ready_queue)
    {
        p->waiting_time++;
    }

    // 2. 如果当前没有运行的进程，或者需要调度
    if (!running_process || running_process->state == ProcessState::TERMINATED)
    {
        schedule();
    }

    // 3. 如果有进程在运行
    if (running_process)
    {
        running_process->remaining_time--;
        current_slice++;

        // 4. 检查进程是否执行完毕
        if (running_process->remaining_time <= 0)
        {
            // 后台终端打印日志，表示模拟结束
            std::cout << "[Scheduler] Process " << running_process->pid << " ('" << running_process->command << "') finished simulation." << std::endl;

            running_process->state = ProcessState::TERMINATED;

            running_process = nullptr;
            schedule(); // 立刻调度下一个
        }
        // 5. 检查RR时间片是否用完
        else if (current_algorithm == SchedulingAlgorithm::RR && current_slice >= time_slice)
        {
            // 后台终端打印日志
            std::cout << "[Scheduler] Time slice end for PID " << running_process->pid << ". Back to ready queue." << std::endl;
            running_process->state = ProcessState::READY;
            ready_queue.push_back(running_process); // 放回队尾
            running_process = nullptr;
            schedule(); // 调度下一个
        }
    }
}

void Scheduler::schedule()
{
    if (ready_queue.empty())
    {
        running_process = nullptr;
        return;
    }

    // 根据不同算法选择下一个进程
    switch (current_algorithm)
    {
    case SchedulingAlgorithm::SJF:
        // 找到剩余时间最短的进程
        std::sort(ready_queue.begin(), ready_queue.end(), [](const auto &a, const auto &b)
                  { return a->burst_time < b->burst_time; });
        // fall through to FCFS to pick the first one
    case SchedulingAlgorithm::FCFS:
        running_process = ready_queue.front();
        ready_queue.pop_front();
        break;

    case SchedulingAlgorithm::RR:
        running_process = ready_queue.front();
        ready_queue.pop_front();
        break;
    }

    if (running_process)
    {
        running_process->state = ProcessState::RUNNING;
        current_slice = 0;
        // 后台终端打印日志
        std::cout << "[Scheduler] Running PID " << running_process->pid << " ('" << running_process->command << "')." << std::endl;
    }
}

const std::vector<std::shared_ptr<Process>> &Scheduler::getProcessList() const
{
    return process_list;
}

const std::shared_ptr<Process> Scheduler::getRunningProcess() const
{
    return running_process;
}