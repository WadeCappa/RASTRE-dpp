#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

static const std::vector<std::vector<double>> DATA = {
    {4,17,20,5231,4,21},
    {5,7,31,45,3,24},
    {2.63212,5.12566,763,15,215,4},
    {12,6,47,32,74,4},
    {1,2,3,4,5,6},
    {6,31,54,3.5,23,547}
};

// Do to rounding errors, these results may not always be exactly equivalent
static const double LARGEST_ACCEPTABLE_ERROR = 0.00000001;

#include "data_tools/tests.h"
#include "representative_subset_calculator/tests.h"