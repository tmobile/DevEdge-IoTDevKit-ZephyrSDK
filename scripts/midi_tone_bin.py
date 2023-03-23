import argparse, os, struct, re
from time import sleep
from tqdm import tqdm
import pretty_midi as pm
import serial

MIN_NOTE = 147

# General midi instrument table
general_midi_table = {
    0: "Acoustic Grand", 32: "Acoustic Bass", 64: "Soprano Sax", 96: "FX 1 (rain)",
    1: "Bright Acoustic", 33: "Electric Bass(finger)", 65: "Alto Sax", 97: "FX 2 (soundtrack)",
    2: "Electric Grand", 34: "Electric Bass(pick)", 66: "Tenor Sax", 98: "FX 3 (crystal)",
    3: "Honky-Tonk", 35: "Fretless Bass", 67: "Baritone Sax", 99: "FX 4 (atmosphere)",
    4: "Electric Piano 1", 36: "Slap Bass 1", 68: "Oboe", 100: "FX 5 (brightness)",
    5: "Electric Piano 2", 37: "Slap Bass 2", 69: "English Horn", 101: "FX 6 (goblins)",
    6: "Harpsichord", 38: "Synth Bass 1", 70: "Bassoon", 102: "FX 7 (echoes)",
    7: "Clav", 39: "Synth Bass 2", 71: "Clarinet", 103: "FX 8 (sci-fi)",
    8: "Celesta", 40: "Violin", 72: "Piccolo", 104: "Sitar",
    9: "Glockenspiel", 41: "Viola", 73: "Flute", 105: "Banjo",
    10: "Music Box", 42: "Cello", 74: "Recorder", 106: "Shamisen",
    11: "Vibraphone", 43: "Contrabass", 75: "Pan Flute", 107: "Koto",
    12: "Marimba", 44: "Tremolo Strings", 76: "Blown Bottle", 108: "Kalimba",
    13: "Xylophone", 45: "Pizzicato Strings", 77: "Shakuhachi", 109: "Bagpipe",
    14: "Tubular Bells", 46: "Orchestral Harp", 78: "Whistle", 110: "Fiddle",
    15: "Dulcimer", 47: "Timpani", 79: "Ocarina", 111: "Shanai",
    16: "Drawbar Organ", 48: "String Ensemble 1", 80: "Lead 1 (square)", 112: "Tinkle Bell",
    17: "Percussive Organ", 49: "String Ensemble 2", 81: "Lead 2 (sawtooth)", 113: "Agogo",
    18: "Rock Organ", 50: "SynthStrings 1", 82: "Lead 3 (calliope)", 114: "Steel Drums",
    19: "Church Organ", 51: "SynthStrings 2", 83: "Lead 4 (chiff)", 115: "Woodblock",
    20: "Reed Organ", 52: "Choir Aahs", 84: "Lead 5 (charang)", 116: "Taiko Drum",
    21: "Accordion", 53: "Voice Oohs", 85: "Lead 6 (voice)", 117: "Melodic Tom",
    22: "Harmonica", 54: "Synth Voice", 86: "Lead 7 (fifths)", 118: "Synth Drum",
    23: "Tango Accordion", 55: "Orchestra Hit", 87: "Lead 8 (bass+lead)", 119: "Reverse Cymbal",
    24: "Acoustic Guitar(nylon)", 56: "Trumpet", 88: "Pad 1 (new age)", 120: "Guitar Fret Noise",
    25: "Acoustic Guitar(steel)", 57: "Trombone", 89: "Pad 2 (warm)", 121: "Breath Noise",
    26: "Electric Guitar(jazz)", 58: "Tuba", 90: "Pad 3 (polysynth)", 122: "Seashore",
    27: "Electric Guitar(clean)", 59: "Muted Trumpet", 91: "Pad 4 (choir)", 123: "Bird Tweet",
    28: "Electric Guitar(muted)", 60: "French Horn", 92: "Pad 5 (bowed)", 124: "Telephone Ring",
    29: "Overdriven Guitar", 61: "Brass Section", 93: "Pad 6 (metallic)", 125: "Helicopter",
    30: "Distortion Guitar", 62: "SynthBrass 1", 94: "Pad 7 (halo)", 126: "Applause",
    31: "Guitar Harmonics", 63: "SynthBrass 2", 95: "Pad 8 (sweep)", 127: "Gunshot",
}

