import pandas as pd
import numpy as np
from scipy.sparse import coo_matrix
from sklearn.metrics.pairwise import cosine_similarity
import multiprocessing as mp

topN = 10
topU = 5
# Read CSV file into a pandas DataFrame
dataDir = '/global/cfs/cdirs/m1641/DPP_MAP/'
df = pd.read_csv(dataDir + 'MovieLens/MovieLens.txt', header=None, delim_whitespace=True)  # Assuming no header in the CSV
df.columns =['Items', 'Users'] #ease of use

print("Dataset read")

data = [1] * len(df)  # 1 for each non-zero entry
rows = df['Items']
cols = df['Users']

# Create a sparse matrix in COO format
sparse_matrix = coo_matrix((data, (rows, cols)))
# Items x Items cosine similarity, can be accessed by similarities[i,j]
similarities = cosine_similarity(sparse_matrix, dense_output=False)

print("Similarity matrix constructed")

# Dict of items rated by every user
PU_allUsers = df.groupby('Users')['Items'].apply(set).to_dict()
# Retain only topU active users
PU = dict(sorted(PU_allUsers.items(), key=lambda x: len(x[1]), reverse=True)[:topU])


print("PU constructed")

CU = {}
RU = {}

for user,items in PU.items(): 
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
    
print("CU constructed")

# for user in PU.keys():
#     P_items = list(PU[user])
#     C_items = list(CU[user])
    
#     R_user = [0] * len(C_items)
#     for i, item in enumerate(C_items):
#     # Compute the sum of similarities between elements of P_items and C_items
#         R_user[i] = sum(similarities[item, y] for y in P_items)
    
#     RU[user] = R_user

# Worker function to compute similarities for a single user
# def process_user(user_data):
#     user, P_items, C_items = user_data
#     R_user = []
#     for item in C_items:
#         total_similarity = sum(similarities[item, y] for y in P_items)
#         R_user.append(total_similarity)
#     return user, R_user

# # Parallel computation using multiprocessing Pool

# # Prepare the data for each user to pass to the worker function
# user_data = [(user, list(PU[user]), list(CU[user])) for user in PU.keys()]

# # Create a pool of workers
# with mp.Pool(32) as pool:
#     # Map the worker function to the data
#     results = pool.map(process_user, user_data)

# # Collect the results in the final dictionary
# RU = {user: R_user for user, R_user in results}
    

# print("RU constructed")

