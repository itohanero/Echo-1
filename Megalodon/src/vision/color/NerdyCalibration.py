import cv2
import numpy as np
import math
import os
import matplotlib.pyplot as plt
from matplotlib import animation

import NerdyConstants

"""Vision Target HSV Color Calibration (Box)"""
__author__ = "tedlin"

# Capture video from camera
cap = cv2.VideoCapture(0)

# Calibration box dimensions
CAL_AREA = 1600
CAL_SIZE = int(math.sqrt(CAL_AREA))
CAL_UP = int(NerdyConstants.FRAME_CY + (CAL_SIZE / 2))
CAL_LO = int(NerdyConstants.FRAME_CY - (CAL_SIZE / 2))
CAL_R = int(NerdyConstants.FRAME_CX - (CAL_SIZE / 2))
CAL_L = int(NerdyConstants.FRAME_CX + (CAL_SIZE / 2))
CAL_UL = (CAL_L, CAL_UP)
CAL_LR = (CAL_R, CAL_LO)

cap.set(cv2.CAP_PROP_FRAME_WIDTH, NerdyConstants.FRAME_X)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, NerdyConstants.FRAME_Y)


def main():

    while 971:
        ret, frame = cap.read()

        cv2.rectangle(frame, CAL_UL, CAL_LR, (0, 255, 0), thickness=1)
        roi = frame[CAL_LO:CAL_UP, CAL_R:CAL_L]
        average_color_per_row = np.average(roi, axis=0)
        average_color = np.average(average_color_per_row, axis=0)
        average_color = np.uint8([[average_color]])
        hsv = cv2.cvtColor(average_color, cv2.COLOR_BGR2HSV)

        print(np.array_str(hsv))
        cv2.imshow("NerdyCalibration", frame)
        # fig = plt.figure(1)
        # plot = fig.add_subplot(1, 1, 1)
        # plot.imshow(frame, animated=True)
        # animation.FuncAnimation(fig, frame, interval=0)
        # plt.show()

        cv2.waitKey(1)

    cap.release()
    cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
