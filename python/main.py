import numpy as np
import cv2


cap = cv2.VideoCapture('../res/BadApple!!.mp4')
res = (640, 380)
div = (20, 32)

def subdiv(size_x, size_y, div_x, div_y):
    boxes = []
    for i in range(0, size_x, div_x):
        for j in range(0, size_y, div_y):
            boxes.append((i, j))

    return boxes


# REGIONS
# A -> x1 + 5, y1       | x2 - 5, y1 + 10
# B -> x2 - 4, y1       | x2, y1 + 16
# C -> x2 - 4, y1 + 16  | x2, y2
# D -> x1 + 5, y2       | x2 - 5, y2 - 10
# E -> x1, y1 + 16      | x2 + 4, y2
# F -> x1, y0           | x1 + 4, y1 + 16
# G -> x1 + 5, y1 + 11  | x2 - 5, y2 - 11

SEG_OFFSETS = {
    "A": (5,   0,   -5,  10),
    "B": (-4,  0,    0,  16),
    "C": (-4, 16,    0,   0),
    "D": (5,  -10,  -5,  0),
    "E": (0,  16,    4,  0),
    "F": (0,   0,    4,  16),
    "G": (5,  11,   -5, -11),
}

def checkPix(x1, y1, x2, y2):

    # --- Region A ---
    # A -> x1 + 5, y1       | x2 - 5, y1 + 10
    reg_A = frame[y1 : y1 + 10, x1 + 5 : x2 - 5]
    mean_A = np.mean(reg_A)

    # --- Region B ---
    # B -> x2 - 4, y1       | x2, y1 + 16
    reg_B = frame[y1 : y1 + 16, x2 - 4 : x2]
    mean_B = np.mean(reg_B)

    # --- Region C ---
    # C -> x2 - 4, y1 + 16  | x2, y2
    reg_C = frame[y1 + 16 : y2, x2 - 4 : x2]
    mean_C = np.mean(reg_C)

    # --- Region D ---
    # D -> x1 + 5, y2       | x2 - 5, y2 - 10
    reg_D = frame[y2 - 10 : y2, x1 + 5 : x2 - 5]
    mean_D = np.mean(reg_D)

    # --- Region E ---
    # E -> x1, y1 + 16      | x2 + 4, y2
    reg_E = frame[y1 + 16 : y2, x1 : x2 + 4]
    mean_E = np.mean(reg_E)

    # --- Region F ---
    # F -> x1, y1           | x1 + 4, y1 + 16
    reg_F = frame[y1 : y1 + 16, x1 : x1 + 4]
    mean_F = np.mean(reg_F)

    # --- Region G ---
    # G -> x1 + 5, y1 + 11  | x2 - 5, y2 - 11
    reg_G = frame[y1 + 11 : y2 - 11, x1 + 5 : x2 - 5]
    mean_G = np.mean(reg_G)

    return mean_A, mean_B, mean_C, mean_D, mean_E, mean_F, mean_G

