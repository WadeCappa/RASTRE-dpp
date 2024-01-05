#include <vector>
#include <string>
#include <iostream>
 
class DataSaver {
    public:
    virtual void save(std::ostream &out) = 0;
};

class FormatingDataSaver : public DataSaver {
    private:
    DataLoader &data;
    const DataFormater &formater;

    public:
    FormatingDataSaver(const DataFormater &formater, DataLoader &data) : formater(formater), data(data) {}

    void save(std::ostream &out) {
        std::vector<double> element;

        while (this->data.getNext(element)) {
            out << this->formater.elementToString(element) << std::endl;
        }
    }
};

std::ostream& operator<<(std::ostream &out, DataSaver &data) {
    data.save(out);
    return out;
}