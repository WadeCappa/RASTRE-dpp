#include <chrono>
#include <vector>

#include <nlohmann/json.hpp>

class Timers {
    public:
    class SingleTimer {
        private:
        std::chrono::high_resolution_clock::time_point start;
        std::vector<std::chrono::duration<double>> timings;

        public:
        void startTimer() {
            start = std::chrono::high_resolution_clock::now();
        }

        void stopTimer() {
            auto stop = std::chrono::high_resolution_clock::now();
            timings.push_back(stop - start);
        }

        double getTotalTime() const {
            double res = 0;
            for (const auto & duration : timings) {
                res += duration.count();
            }

            return res;
        }
    };

    SingleTimer barrierTime;
    SingleTimer totalCalculationTime;
    SingleTimer localCalculationTime;
    SingleTimer globalCalculationTime;
    SingleTimer communicationTime;
    SingleTimer bufferEncodingTime;
    SingleTimer bufferDecodingTime;
    SingleTimer loadingDatasetTime;
    SingleTimer initBucketsTimer;
    SingleTimer insertSeedsTimer;
    SingleTimer waitingTime;

    nlohmann::json outputToJson() const {
        nlohmann::json output {
            {"barrierTime", barrierTime.getTotalTime()},
            {"totalCalculationTime", totalCalculationTime.getTotalTime()},
            {"localCalculationTime", localCalculationTime.getTotalTime()},
            {"globalCalculationTime", globalCalculationTime.getTotalTime()},
            {"communicationTime", communicationTime.getTotalTime()},
            {"bufferEncodingTime", bufferEncodingTime.getTotalTime()},
            {"bufferDecodingTime", bufferDecodingTime.getTotalTime()},
            {"initBucketsTime", initBucketsTimer.getTotalTime()},
            {"insertSeedsTime", insertSeedsTimer.getTotalTime()},
            {"loadingDatasetTime", loadingDatasetTime.getTotalTime()},
            {"waitingTime", waitingTime.getTotalTime()}
        };
    
        return output;
    }
};