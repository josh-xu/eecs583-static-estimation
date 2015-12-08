'''
Run classification on a set of CSV files, doing cross validation at the 
benchmark level (i.e. per CSV file). However, this implementation reads everything
into ram, meaning that this is only suitable for programs with small path counts
'''
import glob
import os

import numpy as np
import pandas as pd

from sklearn.linear_model import LogisticRegression
from sklearn.svm import SVC
from sklearn.metrics import roc_auc_score
from sklearn.ensemble import RandomForestClassifier

THRESH = 0


def get_csv_list(folder):
    '''Get a list of CSV files out of a folder with glob'''
    return glob.glob(os.path.join(folder, '*.csv'))


def combine_files(files):
    '''Combine a set of CSVs into one Pandas dataframe'''
    final_frame = pd.DataFrame()
    sanity_N = 0
    for f in files:
        this_frame = pd.read_csv(f, index_col=0)
        final_frame = final_frame.append(this_frame)
        sanity_N += len(this_frame)

    assert sanity_N == len(final_frame)
    return final_frame


def classify_set(model, train, test):
    '''Classify a set given a train and test frame, as well as a model file. 
    
    Arguments
    ---------
    model : sklearn.model
        Scikit learn interface model

    train : pandas.DataFrame
        Frame with data and ground truth for training

    test : pandas.DataFrame
        Frame with data and ground truth for testing

    Returns
    -------
    auc : float
        Area under the ROC curve

    acc : float
        Raw accuracy score
    '''
    X_train = train.iloc[:, 1:]
    y_train = (train['RealCount'] > THRESH)
    X_test = test.iloc[:, 1:]
    y_test = (test['RealCount'] > THRESH)

    model.fit(X_train, y_train)
    acc = model.score(X_test, y_test)
    probs = model.predict_proba(X_test)
    auc = roc_auc_score(y_test, probs[:, 1])

    return auc, acc


def cv_on_filelist(model, files):
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
    
        train_frame = combine_files(train_files)
        test_frame = combine_files(test_files)
    
        print("Now classifying, this is fold {}/{}\r".format(i+1, len(files)), end='') 
        auc, acc = classify_set(model, train_frame, test_frame)

        all_acc.append(acc)
        all_auc.append(auc)

    print("")
    return (all_auc, all_acc)


def print_statistics(files):
    '''Collect population stats for entire dataset'''
    all_frame = combine_files(files)

    pos = (all_frame['RealCount'] > THRESH).sum()

    print("===========STATS===========")
    print("Positive instances: {}/{}".format(pos, len(all_frame)))
    print("Baseline percentage: {0:.4f}%".format(1-float(pos)/len(all_frame)))
    print("")
    

def main():
    # Get the files and do some population statistics before we start
    files = get_csv_list('../scripts/features_files/')
    print_statistics(files)

    # Create models
    models = []
    models.append((LogisticRegression(), "Base Logistic Regression"))
    models.append((RandomForestClassifier(), "Random Forest"))
    models.append((SVC(C=0.1, kernel='rbf', probability=True), "SVM (w/ RBF)"))

    # Run testing on all models
    for model, name in models:
        print("==========={}===========".format(name))
        all_auc, all_acc = cv_on_filelist(model, files)
        print("Final mean accuracy: {0:.4f}%".format(np.mean(all_acc)))
        print("Final mean AUC score: {0:.3f}".format(np.mean(all_auc)))


if __name__ == "__main__":
    main()
