import numpy as np
from pylsl import StreamInlet, resolve_stream, local_clock
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore
from pyqtgraph.Qt.QtWidgets import QApplication


plot_duration = 2.0


# first resolve an EEG stream on the lab network
print("looking for an EEG stream...")
streams = resolve_stream('type', 'EEG')

# create a new inlet to read from the stream
inlet = StreamInlet(streams[0])
n = inlet.channel_count

# Create the pyqtgraph window
win = pg.GraphicsLayoutWidget()
win.setWindowTitle('LSL Plot ' + inlet.info().name())
win.show()
plt = win.addPlot()
plt.setLimits(xMin=0.0, xMax=plot_duration, yMin=-1.0 * (n - 1), yMax=6.0)

colors = ['b', 'g', 'r', 'c', 'm', 'y', 'brown', 'w']
t0 = [local_clock()] * n
curves = []
for ch_ix in range(n):
    curves.append(plt.plot(pen=colors[ch_ix % len(colors)]))


def update():
    global inlet, curves, t0
    # Read data from the inlet. Use a timeout of 0.0 so we don't block GUI interaction.
    chunk, timestamps = inlet.pull_chunk(timeout=0.0, max_samples=32)
    if timestamps:
        timestamps = np.asarray(timestamps)
        y = np.asarray(chunk) / 1000000

        for ch_ix in range(n):
            old_x, old_y = curves[ch_ix].getData()
            if old_x is not None:
                old_x += t0[ch_ix]  # Undo t0 subtraction
                this_x = np.hstack((old_x, timestamps))
                this_y = np.hstack((old_y, y[:, ch_ix]))# - ch_ix))
            else:
                this_x = timestamps
                this_y = y[:, ch_ix]# - ch_ix
            t0[ch_ix] = this_x[-1] - plot_duration
            this_x -= t0[ch_ix]
            b_keep = this_x >= 0
            curves[ch_ix].setData(this_x[b_keep], this_y[b_keep], pen=colors[ch_ix % len(colors)])



timer = QtCore.QTimer()
timer.timeout.connect(update)
timer.start(50)


# Start Qt event loop unless running in interactive mode or using pyside.
if __name__ == '__main__':
    import sys
    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        QApplication.instance().exec_()