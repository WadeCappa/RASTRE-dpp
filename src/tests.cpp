#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

static const std::vector<std::vector<double>> DATA = {
    {4,17,20},
    {5,7,31},
    {2.63212,5.12566,763}
};

#include "data_tools/tests.h"
#include "similarity_matrix/tests.h"