import sys
import random

def generateSparse(rows: int, cols: int, sparsity: float, alwaysOne: bool):
    print("generating sparse")
    for r in range(rows):
        for c in range(cols):
            if random.uniform(0,1) > sparsity:
                yield [r, c, 1 if alwaysOne else random.uniform(0,1)]

def generateDense(rows: int, cols: int):
    print("generating dense")
    return ([random.uniform(0,1) for c in range(cols)] for r in range(rows))

def outputDataset(dataset, file):
    print(dataset)
    with open(file, "w") as f:
        for d in dataset:
            f.write(" ".join([str(x) for x in d]) + '\n')

if __name__ == "__main__": 
    rows = int(sys.argv[3])
    cols = int(sys.argv[4])
    output = sys.argv[1]
    denseOrSparse = int(sys.argv[2])
    if denseOrSparse == 0:
        outputDataset(generateDense(rows, cols), output)
    elif denseOrSparse == 1:
        sparsity = float(sys.argv[5])
        alwaysOne = int(sys.argv[6])
        outputDataset(generateSparse(rows, cols, sparsity, True if alwaysOne == 1 else False), output)
    else:
        print("bad input")