# %%
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import scipy.signal
import mne
import ast

# %%
signal_1 = pd.read_csv("datoss_11.csv")[['timestamp', 'sample_number', 'channel_data']]
signal_2 = pd.read_csv("datoss_12.csv")[['timestamp', 'sample_number', 'channel_data']]
signal_1.channel_data = signal_1.channel_data.apply(ast.literal_eval)
signal_2.channel_data = signal_2.channel_data.apply(ast.literal_eval)

# %%
channel_data_df = signal_1.channel_data.apply(pd.Series) * 2.4 / 12 / (2**23 - 1) # Convert to a millivolts
channel_data_df.columns = ['channel_1', 'channel_2', 'channel_3', 'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
columnas_a_eliminar = ['channel_3', 'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
channel_data_df = channel_data_df.drop(columns=columnas_a_eliminar)
channel_data_df.shape

# %%
signal_1 = pd.concat([signal_1.drop('channel_data', axis=1), channel_data_df], axis=1)
signal_1.timestamp = signal_1.timestamp.apply(lambda x: (x - signal_1.timestamp.iloc[0]) / 10 ** 6)
signal_1.shape

# %%
channel_data_df = signal_2.channel_data.apply(pd.Series) * 2.4 / 12 / (2**23 - 1) # Convert to a millivolts
channel_data_df.columns = ['channel_1', 'channel_2', 'channel_3', 'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
columnas_a_eliminar = ['channel_3', 'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
channel_data_df = channel_data_df.drop(columns=columnas_a_eliminar)
channel_data_df.shape

# %%
signal_2 = pd.concat([signal_2.drop('channel_data', axis=1), channel_data_df], axis=1)
signal_2.timestamp = signal_2.timestamp.apply(lambda x: (x - signal_2.timestamp.iloc[0]) / 10 ** 6)
signal_2.shape

# %%
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

# %%
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

# %% [markdown]
# ## Filtrado

# %%
fl, fh = 0.5, 35.
eeg_raw_f_1 = eeg_raw_1.copy().filter(l_freq=fl, h_freq=fh, method='fir', phase='zero-double', fir_window='hamming', 
                                      fir_design='firwin', pad='reflect', verbose=True)

# %%
eeg_raw_f_2 = eeg_raw_2.copy().filter(l_freq=fl, h_freq=fh, method='fir', phase='zero-double', fir_window='hamming', 
                                      fir_design='firwin', pad='reflect', verbose=True)

# %%
# Obtener los datos del objeto Raw
x1, t = eeg_raw_f_1.get_data(return_times=True, units='uV')
x2, t = eeg_raw_f_2.get_data(return_times=True, units='uV')

fig, (ax1, ax2, ax3, ax4) = plt.subplots(nrows=4, ncols=1, num=0, figsize=(14, 6), tight_layout=True, sharex=True)
fig.set_facecolor('lightskyblue')

ax1.plot(t, x1[0], lw=1.5, color='k')
ax1.grid(alpha=.8, ls='--')
ax1.margins(0, .1)

ax2.plot(t, x1[1], lw=1.5, color='b')
ax2.grid(alpha=.8, ls='--')

ax3.plot(t, x2[0], lw=1.5, color='k')
ax3.grid(alpha=.8, ls='--')

ax4.plot(t, x2[1], lw=1.5, color='b')
ax4.grid(alpha=.8, ls='--')

plt.show()

# %% [markdown]
# ### SPECTRAL ANALYSIS

# %%
ventana = 4
win = int(ventana * fs)
eeg_PSD_f_1 = eeg_raw_f_1.compute_psd(method='welch', fmax=40.0, picks='data', n_fft=win, n_overlap=win // 2, n_per_seg=win, window='hann')
eeg_PSD_f_2 = eeg_raw_f_2.compute_psd(method='welch', fmax=40.0, picks='data', n_fft=win, n_overlap=win // 2, n_per_seg=win, window='hann')

# %%
# Crear un objeto Figure y Axes personalizados

fig, (ax1, ax2) = plt.subplots(nrows=2, ncols=1, num=0, figsize=(14, 6))#, tight_layout=True)#, sharex=True)
fig.set_facecolor('lightskyblue')

# Graficar la PSD en el Axes personalizado
eeg_PSD_f_1.plot(dB=True, amplitude=False, color='r', spatial_colors=True, axes=ax1)
eeg_PSD_f_2.plot(dB=True, amplitude=False, color='b', spatial_colors=True, axes=ax2)

# Personalizar el Axes
for line in ax1.get_lines():
    line.set_linewidth(1.1)  # Aumentar el grosor de las líneas
for line in ax2.get_lines():
    line.set_linewidth(1.1)  # Aumentar el grosor de las líneas
ax1.set_ylim([-10, 30])  # Cambiar los límites del eje y
ax2.set_ylim([-10, 30])  # Cambiar los límites del eje y

plt.show()

# %%



