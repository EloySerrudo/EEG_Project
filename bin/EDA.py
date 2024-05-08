import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import scipy.signal
import mne
import ast


signal_1 = pd.read_csv("bin/datoss_9.csv")[['timestamp', 'sample_number', 'channel_data']]
signal_2 = pd.read_csv("bin/datoss_10.csv")[['timestamp', 'sample_number', 'channel_data']]
signal_1.channel_data = signal_1.channel_data.apply(ast.literal_eval)
signal_2.channel_data = signal_2.channel_data.apply(ast.literal_eval)

channel_data_df = signal_1.channel_data.apply(pd.Series) * 2.4 / 12 / (2**23 - 1) # Convert to a millivolts
channel_data_df.columns = ['channel_1', 'channel_2', 'channel_3', 'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
columnas_a_eliminar = ['channel_3', 'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
channel_data_df = channel_data_df.drop(columns=columnas_a_eliminar)
channel_data_df.shape

signal_1 = pd.concat([signal_1.drop('channel_data', axis=1), channel_data_df], axis=1)
signal_1.timestamp = signal_1.timestamp.apply(lambda x: (x - signal_1.timestamp.iloc[0]) / 10 ** 6)
signal_1.shape

channel_data_df = signal_2.channel_data.apply(pd.Series) * 2.4 / 12 / (2**23 - 1) # Convert to a millivolts
channel_data_df.columns = ['channel_1', 'channel_2', 'channel_3', 'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
columnas_a_eliminar = ['channel_3', 'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
channel_data_df = channel_data_df.drop(columns=columnas_a_eliminar)
channel_data_df.shape

signal_2 = pd.concat([signal_2.drop('channel_data', axis=1), channel_data_df], axis=1)
signal_2.timestamp = signal_2.timestamp.apply(lambda x: (x - signal_2.timestamp.iloc[0]) / 10 ** 6)
signal_2.shape

fs = 500.
info_1 = mne.create_info(['O2','Fp2'], 
                       sfreq=fs, ch_types='eeg')
info_1.set_montage('standard_1020')
info_1['description'] = 'Pruebas de EEG'
info_1['device_info'] = {'type':'Frontend', 'model':'AD1298'}
info_1['experimenter'] = 'Eloy'
info_1['line_freq'] = 50.0
info_1['subject_info'] = {'id':1,'last_name':'Yapur','first_name':'Jhoseline',
                        'birthday':(2002, 5, 12),'sex':2,'hand':1}
data_1 = signal_1.loc[:, 'channel_1':'channel_2'].values.T
eeg_raw_1 = mne.io.RawArray(data_1, info_1, verbose=True)

info_2 = mne.create_info(['O2','Fp2'], 
                         sfreq=fs, ch_types='eeg')
info_2.set_montage('standard_1020')
info_2['description'] = 'Pruebas de EEG'
info_2['device_info'] = {'type':'Frontend', 'model':'AD1298'}
info_2['experimenter'] = 'Eloy'
info_2['line_freq'] = 50.0
info_2['subject_info'] = {'id':1,'last_name':'Yapur','first_name':'Jhoseline',
                        'birthday':(2002, 5, 12),'sex':2,'hand':1}
data_2 = signal_2.loc[:, 'channel_1':'channel_2'].values.T
eeg_raw_2 = mne.io.RawArray(data_2, info_2, verbose=True)

fl, fh = 0.5, 35.
eeg_raw_f_1 = eeg_raw_1.copy().filter(l_freq=fl, h_freq=fh, method='fir', phase='zero-double', fir_window='hamming', 
                                      fir_design='firwin', pad='reflect_limited', verbose=True)

eeg_raw_f_2 = eeg_raw_2.copy().filter(l_freq=fl, h_freq=fh, method='fir', phase='zero-double', fir_window='hamming', 
                                      fir_design='firwin', pad='reflect_limited', verbose=True)


# Obtener los datos del objeto Raw
data, times = eeg_raw_f_1[:, :]

# Crear una nueva figura de Matplotlib
fig, ax = plt.subplots()

# Trazar los datos en la nueva figura
for i in range(data.shape[0]):
    ax.plot(times, data[i, :])

# Personalizar el gráfico como desees
ax.set_title("My Custom Plot")
ax.set_xlabel("Time (s)")
ax.set_ylabel("Voltage (µV)")

# Mostrar la figura
plt.show()