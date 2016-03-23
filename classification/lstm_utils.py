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
    print('generate matrix for ' + filename)
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
            ids.append(fn + " " + ID)

    X = np.ones((len(data), max_bb, 100)) * 0
    for i, d in enumerate(data):
        steps = d.shape[0]
        X[i, 0:steps, :] = d
    y = np.array(y)
    y = (np.arange(2) == y[:,None]).astype(int).reshape((-1, 2, 1))
    y = np.tile(y, (1, 1, max_bb))
    y = np.swapaxes(y, 1, 2)
    return X, y, ids

def get_mask(X):
    mask_sum = X.sum(axis=2)
    mask_len = np.zeros(len(X))
    for r in range(len(X)):
        zero_lens = np.where(mask_sum[r, :] == 0)
        if len(zero_lens[0]):
            mask_len[r] = zero_lens[0][0]
        else:
            mask_len[r] = len(mask_sum[r, :])
            
    return mask_len


def calc_masked_means(mask_len, preds):
    mean_preds = np.zeros(len(mask_len))
    last_preds = np.zeros(len(mask_len))
    for idx, mask in enumerate(mask_len):
        if mask == 0:
            mean = 0
            last = 0
        else:
            mean = np.mean(preds[idx, 0:mask])
            last = preds[idx, mask-1]
        mean_preds[idx] = mean
        last_preds[idx] = last
    return mean_preds, last_preds

def load_model(fold_no):
    # Save the output to JSON
    with open('saved_model_arch_{}.json'.format(fold_no), 'r') as f:
        model = model_from_json(f.read())
    model.load_weights('saved_model_weights_{}.h5'.format(fold_no))
    return model


def infer_to_dataframe(filename, model):
    X, y, ids = load_features(filename, args.b)
    y_hat = model.predict(X, verbose=1)[:, :, 1]
    mask_len = get_mask(X)
    _, last_preds = calc_masked_means(mask_len, y_hat)
    #print("y: ")
    #print(y)
    #print("ids: ")
    #print(ids)
    #print(last_preds)
    frame = pd.DataFrame(data={'yhat': last_preds}, index=ids)
    print(frame)
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
