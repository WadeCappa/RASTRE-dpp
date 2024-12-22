import pandas as pd
import numpy as np
from scipy.sparse import coo_matrix
from sklearn.metrics.pairwise import cosine_similarity
import multiprocessing as mp
import sys

def calcualteRU(PU, CU, similarities):
    RU = {}
    matrix = similarities.toarray()
    for user in PU.keys():
        RU[user] = []
        for i in CU[user]:
            s = 0
            for j in PU[user]:
                s += float(matrix[i, j])
            RU[user].append(s)
    return RU

def outputUserData(topN, topU, infile, outfile):
    df = getDataset(infile)
    print("Dataset read")

    if (df.shape[1] == 2):
        df.columns =['Items', 'Users'] #ease of use
        data = [1] * len(df)  # 1 for each non-zero entry
    else:
        df.columns =['Items', 'Users', 'data'] #ease of use
        data = df['data']

    rows = df['Items']
    cols = df['Users']

    # Create a sparse matrix in COO format
    sparse_matrix = coo_matrix((data, (rows, cols)))
    
    # Items x Items cosine similarity, can be accessed by similarities[i,j]
    similarities = cosine_similarity(sparse_matrix, dense_output=False)

    print("Similarity matrix constructed")

    # Dict of items rated by every user
    PU_allUsers = df.groupby('Users')['Items'].apply(set).to_dict()

    PU_test = {}

    for user,items in PU_allUsers.items():
        PU_test[user] = items.pop()

    # Retain only topU active users. We should also be able to support looking at specific 
    # rows here too
    PU = dict(sorted(PU_allUsers.items(), key=lambda x: len(x[1]), reverse=True)[:topU])

    print("PU constructed")

    CU = {}
    RU = {}

    for user,items in PU.items(): 
        if (PU_test[user] in items):
            print("This shouldn't be happening")
        C_user = set()
        for item in PU[user]: 
            similaritiesPerElement = similarities[item].toarray().flatten()  # Convert sparse row to dense array

            # Get the indices of the top N similar items (excluding self-similarity if necessary)
            # Exclude the item itself by setting its similarity to a low value (e.g., -1)
            similaritiesPerElement[item] = -1  # Exclude the item itself from being its own top similar item

            # Get the indices of the top N similar items
            top_n_indices = set(np.argsort(similaritiesPerElement)[-topN:][::-1])  # Sort, get last N indices, reverse to get descending order
            C_user.update(top_n_indices)
        
        CU[user] = list(C_user)
        print("PU size:")
        print(len(PU[user]))
        print("CU size:")
        print(len(CU[user]))
        
    print("CU constructed")

    LU = {}
    RU = calcualteRU(PU, CU, similarities)

    # Order of information per row
    # UID TID LPU PU1 PU2 ... PU_LPU LCU CU1 CU2 ... CU_LCU RU1 ... RU_LRU
    # UID: User ID
    # TID: Test item excluded from PU for that user
    # LCU: Length of CU
    # CU1 ... CU_LCU: Items in CU (total LCU items)
    # RU1 ... RU_LRU: Items in RU which is also of size LCU
    with open(outfile, "w") as f:
        for user,items in PU.items():
            l = []
            l.append(user)
            l.append(PU_test[user])
            l.append(len(CU[user]))
            l.extend(CU[user])
            l.extend(RU[user])

            LU[user] = l
            print("LU size:")
            print(len(LU[user]))
            np.savetxt(f, [np.array(l)], fmt='%d', delimiter=' ')

def getDataset(filepath_input):
    return pd.read_csv(filepath_input, header=None, delim_whitespace=True)  # Assuming no header in the CSV

if __name__ == "__main__":
    topN = int(sys.argv[1])
    topU = int(sys.argv[2])
    # Read CSV file into a pandas DataFrame
    filepath_input = sys.argv[3]
    filepath_output = sys.argv[4]
    outputUserData(topN, topU, filepath_input, filepath_output)