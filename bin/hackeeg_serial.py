import argparse
import time
import sys
import winsound
import pandas as pd

import hackeeg
from hackeeg import ads1299
from hackeeg.driver import SPEEDS, GAINS, Status

from pynput import keyboard

DEFAULT_NUMBER_OF_SAMPLES_TO_CAPTURE = 50000
BAUD_RATE = 921600


class HackEegTestApplicationException(Exception):
    pass


class HackEegTestApplication:
    """HackEEG commandline tool."""

    def __init__(self):
        self.serial_port_name = None
        self.hackeeg = None
        self.debug = False
        self.channel_test = False
        self.quiet = False
        self.hex = False
        self.messagepack = False
        self.channels = 8
        self.samples_per_second = 500
        self.gain = 1
        self.max_samples = 5000
        self.read_samples_continuously = True
        self.continuous_mode = False
        self.debug = False

        print(f"Platform: {sys.platform}")

    def find_dropped_samples(self, samples, number_of_samples):
        sample_numbers = {self.get_sample_number(sample): 1 for sample in samples}
        correct_sequence = {index: 1 for index in range(0, number_of_samples)}
        missing_samples = [sample_number for sample_number in correct_sequence.keys()
                           if sample_number not in sample_numbers]
        return len(missing_samples)

    def get_sample_number(self, sample):
        sample_number = sample.get('sample_number', -1)
        return sample_number

    def read_keyboard_input(self, key):
        self.read_samples_continuously = False

    def setup(self, samples_per_second=500, gain=1, messagepack=False):
        if samples_per_second not in SPEEDS.keys():
            raise HackEegTestApplicationException("{} is not a valid speed; valid speeds are {}".format(
                samples_per_second, sorted(SPEEDS.keys())))
        if gain not in GAINS.keys():
            raise HackEegTestApplicationException("{} is not a valid gain; valid gains are {}".format(
                gain, sorted(GAINS.keys())))

        self.hackeeg.stop_and_sdatac_messagepack()
        self.hackeeg.sdatac()
        self.hackeeg.blink_board_led()
        sample_mode = SPEEDS[samples_per_second] | ads1299.CONFIG1_const
        self.hackeeg.wreg(ads1299.CONFIG1, sample_mode)

        gain_setting = GAINS[gain]
        
        # all channels enabled
        self.hackeeg.enable_all_channels()
        time.sleep(0.1)
        if self.channel_test:
            self.channel_config_test()
        else:
            self.channel_config_input(gain_setting)


        # Route reference electrode to SRB1: JP8:1-2, JP7:NC (not connected)
        # use this with humans to reduce noise
        #self.hackeeg.wreg(ads1299.MISC1, ads1299.SRB1 | ads1299.MISC1_const)

        # Single-ended mode - setting SRB1 bit sends mid-supply voltage to the N inputs
        # use this with a signal generator
        #self.hackeeg.wreg(ads1299.MISC1, ads1299.SRB1)<------EDITADO

        # Dual-ended mode
        #self.hackeeg.wreg(ads1299.MISC1, ads1299.MISC1_const)<------EDITADO
        # add channels into bias generation
        self.hackeeg.wreg(ads1299.RLD_SENSP, ads1299.RLD1P)
        # self.hackeeg.wreg(ads1299.RLD_SENSP, ads1299.RLD2P)
        # self.hackeeg.wreg(ads1299.RLD_SENSP, ads1299.RLD3P)
        # self.hackeeg.wreg(ads1299.RLD_SENSP, ads1299.RLD4P)
        self.hackeeg.wreg(ads1299.RLD_SENSN, ads1299.RLD1N)
        # self.hackeeg.wreg(ads1299.RLD_SENSN, ads1299.RLD2N)
        # self.hackeeg.wreg(ads1299.RLD_SENSN, ads1299.RLD3N)
        # self.hackeeg.wreg(ads1299.RLD_SENSN, ads1299.RLD4N)
        RLD_conf = ads1299.CONFIG3_const | ads1299.PD_REFBUF | ads1299.RLDREF_INT | ads1299.PD_RLD
        self.hackeeg.wreg(ads1299.CONFIG3, RLD_conf)

        if messagepack:
            self.hackeeg.messagepack_mode()
            print("Messagepack mode on")
        else:
            self.hackeeg.jsonlines_mode()
            print("JSON Lines mode on")
        print("Mode:", self.hackeeg.protocol().get('STATUS_TEXT'))
        self.hackeeg.start()
        self.hackeeg.rdatac()


    def channel_config_input(self, gain_setting):
        # for channel in range(1, 9):
        #     self.hackeeg.wreg(ads1299.CHnSET + channel, ads1299.TEST_SIGNAL | gain_setting )

        ##############
        mvdd = ads1299.ELECTRODE_INPUT | ads1299.ADS1298_GAIN_1X | ads1299.MVDD
        ##############
        self.hackeeg.wreg(ads1299.CHnSET + 1, ads1299.ELECTRODE_INPUT | gain_setting)
        # self.hackeeg.wreg(ads1299.CHnSET + 2, ads1299.ELECTRODE_INPUT | gain_setting)
        # self.hackeeg.wreg(ads1299.CHnSET + 3, ads1299.ELECTRODE_INPUT | gain_setting)
        # self.hackeeg.wreg(ads1299.CHnSET + 4, ads1299.ELECTRODE_INPUT | gain_setting)
        # self.hackeeg.wreg(ads1299.CHnSET + 5, mvdd)
        # self.hackeeg.wreg(ads1299.CHnSET + 6, mvdd)
        # self.hackeeg.wreg(ads1299.CHnSET + 7, mvdd)
        # self.hackeeg.wreg(ads1299.CHnSET + 8, mvdd)

        self.hackeeg.disable_channel(2)
        self.hackeeg.disable_channel(3)
        self.hackeeg.disable_channel(4)
        self.hackeeg.disable_channel(5)
        self.hackeeg.disable_channel(6)
        self.hackeeg.disable_channel(7)
        self.hackeeg.disable_channel(8)

    def channel_config_test(self):
        # test_signal_mode = ads1299.INT_TEST_DC | ads1299.CONFIG2_const
        test_signal_mode = ads1299.INT_TEST_2HZ | ads1299.CONFIG2_const
        self.hackeeg.wreg(ads1299.CONFIG2, test_signal_mode)
        # self.hackeeg.wreg(ads1299.CHnSET + 1, ads1299.INT_TEST_DC | ads1299.ADS1298_GAIN_1X)
        # self.hackeeg.wreg(ads1299.CHnSET + 2, ads1299.SHORTED | ads1299.ADS1298_GAIN_1X)
        # self.hackeeg.wreg(ads1299.CHnSET + 3, ads1299.MVDD | ads1299.ADS1298_GAIN_1X)
        # self.hackeeg.wreg(ads1299.CHnSET + 4, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)
        # self.hackeeg.wreg(ads1299.CHnSET + 5, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_8X)
        # self.hackeeg.wreg(ads1299.CHnSET + 6, ads1299.TEMP | ads1299.ADS1298_GAIN_1X)
        # self.hackeeg.wreg(ads1299.CHnSET + 7, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_12X)
        # self.hackeeg.disable_channel(8)
        self.hackeeg.wreg(ads1299.CHnSET + 1, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)
        self.hackeeg.wreg(ads1299.CHnSET + 2, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)
        self.hackeeg.wreg(ads1299.CHnSET + 3, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)
        self.hackeeg.wreg(ads1299.CHnSET + 4, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)
        self.hackeeg.wreg(ads1299.CHnSET + 5, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)
        self.hackeeg.wreg(ads1299.CHnSET + 6, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)
        self.hackeeg.wreg(ads1299.CHnSET + 7, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)
        self.hackeeg.wreg(ads1299.CHnSET + 8, ads1299.TEST_SIGNAL | ads1299.ADS1298_GAIN_1X)


    def parse_args(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("serial_port", help="serial port device path",
                            type=str)
        parser.add_argument("--debug", "-d", help="enable debugging output",
                            action="store_true")
        parser.add_argument("--samples", "-S", help="how many samples to capture",
                            default=DEFAULT_NUMBER_OF_SAMPLES_TO_CAPTURE, type=int)
        parser.add_argument("--continuous", "-C", help="read data continuously (until <return> key is pressed)",
                            action="store_true")
        parser.add_argument("--sps", "-s",
                            help=f"ADS1299 samples per second setting- must be one of {sorted(list(SPEEDS.keys()))}, default is {self.samples_per_second}",
                            default=self.samples_per_second, type=int)
        parser.add_argument("--gain", "-g",
                            help=f"ADS1299 gain setting for all channels– must be one of {sorted(list(GAINS.keys()))}, default is {self.gain}",
                            default=self.gain, type=int)
        parser.add_argument("--messagepack", "-M",
                            help=f"MessagePack mode– use MessagePack format to send sample data to the host, rather than JSON Lines",
                            action="store_true")
        parser.add_argument("--channel-test", "-T",
                            help=f"set the channels to internal test settings for software testing",
                            action="store_true")
        parser.add_argument("--hex", "-H",
                            help=f"hex mode– output sample data in hexidecimal format for debugging",
                            action="store_true")
        parser.add_argument("--quiet", "-q",
                            help=f"quiet mode– do not print sample data (used for performance testing)",
                            action="store_true")
        args = parser.parse_args()
        if args.debug:
            self.debug = True
            print("debug mode on")
        self.samples_per_second = args.sps
        self.gain = args.gain

        if args.continuous:
            self.continuous_mode = True

        self.serial_port_name = args.serial_port
        self.hackeeg = hackeeg.HackEEGBoard(self.serial_port_name, baudrate=BAUD_RATE, debug=self.debug)
        time.sleep(7)#<------EDITADO
        self.max_samples = args.samples
        self.channel_test = args.channel_test
        self.quiet = args.quiet
        self.hex = args.hex
        self.messagepack = args.messagepack
        self.hackeeg.connect()
        self.setup(samples_per_second=self.samples_per_second, gain=self.gain, messagepack=self.messagepack)

    def process_sample(self, result, samples):
        data = None
        channel_data = None
        if result:
            status_code = result.get(self.hackeeg.MpStatusCodeKey)
            data = result.get(self.hackeeg.MpDataKey)
            if status_code == Status.Ok and data:
                timestamp = result.get('timestamp')
                sample_number = result.get('sample_number')
                channel_data = result.get('channel_data')
                sample = [sample_number, timestamp]
                sample.extend(channel_data)
                samples.append(sample)
                if not self.quiet:
                    ads_gpio = result.get('ads_gpio')
                    loff_statp = result.get('loff_statp')
                    loff_statn = result.get('loff_statn')
                    print(
                        f"timestamp:{timestamp} sample_number: {sample_number}| gpio:{ads_gpio} loff_statp:{loff_statp} loff_statn:{loff_statn}   ",
                        end='')
                    if self.hex:
                        data_hex = result.get('data_hex')
                        print(data_hex)
                    else:
                        for channel_number, sample in enumerate(channel_data):
                            print(f"{channel_number + 1}:{sample} ", end='')
                        print()
            else:
                if not self.quiet:
                    print(data)
        else:
            print("no data to decode")
            print(f"result: {result}")

    def main(self):
        self.parse_args()

        samples = []
        sample_counter = 0
        
        if self.continuous_mode:
            listener = keyboard.Listener(on_press=self.read_keyboard_input)
            listener.start()

        end_time = time.perf_counter()
        start_time = time.perf_counter()
        while ((sample_counter < self.max_samples and not self.continuous_mode) or \
               (self.read_samples_continuously and self.continuous_mode)):
            result = self.hackeeg.read_rdatac_response()
            end_time = time.perf_counter()
            sample_counter += 1
            self.process_sample(result, samples)

        duration = end_time - start_time
        self.hackeeg.stop_and_sdatac_messagepack()
        self.hackeeg.blink_board_led()
        time.sleep(1)
        self.hackeeg.close()
        winsound.Beep(frequency=2500, duration=2000)

        print("Duration in seconds:", duration)
        samples_per_second = sample_counter / duration
        print("Samples:", sample_counter)
        print("Samples per second:", samples_per_second)
        # dropped_samples = self.find_dropped_samples(samples, sample_counter)
        # print("Dropped samples:", dropped_samples)

        columnas = ['sample_number', 'timestamp', 'channel_1', 'channel_2', 'channel_3', 
                    'channel_4', 'channel_5', 'channel_6', 'channel_7', 'channel_8']
        df = pd.DataFrame(samples, columns=columnas)
        df.to_csv('datos.csv', index=False)


if __name__ == "__main__":
    HackEegTestApplication().main()


# Notes:
# To capture at 16,000 samples per second,
# the Arduino Due driver's SPI DMA must be on,
# and the driver communication mode must be set to MessagePack
# (--messagepack option)