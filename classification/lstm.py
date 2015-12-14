import numpy as np
import matplotlib.pyplot as plt
import glob
import os

from collections import defaultdict

from keras.models import Sequential
from keras.layers.normalization import BatchNormalization
from keras.layers.core import Dense, TimeDistributedDense, Dropout, Activation, Masking
from keras.layers.recurrent import LSTM, GRU
from keras.layers.embeddings import Embedding
from keras.optimizers import RMSprop

from sklearn.metrics import roc_auc_score
from sklearn.cross_validation import train_test_split

THRESH = 0
MAX_EPOCH = 20
MAX_BB = 100


def get_csv_list(folder):
    '''Get a list of CSV files out of a folder with glob'''
    return glob.glob(os.path.join(folder, '*.csv'))


def load_features(filename):
    data = []
    y = []
    with open(filename) as f:
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

    X = np.ones((len(data), MAX_BB, 100)) * -1
    for i, d in enumerate(data):
        steps = d.shape[0]
        X[i, 0:steps, :] = d
    y = np.array(y)
    y = (np.arange(2) == y[:,None]).astype(int).reshape((-1, 2, 1))
    y = np.tile(y, (1, 1, MAX_BB))
    y = np.swapaxes(y, 1, 2)
    return X, y


def cv_on_filelist(files):
    '''Run cross validation at the file level
    
    Arguments
    ---------
    files : list
        List of filenames containing a CSV
        
    Returns
    -------
    all_auc : list
        List of AUC values for each folds
        
    all_acc : list
        List of accuracy values for each fold
    '''
    all_acc = []
    all_auc = []
    for i, f in enumerate(files):
        test_files = [f]
        train_files = list(files)
        train_files.remove(f)
    
        train_X, train_y = combine_files(train_files)
        test_X, test_y = combine_files(test_files)
   
        yreal = test_y[:,:,0].mean(axis=1)
        print("Now classifying, this is fold {}/{}".format(i+1, len(files))) 
        if np.sum(yreal) > 0:
            metrics = train_model(train_X, train_y, test_X, test_y)
            print(metrics)
            trainidx = np.argmax(metrics['train_auc'])
            auc = np.max(metrics['auc'])
            acc = metrics['val_acc'][-1]

            all_acc.append(acc)
            all_auc.append(auc)
            print(all_auc)
            print(np.mean(all_auc))
        else:
            print("Skipping this fold")
    return all_auc, all_acc


def combine_files(files):
    '''Combine a set of CSVs into one large array'''
    Xs = []
    ys = []
    for f in files:
        X, y = load_features(f)
        Xs.append(X)
        ys.append(y)
    allX = np.concatenate(Xs, axis=0)
    ally = np.concatenate(ys, axis=0)
    return allX, ally

def train_model(train_X, train_y, test_X, test_y):
    print("Rebuilding model!")
    print("We have {} train examples and {} test examples".format(train_X.shape[0], test_X.shape[0]))
    model = load_model()
    metric_vals = defaultdict(list)

    for E in range(MAX_EPOCH):
        print("Training epoch {}".format(E))
        hist = model.fit(train_X, train_y, nb_epoch=1, batch_size=256, show_accuracy=True, validation_data=(test_X, test_y), verbose=0)

        # Calc AUC
        preds = model.predict(test_X)[:, :, 0].mean(axis=1)
        train_preds = model.predict(train_X)[:, :, 0].mean(axis=1)
        yreal = test_y[:,:,0].mean(axis=1)
        train_yreal = train_y[:,:,0].mean(axis=1)

        auc = roc_auc_score(yreal, preds)
        train_auc = roc_auc_score(train_yreal, train_preds)
        metric_vals['auc'].append(auc)
        metric_vals['train_auc'].append(train_auc)
        print("AUC: {0:.3f} ({1:.3f} Train)".format(auc, train_auc))

        # Calc other metrics
        metrics = hist.params['metrics']
        for metric in metrics:
            metric_vals[metric].append(hist.history[metric])
    return metric_vals

def load_model():
    model = Sequential()
    model.add(Masking(-1, input_shape=(20, 100)))
    model.add(LSTM(256, return_sequences=True))
    model.add(Dropout(0.7))
    model.add(LSTM(256, return_sequences=True))
    model.add(Dropout(0.7))
    model.add(TimeDistributedDense(2))
    model.add(Activation('time_distributed_softmax'))

    prop = RMSprop(lr=0.0002)
    model.compile(loss='categorical_crossentropy', optimizer=prop)
    return model


def main():
    files = get_csv_list('../scripts/features_files_lstm_all/')
    X = combine_files(files)
    
    all_auc, all_acc = cv_on_filelist(files)
    import pdb; pdb.set_trace()



if __name__ == "__main__":
    main()
