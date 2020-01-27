//
// Created by Remus Lazar on 27.01.20.
//

#ifndef FIRMWARE_MEMORYUTILS_H
#define FIRMWARE_MEMORYUTILS_H

// @see https://github.com/mpflaga/Arduino-MemoryFree/blob/master/MemoryFree.cpp
extern "C" char* sbrk(int incr);

/**
 * Determines the free system memory
 *
 * @see http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1213583720/15
 *
 * @return free memory in bytes
 */
int freeMemory() {
    char top;
    return &top - reinterpret_cast<char*>(sbrk(0));
}

#endif //FIRMWARE_MEMORYUTILS_H
