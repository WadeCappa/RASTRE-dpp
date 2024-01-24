#include "data_tools/normalizer.h"
#include "data_tools/matrix_builder.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/orchestrator/single_machine_orchestrator.h"

#include <CLI/CLI.hpp>
#include "nlohmann/json.hpp"


int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset."};
    Orchestrator* orchestrator = SingleMachineOrchestrator::build(argc, argv, app);
    CLI11_PARSE(app, argc, argv);

    orchestrator->runJob();
    return 0;
}