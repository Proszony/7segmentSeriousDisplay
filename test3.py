import numpy as np
import cv2
import sys
import time
from ffpyplayer.player import MediaPlayer

thresh = 0.5
invert_flag = False
for arg in sys.argv[1:]:
    if arg.startswith("--thresh="):
        threshold = float(arg.split("=")[1])
    elif arg == "--inv":
        invert_flag = True

path = 'BadApple!!.mp4'
cap = cv2.VideoCapture(path)
player = MediaPlayer(path)
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
    "A": (0.25, 0.0, 0.75, 0.1),
    "B": (0.75, 0.0, 0.85, 0.5),
    "C": (0.75, 0.5, 0.85, 1.0),
    "D": (0.25, 0.9, 0.75, 1.0),
    "E": (0.15, 0.5, 0.25, 1.0),
    "F": (0.15, 0.0, 0.25, 0.5),
    "G": (0.25, 0.45, 0.75, 0.55)
}


def draw_seven_seg_binary(frame_shape, boxes, div, gray_frame, threshold=0.5,
                          color_on=(0,255,0), color_off=(0,0,0)):
    """
    Draws 7-segment display on a solid background.
    - frame_shape: tuple (height, width, channels)
    - gray_frame: normalized grayscale of the video for determining on/off
    """
    h, w = frame_shape[:2]
    out = np.zeros(frame_shape, dtype=np.uint8)  # solid black background

    for x1_box, y1_box in boxes:
        x2_box, y2_box = x1_box + div[0], y1_box + div[1]

        # Draw each segment
        for rel_x1, rel_y1, rel_x2, rel_y2 in SEG_RELATIVE.values():
            sx1 = int(x1_box + rel_x1 * div[0])
            sy1 = int(y1_box + rel_y1 * div[1])
            sx2 = int(x1_box + rel_x2 * div[0])
            sy2 = int(y1_box + rel_y2 * div[1])

            # Clip and swap
            sx1, sx2 = sorted([max(0, min(w, sx1)), max(0, min(w, sx2))])
            sy1, sy2 = sorted([max(0, min(h, sy1)), max(0, min(h, sy2))])

            if sx2 <= sx1 or sy2 <= sy1:
                continue

            # Decide ON/OFF fully
            region = gray_frame[sy1:sy2, sx1:sx2]
            mean_val = np.mean(region)
            if mean_val >= threshold:
                cv2.rectangle(out, (sx1, sy1), (sx2, sy2), color_on, -1)
            else:
                cv2.rectangle(out, (sx1, sy1), (sx2, sy2), color_off, -1)

    return out

# Precompute boxes
boxes = subdiv(res[0], res[1], div[0], div[1])

max_fps = 30
frame_time = 1.0 / max_fps
prev_time = time.time()
frame_count = 0

while cap.isOpened():
    start_time = time.time()
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.resize(frame, res)
    cv2.imshow('frame', frame)
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0
    if invert_flag:
        gray = 1.0 - gray  # invert if white background to black segments

    # Draw 7-segment display on black background
    frame_out = draw_seven_seg_binary(
        frame_shape=frame.shape,
        boxes=boxes,
        div=div,
        gray_frame=gray,
        threshold=thresh,
        color_on=(0,255,0),
        color_off=(0,0,0)
    )

    audio_frame, val = player.get_frame()
    if val == 'eof':
        break

    cv2.imshow("7seg_binary", frame_out)

    # FPS calculation
    frame_count += 1
    curr_time = time.time()
    elapsed = curr_time - prev_time
    if elapsed >= 1.0:  # every second
        fps = frame_count / elapsed
        print(f"FPS: {fps:.2f}")
        frame_count = 0
        prev_time = curr_time
    # Limit FPS
    elapsed = time.time() - start_time
    if elapsed < frame_time:
        time.sleep(frame_time - elapsed)

    if cv2.waitKey(1) & 0xFF == 27:
        break

cap.release()
cv2.destroyAllWindows()
player.close_player()