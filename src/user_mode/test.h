
#include <string>  
#include <iostream> 
#include <sstream>  

TEST_CASE("Testing LAZYFAST set finder") {
    const double u_id = 11;
    const double t_id = 54;
    const double l_cu = 3;
    const std::vector<double> cu({43.000, 99.000, 4532.0000});
    const std::vector<double> ru({4.124, 16.6213, 9.2313});
    std::stringstream data;
    data << u_id << " " << t_id << " " << l_cu << " ";
    for (const auto cu_i : cu) {
        data << cu_i << " ";
    }
    for (const auto ru_i : ru) {
        data << ru_i << " ";
    }

    std::vector<std::unique_ptr<UserData>> users (UserDataImplementation::load(data));
    CHECK(users.size() == 1);
    std::unique_ptr<UserData> user(move(users[0]));
    CHECK(user->getUserId() == u_id);
    CHECK(user->getTestId() == t_id);
    for (size_t i = 0; i < l_cu; i++) {
        CHECK(user->getCu()[i] == cu[i]);
        CHECK(user->getRu()[i] == ru[i]);
    }
}