#include <mutex>
#include <queue>

template <typename T>
class SynchronousQueue {
    private:
    std::queue<T> base;
    std::mutex lock;

    public:
    SynchronousQueue() {}

    T pop() {
        this->lock.lock();
        T res = move(this->base.front());
        this->base.pop();
        this->lock.unlock();

        return move(res);
    }

    void push(T val) {
        this->lock.lock();
        this->base.push(move(val));
        this->lock.unlock();
    }

    const size_t size() {
        this->lock.lock();
        size_t res = this->base.size();
        this->lock.unlock();

        return res;
    }
};