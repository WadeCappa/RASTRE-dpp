#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <iostream>
#include <random>
#include <chrono>
#include <unsupported/Eigen/SparseExtra>
#include <fstream>

typedef Eigen::SparseMatrix<float, Eigen::RowMajor> SparseMatrix;
typedef Eigen::MatrixXf DenseMatrix;

int main(int argc, char* argv[]) {

    auto begin = std::chrono::high_resolution_clock::now(); 
    const int rows = std::atoi(argv[1]);  // Number of rows
    const int cols = std::atoi(argv[2]); // Number of columns
    const double sparsity = 0.01;  // Fraction of non-zero elements

    // Step 1: Generate a dense random matrix
    DenseMatrix denseMatrix = DenseMatrix::Random(rows, cols);

    // Step 2: Thresholding for sparsity
    SparseMatrix sparseMatrix(rows, cols);
    #pragma omp parallel for
    for (int i = 0; i < denseMatrix.rows(); ++i) {
        for (int j = 0; j < denseMatrix.cols(); ++j) {
            if (std::abs(denseMatrix(i, j)) < 1 - sparsity) {
                denseMatrix(i, j) = 0.0;  // Set element to zero based on sparsity threshold
            } else {
                denseMatrix(i, j) = 1.0;
            }
        }
    }

    // Step 3: Convert the dense matrix to a sparse matrix
    sparseMatrix = denseMatrix.sparseView();  // Convert dense to sparse
    auto end = std::chrono::high_resolution_clock::now();    
    auto dur = end - begin;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    std::cout << "Generated random sparse matrix with "
              << sparseMatrix.rows() << " rows, " << sparseMatrix.cols() << " cols, "
              << sparseMatrix.nonZeros() << " non-zero elements in " << ms << " milliseconds\n";


    begin = std::chrono::high_resolution_clock::now();
    std::ofstream outFile("sparse_matrix.csv");
    if (outFile.is_open()) {
        // Write non-zero values in row-major order
        // outFile << "row,col,value\n";  // Optional: header line for the CSV

        for (int k = 0; k < sparseMatrix.outerSize(); ++k) {
            for (SparseMatrix::InnerIterator it(sparseMatrix, k); it; ++it) {
                outFile << it.row() << " " << it.col() << " " << it.value() << "\n";
            }
        }

        outFile.close();
        std::cout << "Matrix saved to sparse_matrix.csv.\n";
    } else {
        std::cerr << "Error opening file for writing.\n";
    }
    end = std::chrono::high_resolution_clock::now();  
    dur = end - begin;
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    std::cout << "Time to write matrix to csv file: " << ms << " milliseconds\n";


    // In order to generate in Matrix Market format
    // begin = std::chrono::high_resolution_clock::now();
    // if (Eigen::saveMarket(sparseMatrix, "sparse_matrix.mtx")) {
    //     std::cout << "Matrix saved to sparse_matrix.mtx in Matrix Market format.\n";
    // } else {
    //     std::cerr << "Error saving matrix to file.\n";
    // }
    // end = std::chrono::high_resolution_clock::now();  
    // dur = end - begin;
    // ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    // std::cout << "Time to write matrix to mtx file: " << ms << " milliseconds\n";

    return 0;
}

