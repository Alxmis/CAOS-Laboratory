#include <iostream>
#include <thread>
#include <queue>
#include <random>
#include <vector>
#include <memory>
#include <fstream>

template <typename T>
class BlockingQueue {
private:
    std::queue<int> q;
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;

public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(value);
        }
        cv.notify_one();
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !q.empty() || done; });

        if (!q.empty()) {
            value = q.front();
            q.pop();
            return true;
        } else if (done) {
            return false;
        }
        return false;
    }

    void set_done() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            done = true;
        }
        cv.notify_all();
    }
};

// IProducer Interface
class IProducer {
public:
    virtual void produce() = 0;
    virtual ~IProducer() {}
};

// IConsumer Interface
class IConsumer {
public:
    virtual void consume() = 0;
    virtual ~IConsumer() {}
};

// Logging into file
class Logger {
private:
    std::ofstream log_file;
    std::mutex log_mutex;

public:
    Logger(const std::string& filename) {
        log_file.open(filename, std::ios::out | std::ios::app);
    }

    ~Logger() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }

    void log(const std::string& msg) {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (log_file.is_open()) {
            log_file << msg << std::endl;
        }
    }
};


class Producer : public IProducer {
private:
    int id;
    int num_items;
    BlockingQueue<int>& queue;
    std::mt19937 gen;
    std::uniform_int_distribution<> dis;
    Logger& logger;

public:
    Producer(int id, int num_items, BlockingQueue<int>& queue, Logger& logger)
            : id(id), num_items(num_items), queue(queue), gen(std::random_device{}()), dis(1, 100), logger(logger) {}

    void produce() override {
        for (int i = 0; i < num_items; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen) % 100));
            int item = dis(gen);
            queue.push(item);
            std::string log_message = "Producer " + std::to_string(id) + " produced " + std::to_string(item);
            std::cout << log_message << std::endl;
            logger.log(log_message);
        }

        queue.set_done();
    }
};


class Consumer : public IConsumer {
private:
    int id;
    BlockingQueue<int>& queue;
    std::mt19937 gen;
    std::uniform_int_distribution<> dis;
    Logger& logger;

public:
    Consumer(int id, BlockingQueue<int>& queue, Logger& logger)
            : id(id), queue(queue), gen(std::random_device{}()), dis(1, 100), logger(logger) {}

    void consume() override {
        while (true) {
            int item;
            if (queue.pop(item)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
                std::string log_message = "Consumer " + std::to_string(id) + " consumed " + std::to_string(item);
                std::cout << log_message << std::endl;
                logger.log(log_message);
            } else {
                break;
            }
        }
    }
};


int main() {
    const int num_producers = 3;
    const int num_consumers = 3;
    const int num_items = 10;

    BlockingQueue<int> queue;
    Logger logger("log.txt");

    std::vector<std::shared_ptr<IProducer>> producers;
    std::vector<std::shared_ptr<IConsumer>> consumers;

    std::vector<std::thread> producer_threads;
    std::vector<std::thread> consumer_threads;

    for (int i = 0; i < num_producers; ++i) {
        auto producer = std::make_shared<Producer>(i, num_items, queue, logger);
        producers.push_back(producer);
        producer_threads.emplace_back(&IProducer::produce, producer);
    }

    for (int i = 0; i < num_consumers; ++i) {
        auto consumer = std::make_shared<Consumer>(i, queue, logger);
        consumers.push_back(consumer);
        consumer_threads.emplace_back(&IConsumer::consume, consumer);
    }

    for (auto& p : producer_threads) {
        p.join();
    }

    for (auto& c : consumer_threads) {
        c.join();
    }

    std::cout << "All tasks are completed." << std::endl;

    return 0;
}