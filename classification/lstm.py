import numpy as np
import matplotlib.pyplot as plt
import glob
import os

from collections import defaultdict

from keras.models import Sequential
from keras.callbacks import EarlyStopping
from keras.layers.normalization import BatchNormalization
from keras.layers.core import Dense, TimeDistributedDense, Dropout, Activation, Masking
from keras.layers.recurrent import LSTM, GRU
from keras.layers.embeddings import Embedding
from keras.optimizers import RMSprop

from sklearn.preprocessing import scale, StandardScaler
from sklearn.metrics import roc_auc_score
from sklearn.cross_validation import train_test_split

THRESH = 0
MAX_EPOCH = 10
MAX_BB = 70


def get_csv_list(folder):
    '''Get a list of CSV files out of a folder with glob'''
    return glob.glob(os.path.join(folder, '*.csv'))


def load_features(filename):
    data = []
    y = []
    print('generate matrix')
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

    X = np.ones((len(data), MAX_BB, 100)) * 0
    for i, d in enumerate(data):
        steps = d.shape[0]
        X[i, 0:steps, :] = d
    y = np.array(y)
    y = (np.arange(2) == y[:,None]).astype(int).reshape((-1, 2, 1))
    y = np.tile(y, (1, 1, MAX_BB))
    y = np.swapaxes(y, 1, 2)
    return X, y


def get_mask(X):
    mask_sum = X.sum(axis=2)
    mask_len = np.zeros(len(X))
    for r in range(len(X)):
        mask_len[r] = np.where(mask_sum[r, :] == 0)[0][0]
    return mask_len


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
    print("Running cross-validation, MAX_BB = {}".format(MAX_BB))
    all_acc = []
    all_auc = []
    for i, f in enumerate(files):
        test_files = [f]
        train_files = list(files)
        train_files.remove(f)

        valid_files = [train_files[0]]
        train_files = train_files[1:]
    
        train_X, train_y = combine_files(train_files)
        valid_X, valid_y = combine_files(valid_files)
        test_X, test_y = combine_files(test_files)

        yreal = test_y[:,:,0].mean(axis=1)
        print("Now classifying, this is fold {}/{}".format(i+1, len(files))) 
        if np.sum(yreal) > 0:
            metrics = train_model(train_X, train_y, test_X, test_y, valid_X, valid_y)
            print(metrics)
            trainidx = np.argmax(metrics['val_auc'])
            auc = metrics['auc'][trainidx]
            acc = metrics['val_acc'][-1]

            all_acc.append(acc)
            all_auc.append(auc)
            print(all_auc)
            print(np.mean(all_auc))
        else:
            print("Skipping this fold")
    return all_auc, all_acc


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


def calc_auc(model, X, y):
    preds = model.predict(X)[:, :, 0]
    mask_len = get_mask(X)
    cut_preds, last_preds = calc_masked_means(mask_len, preds)
    yreal = y[:,:,0].mean(axis=1)
    auc = roc_auc_score(yreal, cut_preds)
    last = roc_auc_score(yreal, last_preds)
    return auc, last


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


def train_model(train_X, train_y, test_X, test_y, valid_X, valid_y):
    print("Rebuilding model!")
    print("We have {} train examples and {} test examples".format(train_X.shape[0], test_X.shape[0]))
    model = load_model()
    metric_vals = defaultdict(list)

    for E in range(MAX_EPOCH):
        print("Training epoch {}".format(E))
        hist = model.fit(train_X, train_y, nb_epoch=1, batch_size=256, show_accuracy=True, verbose=1, validation_data=(valid_X, valid_y))

        # Calc AUC
        print('calculating AUCs...')
        auc, lauc = calc_auc(model, test_X, test_y)
        val_auc, lval_auc = calc_auc(model, valid_X, valid_y)
        train_auc, ltrain_auc = calc_auc(model, train_X, train_y)
        metric_vals['auc'].append(lauc)
        metric_vals['val_auc'].append(lval_auc)
        metric_vals['train_auc'].append(ltrain_auc)
        print("AUC: {0:.3f} ({1:.3f} Val, {2:.3f} Train)".format(auc, val_auc, train_auc))
        print("LAST: {0:.3f} ({1:.3f} Val, {2:.3f} Train)".format(lauc, lval_auc, ltrain_auc))

        # Calc other metrics
        metrics = hist.params['metrics']
        for metric in metrics:
            metric_vals[metric].append(hist.history[metric])
    return metric_vals

def load_model():
    model = Sequential()
    #model.add(Masking(-1, input_shape=(MAX_BB, 100)))
    model.add(LSTM(512, input_shape=(MAX_BB, 100), return_sequences=True))
    model.add(LSTM(512, return_sequences=True))
    model.add(TimeDistributedDense(2))
    model.add(Activation('softmax'))

    prop = RMSprop(lr=0.0002)
    model.compile(loss='categorical_crossentropy', optimizer=prop)
    return model


def get_max_BB_len(files):
    max_bb = 0
    for filename in files:
        with open(filename) as f:
            while True:
                line = next(f, None)
                if not line:
                    break

                try:
                    ID, truth, n_bb = line.strip('\n').split(' ')
                    n_bb = int(n_bb)
                except ValueError:
                    pass
                
                max_bb = max(max_bb, n_bb)
    return max_bb


def main():
    global MAX_BB
    files = get_csv_list('../scripts/features_files_lstm_all')
    MAX_BB = get_max_BB_len(files)
    
    all_auc, all_acc = cv_on_filelist(files)
    import pdb; pdb.set_trace()



if __name__ == "__main__":
    main()
