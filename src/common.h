#pragma once

enum class Status : int {
    Idle = 0,
    Reading,
    Writing,
    Verifying,
    Exit,
    Canceled
};