parser = argparse.ArgumentParser(prog='midi_tone_bin')

parser.add_argument('action', choices=['list', 'convert'])
parser.add_argument('filename')
parser.add_argument('-i', '--instruments', default=None, help='Indices of instruments to include')
parser.add_argument('-o', '--output', default=None, help='Output file name')
parser.add_argument('-s', '--serial', default=None, help='Serial device to write the file to')
parser.add_argument('-b', '--baud', default=9600, help='Serial baud rate')

args = parser.parse_args()

if not os.path.exists(args.filename):
	print(f"No such file {args.filename}")
	parser.print_usage()
	exit(1)

mid = pm.PrettyMIDI(args.filename)

instruments = None

if args.instruments != None and args.instruments != '*':
	instruments = [int(i) for i in args.instruments.split(',')]

def list_instruments(mid : pm.PrettyMIDI):
	for i, ins in enumerate(mid.instruments):
		ins : pm.Instrument
		print('{:3}) Instrument {:03}: [{}] ({} Notes) {}'.format(
				i, ins.program, general_midi_table.get(ins.program, 'UNKNOWN'), 
				len(ins.notes), '(is drum)' if ins.is_drum else ''
			))

if args.action == 'list':
	list_instruments(mid)
	exit(0)

def midi_note_to_freq(m : int) -> float:
	return (2 ** ((m - 69) / 12)) * 440

flattened_tracks = []
# Flatten tracks

if instruments != None:
	used_instruments = [mid.instruments[i] for i in instruments]
else:
	used_instruments = mid.instruments

used_instruments : list[pm.Instrument]
all_notes : list[tuple[pm.Note, pm.Instrument]] = []
for instrument in used_instruments:
	all_notes += [(n, instrument) for n in instrument.notes]

all_notes = sorted(all_notes, key=lambda x: x[0].start)
note_n = 0

flattened_song : list[tuple[int,int]] = []

while note_n < len(all_notes):

	n, i = all_notes[note_n]
	if i.is_drum:
		hcn = -1
	else:
		hcn = n.pitch
	dur = n.duration
	
	# Prefer the highest note if multiple notes arrive at once
	while note_n + 1 < len(all_notes) and all_notes[note_n][0].start == all_notes[note_n + 1][0].start:
		note_n += 1
		n, i = all_notes[note_n]
		if not i.is_drum:
			if n.pitch > hcn:
				dur = n.duration
				hcn = n.pitch
	# If we're a drum, emulate with a short-pulsed low note
	if hcn == -1:	
		f = MIN_NOTE
		dur = 20 / 1000
	else:	
		f = midi_note_to_freq(hcn)
		f = max(MIN_NOTE, f)
	
	n, i = all_notes[note_n]
	time = n.start
	
	# Determine time till next note, pause/adjust duration of current note accordingly
	nnt = all_notes[note_n + 1][0].start if note_n + 1 < len(all_notes) else time + dur
	dur = min(nnt - time, dur)
	pdur = nnt - time - dur

	# Convert durations to integer milliseconds and frequency to integer Hz
	pdur = int(pdur * 1000) 
	flattened_song.append((int(f), int(dur * 1000)))

	# If a pause should occur, insert it
	if pdur:
		flattened_song.append((0, pdur))
	
	note_n += 1

# Convert to the double short representation, big endian
song_ns = [struct.pack('!2H', i, j) for i, j in flattened_song]

output_file = args.output 

#If no serial is specified, right to file on the local system
if not args.serial:
	output_file = "out.bin" if output_file == None else output_file
	with open(output_file, 'wb+') as f:
		for i in song_ns:
			f.write(i)
else: # Otherwise, right to the device's file system
	num_notes = len(song_ns)
	output_file = "/tmo/out.bin" if output_file == None else output_file
	with serial.Serial(args.serial, args.baud, timeout=5) as port:
		print("Writing binary to board...")
		bar = tqdm(total=num_notes, unit='Note(s)')
		port.write(f'fs rm "{output_file}"\r'.encode('ascii'))
		base_command = f'fs write "{output_file}" %s'
		port.write(b'\r')
		port.flushInput()

		for ns in song_ns:
			command = base_command % ' '.join(re.findall('..?', ns.hex()))
			port.write(command.encode('ascii'))
			port.write(b'\r')
			while not port.in_waiting:
				sleep(.001)
			while port.in_waiting:
				port.readline()
			bar.update(1)
		bar.close()
