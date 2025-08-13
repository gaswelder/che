// Writes the tetris song wav to stdout. Pipe it to mpv or aplay.
#import formats/wav

typedef {
	float freq;
	float dur;
} note_t;

note_t notes[] = {
    {659.25, 0.5}, {493.88, 0.25}, {523.25, 0.25}, {587.33, 0.5},  {523.25, 0.25}, {493.88, 0.25},
    {440.00, 0.5}, {440.00, 0.25}, {523.25, 0.25}, {659.25, 0.5},  {587.33, 0.25}, {523.25, 0.25},
    {493.88, 0.5}, {493.88, 0.25}, {523.25, 0.25}, {587.33, 0.5},  {659.25, 0.5}, {523.25, 0.5},
	{440.00, 0.5}, {440.00, 0.5},  {0, 0.5}, {0, 0.25}, {587.33, 0.5},  {698.46, 0.25}, {880.00, 0.5},
	{783.99, 0.25}, {698.46, 0.25}, {659.25, 0.75},{523.25, 0.25}, {659.25, 0.5},  {587.33, 0.25}, {523.25, 0.25},
    {493.88, 0.5}, {493.88, 0.25}, {523.25, 0.25}, {587.33, 0.5},  {659.25, 0.5},
    {523.25, 0.5}, {440.00, 0.5},  {440.00, 0.5},  {0, 0.5},
};

const float RATE = 44100;

int main() {
	wav.writer_t *w = wav.open_writer(stdout);
	for (size_t i = 0; i < nelem(notes); i++) {
		float freq = notes[i].freq;
		float dur = notes[i].dur;
		note(w, freq, dur);
	}
	wav.close_writer(w);
	return 0;
}

void note(wav.writer_t *w, float freq, dur) {
	int nsamples = (int) (RATE * dur);
	for (int i = 0; i < nsamples; i++) {
		float v = sinf((float) i * 2.0 * M_PI / RATE * freq);
		int s = v * 32768;
		wav.write_sample(w, s, s);
	}
}