def draw_seven_seg(frame_bgr, x1, y1, x2, y2,
                   threshold=0.5,       # default threshold in normalized range 0..1
                   norm=True,           # whether to normalize grayscale to 0..1
                   invert=False,        # if True, invert grayscale before measuring (black -> bright)
                   color_on=(0, 255, 0),# BGR color for ON segments
                   color_off=(50, 50, 50),# BGR color for OFF segments (outline color)
                   thickness_on=-1,     # fill when ON
                   thickness_off=1):    # outline thickness when OFF

    h, w = frame_bgr.shape[:2]
    # Clip coords and convert to ints
    rx1 = int(max(0, min(w, x1)))
    ry1 = int(max(0, min(h, y1)))
    rx2 = int(max(0, min(w, x2)))
    ry2 = int(max(0, min(h, y2)))

    out = frame_bgr.copy()

    # If input is already grayscale (2D array), skip conversion
    if len(frame_bgr.shape) == 2:
        gray8 = frame_bgr.copy()
    else:
        gray8 = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2GRAY)

    # If normalization requested, convert to float 0..1
    if norm:
        gray = gray8.astype(np.float32) / 255.0
    else:
        gray = gray8.astype(np.float32)

    if invert:
        gray = 1.0 - gray if norm else (255.0 - gray)

    # region definitions relative to your box (same as you used)
    regions = {
        "A": (x1 + 5,  y1,         x2 - 5,  y1 + 10),
        "B": (x2 - 4,  y1,         x2,      y1 + 16),
        "C": (x2 - 4,  y1 + 16,    x2,      y2),
        "D": (x1 + 5,  y2 - 10,    x2 - 5,  y2),
        "E": (x1,      y1 + 16,    x2 + 4,  y2),
        "F": (x1,      y1,         x1 + 4,  y1 + 16),
        "G": (x1 + 5,  y1 + 11,    x2 - 5,  y2 - 11)
    }

    for seg, (sx1, sy1, sx2, sy2) in regions.items():
        # clip each region to image bounds (convert to ints)
        sx1i = int(max(0, min(w, sx1)))
        sy1i = int(max(0, min(h, sy1)))
        sx2i = int(max(0, min(w, sx2)))
        sy2i = int(max(0, min(h, sy2)))

        # skip empty regions
        if sx2i <= sx1i or sy2i <= sy1i:
            continue

        region = gray[sy1i:sy2i, sx1i:sx2i]
        if region.size == 0:
            continue

        mean_val = float(np.mean(region))

        # decide ON / OFF
        if mean_val >= threshold:
            # draw filled rectangle to represent ON segment
            cv2.rectangle(out, (sx1i, sy1i), (sx2i, sy2i), color_on, thickness_on)
        else:
            # draw a thin outline for OFF segments
            cv2.rectangle(out, (sx1i, sy1i), (sx2i, sy2i), color_off, thickness_off)

    return out

def get_segment_brightness(gray, x1, y1, x2, y2):
    brightness = []

    for dx1, dy1, dx2, dy2 in SEG_OFFSETS.values():
        sx1 = x1 + dx1
        sy1 = y1 + dy1
        sx2 = x2 + dx2
        sy2 = y2 + dy2

        # slice
        region = gray[sy1:sy2, sx1:sx2]
        if region.size == 0:
            brightness.append(0)
        else:
            brightness.append(np.mean(region))

    return brightness

def draw_segments(out, x1, y1, x2, y2, brights, threshold=0.5):
    idx = 0
    for (dx1, dy1, dx2, dy2) in SEG_OFFSETS.values():
        sx1 = x1 + dx1
        sy1 = y1 + dy1
        sx2 = x2 + dx2
        sy2 = y2 + dy2

        if brights[idx] > threshold:
            cv2.rectangle(out, (sx1, sy1), (sx2, sy2), (0, 255, 0), -1)
        else:
            cv2.rectangle(out, (sx1, sy1), (sx2, sy2), (80, 80, 80), 1)

        idx += 1

boxes = subdiv(res[0], res[1], div[0], div[1])


# Loop until the end of the video
while (cap.isOpened()):

    # Capture frame-by-frame
    ret, frame = cap.read()
    frame = cv2.resize(frame, (res[0], res[1]), fx = 0, fy = 0,
                         interpolation = cv2.INTER_CUBIC)

    # Display the resulting frame
    cv2.imshow('Frame', frame)

    # conversion of BGR to grayscale is necessary to apply this operation
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    frame_out = gray.copy()

    # drawing subdivision
    for box in boxes:
        # cv2.rectangle(frame, (box[0], box[1]), (box[0] + div[0], box[1] + div[1]),(0,0,125))
        frame_out = draw_seven_seg(frame_out, box[0], box[1], (box[0] + div[0]), (box[1] + div[1]),
                                   threshold=0.5, norm=True, invert=False,
                                   color_on=(0, 255, 0)
                                   )

    cv2.imshow('regions', frame_out)

    # define q as the exit button
    if cv2.waitKey(25) & 0xFF == 27:
        break

# release the video capture object
cap.release()
# Closes all the windows currently opened.
cv2.destroyAllWindows()
