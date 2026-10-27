#pragma once

// GPIO numbers, not header positions. Reserve GPIO19/20 for native USB.
// Avoid boot strapping GPIO0/3/45/46 and memory GPIO26-32.
// GPIO33-37 and onboard LED pins depend on the exact module/board revision.
enum Pin {
    Pin_LeftEncoder_ChannelA = 5,
    Pin_LeftEncoder_ChannelB = 4,
    Pin_RightEncoder_ChannelA = 7,
    Pin_RightEncoder_ChannelB = 6,
    Pin_Magnetic_SDA = 15,
    Pin_Magnetic_SCL = 16
};
