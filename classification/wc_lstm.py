import numpy as np
import matplotlib.pyplot as plt

from collections import defaultdict

from keras.models import Sequential
from keras.layers.core import Dense, TimeDistributedDense, Dropout, Activation, Masking
from keras.layers.recurrent import LSTM
from keras.layers.embeddings import Embedding
from keras.optimizers import RMSprop

from sklearn.metrics import roc_auc_score
from sklearn.cross_validation import train_test_split

THRESH = 0
EPOCH = 1000
MAX_BB = 20


def load_model():
    model = Sequential()
    model.add(Masking(0, input_shape=(20, 100)))
    model.add(LSTM(192, return_sequences=True))
    model.add(LSTM(192, return_sequences=True))
    model.add(TimeDistributedDense(2))
    model.add(Activation('time_distributed_softmax'))

    prop = RMSprop(lr=0.0002) 
    model.compile(loss='categorical_crossentropy', optimizer=prop)
    return model

def load_features():
    data = []
    y = []
    with open("lstm.csv") as f:
        while True:
            line = next(f, None)
            if not line:
                break

            ID, truth, n_bb = line.strip('\n').split(' ')

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

    X = np.zeros((len(data), MAX_BB, 100))
    for i, d in enumerate(data):
        steps = d.shape[0]
        X[i, 0:steps, :] = d
    y = np.array(y)
    y = (np.arange(2) == y[:,None]).astype(int).reshape((-1, 2, 1))
    y = np.tile(y, (1, 1, 20))
    y = np.swapaxes(y, 1, 2)
    return X, y


def calc_masked_means(mask_len, preds):
    mean_preds = np.zeros(len(mask_len))
    last_preds = np.zeros(len(mask_len))
    for idx, mask in enumerate(mask_len):
        if mask == 0:
            mean = 0
        else:
            mean = np.mean(preds[idx, 0:mask])
            last = preds[idx, mask-1]
        mean_preds[idx] = mean
        last_preds[idx] = last
    return mean_preds, last_preds


def calc_auc(model, X, y):
    preds = model.predict(X)[:, :, 0]
    mask_len = (X[:,:,0]!=0).sum(axis=1)
    cut_preds = calc_masked_means(mask_len, preds)
    yreal = y[:,:,0].mean(axis=1)
    auc = roc_auc_score(yreal, cut_preds)
    return auc


def main():
    X, y = load_features()
    model = load_model()
    y_perline = y[:,:,0].mean(axis=1)
    train_X, test_X, train_y, test_y = train_test_split(
        X, y, test_size=0.20, stratify=y_perline)

    metric_vals = defaultdict(list)

    # Calc start AUC
    auc = calc_auc(model, test_X, test_y)
    print("AUC: {0:.3f}".format(auc))


    for E in range(EPOCH):
        print("Training epoch {}".format(E))
        hist = model.fit(train_X, train_y, nb_epoch=1, batch_size=64, show_accuracy=True, validation_data=(test_X, test_y), verbose=0)

        # Calc AUC
        auc = calc_auc(model, test_X, test_y)
        metric_vals['auc'].append(auc)
        print("AUC: {0:.3f}".format(auc))

        # Calc other metrics
        metrics = hist.params['metrics']
        for metric in metrics:
            metric_vals[metric].append(hist.history[metric])

    plt.figure()

    plt.subplot(1,2,1)
    plt.plot(range(EPOCH), metric_vals['acc'], range(EPOCH), metric_vals['val_acc'], range(EPOCH), metric_vals['auc'])
    plt.legend(['Acc', 'Val Acc', 'AUC'])

    plt.subplot(1,2,2)
    plt.plot(range(EPOCH), metric_vals['loss'], range(EPOCH), metric_vals['val_loss'])
    plt.legend(['Loss', 'Val Loss'])
    plt.show()


if __name__ == "__main__":
    main()
