import pandas as pd
import numpy as np
from scipy.sparse import coo_matrix
from sklearn.metrics.pairwise import cosine_similarity

topN = 10
# Read CSV file into a pandas DataFrame
dataDir = '/global/cfs/cdirs/m1641/DPP_MAP/'
df = pd.read_csv(dataDir + 'MovieLens/MovieLens.txt', header=None, delim_whitespace=True)  # Assuming no header in the CSV
df.columns =['Items', 'Users'] #ease of use

# df = df.tail(50)
# print(df)

data = [1] * len(df)  # 1 for each non-zero entry
rows = df['Items']
cols = df['Users']

# Create a sparse matrix in COO format
sparse_matrix = coo_matrix((data, (rows, cols)))
# Items x Items cosine similarity, can be accessed by similarities[i,j]
similarities = cosine_similarity(sparse_matrix, dense_output=False)

# Dict of items rated by every user
PU = df.groupby('Users')['Items'].apply(set).to_dict()
# print(PU[12345])

CU = {}
RU = {}

for user in range(5): #range(max(df['Users'])):
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


for user in range(5):
    R_User = []
    for C_item in CU[user]:
        relevance_C_item = 0
        for P_item in PU[user]: 
            relevance_C_item = relevance_C_item + similarities[C_item,P_item]
        R_User.append(relevance_C_item)
    RU[user] = R_User


print(len(CU[4]))
print(len(RU[4]))
