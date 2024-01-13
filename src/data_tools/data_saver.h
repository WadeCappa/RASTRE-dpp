#include <vector>
#include <string>
#include <optional>
#include <iostream>

#define assertm(exp, msg) assert(((void)msg, exp))
 
class DataSaver {
    public:
    virtual void save(std::ostream &out) = 0;
};

class AsciiDataSaver : public DataSaver {
    private:
    DataLoader &data;

    public:
    AsciiDataSaver(DataLoader &data) : data(data) {}

    void save(std::ostream &out) {
        std::vector<double> element;

        while (this->data.getNext(element)) {
            std::ostringstream outputStream;
            for (const auto & v : element) {
                outputStream << fmt::format("{}", v) << ',';
            }
            std::string output = outputStream.str();
            output.pop_back();
            out << output << std::endl;
        }
    }
};

class BinaryDataSaver : public DataSaver {
    private:
    DataLoader &data;
    std::optional<unsigned int> vectorSize;

    public:
    BinaryDataSaver(DataLoader &data) : data(data) {
        this->vectorSize = std::nullopt;
    }

    void save(std::ostream &out) {
        std::vector<double> element;

        while (this->data.getNext(element)) {
            if (!this->vectorSize.has_value()) {
                this->vectorSize = element.size();
                unsigned int size = this->vectorSize.value();
                out.write(reinterpret_cast<const char *>(&size), sizeof(size));
            }
            
            assertm(element.size() == this->vectorSize.value(), "Mismatch on write size.");
            out.write(reinterpret_cast<const char *>(element.data()), this->vectorSize.value() * sizeof(double));
        }
    }
};

std::ostream& operator<<(std::ostream &out, DataSaver &data) {
    data.save(out);
    return out;
}