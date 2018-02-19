#include "stdafx.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "rad.h"


#define NULL 0


FILE *oplFile;




unsigned char fileRead(FILE *file) {
	unsigned char byte[1];
	if (fread(byte, sizeof(byte), 1, file)) {
		return byte[0];
	}
	return 0;
}


unsigned short fileReadWord(FILE *file) {
	unsigned short value = fileRead(file);
	value += fileRead(file) << 8;
	return value;
}


Song loadRAD(FILE *radFile) {
	Song song;

	fseek(radFile, 0x11, SEEK_SET);
	unsigned char songProps = fileRead(radFile);
	song.hasSlowTimer = (songProps & 0x40) != 0x00;
	song.initialSpeed = songProps & 0x1F;

	// Skip song description block.
	if ((songProps & 0x80) != 0x00) {
		while (fileRead(radFile) != 0x00);
	}

	// Read instruments.
	unsigned char instrumentNum = 0;
	while ((instrumentNum = fileRead(radFile)) != 0x00) {
		Instrument *instrument = new Instrument;
		instrument->carProps = fileRead(radFile);
		instrument->modProps = fileRead(radFile);
		instrument->carLevel = fileRead(radFile);
		instrument->modLevel = fileRead(radFile);
		instrument->carAttackDecay = fileRead(radFile);
		instrument->modAttackDecay = fileRead(radFile);
		instrument->carSustainRelease = fileRead(radFile);
		instrument->modSustainRelease = fileRead(radFile);
		instrument->channelProps = fileRead(radFile);
		instrument->carWaveform = fileRead(radFile);
		instrument->modWaveform = fileRead(radFile);
		song.instruments[instrumentNum - 1] = instrument;
	}

	// Read order list.
	song.songLength = fileRead(radFile);
	for (int i = 0; i < song.songLength; i++) {
		song.orders[i] = fileRead(radFile);
	}

	// Read pattern offsets.
	for (int i = 0; i < 32; i++) {
		Pattern *pattern = new Pattern;
		pattern->fileOffset = fileReadWord(radFile);
		song.patterns[i] = pattern;
	}

	// Read pattern data.
	for (int i = 0; i < 32; i++) {
		Pattern *pattern = song.patterns[i];
		if (pattern->fileOffset != 0x00) {

			// Clear the pattern.
			for (int l = 0; l < 64; l++) {
				for (int c = 0; c < 9; c++) {
					Note *note = new Note;
					pattern->notes[l][c] = note;
				}
			}

			// Load pattern data.
			unsigned char line     = 0;
			unsigned char channel  = 0;
			unsigned char noteData = 0;
			fseek(radFile, pattern->fileOffset, SEEK_SET);
			do {
				line = fileRead(radFile);

				do {
					channel = fileRead(radFile);
					Note *note = pattern->notes[line & 0x3F][channel & 0x0F];
	
					noteData = fileRead(radFile);
					note->instrument = (noteData & 0x80) >> 3;
					note->octave     = (noteData & 0x70) >> 4;
					note->note       = noteData & 0x0F;

					noteData = fileRead(radFile);
					note->instrument += (noteData & 0xF0) >> 4;
					note->effect     = noteData & 0x0F;
					note->parameter  = note->effect ? fileRead(radFile) : 0x00;
				} while ((channel & 0x80) == 0x00);
			} while ((line & 0x80) == 0x00);
		}
	}

	return song;
}


unsigned char tick = 0;
unsigned char line = 0;
unsigned char order = 0;
unsigned char speed;
Pattern *currPattern;


void setInstrument(unsigned char channel, Instrument *instrument) {
	unsigned char registerOffset;

	registerOffset = OPL2.getRegisterOffset(channel, OPL2.MODULATOR);
	OPL2.setRegister(0x20 + registerOffset, instrument->modProps);
	OPL2.setRegister(0x60 + registerOffset, instrument->modAttackDecay);
	OPL2.setRegister(0x80 + registerOffset, instrument->modAttackDecay);
	OPL2.setRegister(0xE0 + registerOffset, instrument->modWaveform);

	registerOffset = OPL2.getRegisterOffset(channel, OPL2.CARRIER);
	OPL2.setRegister(0x20 + registerOffset, instrument->carProps);
	OPL2.setRegister(0x60 + registerOffset, instrument->carAttackDecay);
	OPL2.setRegister(0x80 + registerOffset, instrument->carAttackDecay);
	OPL2.setRegister(0xE0 + registerOffset, instrument->carWaveform);

	OPL2.setRegister(0xC0 + channel, instrument->channelProps);
}


// !!! In OPL2 the result of getFrequency should be unsigned!!1!
void setFrequency(unsigned char channel, unsigned char octave, unsigned char note) {
	float frequency = OPL2.getNoteFrequency(channel, octave, note - 1);
	OPL2.setBlock(channel, OPL2.getFrequencyBlock(frequency));
	OPL2.setFrequency(channel, frequency);
}


void initPlayer(Song song) {
	order = 0;
	line = 0;
	tick = 0;

	speed = song.initialSpeed;
}


void playMusic(Song song) {
	unsigned long time = millis();

	if (tick == 0) {
		for (int channel = 0; channel < 9; channel ++) {
			Note *note = currPattern->notes[line][channel];
			
			// Stop note.
			if (note->note == 0x0F) {
				OPL2.setKeyOn(channel, false);
			}
			
			// Trigger note.
			else if (note->note > 0x00) {
				setInstrument(channel, song.instruments[note->instrument]);
				setFrequency(channel, note->octave, note->note);
				OPL2.setKeyOn(channel, true);
			}


		}
	} else {

	}

	tick = (tick + 1) % speed;
	if (tick == 0) {
		line = (line + 1) % 64;
		if (line == 0) {

		}
	}
	
	delay(max(0, (song.hasSlowTimer ? 55 : 20) - (millis() - time)));
}


unsigned long millis() { return 0; }
void delay(unsigned long t) {}
unsigned long max(int a, int b) { return 0; }


int main() {
	oplFile = fopen("c:\\SHOOT.RAD", "rb");
	Song song = loadRAD(oplFile);

	for (int i = 0; i < 32; i++) {
		if (i < 31 && song.instruments[i]) {
			delete song.instruments[i];
		}

		if (song.patterns[i]) {
			for (int l = 0; l < 64; l++) {
				for (int c = 0; c < 9; c++) {
					delete song.patterns[i]->notes[l][c];
				}
			}
			delete song.patterns[i];
		}
	}

	return 0;
}
