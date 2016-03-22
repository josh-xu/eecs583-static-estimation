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

from lstm_utils import *

MAX_EPOCH = 5
MAX_BB = 70


def get_csv_list(folder):
    '''Get a list of CSV files out of a folder with glob'''
    return glob.glob(os.path.join(folder, '*.csv'))

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
:q



        List of accuracy values for each fold
    '''
    if sys.argv[2]:
        MAX_EPOCH = sys.argv[2]
    print("Running cross-validation, MAX_BB = {}".format(MAX_BB))
    print("MAX_EPOCH = ", MAX_EPOCH)
    all_acc = []
    all_auc = []
    for i, f in enumerate(files):
        if f.find(".RUN") == 0:
            print("Skipping file: ", f)

        else: 
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
        
            print("File removed: ", f)
            if np.sum(yreal) > 0:
                metrics = train_model(train_X, train_y, test_X, test_y, valid_X, valid_y, i)
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
        X, y, _ = load_features(f, MAX_BB)
        Xs.append(X)
        ys.append(y)
    allX = np.concatenate(Xs, axis=0)
    ally = np.concatenate(ys, axis=0)
    return allX, ally


def train_model(train_X, train_y, test_X, test_y, valid_X, valid_y, fold_no):
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

    # Save the output to JSON
    json_string = model.to_json()
    with open('saved_model_arch_{}.json'.format(fold_no), 'w') as f:
        f.write(json_string)
    model.save_weights('saved_model_weights_{}.h5'.format(fold_no), overwrite=True)
        
    return metric_vals


def load_model():
    model = Sequential()
    #model.add(Masking(-1, ))
    model.add(LSTM(256, input_shape=(MAX_BB, 100), return_sequences=True))
    model.add(LSTM(256, return_sequences=True))
    model.add(TimeDistributedDense(2))
    model.add(Activation('softmax'))

    prop = RMSprop(lr=0.0002)
    model.compile(loss='categorical_crossentropy', optimizer=prop)
    return model


def get_max_BB_len(files):
    max_bb = 0
    for filename in files:
        with open(filename) as f:
            file_bb = 0;
            while True:
                line = next(f, None)
                if not line:
                    break

                try:
                    fn, ID, truth, n_bb = line.strip('\n').split(' ')
                    n_bb = int(n_bb)
                except ValueError:
                    pass
                max_bb = max(max_bb, n_bb)
                file_bb = max(file_bb, n_bb)
        print(filename, " longest path is ", file_bb)
    print ("MAX_BB = ", max_bb);
    return max_bb


def main():
    global MAX_BB
    folder = "../scripts/feature_files_lstm_" + sys.argv[1]
    if (not os.path.exists(folder)):
        print(folder + " does not exist. Exiting...")
        exit()
    files = get_csv_list(folder)
    MAX_BB = get_max_BB_len(files)
    
    all_auc, all_acc = cv_on_filelist(files)
    import pdb; pdb.set_trace()


if __name__ == "__main__":
    main()
