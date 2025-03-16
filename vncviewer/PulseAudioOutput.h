/* Copyright 2025 Lixmantop
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifndef __PulseAudioOutput_H__
#define __PulseAudioOutput_H__

#include <stddef.h>
#include <pulse/simple.h>



class PulseAudioOutput {
  private:

  public:

    PulseAudioOutput();
    size_t addSamples(const uint8_t* data, size_t size);
    void notifyStreamingStartStop(bool isStart);

    bool submitSamples();
    bool isInit() const { return init; }
    void cleanBuffer();
    ~PulseAudioOutput();


  private:
    bool init;
    char *buffer;
    size_t size_buffer;

};

#endif // __PulseAudioOutput_H__
