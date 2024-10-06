#include <mutex>
#include <deque>

template <typename T>
class SynchronousQueue {
    private:
    std::deque<T> base;
    std::mutex lock;
    std::atomic_bool emptyFlag;

    public:
    SynchronousQueue() : emptyFlag(false) {}

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
        emptyFlag.store(false);
        this->lock.unlock();
    }

    const size_t size() const {
        return this->base.size();
    }

    typename std::vector<T> emptyQueueIntoVector() {
        std::vector<T> res;
        this->lock.lock();
        while (this->base.size() > 0) {
            res.push_back(move(this->base.front()));
            this->base.pop_front();
        }
        emptyFlag.store(false);
        this->lock.unlock();

        return move(res);
    }

    void emptyVectorIntoQueue(std::vector<T> vals) {
        this->lock.lock();
        for (size_t i = 0; i < vals.size(); i++) {
            this->base.push_back(move(vals[i]));
        }
        emptyFlag.store(this->base.size() > 0);
        this->lock.unlock();
    }

    bool isEmpty() const {
        return this->emptyFlag.load();
    }
};