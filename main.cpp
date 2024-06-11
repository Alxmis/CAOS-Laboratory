#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <random>
#include <chrono>

std::queue<int> q;
std::mutex mtx;
std::condition_variable cv;
bool done = false;

void producer(int id, int num_items) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);

    for (int i = 0; i < num_items; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen) % 100));
        int item = dis(gen);
        {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(item);
            std::cout << "Producer " << id << " produced " << item << std::endl;
        }
        cv.notify_one();
    }

    // Индикатор завершения работы производителя
    {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
    }
    cv.notify_all();
}

void consumer(int id) {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return !q.empty() || done; });

        if (!q.empty()) {
            int item = q.front();
            q.pop();
            lock.unlock();
            std::cout << "Consumer " << id << " consumed " << item << std::endl;
        } else if (done) {
            break;
        }
    }
}

int main() {
    const int num_producers = 3;
    const int num_consumers = 3;
    const int num_items = 10;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back(producer, i, num_items);
    }

    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back(consumer, i);
    }

    for (auto& p : producers) {
        p.join();
    }

    for (auto& c : consumers) {
        c.join();
    }

    std::cout << "All tasks are completed." << std::endl;

    return 0;
}
