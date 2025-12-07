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
# F -> x1, y0           | x2 - 16, y2 - 16
# G -> x1 + 5, y1 + 11  | x2 - 5, y2 - 11

SEG_OFFSETS = {
    "A": (5,   0,   -5,  10),
    "B": (-4,  0,    0,  16),
    "C": (-4, 16,    0,   0),
    "D": (5,  -10,  -5,  0),
    "E": (0,  16,    4,  0),
    "F": (0,   0,   -16, -16),
    "G": (5,  11,   -5, -11),
}

SEG_RELATIVE = {
    "A": (0.25, 0.0, 0.75, 0.1),  # x1_rel, y1_rel, x2_rel, y2_rel
    "B": (0.75, 0.0, 0.85, 0.5),
    "C": (0.75, 0.5, 0.85, 1.0),
    "D": (0.25, 0.9, 0.75, 1.0),
    "E": (0.15, 0.5, 0.25, 1.0),
    "F": (0.15, 0.0, 0.25, 0.5),
    "G": (0.25, 0.45, 0.75, 0.55)
}


def draw_seven_seg_fast(frame_bgr, boxes, div, threshold=0.5, invert=True,
                        color_on=(0, 255, 0), color_off=(50,50,50),
                        thickness_on=-1, thickness_off=1):

    h, w = frame_bgr.shape[:2]
    out = frame_bgr.copy()

    # Convert to grayscale once
    gray = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0
    if invert:
        gray = 1.0 - gray  # black segments on white background become bright

    for x1_box, y1_box in boxes:
        x2_box, y2_box = x1_box + div[0], y1_box + div[1]

        # Draw each segment using proportional offsets
        for rel_x1, rel_y1, rel_x2, rel_y2 in SEG_RELATIVE.values():
            sx1 = int(x1_box + rel_x1 * div[0])
            sy1 = int(y1_box + rel_y1 * div[1])
            sx2 = int(x1_box + rel_x2 * div[0])
            sy2 = int(y1_box + rel_y2 * div[1])

            # Clip to frame boundaries
            sx1 = max(0, min(w, sx1))
            sx2 = max(0, min(w, sx2))
            sy1 = max(0, min(h, sy1))
            sy2 = max(0, min(h, sy2))

            # Swap if coordinates are reversed
            sx1, sx2 = sorted([sx1, sx2])
            sy1, sy2 = sorted([sy1, sy2])

            if sx2 <= sx1 or sy2 <= sy1:
                continue

            region = gray[sy1:sy2, sx1:sx2]
            mean_val = np.mean(region)

            if mean_val >= threshold:
                cv2.rectangle(out, (sx1, sy1), (sx2, sy2), color_on, thickness_on)
            else:
                cv2.rectangle(out, (sx1, sy1), (sx2, sy2), color_off, thickness_off)

    return out

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
    gray_norm = gray.astype(np.float32) / 255.0

    out = frame.copy()

    # drawing subdivision
    out = draw_seven_seg_fast(frame, boxes, div, threshold=0.3, invert=True)

    cv2.imshow('regions', out)

    # define q as the exit button
    if cv2.waitKey(25) & 0xFF == 27:
        break

# release the video capture object
cap.release()
# Closes all the windows currently opened.
cv2.destroyAllWindows()
