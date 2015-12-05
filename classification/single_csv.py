import pandas as pd
import numpy as np

from sklearn.cross_validation import StratifiedKFold
from sklearn.metrics import roc_auc_score

from keras.models import Sequential
from keras.layers.core import Dense, Dropout, Activation
from keras.optimizers import SGD

N_FEATURES = 15
N_EPOCH = 40
THRESH = 0


def build_model():
    """Build a simple model with one hidden layer and a softmax output"""
    model = Sequential()
    model.add(Dense(128, input_dim=N_FEATURES, init='uniform', activation='tanh'))
    model.add(Dense(2, init='uniform', activation='softmax'))

    model.compile(loss='categorical_crossentropy', optimizer='rmsprop')
    return model


def main():
    """
    Train an MLP on our data for a set number of epochs. Note there is 
    no cross validation or anything! This is terrible methodology, but is 
    more a proof of concept. The model is simple enough we aren't going to fit
    perfectly, and once we get all the benchmarks we can use this code for cross
    validation with a real MLP model (w dropout, batch normalization, relu, etc)

    Also note that the class imbalance is enormous here, therefore I used AUC as 
    the metric for discrimination. Even so, maybe in the future we'll want to weight
    positive instances much higher than negative.
    """
    # Compile the MLP
    model = build_model()
    
    aucs = []
    for E in range(N_EPOCH):
        # Read the csv but in chunks because it's so big
        reader = pd.read_csv('feature_output.csv', sep=',', chunksize=500000, index_col=0)
        for i, chunk in enumerate(reader):
            y = (chunk['RealCount'] > THRESH).astype(int).values
            y = (np.arange(2) == y[:,None]).astype(int)
            X = (chunk.iloc[:, 1:]).values

            model.train_on_batch(X, y)
            print("Training on batch {}".format(i))

        # Run testing in batches
        y_real = []
        y_hat = []
        reader = pd.read_csv('feature_output.csv', sep=',', chunksize=1000000, index_col=0)
        for i, chunk in enumerate(reader):
            y = (chunk['RealCount'] > THRESH).astype(int).values
            y = (np.arange(2) == y[:,None]).astype(int)
            X = (chunk.iloc[:, 1:]).values

            pred = model.predict(X)
            print("Testing on batch {}".format(i))

            y_real.extend(y[:, 0].tolist())
            y_hat.extend(pred[:, 0].tolist())

        auc = roc_auc_score(y_real, y_hat)
        aucs.append(auc)
        print("AUC for EPOCH {}: {}".format(E, auc))

    print(aucs)

if __name__ == "__main__":
    main()
