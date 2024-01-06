#include <vector>
#include <string>
#include <iostream>
 
class DataSaver {
    public:
    virtual void save(std::ostream &out) = 0;
};

class IncrementalDataSaver : public DataSaver {
    private:
    DataLoader &data;
    DataWriter &writer;

    public:
    IncrementalDataSaver(DataWriter &writer, DataLoader &data) : writer(writer), data(data) {}

    void save(std::ostream &out) {
        std::vector<double> element;

        while (this->data.getNext(element)) {
            this->writer.write(element, out);
        }
    }
};

std::ostream& operator<<(std::ostream &out, DataSaver &data) {
    data.save(out);
    return out;
}