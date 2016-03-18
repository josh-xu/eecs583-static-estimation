import numpy as np
import matplotlib.pyplot as plt
import glob
import os
import sys
import argparse

import pandas as pd

from keras.models import model_from_json

THRESH = 0

def load_features(filename, max_bb):
    data = []
    y = []
    ids = []
    print('generate matrix')
    with open(filename) as f:
        while True:
            line = next(f, None)
            if not line:
                break

            fn, ID, truth, n_bb = line.strip('\n').split(' ')

            truth = int(truth)
            n_bb = int(n_bb)

            text_bbs = [next(f).strip('\n') for x in range(n_bb)]
            num_bbs = [[int(v) for v in d.split(',')] for d in text_bbs]
            bb_data = np.array(num_bbs)
            next(f) # Should be empty line
            if truth > THRESH:
                y.append(1)
            else:
                y.append(0)
            data.append(bb_data)
            ids.append(int(ID))

    X = np.ones((len(data), max_bb, 100)) * 0
    for i, d in enumerate(data):
        steps = d.shape[0]
        X[i, 0:steps, :] = d
    y = np.array(y)
    y = (np.arange(2) == y[:,None]).astype(int).reshape((-1, 2, 1))
    y = np.tile(y, (1, 1, max_bb))
    y = np.swapaxes(y, 1, 2)
    return X, y, ids


def load_model(fold_no):
    # Save the output to JSON
    with open('saved_model_arch_{}.json'.format(fold_no), 'r') as f:
        model = model_from_json(f.read())
    model.load_weights('saved_model_weights_{}.h5'.format(fold_no))
    return model


def infer_to_dataframe(filename, model):
    X, y, ids = load_features(filename, args.b)
    y_hat = model.predict(X, verbose=1)
    #print("y: ")
    #print(y)
    #print("ids: ")
    #print(ids)
    print(y_hat)
    frame = pd.DataFrame(data={'yhat': y_hat}, index=ids)
    return frame


def main():
    global args
    parser = argparse.ArgumentParser(description='Save predictions from LSTM model, given feature input')
    parser.add_argument('-i', help='Feature input filename')
    parser.add_argument('-o', help='Output CSV mapping IDs to probabilities')
    parser.add_argument('-b', type=int, help='Max number of basic blocks')
    args = parser.parse_args()

    model = load_model(0)
    frame = infer_to_dataframe(args.i, model)
    frame.to_csv(args.o, ',')
    print('Wrote output to {}'.format(args.o))


if __name__ == "__main__":
    main()
