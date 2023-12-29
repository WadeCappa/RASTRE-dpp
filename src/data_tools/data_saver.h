#include <vector>
#include <string>
#include <iostream>
 
class DataSaver {
    public:
    virtual void save(std::ostream &out) = 0;
};

class AsciiDataSaver : public DataSaver {
    private:
    DataLoader &data;
    
    std::string elementToString (const std::vector<double> &element) {
        std::string output = "";
        for (const auto & v : element) {
            output += std::to_string(v) + ",";
        }
        output.pop_back();
        return output;
    }

    public:
    AsciiDataSaver(DataLoader &data) : data(data) {}

    void save(std::ostream &out) {
        std::vector<double> element;

        while (this->data.getNext(element)) {
            out << this->elementToString(element) << std::endl;
        }
    }
};

std::ostream& operator<<(std::ostream &out, DataSaver &data) {
    data.save(out);
    return out;
}