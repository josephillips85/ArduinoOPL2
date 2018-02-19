/**
 * This is a demonstration for the OPL2 Audio Board. It demonstrates playing a little tune on 3 channels using a piano
 * from the MIDI instrument defenitions.
 *
 * The OPL2 board is connedted as follows:
 *   OPL2 Board | Raspberry Pi | Orange Pi
 *  ------------+--------------+-----------
 *     Reset    |      13      |    18     
 *     A0       |      12      |    16     
 *     Latch    |      11      |    12     
 *     Data     |      19      |    19     
 *     Shift    |      23      |    23     
 *
 * IMPORTANT: Make sure you set the correct BOARD_TYPE in OPL2.h. Default is set to Arduino.
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2016-05-04
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <OPL2.h>
#include <stdio.h>
#include <wiringPi.h>
#include "midi_instruments.h"


const int noteDefs[21] = {
  NOTE_A, NOTE_A - 1, NOTE_A + 1,
  NOTE_B, NOTE_B - 1, NOTE_B + 1,
  NOTE_C, NOTE_C - 1, NOTE_C + 1,
  NOTE_D, NOTE_D - 1, NOTE_D + 1,
  NOTE_E, NOTE_E - 1, NOTE_E + 1,
  NOTE_F, NOTE_F - 1, NOTE_F + 1,
  NOTE_G, NOTE_G - 1, NOTE_G + 1
};

float tempo;

struct Tune {
  const char *data;
  unsigned int channel;
  unsigned int octave;
  float noteDuration;
  float noteLength;
  unsigned int nextNoteTime;
  unsigned int releaseTime;
  unsigned int index;
};

const char tuneData[3][200] = {
  "t150m200o5l8egredgrdcerc<b>er<ba>a<a>agdefefedr4.regredgrdcerc<b>er<ba>a<a>agdedcr4.c<g>cea>cr<ag>cr<gfarfearedgrdcfrc<bagab>cdfegredgrdcerc<b>er<ba>a<a>agdedcr4.cro3c2",
  "m85o3l8crer<br>dr<ar>cr<grbrfr>cr<grbr>crer<gb>dgcrer<br>dr<ar>cr<grbrfr>cr<grbr>ceger4.rfrafergedrfdcrec<br>d<bar>c<agrgd<gr4.o4crer<br>dr<ar>cr<grbrfr>cr<grbr>cege",
  "m85o3l8r4gr4.gr4.er4.err4fr4.gr4.gr4.grr4gr4.er4.er4.frr4gr4>ccr4ccr4<aarraar4ggr4ffr4.ro4gab>dr4.r<gr4.gr4.err4er4.fr4.g"
};


OPL2 opl2;
struct Tune music[3];


float parseNumber(struct Tune *tune) {
  float number = 0.0f;
  if (tune->data[tune->index] != 0 && tune->data[tune->index] >= '0' && tune->data[tune->index] <= '9') {
    while (tune->data[tune->index] != 0 && tune->data[tune->index] >= '0' && tune->data[tune->index] <= '9') {
      number = number * 10 + (tune->data[tune->index ++] - 48);
    }
    tune->index --;
  }
  return number;
}


float parseDuration(struct Tune *tune) {
  // Get custom note duration or use default note duration.
  float duration = parseNumber(tune);
  if (duration == 0) {
    duration = 4.0f / tune->noteDuration;
  } else {
    duration = 4.0f / duration;
  }

  // See whether we need to double the duration
  if (tune->data[++ tune->index] == '.') {
    duration *= 1.5f;
  } else {
    tune->index --;
  }
  
  // Calculate note duration in ms.
  duration = (60.0f / tempo) * duration * 1000;
  return duration;
}


void parseNote(struct Tune *tune) {
  // Get index of note in base frequenct table.
  int note = (tune->data[tune->index ++] - 97) * 3;
  if (tune->data[tune->index] == '-') {
    note ++;
    tune->index ++;
  } else if (tune->data[tune->index] == '+') {
    note += 2;
    tune->index ++;
  }

  // Get duration, set delay and play note.
  float duration = parseDuration(tune);
  tune->nextNoteTime = millis() + duration;
  tune->releaseTime = millis() + (duration * tune->noteLength);

  opl2.playNote(tune->channel, tune->octave, noteDefs[note]);
}


void parseTune(struct Tune *tune) {
  while (tune->data[tune->index] != 0) {

    // Decrease octave if greater than 1.
    if (tune->data[tune->index] == '<' && tune->octave > 1) {
      tune->octave --;

    // Increase octave if less than 7.
    } else if (tune->data[tune->index] == '>' && tune->octave < 7) {
      tune->octave ++;

    // Set octave.
    } else if (tune->data[tune->index] == 'o' && tune->data[tune->index + 1] >= '1' && tune->data[tune->index + 1] <= '7') {
      tune->octave = tune->data[++ tune->index] - 48;

    // Set default note duration.
    } else if (tune->data[tune->index] == 'l') {
      tune->index ++;
      float duration = parseNumber(tune);
      if (duration != 0) tune->noteDuration = duration;

    // Set note length in percent.
    } else if (tune->data[tune->index] == 'm') {
      tune->index ++;
      tune->noteLength = parseNumber(tune) / 100.0;

    // Set song tempo.
    } else if (tune->data[tune->index] == 't') {
      tune->index ++;
      tempo = parseNumber(tune);

    // Pause.
    } else if (tune->data[tune->index] == 'p' || tune->data[tune->index] == 'r') {
      tune->index ++;
      tune->nextNoteTime = millis() + parseDuration(tune);
      break;

    // Next character is a note A..G so play it.
    } else if (tune->data[tune->index] >= 'a' && tune->data[tune->index] <= 'g') {
      parseNote(tune);
      break;
    }

    tune->index ++;
  }
}


int main(int argc, char **argv) {
  tempo = 120.0f;

  // Initialize 3 channels of the tune.
  for (int i = 0; i < 3; i ++) {
    struct Tune channel;
    channel.data = tuneData[i];
    channel.channel = i;
    channel.octave = 4;
    channel.noteDuration = 4.0f;
    channel.noteLength = 0.85f;
    channel.releaseTime = 0;
    channel.nextNoteTime = 0;
    channel.index = 0;
    music[i] = channel;
  }

  opl2.init();

  // Setup channels 0, 1 and 2.
  opl2.setInstrument(0, PIANO1);
  opl2.setBlock     (0, 5);
  opl2.setInstrument(1, PIANO1);
  opl2.setBlock     (1, 4);
  opl2.setInstrument(2, PIANO1);
  opl2.setBlock     (2, 4);
  
  int hasData = 1;
  while(hasData) {
  	hasData = 0;
	  for (int i = 0; i < 3; i ++) {
			if (millis() >= music[i].releaseTime && opl2.getKeyOn(music[i].channel)) {
				opl2.setKeyOn(music[i].channel, false);
			}
			if (millis() >= music[i].nextNoteTime && music[i].data[music[i].index] != 0) {
				parseTune(&music[i]);
			}
			hasData += music[i].data[music[i].index];
	  }
	  delay(1);
  }
  
  return 0;
}

