#include <mutex>
#include <deque>

template <typename T>
class SynchronousQueue {
    private:
    std::deque<T> base;
    std::mutex lock;

    public:
    SynchronousQueue() {}

    T pop() {
        this->lock.lock();
        T res = move(this->base.front());
        this->base.pop_front();
        this->lock.unlock();

        return move(res);
    }

    void push(T val) {
        this->lock.lock();
        this->base.push_back(move(val));
        this->lock.unlock();
    }

    const size_t size() {
        this->lock.lock();
        size_t res = this->base.size();
        this->lock.unlock();

        return res;
    }

    typename std::vector<T> emptyQueueIntoVector() {
        std::vector<T> res;
        this->lock.lock();
        while (this->base.size() > 0) {
            res.push_back(move(this->base.front()));
            this->base.pop_front();
        }
        this->lock.unlock();

        return move(res);
    }

    void emptyVectorIntoQueue(std::vector<T> vals) {
        this->lock.lock();
        for (size_t i = 0; i < vals.size(); i++) {
            this->base.push_back(move(vals[i]));
        }
        this->lock.unlock();
    }
};