-- Custom motor matrix script for ArduPilot QuadPlane
-- Reflects a modified Y4 layout with non-standard motor positions
-- Motors 1 and 4 are front tiltrotors, 2 and 3 are fixed rear

-- Motor definitions:
-- {x, y, yaw_factor, motor_num}
-- Front Left  (Motor 1): -1.0,  0.7,  CCW  => +1 yaw
-- Rear Left   (Motor 2):  0.0, -1.0,  CW   => -1 yaw
-- Rear Right  (Motor 3):  0.0, -1.0,  CCW  => +1 yaw
-- Front Right (Motor 4):  1.0,  0.7,  CW   => -1 yaw

MotorsMatrix:add_motor_raw(1, -1.0,  0.7,  1, 1) -- Motor 1
MotorsMatrix:add_motor_raw(2,  0.0, -1.0, -1, 2) -- Motor 2
MotorsMatrix:add_motor_raw(3,  0.0, -1.0,  1, 3) -- Motor 3
MotorsMatrix:add_motor_raw(4,  1.0,  0.7, -1, 4) -- Motor 4

-- Finalize and set frame label
assert(MotorsMatrix:init(4), "Failed to init MotorsMatrix")
motors:set_frame_string("XTI Aerospace Kestrel V2")